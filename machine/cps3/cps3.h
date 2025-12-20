#include "arkad.h"

#ifndef __CPS3H__
#define __CPS3H__

#include "cpu.h.inc"

#define MI_DUMMY	KB(512)
#define MI_USER4	MI_DUMMY
#define MI_USER5	(MI_DUMMY + MB(16))
#define MI_ROM 		MB(80)
#define MI_BIOS		(MI_ROM + MB(16))
#define MI_RAM		(MI_BIOS + KB(512))
#define MI_CRAM		(MI_RAM + KB(512))
#define MI_DCRAM	(MI_CRAM + 0x400)
#define MI_VRAM  	(MI_DCRAM + 0x400)

#define MI_PRAM		(MI_VRAM + KB(512))
#define MI_SIMM		(MI_PRAM + KB(256))
#define MI_CHRAM	(MI_SIMM + MB(1))

#define MI_PPU		(MI_DCRAM + MB(12))
#define MI_SPU		(MI_PPU + 0x400)
#define MI_EXT		(MI_SPU + 0x400)
#define MI_EEPROM	(MI_EXT + 0x400)
#define MI_SSRAM	(MI_EXT + KB(32))

#endif