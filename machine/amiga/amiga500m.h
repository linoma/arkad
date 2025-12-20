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
	virtual int Draw(HDC cr=NULL){return Denise::Draw(cr);};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);

	virtual int LoadSettings(void * &);

	virtual int Run(u8 *,int cyc,void *obj);
protected:
	s32 fn_write_cia(u32,void *,void *,u32);
	s32 fn_read_cia(u32,void *,void *,u32);

	int _loadBios(char *);

	u32 mo[10],keys[20];
//private:
};

};

#endif