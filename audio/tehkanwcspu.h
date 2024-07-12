#include "pcmdac.h"
#include "ccore.h"
#include "general_device.h"

#ifndef __TEHKANWSPUCH__
#define __TEHKANWSPUCH__

namespace tehkanwc{

#include "z80.h.inc"
#include "ay8910.h.inc"

class tehkanwc;

class tehkanwcSpu : public PCMDAC{
public:
	tehkanwcSpu();
	virtual ~tehkanwcSpu();

	virtual int Init(tehkanwc &);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
	virtual int Exec(u32);

	virtual s32 fn_mem_w(u32,u8 );
	virtual s32 fn_mem_r(u32,u8 *);
	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32);
	virtual int _dumpRegisters(char *p);
	int _enterIRQ(int n,int v,u32 pc=0);
	int Load(void *m);
	void _writeLatch(u32);
	u32 _readLatch();

	struct __latch<u8,2> _latch;
private:
	struct __cpu : public Z80Cpu{
		public:
			virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
			virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
			virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
			virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32);
			virtual int Exec(u32);
			virtual int OnChangeIRQ(u32,u32 *){return -1;};
			__cpu() : Z80Cpu(){_idx=1;_freq=MHZ(4.608);};
			tehkanwcSpu *_spu;
	 } _cpu;

	u16 *_samples;
	u32 _cycles,_nmi;

	struct __msm5205{
		public:
			static const int _freq=KHZ(8);

			union{
				struct{
				};
				u32 _value;
			};

			u8 *_mem,_data;
			s32 _signal,_step;
			int reset();
			int output(int);
			int init(int,tehkanwcSpu &);
		protected:
			int _lookup[0x310];
	} _adpcm;

	struct  __ay8910 _pwms[2];

	friend class tehkanwc;
};

};

#endif