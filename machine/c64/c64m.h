#include "machine.h"
#include "ccore.h"
#include "cpu.h.inc"
#include "vicii.h"
#include "sid.h"
#include "c64dev.h"

#ifndef __C64MH__
#define __C64MH__

namespace c64{

#undef ISIO
#define ISIO(a) (SR(a,12)==0xd || a < 0x10)
#undef RMAP_IO
#define RMAP_IO(a) (SR(a & 0x1f00,8) )

#define W_(a,b,c,d,e){\
	void *tmp__;u32 __a=(a);\
	RMAP_##e(__a,tmp__,c);\
	__data=(b);\
	if(ISIO(a)){\
		if(_portfnc_write[RMAP_IO(a)] && !(((CCore *)this)->*_portfnc_write[RMAP_IO(a)])(a,tmp__,&__data,AM_WRITE|d))\
			tmp__=NULL;\
	}\
	if(tmp__) *((c *)tmp__)=__data;\
	ONMEMORYUPDATE(a,d|AM_WRITE,this);\
}

#define R_(a,b,c,d,e){\
	void *tmp__;u32 __a=(a);\
	RMAP_##e(__a,tmp__,c);\
	__data=*((c *)tmp__);\
	if(ISIO(a)){\
		if(_portfnc_read[RMAP_IO(a)])\
			(((CCore *)this)->*_portfnc_read[RMAP_IO(a)])(a,tmp__,&__data,AM_READ|d);\
	}\
	(b)=__data;\
	ONMEMORYUPDATE(a,d|AM_READ,this);\
}

#define WB(a,b)	W_(a,b,u8,AM_BYTE,W)
#define WW(a,b)	W_(a,b,u16,AM_WORD,W)
#define WL(a,b)

#define RW(a,b) R_(a,b,u16,AM_WORD,R)
#define RB(a,b) R_(a,b,u8,AM_BYTE,R)
#define RL(a,b) (b)=*((u32 *)&CCore::_mem[(a)])

#define RLPC(a,b) (b)=*((u32 *)&CCore::_mem[(a)])
#define RWPC(a,b) (b)=*((u16 *)&CCore::_mem[(a)])
#define RBPC(a,b) (b)=CCore::_mem[(a)]
#define WBPC(a,b) CCore::_mem[(a)]=(b)

#define REGI_A		0
#define REGI_X	 	1
#define REGI_Y		2
#define REGI_P		3
#define REGI_SP		4

#define C_BIT		0x1
#define Z_BIT		0x2
#define V_BIT		0x40
#define N_BIT		0x80
#define I_BIT		0x4
#define D_BIT		0x8
#define B_BIT		0x10

#define REG_(a) 	((u8 *)_regs)[(a)]
#define REG16_(a) 	(*((u16 *)&_regs[a]))
#define REG_SP 		REG16_(REGI_SP)
#define REG_A  		REG_(REGI_A)
#define REG_X  		REG_(REGI_X)
#define REG_Y  		REG_(REGI_Y)
#define REG_P  		REG_(REGI_P)


class m6502 :public CCore{
public:
	m6502();
	virtual ~m6502();
	virtual int Destroy();
	virtual int Reset();
	int Init(void *,u32 s=0x1000,u32 f=0);
	virtual int _enterIRQ(int n,u32 pc=0);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
	virtual int Disassemble(char *dest,u32 *padr);
	virtual int _dumpRegisters(char *p);
	virtual int OnException(u32,u32);
	virtual int Query(u32,void *);
	virtual int SaveState(IStreamer *p);
	virtual int LoadState(IStreamer *p);
protected:
	int _exec(u32);
	u8 _opcode,_irq_pending;
	u8 *_ioreg;
private:
	u8 ipc,dipc;
};

class c64m : public Machine,m6502,VICII,SID,c64dev{
public:
	c64m();
	virtual ~c64m();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return VICII::Draw(cr);};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);
	virtual int Run(u8 *,int cyc,void *obj);
	virtual int LoadSettings(void * &);
	virtual int SaveState(IStreamer *p);
	virtual int LoadState(IStreamer *p);
protected:
	s32 fn_write_io(u32,void *,void *,u32);
	s32 fn_read_io(u32,void *,void *,u32);
	s32 fn_write_vic(u32,void *,void *,u32);
	s32 fn_read_vic(u32,void *,void *,u32);
	s32 fn_write_sid(u32,void *,void *,u32);
	s32 fn_read_sid(u32,void *,void *,u32);
	s32 fn_write_cia(u32,void *,void *,u32);
	s32 fn_read_cia(u32,void *,void *,u32);
	void _set_keyboard_buffer(const char * str);
	void _write_to_screen(const char * str);

};

};

#endif