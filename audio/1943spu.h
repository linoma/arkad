#include "pcmdac.h"
#include "ccore.h"
#include "cpu.h.inc"

#ifndef __1943SPUH__
#define __1943SPUH__

namespace M1943{

#include "z80.h.inc"

class M1943;

#include "ym2203.h.inc"

class M1943Spu : public PCMDAC,public ICpuTimerObj{
public:
	M1943Spu();
	virtual ~M1943Spu();
	virtual int Init(M1943 &);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
	virtual int Exec(u32);
	virtual int Run(u8 *,int,void *);

	virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32){return  -1;};

	virtual int _dumpRegisters(char *p);
	int _enterIRQ(int n,int v,u32 pc=0);
	int Load(void *m);
protected:
	struct __cpu : public Z80Cpu{
		public:
			virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
			virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
			virtual int OnChangeIRQ(u32,u32 *){return -1;};
			virtual int Exec(u32);

			u32 getTicks(){return _cycles;};
			u32 getFrequency(){return _freq;};
			__cpu() : Z80Cpu(){_idx=1;};
			M1943Spu *_spu;
	 } _cpu;

	struct __ym2203 _ym[2];

	u8  *_ipc;
	u16 *_samples;
	u32 _cycles;

	friend class M1943;
private:
	const int _freq=55930;
};

};

#endif