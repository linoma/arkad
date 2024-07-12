#include "pcmdac.h"
#include "ccore.h"
#include "general_device.h"
#include "cpu.h.inc"

#ifndef __AMIGAPAULAH__
#define __AMIGAPAULAH__

namespace amiga{

class Paula :  public PCMDAC{
public:
	Paula();
	virtual ~Paula();
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