#include "pcmdac.h"

#ifndef __CPS3DACH__
#define __CPS3DACH__

namespace cps3{
class CPS3M;

class CPS3DAC : public PCMDAC{
public:
	CPS3DAC();
	virtual ~CPS3DAC();
	virtual int Init(CPS3M &);
	void write_reg(u32,u32);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
protected:
	struct __channel{
		friend class CPS3DAC;
		union{
			struct{
				unsigned int _enabled:1;
				unsigned int _init:1;
				unsigned int _loop:1;
				unsigned int _play:1;
				unsigned int _idx:4;
			};
			u32 _value;
		};

		u32 _pos,_start,_end,_seek,_loop_start,_step;
		u32 _regs[8];
		s16 _volL,_volR;

		void write_reg(u8,u32);
		void enable(int);
		void reset();
		void start();
		int init(int,CPS3DAC &);
	} _channels[16];
private:
	u32 _cycles;
	u16 *_samples;
	void *_data,*_pcm_regs;

	const int _freq=37286;

	friend class CPS3M;
};

};

#endif