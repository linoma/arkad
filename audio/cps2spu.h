#include "pcmdac.h"
#include "ccore.h"
#include "general_device.h"
#include "machine.h"

namespace cps2{


#ifndef __CPS2SPUH__
#define __CPS2SPUH__

#include "z80.h.inc"

class CPS2M;

class CPS2Spu : public PCMDAC,public ICpuTimerObj{
public:
	CPS2Spu();
	virtual ~CPS2Spu();
	virtual int Init(void *,void *);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
	virtual int Exec(u32);
	virtual int Run(u8 *,int,void *);

	virtual s32 fn_mem_w(u32,u8 );
	virtual s32 fn_mem_r(u32,u8 *);
	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual int _dumpRegisters(char *p);
	int _enterIRQ(int n,int v,u32 pc=0);
	int Load(void *m);

	struct __latch<u8,1> _latch;
protected:
	struct __cpu : public Z80CpuShared{
		public:
			virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
			virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
			virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32){return -1;};
			virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32){return -1;};
			virtual int Exec(u32);
			virtual int OnChangeIRQ(u32,u32 *){return -1;};
			u32 getTicks(){return _cycles;};
			u32 getFrequency(){return _freq;};
			__cpu() : Z80CpuShared(){_idx=1;_freq=MHZ(8);};

			void *_spu;
	} _cpu;

	u16 *_samples;
	u32 _cycles;

	friend class CPS2M;
private:
	const int _freq=55930;
};

#endif

};
