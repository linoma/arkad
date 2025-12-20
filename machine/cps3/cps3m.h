#include "arkad.h"
#include "machine.h"
#include "ccore.h"
#include "cps3gpu.h"
#include "cps3dac.h"
#include "cps3dev.h"
#include "cpu.h.inc"

#ifndef __CPS3MH__
#define __CPS3MH__

namespace cps3{

#include "sh.h.inc"

#define ISIO(a) 	1
#define RMAP_IO(a)	SR(a,24)

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

#define M_ROM 		(&_mem[MI_ROM])
#define M_BIOS		(&_mem[MI_BIOS])
#define M_RAM		(&_mem[MI_RAM])
#define M_VRAM  	(&_mem[MI_VRAM])
#define M_PRAM		(&_mem[MI_PRAM])
#define M_SIMM		(&_mem[MI_SIMM])
#define M_CHRAM		(&_mem[MI_CHRAM])
#define M_CRAM		(&_mem[MI_CRAM])
#define M_DCRAM		(&_mem[MI_DCRAM])
#define M_PPU		(&_mem[MI_PPU])
#define M_SPU		(&_mem[MI_SPU])
#define M_EXT		(&_mem[MI_EXT])
#define M_EEPROM	(&_mem[MI_EEPROM])
#define M_SSRAM		(&_mem[MI_SSRAM])

#define MMA__(a,b,c) 	(((a)>=b && (a)<=c) || ((a)>=(0x20000000|b)	&& (a)<=(0x20000000|c)))
#define MMA_(a,b,c)  	if MMA__(a,b,c)

#define BIOS_(a) MA_(a,0,0x7ffff)
#define BIOS_R(a,b) BIOS_(a){b=&M_BIOS[(a)&0x7ffff];}
#define BIOS_W(a,b) BIOS_(a){b=NULL;LOGD("MEM bios write %x %x\n",a,_pc);}

#define RAM_(a) MA_(a,0x02000000,0x0207ffff)
#define RAM_R(a,b) RAM_(a){b=&M_RAM[(a)&0x7ffff];}
#define RAM_W(a,b) RAM_R(a,b)

#define CRAM_(a) MA_(a,0xc0000000,0xc00003FF)
#define CRAM_R(a,b) CRAM_(a){b=&M_DCRAM[(a)&0x3ff];}
#define CRAM_W(a,b) CRAM_(a){b=&M_CRAM[(a)&0x3ff];}

#define IOMEM_(a) MA_(a,0xE0000000,0xFFFFFFFF)
#define IOMEM_R(a,b) IOMEM_(a){b=&_ioreg[(a) & 0x1ff];}
#define IOMEM_W(a,b) IOMEM_(a){b=&_ioreg[(a) & 0x1ff];}

#define FRAM_(a) MA_(a,0x06000000,0x06ffffff)
#define FRAM_R(a,b) FRAM_(a){b=&M_ROM[(a)&0xffffff];}
#define FRAM_W(a,b) FRAM_R(a,b)

#define SPRITERAM_(a) MMA_(a,0x04000000,0x040bffff)
#define SPRITERAM_R(a,b) SPRITERAM_(a){b=&M_VRAM[(a) & 0xfffff];a &= 0xFFFFFFF;}
#define SPRITERAM_W(a,b) SPRITERAM_R(a,b)

#define PPU_(a) MA_(a,0x040c0000,0x040cffff)
#define PPU_R(a,b) PPU_(a){b=&M_PPU[(a) & 0x1ff];EnterDebugMode();}
#define PPU_W(a,b) PPU_R(a,b)

#define SPU_(a) MA_(a,0x040e0000,0x040effff)
#define SPU_R(a,b) SPU_(a){b=&M_SPU[(a) & 0x3ff];EnterDebugMode();}
#define SPU_W(a,b) SPU_R(a,b)

#define USERPPU_(a) MA_(a,0x240C0000,0x240CFFFF)
#define USERPPU_R(a,b) USERPPU_(a){b=&M_PPU[(a) & 0x3ff];a&=0xFFFFFFF;}
#define USERPPU_W(a,b) USERPPU_(a){b=&M_PPU[(a) & 0x3ff];a&=0xFFFFFFF;}

#define USERSPU_(a) MA_(a,0x240E0000,0x240EFFFF)
#define USERSPU_R(a,b) USERSPU_(a){b=&M_SPU[(a) & 0x3ff];a&=0xFFFFFFF;}
#define USERSPU_W(a,b) USERSPU_(a){b=&M_SPU[(a) & 0x3ff];a&=0xFFFFFFF;}

#define GFX_(a) MA_(a,0x24100000,0x241FFFFF)
#define GFX_R(a,b) GFX_(a){b=&M_CHRAM[(a&0xfffff)];a &= 0xFFFFFFF;}
#define GFX_W(a,b) GFX_R(a,b)

#define CHARRAM_(a) MMA_(a,0x24200000,0x243fffff)
#define CHARRAM_R(a,b) CHARRAM_(a){b=&M_SIMM[(a) & 0x1fffff];}
#define CHARRAM_W(a,b) CHARRAM_(a){b=&M_SIMM[(a) & 0x1fffff];}

#define USERIO_(a) MA_(a,0x25000000,0x2505FFFF)
#define USERIO_R(a,b) USERIO_(a){b=&M_EXT[((a)&0x3ff)+SR(a&0x1f0000,6)];a&=0xFFFFFFF;}
#define USERIO_W(a,b) USERIO_R(a,b)

#define USERSS_(a) MMA_(a,0x5040000,0x504FFFF)
#define USERSS_R(a,b) USERSS_(a){b=&M_SSRAM[a&0xffff];a&=0xFFFFFFF;}
#define USERSS_W(a,b) USERSS_(a){b=&M_SSRAM[a&0xffff];a&=0xFFFFFFF;}

#define EEPROM_(a) MMA_(a,0x5001000,0x50012FF)
#define EEPROM_R(a,b) EEPROM_(a){b=&M_EEPROM[a&0x3ff];a&=0xFFFFFFF;}
#define EEPROM_W(a,b) EEPROM_(a){b=0;a&=0xFFFFFFF;}

#define USERMMU_(a) MA_(a,0x24000000,0x24FFFFFF)
#define USERMMU_R(a,b) USERMMU_(a){b=&_mem[(a)&0x3FFFFFF];}
#define USERMMU_W(a,b) USERMMU_(a){b=NULL;}

#define USERMMU2_(a) MA_(a,0x26000000,0x27FFFFFF)
#define USERMMU2_R(a,b) USERMMU2_(a){b=&_mem[(a)&0x3FFFFFF];}
#define USERMMU2_W(a,b) USERMMU2_(a){b=NULL;}

#define USERRAM_(a) MA_(a,0x22000000,0x2207FFFF)
#define USERRAM_R(a,b) USERRAM_(a){b=&M_RAM[(a)&0x7ffff];}
#define USERRAM_W(a,b) USERRAM_R(a,b)

#define RMAP_(a,b,c)\
BIOS_##c(a,b)\
MAE_ RAM_##c(a,b)\
MAE_ SPRITERAM_##c(a,b)\
MAE_ FRAM_##c(a,b)\
MAE_ USERRAM_##c(a,b)\
MAE_ USERPPU_##c(a,b)\
MAE_ USERSPU_##c(a,b)\
MAE_ GFX_##c(a,b)\
MAE_ CHARRAM_##c(a,b)\
MAE_ EEPROM_##c(a,b)\
MAE_ USERSS_##c(a,b)\
MAE_ USERIO_##c(a,b)\
MAE_ USERMMU_##c(a,b)\
MAE_ USERMMU2_##c(a,b)\
MAE_ CRAM_W(a,b)\
MAE_ IOMEM_##c(a,b)\
MAE_{b=NULL;LOGE("MEM %x\n",a);}

#define RMAP_PC(a,b,c)\
BIOS_R(a,b)\
MAE_ CRAM_R(a,b)\
MAE_ FRAM_R(a,b)\
MAE_ b=NULL;

#define RMAP_L(a,b,c)\
BIOS_(a){b=&_memory[(a)&0x7ffff];}\
MAE_ RAM_##c(a,b)\
MAE_ SPRITERAM_##c(a,b)\
MAE_ USERRAM_##c(a,b)\
MAE_ USERMMU_##c(a,b)\
MAE_ CRAM_W(a,b)\
MAE_ IOMEM_##c(a,b)\
MAE_{b=NULL;LOGE("MEM L: %x\n",a);}

class CPS3M : public Machine,public SH2Cpu,public CPS3GPU,public CPS3DAC,public CPS3DEV{
public:
	CPS3M();
	virtual ~CPS3M();
	virtual int Query(u32,void *);

	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();
	virtual int Exec(u32);
	virtual int Draw(HDC cr=NULL){return CPS3GPU::Draw(cr);};
	virtual int OnEvent(u32,...);
	virtual int Dump(char **p);

	virtual int LoadSettings(void * &);
	virtual int SaveState(IStreamer *);
	virtual int LoadState(IStreamer *);

	virtual s32 fn_device(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_flash_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_flash_r(u32 a,pvoid,pvoid,u32);

	virtual s32 fn_crypted_rom_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_crypted_rom_r(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gfx_device_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gfx_device_r(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gpu_device_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gpu_device_r(u32 a,pvoid,pvoid,u32);
protected:
	u32 _decrypt(u32 address);
	u16 rotate_left(u16 value, int n);
	u16 rotxor(u16 val, u16 x);
	virtual int OnJump(u32){return -1;};
	virtual int OnException(u32,u32){return -1;};
	u32 _key1,_key2;
	int _do_dma(int ch);

};

};

#endif