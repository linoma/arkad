#include "machine.h"
#include "amiga500dev.h"

#ifndef __AMIGA500MH__
#define __AMIGA500MH__

namespace amiga{

class amiga500m :  public Machine,public amiga500dev{
public:
	amiga500m();
	virtual ~amiga500m();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return -1;};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);

	virtual s32 fn_write_io(u32,void *,void *,u32);
	virtual s32 fn_read_io(u32,void *,void *,u32);
	virtual s32 fn_write_cia(u32,void *,void *,u32);
	virtual s32 fn_read_cia(u32,void *,void *,u32);
};

};

#endif