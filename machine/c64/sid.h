#ifndef __SIDH__
#define __SIDH__

#include <pcmdac.h>

namespace c64{

class SID: public PCMDAC{
public:
	SID();
	virtual ~SID();
	virtual int Init(void *,void *,u32);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();

	static u32 FREQ;
	static u16 *_lfsr;
	static const u32 SID_FREQ=38912;
protected:
	int update(int);
	virtual int write(u32,u8);
	virtual int read(u32,u8 *);
	virtual int _remap(void *,void *);
	virtual int _dumpRegisters(char *p);
private:
	u8 *_ioreg,*_mem,_vol;
	u16 *_samples;
	u32 _cycles;
};

};

#endif
