#include "arkad.h"
#include "ccore.h"
#include "general_device.h"
#include "cpu.h.inc"
#include <vector>
#include "amigadenise.h"
#include "amigapaula.h"

#ifndef __AMIGA500DEVH__
#define __AMIGA500DEVH__

namespace amiga{

#define ISIO(a) 	((SR(a,20)&15)==0xb || (SR(a,20)&15)==0xd)
#define ISM68IO(a) 	ISIO(a)

#define MS_RAM2		KB(512)
#define MS_RAM		KB(512)

#define MI_RAM 		0
#define MI_RAM2 	(MI_RAM+MS_RAM)
#define MI_BIOS 	(MI_RAM2+MS_RAM2)

#define M_RAM 		(&CCore::_mem[MI_RAM])
#define M_RAM2 		(&CCore::_mem[MI_RAM2])
#define M_BIOS 		(&CCore::_mem[MI_BIOS])

#define RAM_(a) 	MA_(a,0,MS_RAM)
#define RAM_R(a,b) 	RAM_(a){b=&M_RAM[(a) & (MS_RAM-1)];}
#define RAM_W(a,b) 	RAM_R(a,b)

#define RAM2_(a) 	MA_(a,0xc00000,(0xc00000|(MS_RAM2-1)))
#define RAM2_R(a,b) RAM2_(a){b=&M_RAM2[(a) & (MS_RAM2-1)];}
#define RAM2_W(a,b) RAM2_R(a,b)

#define BIOS_(a) 	MA_(a,0xf80000,0xFFFFFF)
#define BIOS_R(a,b) BIOS_(a){b=&M_BIOS[(a) & 0x7ffff];}
#define BIOS_W(a,b) BIOS_(a){b=0;}

#define RMAP_(a,b,c){u32 __a__ = (a)&0xffffff;\
BIOS_##c(__a__,b)\
MAE_ RAM2_##c(__a__,b)\
MAE_ RAM_##c(__a__,b)\
MAE_ MA_(__a__,0xa00000,0xbFFFFF){b=(u8 *)M68000Cpu::_ioreg + M68IO(a);}\
MAE_ MA_(__a__,0xdff000,0xdFFFFF){b=(u8 *)M68000Cpu::_ioreg + M68IO(a);}\
MAE_ b=NULL;}

#define RMAP_PC(a,b,c) RMAP_(a,b,c)

#include "m68000.h.inc"

#define REG_BLTDDAT     (0x000/2)   /* ER A      Blitter destination early read (dummy address) */
#define REG_DMACONR     (0x002/2)   /* R  A   P  DMA control (and blitter status) read */
#define REG_VPOSR       (0x004/2)   /* R  A      Read vert most signif. bit (and frame flop) */
#define REG_VHPOSR      (0x006/2)   /* R  A      Read vert and horiz. position of beam */
#define REG_DSKDATR     (0x008/2)   /* ER     P  Disk data early read (dummy address) */
#define REG_JOY0DAT     (0x00A/2)   /* R    D    Joystick-mouse 0 data (vert,horiz) */
#define REG_JOY1DAT     (0x00C/2)   /* R    D    Joystick-mouse 1 data (vert,horiz) */
#define REG_CLXDAT      (0x00E/2)   /* R    D    Collision data register (read and clear) */
#define REG_ADKCONR     (0x010/2)   /* R      P  Audio, disk control register read */
#define REG_POT0DAT     (0x012/2)   /* R      P  Pot counter pair 0 data (vert,horiz) */
#define REG_POT1DAT     (0x014/2)   /* R      P  Pot counter pair 1 data (vert,horiz) */
#define REG_POTGOR      (0x016/2)   /* R      P  Pot port data read (formerly POTINP) */
#define REG_SERDATR     (0x018/2)   /* R      P  Serial port data and status read */
#define REG_DSKBYTR     (0x01A/2)   /* R      P  Disk data byte and status read */
#define REG_INTENAR     (0x01C/2)   /* R      P  Interrupt enable bits read */
#define REG_INTREQR     (0x01E/2)   /* R      P  Interrupt request bits read */
#define REG_DSKPTH      (0x020/2)   /* W  A      Disk pointer (high 3 bits) */
#define REG_DSKPTL      (0x022/2)   /* W  A      Disk pointer (low 15 bits) */
#define REG_DSKLEN      (0x024/2)   /* W      P  Disk length */
#define REG_DSKDAT      (0x026/2)   /* W      P  Disk DMA data write */
#define REG_REFPTR      (0x028/2)   /* W  A      Refresh pointer */
#define REG_VPOSW       (0x02A/2)   /* W  A      Write vert most signif. bit (and frame flop) */
#define REG_VHPOSW      (0x02C/2)   /* W  A      Write vert and horiz position of beam */
#define REG_COPCON      (0x02E/2)   /* W  A      Coprocessor control register (CDANG) */
#define REG_SERDAT      (0x030/2)   /* W      P  Serial port data and stop bits write */
#define REG_SERPER      (0x032/2)   /* W      P  Serial port period and control */
#define REG_POTGO       (0x034/2)   /* W      P  Pot port data write and start */
#define REG_JOYTEST     (0x036/2)   /* W    D    Write to all 4 joystick-mouse counters at once */
#define REG_STREQU      (0x038/2)   /* S    D    Strobe for horiz sync with VB and EQU */
#define REG_STRVBL      (0x03A/2)   /* S    D    Strobe for horiz sync with VB (vert. blank) */
#define REG_STRHOR      (0x03C/2)   /* S    D P  Strobe for horiz sync */
#define REG_STRLONG     (0x03E/2)   /* S    D    Strobe for identification of long horiz. line. */
#define REG_BLTCON0     (0x040/2)   /* W  A      Blitter control register 0 */
#define REG_BLTCON1     (0x042/2)   /* W  A      Blitter control register 1 */
#define REG_BLTAFWM     (0x044/2)   /* W  A      Blitter first word mask for source A */
#define REG_BLTALWM     (0x046/2)   /* W  A      Blitter last word mask for source A */
#define REG_BLTCPTH     (0x048/2)   /* W  A      Blitter pointer to source C (high 3 bits) */
#define REG_BLTCPTL     (0x04A/2)   /* W  A      Blitter pointer to source C (low 15 bits) */
#define REG_BLTBPTH     (0x04C/2)   /* W  A      Blitter pointer to source B (high 3 bits) */
#define REG_BLTBPTL     (0x04E/2)   /* W  A      Blitter pointer to source B (low 15 bits) */
#define REG_BLTAPTH     (0x050/2)   /* W  A      Blitter pointer to source A (high 3 bits) */
#define REG_BLTAPTL     (0x052/2)   /* W  A      Blitter pointer to source A (low 15 bits) */
#define REG_BLTDPTH     (0x054/2)   /* W  A      Blitter pointer to destination D (high 3 bits) */
#define REG_BLTDPTL     (0x056/2)   /* W  A      Blitter pointer to destination D (low 15 bits) */
#define REG_BLTSIZE     (0x058/2)   /* W  A      Blitter start and size (window width, height) */
#define REG_BLTCON0L    (0x05A/2)   /* W  A      Blitter control 0, lower 8 bits (minterms) */
#define REG_BLTSIZV     (0x05C/2)   /* W  A      Blitter V size (for 15 bit vertical size) (ECS) */
#define REG_BLTSIZH     (0x05E/2)   /* W  A      Blitter H size and start (for 11 bit H size) (ECS) */
#define REG_BLTCMOD     (0x060/2)   /* W  A      Blitter modulo for source C */
#define REG_BLTBMOD     (0x062/2)   /* W  A      Blitter modulo for source B */
#define REG_BLTAMOD     (0x064/2)   /* W  A      Blitter modulo for source A */
#define REG_BLTDMOD     (0x066/2)   /* W  A      Blitter modulo for destination D */
#define REG_BLTCDAT     (0x070/2)   /* W  A      Blitter source C data register */
#define REG_BLTBDAT     (0x072/2)   /* W  A      Blitter source B data reglster */
#define REG_BLTADAT     (0x074/2)   /* W  A      Blitter source A data register */
#define REG_DENISEID    (0x07C/2)   /* R    D    Denise ID: OCS = <open bus>, ECS = 0xFC, AGA = 0xF8 */
#define REG_DSKSYNC     (0x07E/2)   /* W      P  Disk sync pattern register for disk read */
#define REG_COP1LCH     (0x080/2)   /* W  A      Coprocessor first location register (high 3 bits) */
#define REG_COP1LCL     (0x082/2)   /* W  A      Coprocessor first location register (low 15 bits) */
#define REG_COP2LCH     (0x084/2)   /* W  A      Coprocessor second location register (high 3 bits) */
#define REG_COP2LCL     (0x086/2)   /* W  A      Coprocessor second location register (low 15 bits) */
#define REG_COPJMP1     (0x088/2)   /* S  A      Coprocessor restart at first location */
#define REG_COPJMP2     (0x08A/2)   /* S  A      Coprocessor restart at second location */
#define REG_COPINS      (0x08C/2)   /* W  A      Coprocessor instruction fetch identify */
#define REG_DIWSTRT     (0x08E/2)   /* W  A      Display window start (upper left vert-horiz position) */
#define REG_DIWSTOP     (0x090/2)   /* W  A      Display window stop (lower right vert.-horiz. position) */
#define REG_DDFSTRT     (0x092/2)   /* W  A      Display bit plane data fetch start (horiz. position) */
#define REG_DDFSTOP     (0x094/2)   /* W  A      Display bit plane data fetch stop (horiz. position) */
#define REG_DMACON      (0x096/2)   /* W  A D P  DMA control write (clear or set) */
#define REG_CLXCON      (0x098/2)   /* W    D    Collision control */
#define REG_INTENA      (0x09A/2)   /* W      P  Interrupt enable bits (clear or set bits) */
#define REG_INTREQ      (0x09C/2)   /* W      P  Interrupt request bits (clear or set bits) */
#define REG_ADKCON      (0x09E/2)   /* W      P  Audio, disk, UART control */
#define REG_AUD0LCH     (0x0A0/2)   /* W  A      Audio channel 0 location (high 3 bits) */
#define REG_AUD0LCL     (0x0A2/2)   /* W  A      Audio channel 0 location (low 15 bits) */
#define REG_AUD0LEN     (0x0A4/2)   /* W      P  Audio channel 0 length */
#define REG_AUD0PER     (0x0A6/2)   /* W      P  Audio channel 0 period */
#define REG_AUD0VOL     (0x0A8/2)   /* W      P  Audio channel 0 volume */
#define REG_AUD0DAT     (0x0AA/2)   /* W      P  Audio channel 0 data */
#define REG_AUD1LCH     (0x0B0/2)   /* W  A      Audio channel 1 location (high 3 bits) */
#define REG_AUD1LCL     (0x0B2/2)   /* W  A      Audio channel 1 location (low 15 bits) */
#define REG_AUD1LEN     (0x0B4/2)   /* W      P  Audio channel 1 length */
#define REG_AUD1PER     (0x0B6/2)   /* W      P  Audio channel 1 period */
#define REG_AUD1VOL     (0x0B8/2)   /* W      P  Audio channel 1 volume */
#define REG_AUD1DAT     (0x0BA/2)   /* W      P  Audio channel 1 data */
#define REG_AUD2LCH     (0x0C0/2)   /* W  A      Audio channel 2 location (high 3 bits) */
#define REG_AUD2LCL     (0x0C2/2)   /* W  A      Audio channel 2 location (low 15 bits) */
#define REG_AUD2LEN     (0x0C4/2)   /* W      P  Audio channel 2 length */
#define REG_AUD2PER     (0x0C6/2)   /* W      P  Audio channel 2 period */
#define REG_AUD2VOL     (0x0C8/2)   /* W      P  Audio channel 2 volume */
#define REG_AUD2DAT     (0x0CA/2)   /* W      P  Audio channel 2 data */
#define REG_AUD3LCH     (0x0D0/2)   /* W  A      Audio channel 3 location (high 3 bits) */
#define REG_AUD3LCL     (0x0D2/2)   /* W  A      Audio channel 3 location (low 15 bits) */
#define REG_AUD3LEN     (0x0D4/2)   /* W      P  Audio channel 3 length */
#define REG_AUD3PER     (0x0D6/2)   /* W      P  Audio channel 3 period */
#define REG_AUD3VOL     (0x0D8/2)   /* W      P  Audio channel 3 volume */
#define REG_AUD3DAT     (0x0DA/2)   /* W      P  Audio channel 3 data */
#define REG_BPL1PTH     (0x0E0/2)   /* W  A      Bit plane 1 pointer (high 3 bits) */
#define REG_BPL1PTL     (0x0E2/2)   /* W  A      Bit plane 1 pointer (low 15 bits) */
#define REG_BPL2PTH     (0x0E4/2)   /* W  A      Bit plane 2 pointer (high 3 bits) */
#define REG_BPL2PTL     (0x0E6/2)   /* W  A      Bit plane 2 pointer (low 15 bits) */
#define REG_BPL3PTH     (0x0E8/2)   /* W  A      Bit plane 3 pointer (high 3 bits) */
#define REG_BPL3PTL     (0x0EA/2)   /* W  A      Bit plane 3 pointer (low 15 bits) */
#define REG_BPL4PTH     (0x0EC/2)   /* W  A      Bit plane 4 pointer (high 3 bits) */
#define REG_BPL4PTL     (0x0EE/2)   /* W  A      Bit plane 4 pointer (low 15 bits) */
#define REG_BPL5PTH     (0x0F0/2)   /* W  A      Bit plane 5 pointer (high 3 bits) */
#define REG_BPL5PTL     (0x0F2/2)   /* W  A      Bit plane 5 pointer (low 15 bits) */
#define REG_BPL6PTH     (0x0F4/2)   /* W  A      Bit plane 6 pointer (high 3 bits) */
#define REG_BPL6PTL     (0x0F6/2)   /* W  A      Bit plane 6 pointer (low 15 bits) */
#define REG_BPLCON0     (0x100/2)   /* W  A D    Bit plane control register (misc. control bits) */
#define REG_BPLCON1     (0x102/2)   /* W    D    Bit plane control reg. (scroll value PF1, PF2) */
#define REG_BPLCON2     (0x104/2)   /* W    D    Bit plane control reg. (priority control) */
#define REG_BPLCON3     (0x106/2)   /* W    D    Bit plane control reg (enhanced features) */
#define REG_BPL1MOD     (0x108/2)   /* W  A      Bit plane modulo (odd planes) */
#define REG_BPL2MOD     (0x10A/2)   /* W  A      Bit Plane modulo (even planes) */
#define REG_BPLCON4     (0x10C/2)   /* W    D    Bit plane control reg. (display masks) */
#define REG_BPL1DAT     (0x110/2)   /* W    D    Bit plane 1 data (parallel-to-serial convert) */
#define REG_BPL2DAT     (0x112/2)   /* W    D    Bit plane 2 data (parallel-to-serial convert) */
#define REG_BPL3DAT     (0x114/2)   /* W    D    Bit plane 3 data (parallel-to-serial convert) */
#define REG_BPL4DAT     (0x116/2)   /* W    D    Bit plane 4 data (parallel-to-serial convert) */
#define REG_BPL5DAT     (0x118/2)   /* W    D    Bit plane 5 data (parallel-to-serial convert) */
#define REG_BPL6DAT     (0x11A/2)   /* W    D    Bit plane 6 data (parallel-to-serial convert) */
#define REG_BPL7DAT     (0x11C/2)   /* W    D    Bit plane 7 data (parallel-to-serial convert) */
#define REG_BPL8DAT     (0x11E/2)   /* W    D    Bit plane 8 data (parallel-to-serial convert) */
#define REG_SPR0PTH     (0x120/2)   /* W  A      Sprite 0 pointer (high 3 bits) */
#define REG_SPR0PTL     (0x122/2)   /* W  A      Sprite 0 pointer (low 15 bits) */
#define REG_SPR1PTH     (0x124/2)   /* W  A      Sprite 1 pointer (high 3 bits) */
#define REG_SPR1PTL     (0x126/2)   /* W  A      Sprite 1 pointer (low 15 bits) */
#define REG_SPR2PTH     (0x128/2)   /* W  A      Sprite 2 pointer (high 3 bits) */
#define REG_SPR2PTL     (0x12A/2)   /* W  A      Sprite 2 pointer (low 15 bits) */
#define REG_SPR3PTH     (0x12C/2)   /* W  A      Sprite 3 pointer (high 3 bits) */
#define REG_SPR3PTL     (0x12E/2)   /* W  A      Sprite 3 pointer (low 15 bits) */
#define REG_SPR4PTH     (0x130/2)   /* W  A      Sprite 4 pointer (high 3 bits) */
#define REG_SPR4PTL     (0x132/2)   /* W  A      Sprite 4 pointer (low 15 bits) */
#define REG_SPR5PTH     (0x134/2)   /* W  A      Sprite 5 pointer (high 3 bits) */
#define REG_SPR5PTL     (0x136/2)   /* W  A      Sprite 5 pointer (low 15 bits) */
#define REG_SPR6PTH     (0x138/2)   /* W  A      Sprite 6 pointer (high 3 bits) */
#define REG_SPR6PTL     (0x13A/2)   /* W  A      Sprite 6 pointer (low 15 bits) */
#define REG_SPR7PTH     (0x13C/2)   /* W  A      Sprite 7 pointer (high 3 bits) */
#define REG_SPR7PTL     (0x13E/2)   /* W  A      Sprite 7 pointer (low 15 bits) */
#define REG_SPR0POS     (0x140/2)   /* W  A D    Sprite 0 vert-horiz start position data */
#define REG_SPR0CTL     (0x142/2)   /* W  A D    Sprite 0 vert stop position and control data */
#define REG_SPR0DATA    (0x144/2)   /* W    D    Sprite 0 image data register A */
#define REG_SPR0DATB    (0x146/2)   /* W    D    Sprite 0 image data register B */
#define REG_SPR1POS     (0x148/2)   /* W  A D    Sprite 1 vert-horiz start position data */
#define REG_SPR1CTL     (0x14A/2)   /* W  A D    Sprite 1 vert stop position and control data */
#define REG_SPR1DATA    (0x14C/2)   /* W    D    Sprite 1 image data register A */
#define REG_SPR1DATB    (0x14E/2)   /* W    D    Sprite 1 image data register B */
#define REG_SPR2POS     (0x150/2)   /* W  A D    Sprite 2 vert-horiz start position data */
#define REG_SPR2CTL     (0x152/2)   /* W  A D    Sprite 2 vert stop position and control data */
#define REG_SPR2DATA    (0x154/2)   /* W    D    Sprite 2 image data register A */
#define REG_SPR2DATB    (0x156/2)   /* W    D    Sprite 2 image data register B */
#define REG_SPR3POS     (0x158/2)   /* W  A D    Sprite 3 vert-horiz start position data */
#define REG_SPR3CTL     (0x15A/2)   /* W  A D    Sprite 3 vert stop position and control data */
#define REG_SPR3DATA    (0x15C/2)   /* W    D    Sprite 3 image data register A */
#define REG_SPR3DATB    (0x15E/2)   /* W    D    Sprite 3 image data register B */
#define REG_SPR4POS     (0x160/2)   /* W  A D    Sprite 4 vert-horiz start position data */
#define REG_SPR4CTL     (0x162/2)   /* W  A D    Sprite 4 vert stop position and control data */
#define REG_SPR4DATA    (0x164/2)   /* W    D    Sprite 4 image data register A */
#define REG_SPR4DATB    (0x166/2)   /* W    D    Sprite 4 image data register B */
#define REG_SPR5POS     (0x168/2)   /* W  A D    Sprite 5 vert-horiz start position data */
#define REG_SPR5CTL     (0x16A/2)   /* W  A D    Sprite 5 vert stop position and control data */
#define REG_SPR5DATA    (0x16C/2)   /* W    D    Sprite 5 image data register A */
#define REG_SPR5DATB    (0x16E/2)   /* W    D    Sprite 5 image data register B */
#define REG_SPR6POS     (0x170/2)   /* W  A D    Sprite 6 vert-horiz start position data */
#define REG_SPR6CTL     (0x172/2)   /* W  A D    Sprite 6 vert stop position and control data */
#define REG_SPR6DATA    (0x174/2)   /* W    D    Sprite 6 image data register A */
#define REG_SPR6DATB    (0x176/2)   /* W    D    Sprite 6 image data register B */
#define REG_SPR7POS     (0x178/2)   /* W  A D    Sprite 7 vert-horiz start position data */
#define REG_SPR7CTL     (0x17A/2)   /* W  A D    Sprite 7 vert stop position and control data */
#define REG_SPR7DATA    (0x17C/2)   /* W    D    Sprite 7 image data register A */
#define REG_SPR7DATB    (0x17E/2)   /* W    D    Sprite 7 image data register B */
#define REG_COLOR00     (0x180/2)   /* W    D    Color table 00 */
#define REG_COLOR01     (0x182/2)   /* W    D    Color table 01 */
#define REG_COLOR02     (0x184/2)   /* W    D    Color table 02 */
#define REG_COLOR03     (0x186/2)   /* W    D    Color table 03 */
#define REG_COLOR04     (0x188/2)   /* W    D    Color table 04 */
#define REG_COLOR05     (0x18A/2)   /* W    D    Color table 05 */
#define REG_COLOR06     (0x18C/2)   /* W    D    Color table 06 */
#define REG_COLOR07     (0x18E/2)   /* W    D    Color table 07 */
#define REG_COLOR08     (0x190/2)   /* W    D    Color table 08 */
#define REG_COLOR09     (0x192/2)   /* W    D    Color table 09 */
#define REG_COLOR10     (0x194/2)   /* W    D    Color table 10 */
#define REG_COLOR11     (0x196/2)   /* W    D    Color table 11 */
#define REG_COLOR12     (0x198/2)   /* W    D    Color table 12 */
#define REG_COLOR13     (0x19A/2)   /* W    D    Color table 13 */
#define REG_COLOR14     (0x19C/2)   /* W    D    Color table 14 */
#define REG_COLOR15     (0x19E/2)   /* W    D    Color table 15 */
#define REG_COLOR16     (0x1A0/2)   /* W    D    Color table 16 */
#define REG_COLOR17     (0x1A2/2)   /* W    D    Color table 17 */
#define REG_COLOR18     (0x1A4/2)   /* W    D    Color table 18 */
#define REG_COLOR19     (0x1A6/2)   /* W    D    Color table 19 */
#define REG_COLOR20     (0x1A8/2)   /* W    D    Color table 20 */
#define REG_COLOR21     (0x1AA/2)   /* W    D    Color table 21 */
#define REG_COLOR22     (0x1AC/2)   /* W    D    Color table 22 */
#define REG_COLOR23     (0x1AE/2)   /* W    D    Color table 23 */
#define REG_COLOR24     (0x1B0/2)   /* W    D    Color table 24 */
#define REG_COLOR25     (0x1B2/2)   /* W    D    Color table 25 */
#define REG_COLOR26     (0x1B4/2)   /* W    D    Color table 26 */
#define REG_COLOR27     (0x1B6/2)   /* W    D    Color table 27 */
#define REG_COLOR28     (0x1B8/2)   /* W    D    Color table 28 */
#define REG_COLOR29     (0x1BA/2)   /* W    D    Color table 29 */
#define REG_COLOR30     (0x1BC/2)   /* W    D    Color table 30 */
#define REG_COLOR31     (0x1BE/2)   /* W    D    Color table 31 */
#define REG_BEAMCON0    (0x1dc/2)   // W  A      Programmable signal generator (ECS Agnus)
#define REG_DIWHIGH     (0x1E4/2)   /* W  A D    Display window upper bits for start/stop */
#define REG_FMODE       (0x1FC/2)   /* W  A D    Fetch mode */

/* DMACON bit layout */
#define DMACON_AUD0EN   0x0001
#define DMACON_AUD1EN   0x0002
#define DMACON_AUD2EN   0x0004
#define DMACON_AUD3EN   0x0008
#define DMACON_DSKEN    0x0010
#define DMACON_SPREN    0x0020
#define DMACON_BLTEN    0x0040
#define DMACON_COPEN    0x0080
#define DMACON_BPLEN    0x0100
#define DMACON_DMAEN    0x0200
#define DMACON_BLTPRI   0x0400
#define DMACON_RSVED1   0x0800
#define DMACON_RSVED2   0x1000
#define DMACON_BZERO    0x2000
#define DMACON_BBUSY    0x4000
#define DMACON_SETCLR   0x8000

/* BPLCON0 bit layout */
#define BPLCON0_RSVED1  0x0001
#define BPLCON0_ERSY    0x0002
#define BPLCON0_LACE    0x0004
#define BPLCON0_LPEN    0x0008
#define BPLCON0_BPU3    0x0010
#define BPLCON0_RSVED3  0x0020
#define BPLCON0_RSVED4  0x0040
#define BPLCON0_RSVED5  0x0080
#define BPLCON0_GAUD    0x0100
#define BPLCON0_COLOR   0x0200
#define BPLCON0_DBLPF   0x0400
#define BPLCON0_HOMOD   0x0800
#define BPLCON0_BPU0    0x1000
#define BPLCON0_BPU1    0x2000
#define BPLCON0_BPU2    0x4000
#define BPLCON0_HIRES   0x8000

/* INTENA/INTREQ bit layout */
#define INTENA_TBE      0x0001
#define INTENA_DSKBLK   0x0002
#define INTENA_SOFT     0x0004
#define INTENA_PORTS    0x0008
#define INTENA_COPER    0x0010
#define INTENA_VERTB    0x0020
#define INTENA_BLIT     0x0040
#define INTENA_AUD0     0x0080
#define INTENA_AUD1     0x0100
#define INTENA_AUD2     0x0200
#define INTENA_AUD3     0x0400
#define INTENA_RBF      0x0800
#define INTENA_DSKSYN   0x1000
#define INTENA_EXTER    0x2000
#define INTENA_INTEN    0x4000
#define INTENA_SETCLR   0x8000

#undef IOREG__
#undef M68IO
#undef IOREG_
#undef IOREG

#define IOREG__(a,b) 		((u16 *)(a) + (b))
#define IOREG_(a,b) 		*IOREG__(a,b)
#define IOREG(b) 			IOREG_(_ioreg,b)

#define M68IO(a) 			(SR((a) & 0x700000,9)| SR((a)&0x4000,5) | ((a)&0x1ff))
#define ACHIPREG_(b,a) 		IOREG_(b,SR(M68IO(0xdff000),1)|(a))
#define ACHIPREG32_(b,a) 	(*(u32 *)IOREG__(b,SR(M68IO(0xdff000),1)|(a)))

#define ACHIPREG(a) 		ACHIPREG_(_ioreg,a)
#define REG_SETCLR(a,b){ if (b & 0x8000){ a |= b & 0x7FFF;} else{ a &= ~b;}}

#define AMIGA_DISKSELECT				MACHINE_EVENT(0x100)

#define CIAA		1
#define CIAB		2
#define KEYBOARD	3
#define FLOPPY		4
#define AGNUS		5
#define COPPER		6
#define BLITTER		7

using namespace std;

class amiga500dev : public M68000Cpu,public Denise,public Paula{
public:
	amiga500dev();
	virtual ~amiga500dev();
	virtual int Reset();
	int Init(void *,void *);
	virtual int Destroy();
	virtual int _enterIRQ(int n,u32 pc=0);

	typedef  struct __copper_item{
		u8 _r;
		u16 _val,_x,_y;

		__copper_item(){_x=_y=_r=0;_val=0;};
		__copper_item(u16 x,u16  y,u8 r,u16 val){_x=x;_y=y;_r=r;_val=val;};
	} COPPERITEM;

	vector<amiga500dev::COPPERITEM> &_getCopperOpcodes(){return _agnus._copper._opcodes;};

	struct __fat_agnus{
		union{
			struct{
				unsigned int _enabled:1;
				unsigned int _busy:1;
				unsigned int _lock:1;
			};
			u32 _status;
		};

		int reset();
		int init(int,void *,void *,u32);
		int write(u32,u16);
		int read(u32,u16 *);
		int update(int);
		int _dumpRegisters(char *);

		struct __copper{
			vector<COPPERITEM>_opcodes;
			enum : u8{
				MOVE=1,
				WAIT,
				SKIP,
				STOP
			};
			union{
				struct{
					unsigned int _enabled:1;
					unsigned int _changed:1;
					unsigned int _busy:1;
					unsigned int _lock:1;
					unsigned int _state:3;
					unsigned int _mode:1;

				};
				u32 _status;
			};
			__copper();
			int init(int,void *,void *,u32);
			int reset();
			int update(int);
			int write(u32,u16);
			int read(u32,u16 *);
			int _dumpRegisters(char *);

			u32 _pc,_lc[2],_cycles,_freq;
			private:
			u16 *_ioreg;
			u8 *_mem;

			struct{
				union{
					u16 val;
					struct{
						unsigned int cv:1;
						unsigned int hv:7;
						unsigned int vv:7;
						unsigned int bw:1;
					};
					struct{
						unsigned int cv:1;
						unsigned int hv:7;
						unsigned int vv:8;
					} v;
				} w[2];

				int reset(){w[0].val=w[1].val=0;return 0;};
			} _wait;
		} _copper;

		struct __blitter{
			union{
				struct{
					unsigned int _enabled:1;
					unsigned int _changed:4;
					unsigned int _busy:1;
					unsigned int _nasty:1;
					unsigned int _state:3;
				};
				u16 _status;
			};
			__blitter();
			int init(int,void *,void *,u32);
			int reset();
			int update(int);
			int write(u32,u16);
			int read(u32,u16 *);
			private:
				int _ascending();
				int _line();
				int _descending();

			u16 *_ioreg,_width,_height;
			u8 *_mem;
			u32 _cycles,_delay,_freq;

			union{
				u8 *mem;
				u32 v32;
				s32 sv32;
				s16 sv;
				u16 v;
				u8 v8;
				s8 sv8;
			} _regs[30];

			enum : u8{
				CON0=0,CON1,
				APTH,BPTH,CPTH,DPTH,
				ADAT,BDAT,CDAT,
				AFWM,ALWM,
				AMOD,BMOD,CMOD,DMOD,
				APTL,
				APM=21,	BPM,CPM,DPM
			};
		} _blitter;

		__fat_agnus();

		private:
			u16 *_ioreg;
			u8 *_mem;
	} _agnus;

	struct __cia;
	struct __keyboard;

	class __fdc : public ADevice{
		public:
		typedef enum { ADF_NORMAL, ADF_EXT1, ADF_EXT2, ADF_FDI, ADF_IPF, ADF_CATWEASEL, ADF_PCDOS } drive_filetype;
		typedef enum { TRACK_AMIGADOS, TRACK_RAW, TRACK_RAW1, TRACK_PCDOS } image_tracktype;
		typedef struct __trackid{
			__trackid(){len=0;type=TRACK_AMIGADOS;offs=bitlen=track=sync=elen=0;};
			__trackid(u16 a,u32 b,image_tracktype c){len=a;offs=b;type=c;bitlen=track=sync=elen=0;};
			u16 elen;
			u32 offs,bitlen, track, sync,len;
			image_tracktype type;
		} trackid;

		struct __floppy : vector<trackid>{
			__floppy();
			virtual ~__floppy();
			union{
				struct{
					unsigned int _changed:1;
					unsigned int _on:1;
					unsigned int _ready:1;
					unsigned int _wp:1;
					unsigned int _dir:1;
					unsigned int _side:1;
					unsigned int _sides:1;
					unsigned int _ddhd:1;
					unsigned int _selected:1;
					unsigned int _eject:1;
					unsigned int _empty:1;
					unsigned int _refill:1;
					unsigned int _0:4;
					unsigned int _idx:2;
					unsigned int _turbo:1;
				};
				u16 _status;
			};

			u32 _cyl,_mfmpos,_len,_cycles[5],_ntracks,_nsecs;
			u32 _filetype,_ntrack,_speed,_cyls,_skip,_index;

			int _add(char *c=NULL);
			int _close();
			int _reset();
			int _readbit(int &);
			int _writebit(int &);
			int _decode(u32,void *);
			int _step(int);

			//protected:
			u16 *_buf;
			IStreamer *_streamer;
			string _fn;
			trackid _trackdata[2*83],*_track;
			u64 _size;
		} _floppies[4],*_floppy;

		__fdc();
		~__fdc();
		virtual int Init(int,void *,void *,u32);
		int reset();
		int update(int);
		int write(u32,u16);
		int read(u32,u16 *);
		virtual int Trigger(u32,u32,u32,void *);
		int _add(char *c=NULL,int idx=-1);

		protected:
		int _write();
		int _start();
		int _end();
		int _abort();

		union{
			struct{
				unsigned int _dmaen:1;
				unsigned int _dir:1;
				unsigned int _step_pulse:1;
				unsigned int _step:1;
				unsigned int _side:1;
				unsigned int _selected:4;
				unsigned int _state:2;
				unsigned int _on:1;
				unsigned int _bit:4;
				unsigned int _index:1;
				unsigned int _sync:1;
			};
			u32 _status;
		};
		enum : u8{
			ADKCON=0,DSKSYNC,DSKLEN,DMACON,DSKBYT,DMAVAL,DMAREG,LINE,DSKPT
		};

		private:
		u16 _regs[12];
		u8 *_ciaareg,*_ciabreg;
		u32 _cycles;
		IDevice *_ciaa,*_ciab;
	} _fdc;

	struct __cia : IBridge{
		enum :u8 {A,B};

		enum :u8 {
			PRA = 0,PRB,DDRA,DDRB,TA_LO,TA_HI,
			TB_LO,TB_HI,TOD_10THS,
			TOD_SEC,TOD_MIN,TOD_HR,	SDR,ICR,CRA,CRB,IMR,TOD_A10THS,TOD_ASEC,TOD_AMIN,TOD_AHR
		};

		struct __timer{
			union{
				struct{
					unsigned int _start:1;
					unsigned int _pbon:1;
					unsigned int _omode:1;
					unsigned int _rmode:1;
					unsigned int _load:1;
					unsigned int _inmode:1;
					unsigned int _spmode:1;
					unsigned int _todin:1;
				} a;
				struct{
					unsigned int _start:1;
					unsigned int _pbon:1;
					unsigned int _omode:1;
					unsigned int _rmode:1;
					unsigned int _load:1;
					unsigned int _inmode:2;
					unsigned int _alarm:1;
				} b;
				struct{
					unsigned int _start:1;
					unsigned int _pbon:1;
					unsigned int _omode:1;
					unsigned int _rmode:1;
					unsigned int _load:1;
					unsigned int _inmode:2;
				} c;
				u8 _value;
			} _control;
			int reset();
			int update(int cyc=0);
			int start();

			u16 _count,_load;
			u32 _cycles,_freq;
			u8 _idx;
		} _timers[2];

		struct __tod{
			union{
				struct{
					unsigned int _enabled:1;
					unsigned int _sync:1;
					unsigned int _ae:1;
					unsigned int _changed:1;
					unsigned int _latch:1;
				};
				u32 _status;
			};
			u32 _count,_cycles,_freq,_alarm,_countl;
			u8 reg[8];
			int reset();
			int enable(int);
			int update(int cyc=0);
			int alarm(u32);
			int latch(int);
		} _tod;

		struct __sdr{
			union{
				struct{
					unsigned int _enabled:1;
					unsigned int _mode:1;
				};
				u32 _status;
			};
			int reset();
			int update(int cyc=0);
			int write(u8);
			u8 _idx,_shift,_bits;
			u32 _count,_cycles,_freq;
		} _sdr;

		__cia();
		virtual int Trigger(u32,u32,u32,void *);
		virtual int Connect(IDevice *,u32,u32);
		virtual int Read(void *,u32,u32 *r=0){return -1;};
		virtual int Write(void *,u32,u32 *r=0){return -1;};
		int enterIRQ(u8);
		int reset();
		virtual int Init(int,void *,void *,u32);
		int write(u32,u16);
		int read(u32,u16 *);
		int update(int);
		int dump();
		//private:
			u16 *_ioreg;
			u8 *_mem,_regs[25],_idx;
		union{
			class __fdc *fdc;
			__keyboard *keyboard;
		} _extdevices;
		vector<IDevice *> _devices;
	} _cia[2] ;

	class __rs232 : public ADevice{
		public:

		enum:u16{
			SERDATR_RXD   = 0x0800, // serial data
			SERDATR_TSRE  = 0x1000, // transmit ready
			SERDATR_TBE   = 0x2000, // transmit buffer empty
			SERDATR_RBF   = 0x4000, // receive buffer full
			SERDATR_OVRUN = 0x8000  // receive buffer overrun
		};
		union{
			u32 _status;
			struct{
				unsigned int _enabled:1;
				unsigned int _rx_state:4;
				unsigned int _tx_state:4;
			};
		};

		int reset();
		int update(int cyc);
		virtual int Init(int,void *,void *,u32 f);
		int write(u32,u16);
		int read(u32,u16 *);

		u32 _cycles;
		private:
			u16 _rx_shift,_tx_shift;
	} _rs232;

	class __mouse : public ADevice,public vector<EVENTMSG>{
		public:
		union{
			u8 _status;
			struct{
				unsigned int __a:8;
				unsigned int _enabled:1;
			};
		};

		int reset();
		int update(int cyc);
		virtual int Init(int,void *,void *,u32 f);
		int write(u32,u16);
		int read(u32,u16 *);

		u32 _cycles,_buf[10];
		private:
			u8 *_ciaareg;
		IDevice *_ciaa;
	} _joy[2];

	struct __potgo{
		enum : u8{
			GO,POT0,POT1,GOR
		};

		union{
			u32 _status;
			struct{
				unsigned int _enabled:1;
				unsigned int _changed:1;
				unsigned int _wait:1;
			};
		};

		int reset();
		int update(int cyc);
		int Init(int,void *,void *,u32 f);
		int write(u32,u16);
		int read(u32,u16 *);

		u32 _cycles,_freq;
		private:
			u16 *_ioreg,_regs[5];
			u8 *_mem;
	} _potgo;

	struct __keyboard : vector<EVENTMSG>{
		union{
			u32 _status;
			struct{
				unsigned int _init:2;
				unsigned int _unread:2;
				unsigned int _wait:4;
			};
		};

		int reset();
		int update(int cyc);
		int Init(int,void *,void *,u32 f);
		int read(u8 *p);
		int _translate(u32 &,u32);
		protected:
		u32 _cycles,_freq;
		u8 _key,*_ciaareg;
		IDevice *_ciaa;
	} _keyboard;

protected:
	s32 fn_write_io(u32,void *,void *,u32);
	s32 fn_read_io(u32,void *,void *,u32);
};

};

#endif
