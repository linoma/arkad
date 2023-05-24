#include "machine.h"
#include "ccore.h"

#ifndef __CPS2MH__
#define __CPS2MH__
namespace cps2{

#include "m68000.h.inc"

class CPS2M : public Machine,M68000Cpu{
public:
	CPS2M();
	virtual ~CPS2M();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...){return 0;};
	virtual int Draw(cairo_t *){return 0;};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);
};

};

#endif