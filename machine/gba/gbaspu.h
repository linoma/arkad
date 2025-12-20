#include "arkad.h"
#include <pcmdac.h>

#ifndef __GBASPUH__
#define __GBASPUH__


namespace gba{

class gbaspu: public PCMDAC{
public:
	gbaspu();
	virtual ~gbaspu();
	virtual int Init(void *,void  *);
	virtual int Reset();
	virtual int Update(u32 flags=0);
	virtual int Destroy();
protected:
	virtual int write(u32,u32);

	struct __pwm{
		union{
			u64 _status;
			struct{
				unsigned int _enabled:1;
				unsigned int _changed:1;
				unsigned int _vol:3;
				unsigned int _le:1;
				unsigned int _re:1;
				unsigned int _length_enabled:1;
				unsigned int _length:6;
				unsigned int _env:3;
				unsigned int _env_dir:1;
				unsigned int _env_vol:4;
				unsigned int _swp_freq:1;
				unsigned int _swp_shift:3;
				unsigned int _swp_length:3;
			};
		};
		__pwm(){_status=0;};
		virtual ~__pwm(){};
		virtual int Init(int,void *,void *,u32);
		virtual int write(u32,u16);
		virtual int reset();
		virtual int output(int,int &sampleL,int &sampleR);
		protected:
			void *_ioreg;
			u8 *_mem,_idx;
			u32 _pos,_fpos,_epos,_spos,_freq,_pllHz,_duty;
	} _pwm[2];

	struct __noiseg : __pwm{
		__noiseg();
		virtual ~__noiseg();
		virtual int reset();
		virtual int output(int,int &sampleL,int &sampleR);
		virtual int Init(int,void *,void *,u32);
		u8 *_lfsr7;
		u16 *_lfsr15;
	} _noiseg;

	struct __waveg  : __pwm {
		u8 *_wave;

		virtual int Init(int,void *,void *,u32);
		virtual int reset();
		virtual int output(int,int &sampleL,int &sampleR);
	} _waveg;

	struct __pcm{
		u8 *_samples;
		u32 _size,_freq;

		union{
			u16 _status;
			struct{
				unsigned int _enabled:1;
				unsigned int _changed:1;
				unsigned int _timer:1;
				unsigned int _dma:1;
				unsigned int _vol:1;
				unsigned int _le:1;
				unsigned int _re:1;
				unsigned int _reset:1;
				unsigned int _clock:4;
				unsigned int a:4;
				unsigned int _idx:1;
			};
		};
		__pcm(){_samples=0;_cw=0;}
		int Init(int,void *,void *,u32);
		int write(u16);
		int reset();
		int output(int,int &sampleL,int &sampleR);
		int clock(int);
		protected:
		void *_ioreg;
		u8 *_mem;
		u32 _cw,_cr,_step;
	} _pcms[2];

	struct __pwm *_pwms[4]={&_pwm[0],&_pwm[1],&_waveg,&_noiseg};
private:
	u16 *_samples;
	u32 _cycles,_freq;
};

};

#endif /* GBASPU_H */
