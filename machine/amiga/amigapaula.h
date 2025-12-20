#include "pcmdac.h"
#include "ccore.h"
#include "general_device.h"

#ifndef __AMIGAPAULAH__
#define __AMIGAPAULAH__

namespace amiga{

class Paula : public PCMDAC{
public:
	Paula();
	virtual ~Paula();
	virtual int Init(void *,void *,u32);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
protected:
	int update(int);
	virtual int write(u32,u16);
	virtual int read(u32,u16 *);
	virtual int _dumpRegisters(char *);
private:
	u16 *_ioreg;
	u8 *_mem;
	u16 *_samples;
	u32 _cycles,_freq,_clock,_rinc;
};

};


#endif