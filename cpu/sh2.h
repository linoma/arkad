#include "arkad.h"
#include "cps3.h"
#include "ccore.h"
#include <vector>

#ifndef __SH2CPPUH__
#define __SH2CPPUH__

#define SH_T   	BV(0)
#define SH_S  	BV(1)
#define SH_I   	0xf0
#define SH_Q   	BV(8)
#define SH_M   	BV(9)

#define R_IDX(a,b) (SR(a,b) & 0xf)
#define Ri(a) R_IDX(_opcode,a)
#define REG_ ((u32 *)_regs)
#define REG(i) (REG_[Ri(i)])
#define REG_SP (REG_[15])
#define REG_0   (REG_[0])

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

#define IOREG_(a) MA_(a,0xE0000000,0xFFFFFFFF)
#define IOREG_R(a,b) IOREG_(a){b=&_ioreg[(a) & 0x1ff];}
#define IOREG_W(a,b) IOREG_(a){b=&_ioreg[(a) & 0x1ff];}

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
MAE_ IOREG_##c(a,b)\
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
MAE_ IOREG_##c(a,b)\
MAE_{b=NULL;LOGE("MEM L: %x\n",a);}

#define DECRYPT(a)  MA__(a,0xc0000000,0xc00003FF)

#define R_(a,b,c,d,e,f){\
	void *__tmp;register u32 __a=(a);\
	RMAP_##d(__a,__tmp,c);\
	if(__tmp) __data=*((e *)__tmp);	else __data=0xFFFFFFFF;\
	if(_portfnc_read[SR(__a,24)])\
		(((CCore *)this)->*_portfnc_read[SR(__a,24)])(a,__tmp,&__data,AM_READ|f);\
	(b)=(e)__data;\
	ONMEMORYUPDATE(a,AM_READ|f,0);\
}

#define W_(a,b,c,d,e,f){\
	void *__tmp;register u32 __a=(a);\
	RMAP_##d(__a,__tmp,c);\
	__data=b;\
	if(__tmp){\
		*((e *)__tmp)=(e)__data;\
		if(DECRYPT((a))){\
			e __v=__data ^_decrypt(a);\
			RMAP_PC((a),__tmp,R);\
			*((e *)__tmp)=(e)__v;\
		}\
	}\
	if(_portfnc_write[SR(__a,24)]){\
		if(!(((CCore *)this)->*_portfnc_write[SR(__a,24)])(a,__tmp,&__data,AM_WRITE|f)){\
			__data=(b);\
		}\
	}\
	ONMEMORYUPDATE(a,f|AM_WRITE,0);\
}

#define RB_(a,b,c,d) R_(a,b,c,d,u8,AM_BYTE)
#define RW_(a,b,c,d) R_(a,b,c,d,u16,AM_WORD)
#define RL_(a,b,c,d) R_(a,b,c,d,u32,AM_DWORD)

#define WB_(a,b,c,d) W_(a,b,c,d,u8,AM_BYTE)
#define WW_(a,b,c,d) W_(a,b,c,d,u16,AM_WORD)
#define WL_(a,b,c,d) W_(a,b,c,d,u32,AM_DWORD)

#define WL(a,b) 	WL_((a),b,W, )
#define WW(a,b) 	WW_((((a)^2)),b,W, )
#define WB(a,b) 	WB_(((a)^3),b,W, )

#define RB(a,b) 	RB_(((a)^3),b,R, )
#define RW(a,b) 	RW_((((a)^2)),b,R, )
#define RL(a,b) 	RL_(((a)),b,R, )

#define RWPC(a,b){\
	register void *tmp__;\
	RMAP_PC(((a)^2),tmp__,NOARG);\
	__data=tmp__ ? *((u16 *)tmp__) : 0;\
	(b)=__data;\
}

//RW_(((a)^2),b,R,PC)
#define RLPC(a,b) 	RL_(((a) & ~1),b,R,PC)

class SH2Cpu : public CCore{
public:
	SH2Cpu();
	virtual ~SH2Cpu();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(void *);

	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);

	virtual int Query(u32,void *);
	virtual int _enterIRQ(int n,int v,u32 pc=0);
	virtual int Dump(char **p){if(_machine) return ((CCore *)_machine)->Dump(p);return -1;};
	virtual int _dumpRegisters(char *p);
protected:
	virtual int SaveState(IStreamer *);
	virtual int LoadState(IStreamer *);

	int _exec(u32);
	u32 _decrypt(u32 address);
	u16 rotate_left(u16 value, int n);
	u16 rotxor(u16 val, u16 x);
	virtual int Disassemble(char *dest,u32 *padr);

	u8 *_ioreg;
	u16 _opcode;
	u32 _sr,_pr,_mach,_macl,_gbr,_vbr,_key1,_key2;

	union{
		struct{
			unsigned int delay:3;
			unsigned int pc:32;
			unsigned int irq:1;
		};
		u64 _value;

		void _create_slot(int d,u32 p){
			delay=d+1;
			pc=p;
		};
	} _ppl;
};
#endif