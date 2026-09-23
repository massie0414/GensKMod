#ifndef GENS_32X_BOOT_H
#define GENS_32X_BOOT_H

// Input cartridge is in file (big endian) order. Builds original replacement
// boot programs when a complete external BIOS set is not available.
int Load_32X_Boot(const unsigned char *rom, unsigned size);
extern int Boot_32X_Internal;

#endif
