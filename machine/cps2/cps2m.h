#include "machine.h"
#include "ccore.h"
#include "cps2gpu.h"
#include "cps2d.h"
#include "cps2spu.h"
#include "eeprom.h"
#include "cpu.h.inc"

#ifndef __CPS2MH__
#define __CPS2MH__

namespace cps2{

#define CPS2IO(a) (SR(a&0xF000,12)|SR(a&0x00f00000,16))
#define M68IO(a) 0

#define MI_ROM 		MB(5)
#define MI_RAM 		(MI_ROM+MB(4))
#define MI_RAM1 	(MI_RAM+KB(64))
#define MI_VRAM 	(MI_RAM1+KB(4))
#define MI_SOUND 	(MI_VRAM+KB(192))

#define M_ROM 		(&_mem[MI_ROM])
#define M_RAM		(&_mem[MI_RAM])
#define M_RAM1		(&_mem[MI_RAM1])
#define M_VRAM		(&_mem[MI_VRAM])
#define M_SOUND		(&_mem[MI_SOUND])

#define ROM_(a) MA_(a,0,0x3fffff)
#define ROM_R(a,b) ROM_(a){b=&M_ROM[(a)&0x3fffff];}
#define ROM_W(a,b) ROM_(a){b=0;}

#define RAM_(a) MA_(a,0xff0000,0xffffff)
#define RAM_R(a,b) RAM_(a){b=&M_RAM[(u16)a];}
#define RAM_W(a,b) RAM_R(a,b)

#define RAM1_(a) MA_(a,0x660000,0x664000)
#define RAM1_R(a,b) RAM1_(a){b=&M_RAM1[(a)&0x3fff];}
#define RAM1_W(a,b)	RAM1_R(a,b)

#define VRAM_(a) MA_(a,0x900000,0x92ffff)
#define VRAM_R(a,b) VRAM_(a){b=&M_VRAM[(a)&0x3ffff];}
#define VRAM_W(a,b) VRAM_R(a,b)

#define SOUNDRAM_(a) MA_(a,0x618000,0x619fff)
#define SOUNDRAM_R(a,b) SOUNDRAM_(a){b=&M_SOUND[a&0x1fff];}
#define SOUNDRAM_W(a,b) SOUNDRAM_R(a,b)

#define RMAP_(a,b,c)\
MA_(a,0,0x3fffff){b=&_mem[KB(1) + ((a)&0x3fffff)];}\
MAE_ RAM_##c(a,b)\
MAE_ RAM1_##c(a,b)\
MAE_ SOUNDRAM_##c(a,b)\
MAE_ VRAM_##c(a,b)\
MAE_ MA_(a,0x400000,0x40000b){b=&_mem[MB(4)+(u16)a];}\
MAE_ b=NULL;

#define RMAP_PC(a,b,c)\
ROM_##c(a,b)\
MAE_ RAM_##c(a,b)\
MAE_ RAM1_##c(a,b)\
MAE_ SOUNDRAM_##c(a,b)\
MAE_ VRAM_##c(a,b)\
MAE_ MA_(a,0x400000,0x40000b){b=&_mem[(u16)a];}\
MAE_ b=NULL;

#include "m68000.h.inc"

class CPS2M : public Machine,M68000Cpu,CPS2GPU,CPS2D,CPS2Spu{
public:
	CPS2M();
	virtual ~CPS2M();
	virtual int Load(IGame *,char *);
	virtual int Load(void *){return 0;};
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return CPS2GPU::Draw(cr);};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);
	virtual int LoadSettings(void * &v);
protected:
	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32);
private:
	u32 _active_cpu;
	u16 _a_regs[0x80],*_b_regs;

	eeprom_device _eeprom;
};

};

#endif