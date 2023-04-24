#include "arkad.h"

#ifndef __MACHINEH__
#define __MACHINEH__

#define MS_VBLANK	BV(16)

class Machine : public IMachine{
public:
	Machine(u32 sz=0);
	virtual ~Machine();
	virtual int Init();
	virtual int Destroy();
	virtual int Reset();
	virtual int Exec(u32)=0;
	virtual int Load(char *)=0;
	virtual int Redraw()=0;
protected:
	u8 *_memory;
	u32 _memsize;
	u64 _status;
};

#endif