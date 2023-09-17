#include "pcmdac.h"
#include "ccore.h"

#ifndef __BLKTIGERSPUH__
#define __BLKTIGERSPUH__

namespace blktiger{

#include "z80.h.inc"

class BlackTiger;

class BlkTigerSpu : public PCMDAC,public  ICpuTimerObj{
public:
	BlkTigerSpu();
	virtual ~BlkTigerSpu();
	virtual int Init(BlackTiger &);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
	virtual int Load(void *);
	virtual int Exec(u32);
	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual int Run(u8 *,int);
	virtual s32 fn_mem_w(u32,u8 );
	virtual s32 fn_mem_r(u32,u8 *);
	virtual int _dumpRegisters(char *p);
	int _enterIRQ(int n,int v,u32 pc=0);

	static u16 _waveform[1024];
protected:
	struct __cpu : Z80Cpu{
		public:
			virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
			virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
			virtual int Exec(u32);

			u32 getTicks(){return _cycles;};
			u32 getFrequency(){return _freq;};
			__cpu() : Z80Cpu(){_idx=1;};
			BlkTigerSpu *_spu;
	 } _cpu;

	struct __ym2203 {
		public:
			__ym2203();

			int reset();
			int read(u8);
			int write(u8,u8);
			int step(int);
			int init(BlkTigerSpu &);
			int update(u32);
			int set_clock_prescale(u8);
			u8 get_clock_prescale(){return _clock_prescale;};

			u8 _regs[255];

			union{
				struct{
					unsigned int _timer_a:1;
					unsigned int _timer_b:1;
					unsigned int ___a:5;
					unsigned int _busy:1;

					unsigned int _env:1;
					unsigned int _enabled:1;
					unsigned int _data:8;
					unsigned int _irq:6;
					unsigned int _reg_idx:8;
				};
				u8 _status;
			};
		protected:
			u8 _clock_prescale;
			u32 _counter;

			u8 _env_state,_env_vol;
			u16 _noise_count,_noise_period;
			u32 _env_count,_env_period,_noise_state;

			struct __pwm{
				union{
					struct{
						unsigned int _enabled:1;
						unsigned int _idx:2;
						unsigned int _state:1;
						unsigned int _noise:1;
						unsigned int _env:1;
						unsigned int _vol:4;
					};
					u32 _status;
				};
				protected:
					u32 _duty,_count;
					u8 *_regs;
				public:
					int reset();
					int write(u8,u8);
					int init(int,struct __ym2203 &);
					int output(u32,struct __ym2203 &);
			} _pwm[3];

			struct __fm{
				union{
					struct{
						unsigned int _enabled:1;
						unsigned int _idx:2;
						unsigned int _modified:1;
					};
					u32 _status;
				};
				protected:
					u8  *_regs;
					s16 _fb[3];
					u32 _alg_op;

					struct __op{
						u8 *_regs,_rates[4],_sustain_level;

						enum : u8{
							EG_ATTACK=1,
							EG_DECAY,
							EG_SUSTAIN,
							EG_RELEASE
						};

						union{
							struct{
								unsigned int _key:1;
								unsigned int _key_state:1;
								unsigned int _idx:2;
								unsigned int _ch:2;
								unsigned int _modified:1;
								unsigned int _env_vol:11;
								unsigned int _level:11;
							};
							u32 _status;
						};

						union{
							struct{
								unsigned int _enabled:1;
								unsigned int _state:3;
								unsigned int _ssg:1;
								unsigned int _inverted:1;
								unsigned int _vol:12;
								unsigned int _level:10;
								unsigned int _modified:1;
								unsigned int _mode:3;
							};
							u32 _values;
						} _env;

						u32 _phase_step,_phase;

						int output(u32,u32);
						int reset();
						int init(int,int,struct __ym2203 &);
						int attack();
						int release();

						friend struct __ym2203;
					} _op[4];

				public:
					int reset();
					int write(u8,u8);
					int key(u8);
					int init(int,struct __ym2203  &);
					int output(u32);
			} _fm[3];

			struct __timer{
				union{
					struct{
						unsigned int _enabled:1;
						unsigned int _idx:2;
					};
					u32 _status;
				};
				protected:
					u32 _load,_count;
				public:
					int reset();
					int write(u8 *,u8,u8);
					int step(int);
					int load(u32);
					int init(int,struct __ym2203 &);

					friend struct  __ym2203;
			} _timer[2];

			friend class BlkTigerSpu;
	} _ym[2];

	u8  *_ipc;
	u16 *_samples;
	u32 _cycles;

	friend class BlackTiger;
private:
	const int _freq=55930;
};

};

#endif