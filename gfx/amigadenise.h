#include "gpu.h"
#include "tilecharmanager.h"

#ifndef __AMIGADENISEH__
#define __AMIGADENISEH__

namespace amiga{

class Denise : public GPU{
public:
	Denise();
	virtual ~Denise();
	int Run(u8 *,int cyc,void *obj);
	virtual int Init(void *,void *);
protected:
	virtual int write(u32,u16);
	virtual int read(u32,u16 *);

private:
	u16 *_io_regs;
	u8 *_mem;
};

};


#endif