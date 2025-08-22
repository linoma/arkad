#include "ccore.h"
#include "icore.h"
#include <vector>

#ifndef __C64DEVH__
#define __C64DEVH__

namespace c64{

using namespace std;

class c64dev{
public:
	c64dev();
	virtual ~c64dev();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(int,void *,void *,u32 f);
public:
	struct c64_dir_entry{
		char name[17];
		u32 size,offset;
		u8 sa_lo,sa_hi;
		c64_dir_entry(u32 o=0,u32 s=0,char *p=0,u8 lo=0,u8 hi=0){
			name[0]=0;
			offset=o;
			size=s;
			sa_lo=lo;
			sa_hi=hi;
			if(p) strcpy(name,p);
		};
	};

	struct __message{
		u32 _frame,_buf[5];
		__message(u32 f,...){
			va_list arg;

			_frame=f;
			memset(_buf,0,sizeof(_buf));
			va_start(arg,f);
			for(u32 n=0,i=va_arg(arg,u32);i;i--,n++)
				_buf[n]=va_arg(arg,u32);
			va_end(arg);
		}
	};
protected:
	u8 *_ports;

	class __keyboard : public vector<__message>,public ADevice{
		public:
		__keyboard();
		virtual ~__keyboard();
		virtual int Read(void *,u32,u32 *r=0);
		union{
			u32 _status;
			struct{
				unsigned int _init:2;
				unsigned int _unread:2;
				unsigned int _wait:4;
			};
		};

		int reset();
		int update(int cyc);
		int Init(int,void *,void *,u32 f);
		int _translate(u32 &,u32);
		protected:
		u32 _cycles;
		u8 _key,_rows[16],*_cols;
	} _keyboard;

	class __joystick : public ADevice,public vector<__message>{
		public:
		union{
			u8 _status;
			struct{
				unsigned int _enabled:1;
				unsigned int __a:7;
			};
		};

		int reset();
		int update(int cyc);
		virtual int Init(int,void *,void *,u32 f);
		virtual int Read(void *,u32,u32 *r=0);
		int write(u32,u8);
		int read(u32,u8 *);

		u32 _cycles,_buf[10];
		private:
			u8 *_ciareg;
			IDevice *_cia;
	} _joy[2];

	struct __cia : IBridge{
		enum :u8 {A,B};

		enum :u8 {
			PRA = 0,PRB,DDRA,DDRB,TA_LO,TA_HI,
			TB_LO,TB_HI,TOD_10THS,
			TOD_SEC,TOD_MIN,TOD_HR,	SDR,ICR,CRA,CRB,IMR,TOD_A10THS,TOD_ASEC,TOD_AMIN,TOD_AHR
		};

		struct __timer{
			union{
				struct{
					unsigned int _start:1;
					unsigned int _pbon:1;
					unsigned int _omode:1;
					unsigned int _rmode:1;
					unsigned int _load:1;
					unsigned int _inmode:1;
					unsigned int _spmode:1;
					unsigned int _todin:1;
				} a;
				struct{
					unsigned int _start:1;
					unsigned int _pbon:1;
					unsigned int _omode:1;
					unsigned int _rmode:1;
					unsigned int _load:1;
					unsigned int _inmode:2;
					unsigned int _alarm:1;
				} b;
				struct{
					unsigned int _start:1;
					unsigned int _pbon:1;
					unsigned int _omode:1;
					unsigned int _rmode:1;
					unsigned int _load:1;
					unsigned int _inmode:2;
				} c;
				u8 _value;
			} _control;
			int reset();
			int update(int cyc=0);
			int start();

			u16 _count,_load;
			u32 _cycles,_freq;
		} _timers[2];

		struct __tod{
			union{
				struct{
					unsigned int _enabled:1;
					unsigned int _sync:1;
					unsigned int _ae:1;
					unsigned int _changed:1;
					unsigned int _latch:1;
				};
				u32 _status;
			};
			u32 _count,_cycles,_freq,_alarm,_countl;
			u8 reg[8];
			int reset();
			int enable(int);
			int update(int cyc=0);
			int alarm(u32);
			int latch(int);
		} _tod;

		struct __sdr{
			union{
				struct{
					unsigned int _enabled:1;
					unsigned int _mode:1;
				};
				u32 _status;
			};
			int reset();
			int update(int cyc=0);
			int write(u8);
			u8 _idx,_shift,_bits;
			u32 _count,_cycles,_freq;
		} _sdr;

		__cia();
		int enterIRQ(u8);
		int reset();
		virtual int Init(int,void *,void *,u32);
		virtual int Trigger(u32,u32,u32,void *);
		virtual int Connect(IDevice *,u32,u32);
		virtual int Read(void *,u32,u32 *r=0){return -1;};
		virtual int Write(void *,u32,u32 *r=0){return -1;};
		int write(u32,u8);
		int read(u32,u8 *);
		int update(int);
		int flush();
		int _remap(void *,void *);
		//private:
		u8 *_ioreg;
		u8 *_mem,_regs[25],_idx;
		vector<IDevice *> _devices;
	} _cia[2];
};


};

#endif