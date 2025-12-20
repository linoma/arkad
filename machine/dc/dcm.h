#include "arkad.h"
#include "machine.h"
#include "dcbios.h"

#ifndef ___DREMCASTMH__
#define ___DREMCASTMH__

namespace dc{

class DreamCast : public Machine,dcbios{
public:
	DreamCast();
	virtual ~DreamCast();

	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);

	virtual int Run(u8 *,int cyc,void *obj);
	virtual int Draw(HDC cr=NULL){return PVR2::Draw(cr);};
	virtual int LoadSettings(void * &);
protected:
	virtual int OnJump(u32);
	virtual int OnException(u32,u32);

	s32 fn_sh4_cache_regs_w(u32,pvoid,pvoid,u32);
	s32 fn_sh4_cache_regs_r(u32,pvoid,pvoid,u32);

};

};
#endif