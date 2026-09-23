#include <stdio.h>
#include <string.h>
#include "32x_boot.h"
#include "Rom.h"
#include "Mem_M68K.h"
#include "Mem_SH2.h"

int Boot_32X_Internal;

static unsigned be32(const unsigned char *p)
{
    return (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) |
           (unsigned(p[2]) << 8) | p[3];
}
static void put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v;
}
static void put32(unsigned char *p, unsigned v)
{
    put16(p, v >> 16); put16(p + 2, v);
}
static int read_bios(const char *path, unsigned char *out, unsigned size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int ok = fread(out, 1, size, f) == size && !ferror(f);
    fclose(f);
    return ok;
}

// Small SH2 assembler. Instructions and literal data stay in guest byte order.
// Using executable boot code also makes reset and save states work without a
// host-side pending-boot state or interception of game instructions.
struct BootCode {
    unsigned char *rom;
    unsigned pc, pool;
    BootCode(unsigned char *r) : rom(r), pc(0x200), pool(0x300) {}
    void op(unsigned v) { put16(rom + pc, v); pc += 2; }
    void imm(unsigned reg, unsigned value) {
        op(0xd000 | (reg << 8) | ((pool - ((pc + 4) & ~3)) / 4));
        put32(rom + pool, value); pool += 4;
    }
    void bf(unsigned target) { op(0x8b00 | (((int(target) - int(pc) - 4) / 2) & 255)); }
};

static void sh2_boot(unsigned char *out, unsigned size,
                     const unsigned char *cart, int slave)
{
    memset(out, 0, size);
    for (unsigned i = 0; i < 0x200; i += 4) put32(out + i, 0x1f0);
    put16(out + 0x1f0, 0xaffe); // unhandled exception: bra self; nop
    put16(out + 0x1f2, 0x0009);
    put32(out, 0x200); put32(out + 8, 0x200);
    put32(out + 4, slave ? 0x0603f800 : 0x06040000);
    put32(out + 12, be32(out + 4));
    BootCode b(out);
    b.imm(0, 0x20004000); b.op(0x401e); // ldc r0,gbr
    b.imm(1, 0x20004000);
    unsigned wait = b.pc;
    b.op(0x6010); b.op(0xc802); // mov.b @r1,r0; tst #ADEN,r0
    b.op(0x8900 | (((int(wait) - int(b.pc) - 4) / 2) & 255));
    if (!slave) {
        b.imm(2, 0x06000000); b.imm(3, sizeof(_32X_Ram) / 4);
        b.op(0xe000); // mov #0,r0
        unsigned clear = b.pc;
        b.op(0x2202); b.op(0x7204); b.op(0x4310); b.bf(clear);
        b.imm(1, 0x02000000 + be32(cart + 0x3d4));
        b.imm(2, 0x06000000 + be32(cart + 0x3d8));
        b.imm(3, be32(cart + 0x3dc));
        // Empty initial loads are valid. Skip the copy loop in that case.
        if (be32(cart + 0x3dc)) {
            unsigned copy = b.pc;
            b.op(0x6014); b.op(0x2200); b.op(0x7201); // byte copy
            b.op(0x4310); b.bf(copy); // dt r3; bf copy
        }
        // Let the cartridge's 68000 startup finish clearing communication RAM.
        b.imm(3, 0x10000);
        unsigned delay = b.pc; b.op(0x4310); b.bf(delay);
        // Read at execution time: Auto Fix Checksum can update the cartridge.
        b.imm(1, 0x0200018e); b.op(0x6011); // mov.w @r1,r0
        b.op(0xc114); // mov.w r0,@(0x28,gbr): checksum
    } else {
        b.imm(1, 0x4d5f4f4b);
        unsigned ready = b.pc;
        b.op(0xc608); b.op(0x3010); b.bf(ready); // wait for M_OK
    }
    b.imm(0, be32(cart + (slave ? 0x3ec : 0x3e8)));
    b.op(0x402e); // ldc r0,vbr
    b.imm(0, slave ? 0x535f4f4b : 0x4d5f4f4b);
    b.op(slave ? 0xc209 : 0xc208); // S_OK / M_OK
    b.imm(0, be32(cart + (slave ? 0x3e4 : 0x3e0)));
    b.op(0x402b); b.op(0x0009); // jmp @r0; nop
}

int Load_32X_Boot(const unsigned char *rom, unsigned size)
{
    int g = read_bios(_32X_Genesis_Bios, _32X_Genesis_Rom, 256);
    int m = read_bios(_32X_Master_Bios, _32X_MSH2_Rom, 2048);
    int s = read_bios(_32X_Slave_Bios, _32X_SSH2_Rom, 1024);
    Boot_32X_Internal = !(g && m && s);
    if (Boot_32X_Internal) {
        if (size < 0x400 || size > sizeof(_32X_Rom)) return 0;
        unsigned src = be32(rom + 0x3d4), dst = be32(rom + 0x3d8);
        unsigned len = be32(rom + 0x3dc);
        if (src > size || len > size - src || dst > sizeof(_32X_Ram) ||
            len > sizeof(_32X_Ram) - dst) return 0;
        memset(_32X_Genesis_Rom, 0, 256);
        put32(_32X_Genesis_Rom, be32(rom));
        // The cartridge supplies the 68000 jump table at ROM+$200.
        for (unsigned i = 1; i < 48; ++i)
            put32(_32X_Genesis_Rom + 4*i, 0x880200 + 6*(i-1));
        // Standard ROM access helpers at $c0 (byte write) and $d4 (banks).
        const unsigned short write[] = {
            0x08f9,0x0000,0x00a1,0x5107, // bset #0,$a15107 (RV)
            0x1280,                     // move.b d0,(a1)
            0x08b9,0x0000,0x00a1,0x5107, // bclr #0,$a15107
            0x4e75
        };
        const unsigned short banks[] = {
            0x08f9,0x0000,0x00a1,0x5107,
            0x48e7,0x8040,0x227c,0x00a1,0x30f1,0x7007,
            0x12d8,0x5289,0x51c8,0xfffa,0x4cdf,0x0201,
            0x08b9,0x0000,0x00a1,0x5107,0x4e75
        };
        for (unsigned i=0; i<sizeof(write)/2; ++i) put16(_32X_Genesis_Rom+0xc0+2*i,write[i]);
        for (unsigned i=0; i<sizeof(banks)/2; ++i) put16(_32X_Genesis_Rom+0xd4+2*i,banks[i]);
        sh2_boot(_32X_MSH2_Rom, 2048, rom, 0);
        sh2_boot(_32X_SSH2_Rom, 1024, rom, 1);
    }
    // Only the 68000 core uses word-swapped memory.
    for (unsigned i=0; i<256; i+=2) {
        unsigned char t=_32X_Genesis_Rom[i];
        _32X_Genesis_Rom[i]=_32X_Genesis_Rom[i+1]; _32X_Genesis_Rom[i+1]=t;
    }
    return 1;
}
