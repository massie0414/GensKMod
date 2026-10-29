#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "..\gens.h"

#include "common.h"
#include "message.h"
#include "watchers.h"
#include "layers.h"
#include "planes.h"
#include "disassembly.h"

#include "m68k.h"
#include "z80.h"
#include "z80_ram_panel.h"
#include "main_ram_panel.h"
#include "vdp.h"
#include "vdp_reg.h"
#include "sprites.h"
#include "ym2612.h"
#include "psg.h"

#include "s68k.h"
#include "cdc.h"
#include "cdgfx.h"
#include "cdprg.h"
#include "cd_transfer_panel.h"
#include "word_ram_transfer_panel.h"
#include "main_word_ram_transfer_panel.h"
#include "vram_transfer_panel.h"
#include "vdp_transfer_panel.h"
#include "genesis_ram_transfer_panel.h"
#include "sh2_dma_transfer_panel.h"
#include "sdram_transfer_panel.h"
#include "vram32x_transfer_panel.h"
#include "pwm_fifo_transfer_panel.h"
#include "../sh2_dma_transfer.h"
#include "../genesis_ram_transfer.h"
#include "../vram_transfer.h"
#include "program_ram_transfer_panel.h"
#include "cd_reg.h"

#include "mSH2.h"
#include "sSH2.h"
#include "vdp_32x.h"
#include "s32x_reg.h"
#include "sdram_32x.h"
#include "fps_32x.h"
#include "sound_32x.h"
#include "sound_cd.h"
#include "memory_32x.h"


ULONG	timer_KMod;

// timer related : TO REVAMP

void IncTimer_KMod(unsigned odom)
{
	if (timer_KMod)	timer_KMod += odom;
}

/* Dirty way to know which window is open, using original Gens Debug values */
UCHAR OpenedWindow_KMod[WIN_NUMBER]; /* 0: Window Closed, 1: Window Opened */
//TODO remove
HWND  HandleWindow_KMod[WIN_NUMBER];

void CloseWindow_KMod(UCHAR mode)
{
	if (mode < 1)	return;
	if (mode > WIN_NUMBER)	return;

	OpenedWindow_KMod[mode - 1] = 0;

	switch (mode)
	{
	case DMODE_DISASSEMBLY:
        disassembly_show(OpenedWindow_KMod[mode - 1]);
        break;
    case DMODE_MSG:
		message_show(FALSE);
		break;
	case DMODE_WATCHERS:
		watchers_show(FALSE);
		break;
	case DMODE_LAYERS:
		layers_show(FALSE);
		break;
	case DMODE_SHADOW:
		planes_show(4, FALSE);
		break;
	case DMODE_HIGHLIGHT:
		planes_show(5, FALSE);
		break;
	case DMODE_WINDOW:
		planes_show(3, FALSE);
		break;
	case DMODE_PLANE_A:
		planes_show(0, FALSE);
		break;
	case DMODE_SPRITE_SCREEN:
		planes_show(2, FALSE);
		break;
	case DMODE_PLANE_B:
		planes_show(1, FALSE);
		break; 
	case DMODE_68K:
		m68kdebug_show(FALSE);
		break;
	case DMODE_Z80:
		z80debug_show(FALSE);
		break;
	case DMODE_Z80_RAM:
		z80ram_show(FALSE);
		break;
	case DMODE_MAIN_RAM_USAGE:
		mainram_show(FALSE);
		break;
	case DMODE_VDP:
		vdpdebug_show(FALSE);
		break;
	case DMODE_VDP_REG:
		vdpreg_show(FALSE);
		break;
	case DMODE_SPRITES:
		sprites_show(FALSE);
		break;
	case DMODE_YM2612:
		ym2612_show(FALSE);
		break;
	case DMODE_PSG:
		psg_show(FALSE);
		break;
	case DMODE_CD_68K:
		s68kdebug_show(FALSE);
		break;
	case DMODE_CD_CDC:
		cdcdebug_show(FALSE);
		break;
	case DMODE_CD_TRANSFER:
		cdtransfer_show(FALSE);
		break;
	case DMODE_WORD_RAM_TRANSFER:
		wordramtransfer_show(FALSE);
		break;
	case DMODE_MAIN_WORD_RAM_TRANSFER:
		mainwordramtransfer_show(FALSE);
		break;
	case DMODE_VRAM_TRANSFER:
		vramtransfer_show(FALSE);
		break;
	case DMODE_PWM_FIFO_TRANSFER:
		pwmfifotransfer_show(FALSE);
		break;
	case DMODE_VRAM32X_TRANSFER:
		vram32xtransfer_show(FALSE);
		break;
	case DMODE_SDRAM_TRANSFER:
		sdramtransfer_show(FALSE);
		break;
	case DMODE_MASTER_SH2_DMA_TRANSFER:
		sh2dmatransfer_show(SH2_DMA_MASTER, FALSE);
		break;
	case DMODE_SLAVE_SH2_DMA_TRANSFER:
		sh2dmatransfer_show(SH2_DMA_SLAVE, FALSE);
		break;
	case DMODE_MAIN_RAM_TRANSFER:
		genesisramtransfer_show(GENESIS_RAM_MAIN, FALSE);
		break;
	case DMODE_SUB_RAM_TRANSFER:
		genesisramtransfer_show(GENESIS_RAM_SUB, FALSE);
		break;
	case DMODE_CRAM_TRANSFER:
		vdptransfer_show(VDP_TRANSFER_CRAM, FALSE);
		break;
	case DMODE_VSRAM_TRANSFER:
		vdptransfer_show(VDP_TRANSFER_VSRAM, FALSE);
		break;
	case DMODE_VDP_DMA_TRANSFER:
		vdptransfer_show(VDP_TRANSFER_DMA, FALSE);
		break;
	case DMODE_PROGRAM_RAM_TRANSFER:
		programramtransfer_show(FALSE);
		break;
	case DMODE_CD_GFX:
		cdgfx_show(FALSE);
		break;
	case DMODE_CD_PRG:
		cdprg_show(FALSE);
		break;
	case DMODE_CD_REG:
		cdreg_show(FALSE);
		break;
	case DMODE_32_MSH2:
		mSH2_show(FALSE);
		break;
	case DMODE_32_SSH2:
		sSH2_show(FALSE);
		break;
	case DMODE_32_VDP:
		vdp32x_show(FALSE);
		break;
	case DMODE_32_MEMORY:
        memory32x_show(FALSE);
        break;
	case DMODE_CD_PCM:
        soundcd_show(0, FALSE);
        break;
    case DMODE_CD_CDDA:
        soundcd_show(1, FALSE);
        break;
    case DMODE_32_SOUND:
        sound32x_show(FALSE);
        break;
	case DMODE_32_FPS:
		fps32x_show(FALSE);
		break;
	case DMODE_32_SDRAM:
		sdram32x_show(FALSE);
		break;
	case DMODE_32_REG:
		s32xreg_show(FALSE);
		break;
	default:
		ShowWindow(HandleWindow_KMod[mode - 1], SW_HIDE);
	}
}

void OpenWindow_KMod(UCHAR mode)
{
	if (mode < 1)	return;
	if (mode > WIN_NUMBER)	return;

	OpenedWindow_KMod[mode - 1] = 1;

	switch (mode)
	{
	case DMODE_DISASSEMBLY:
        disassembly_show(OpenedWindow_KMod[mode - 1]);
        break;
    case DMODE_MSG:
		message_show(TRUE);
		break;
	case DMODE_WATCHERS:
		watchers_show(TRUE);
		break;
	case DMODE_LAYERS:
		layers_show(TRUE);
		break;
	case DMODE_SHADOW:
		planes_show(4, TRUE);
		break;
	case DMODE_HIGHLIGHT:
		planes_show(5, TRUE);
		break;
	case DMODE_WINDOW:
		planes_show(3, TRUE);
		break;
	case DMODE_PLANE_A:
		planes_show(0, TRUE);
		break;
	case DMODE_SPRITE_SCREEN:
		planes_show(2, TRUE);
		break;
	case DMODE_PLANE_B:
		planes_show(1, TRUE);
		break;
	case DMODE_68K:
		m68kdebug_show(TRUE);
		break;
	case DMODE_Z80:
		z80debug_show(TRUE);
		break;
	case DMODE_Z80_RAM:
		z80ram_show(TRUE);
		break;
	case DMODE_MAIN_RAM_USAGE:
		mainram_show(TRUE);
		break;
	case DMODE_VDP:
		vdpdebug_show(TRUE);
		break;
	case DMODE_VDP_REG:
		vdpreg_show(TRUE);
		break;
	case DMODE_SPRITES:
		sprites_show(TRUE);
		break;
	case DMODE_YM2612:
		ym2612_show(TRUE);
		break;
	case DMODE_PSG:
		psg_show(TRUE);
		break;
	case DMODE_CD_68K:
		s68kdebug_show(TRUE);
		break;
	case DMODE_CD_CDC:
		cdcdebug_show(TRUE);
		break;
	case DMODE_CD_TRANSFER:
		cdtransfer_show(TRUE);
		break;
	case DMODE_WORD_RAM_TRANSFER:
		wordramtransfer_show(TRUE);
		break;
	case DMODE_MAIN_WORD_RAM_TRANSFER:
		mainwordramtransfer_show(TRUE);
		break;
	case DMODE_VRAM_TRANSFER:
		vramtransfer_show(TRUE);
		break;
	case DMODE_PWM_FIFO_TRANSFER:
		pwmfifotransfer_show(TRUE);
		break;
	case DMODE_VRAM32X_TRANSFER:
		vram32xtransfer_show(TRUE);
		break;
	case DMODE_SDRAM_TRANSFER:
		sdramtransfer_show(TRUE);
		break;
	case DMODE_MASTER_SH2_DMA_TRANSFER:
		sh2dmatransfer_show(SH2_DMA_MASTER, TRUE);
		break;
	case DMODE_SLAVE_SH2_DMA_TRANSFER:
		sh2dmatransfer_show(SH2_DMA_SLAVE, TRUE);
		break;
	case DMODE_MAIN_RAM_TRANSFER:
		genesisramtransfer_show(GENESIS_RAM_MAIN, TRUE);
		break;
	case DMODE_SUB_RAM_TRANSFER:
		genesisramtransfer_show(GENESIS_RAM_SUB, TRUE);
		break;
	case DMODE_CRAM_TRANSFER:
		vdptransfer_show(VDP_TRANSFER_CRAM, TRUE);
		break;
	case DMODE_VSRAM_TRANSFER:
		vdptransfer_show(VDP_TRANSFER_VSRAM, TRUE);
		break;
	case DMODE_VDP_DMA_TRANSFER:
		vdptransfer_show(VDP_TRANSFER_DMA, TRUE);
		break;
	case DMODE_PROGRAM_RAM_TRANSFER:
		programramtransfer_show(TRUE);
		break;
	case DMODE_CD_GFX:
		cdgfx_show(TRUE);
		break;
	case DMODE_CD_PRG:
		cdprg_show(TRUE);
		break;
	case DMODE_CD_REG:
		cdreg_show(TRUE);
		break;
	case DMODE_32_MSH2:
		mSH2_show(TRUE);
		break;
	case DMODE_32_SSH2:
		sSH2_show(TRUE);
		break;
	case DMODE_32_VDP:
		vdp32x_show(TRUE);
		break;
	case DMODE_32_MEMORY:
        memory32x_show(TRUE);
        break;
	case DMODE_CD_PCM:
        soundcd_show(0, TRUE);
        break;
    case DMODE_CD_CDDA:
        soundcd_show(1, TRUE);
        break;
    case DMODE_32_SOUND:
        sound32x_show(TRUE);
        break;
	case DMODE_32_FPS:
		fps32x_show(TRUE);
		break;
	case DMODE_32_SDRAM:
		sdram32x_show(TRUE);
		break;
	case DMODE_32_REG:
		s32xreg_show(TRUE);
		break;
	default:
		ShowWindow(HandleWindow_KMod[mode - 1], SW_SHOW);
	}

	Update_KMod();
}
