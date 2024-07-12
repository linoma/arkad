#include "machine.h"
#include "popeyespu.h"
#include "popeyegpu.h"
#include "game.h"
#include "cpu.h.inc"

#ifndef __POPEYEH__
#define __POPEYEH__

namespace POPEYE{

#include "z80.h.inc"

class Popeye : public Machine,public Z80Cpu,public PopeyeSpu,public PopeyeGpu{
public:
	Popeye();
	virtual ~Popeye();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();
	virtual int Exec(u32);

	//virtual int Load(void *){return 0;};

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return PopeyeGpu::Draw(cr);};
	virtual int Query(u32 what,void *pv);

	virtual int Dump(char **);

	virtual int LoadSettings(void * &);

	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
	virtual int OnChangeIRQ(u32,u32 *);

private:
	struct __dev{
		struct {
			u32 counter,enabled;
		} wd;
		struct{
			u8 val[2],shift;
		} prot;
		u8 _vblank,_ports[5];
		u32 nmi;
	} _dev;
};

class PopeyeGame : public FileGame{
public:
	PopeyeGame();
	virtual ~PopeyeGame();
};

};

#endif