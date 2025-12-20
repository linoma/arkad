#include "machine.h"
#include "popeyespu.h"
#include "popeyegpu.h"
#include "game.h"

#ifndef __POPEYEH__
#define __POPEYEH__

namespace POPEYE{

//#include "z80.h.inc"

class PopeyeM : public Machine,public Z80Cpu, public PopeyeSpu, public PopeyeGpu{
public:
	PopeyeM();
	virtual ~PopeyeM();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();
	virtual int Exec(u32);

	//virtual int Load(void *){return 0;};

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL);
	virtual int Query(u32 what,void *pv);

	virtual int Dump(char **);

	virtual int LoadSettings(void * &);

	s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	s32 fn_port_r(u32 a,pvoid,pvoid data,u32);
	s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
	s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
	virtual int OnChangeIRQ(u32,u32 *);
protected:
	u32 bitswap(int n,int val,...);
	struct __dev{
		public:
		struct {
			u32 counter,enabled;
		} wd;
		struct{
			u8 val[2],shift;
		} prot;
		u8 _vblank,_ports[9];
		u32 nmi;
		__dev(){reset();};
		int reset();
	} _dev;
};

class PopeyeGame : public FileGame{
public:
	PopeyeGame();
	virtual ~PopeyeGame();
};

};

#endif