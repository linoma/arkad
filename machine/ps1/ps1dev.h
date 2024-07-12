#include "arkad.h"
#include "ccore.h"
#include "ps1gpu.h"
#include "ps1spu.h"
#include "ps1gte.h"
#include "ps1mdec.h"

#ifndef __PS1DEVH__
#define __PS1DEVH__

namespace ps1{

class PS1M;

#define PS1ME_PLAYTRACK				MACHINE_EVENT(0x100)
#define PS1ME_PLAY_XA_SECTOR		MACHINE_EVENT(0x101)

#include "r3000.h.inc"

#define RMAP_(a,b,c)\
if(((a)>= 0x80000000 && (a)<=0x807FFFFF) || ((a)>=0 && a <= 0x7fffff) || ((a) >= 0xa0000000 && (a)<= 0xa07fffff)) b=&((u8 *)_mem)[(a)&0x1fffff];\
else if((a) >=0x1f800000 && (a)<=0x1f8003ff) b=(u8 *)&((u8 *)_mem)[MB(9) +(a&0x3ff)];\
else if((a) >=0x1f801000 && (a)<=0x1f803000) b=(u8 *)&((u8 *)_io_regs)[RMAP_IO(a)];\
else if((a) >=0x1f900000 && (a)<=0x1f9fffff) b=(u8 *)&((u8 *)_mem)[MB(5) +(a&0xfffff)];\
else b=NULL;

#define ISIO(a) (a&0xfffff000)==0x1f801000
#define RIO(a,b,c,d) if(ISIO(a)){register u32 a__=RMAP_IO(a);\
	if(_portfnc_read[a__]) (((CCore *)this)->*_portfnc_read[a__])(a,b,&__data,d);\
}

#define WIO(a,b,c,d,e) if(ISIO(a)){register u32 a__ = RMAP_IO(a);\
	if(_portfnc_write[a__]) e (((CCore *)this)->*_portfnc_write[a__])(a,b,c,d);\
}
#define RMAP_IO(a) (((a) & 0xfff)|SR((a)&0x200000,5))

class PS1DEV : public R3000Cpu,public PS1GPU, public PS1SPU{
public:
	PS1DEV();
	virtual ~PS1DEV();
	virtual int Reset();
	virtual int Init(PS1M &);
	virtual int do_sync(u32,...);
protected:
	struct __joy{
		struct __card{
			u32 *_io_regs;
			u8 *_buf;

			union{
				struct{
					unsigned int _tx:1;
				};
				u32 _status;
			};
			__card(){_buf=NULL;};
			virtual ~__card(){if(_buf) delete []_buf;_buf=NULL;};

			int _reset();
			int _write(u32,u32);
			int _read(u32,u32 *);
			int _init(void *,void *);
			int _open(char *,u32);
		} _cards[2];

		struct{
			union{
				u8 _buf[0x22];
#pragma pack(push)
#pragma pack(1)
				struct{
					u8 _state;
					u8 _id;
					u8 _type;
					u16 _buttons;
				};
#pragma pop()
			};
		} _pads[2];

		u32 *_io_regs;
		struct{
			u8 _ch,_buf[21],_cw,_cr;

			union{
				struct{
					unsigned int _tx:1;
					unsigned int _rx:1;
					unsigned int _tx1:1;
					unsigned int _0:4;
					unsigned int _ack:1;
				};
				u32 _status;
				u64 _status64;
			};
		};
		int _reset();
		int _write(u32,u32);
		int _read(u32,u32 *);
		int _key_event(u32 k,u32 a);
		int init(void *,void *);
	} _joy;

	struct __timer : ICpuTimerObj {
		union{
			u16 _status;
			struct{
				unsigned int _mode:1;
				unsigned int _sync:2;
				unsigned int _reset:1;
				unsigned int _imode:4;
				unsigned int _source:2;
				unsigned int _ienable:1;
				unsigned int _reachedT:1;
				unsigned int _reached:1;
				unsigned int _d0:4;
				unsigned int _idx:2;
			};
		};
		u32 *_io_regs,_elapsed,_step;
		u16 _counter;
		u8 *_mem;

		__timer();
		int reset();
		int init(int,void *,void *);
		int write(u32,u32);
		int read(u32,u32 *);
		virtual int Run(u8 *,int,void *);
		virtual int Query(u32,void *){return -1;};
		u32 _count();
	} _timers[3];

	struct __dma : ICpuTimerObj{
		u32 *_io_regs,_src,_dst,_count;
		u8 *_mem;
		int _inc;

		union{
			u64 _status64;
			u32 _status;
			struct{
				unsigned int _dir:1;
				unsigned int _step:1;
				unsigned int _0:6;
				unsigned int _chopping:1;
				unsigned int _sync:2;
				unsigned int __0:5;
				unsigned int _dma_win:3;
				unsigned int _nu:1;
				unsigned int _cpu_win:3;
				unsigned int __nu:1;
				unsigned int _start:1;
				unsigned int ___0:3;
				unsigned int _enabled:1;
				unsigned int ____0:3;
				unsigned int _idx:3;
				unsigned int _state:2;
			};
		};

		__dma();
		int reset();
		int init(int,void *,void *);
		int write(u32,u32,PS1M &);
		virtual int _do_transfer(u32,u32 *);
		virtual int _prepare_dma();
		virtual int _transfer(PS1M &);
		virtual int _end_transfer(u32,PS1M &);

		virtual int Run(u8 *,int,void *);
		virtual int Query(u32,void *){return -1;};

		friend class PS1DEV;
		friend class PS1M;
	} __dmas[2];

	struct __gpu_dma : public __dma{
		virtual int _prepare_dma();
		virtual int _do_transfer(u32,u32 *);
		virtual int _transfer(PS1M &);
	} _gpu_dma;

	struct __spu_dma : public __dma{
		virtual int _prepare_dma();
		virtual int _do_transfer(u32,u32 *);
		virtual int _end_transfer(u32,PS1M &);
	} _spu_dma;

	struct __otc_dma : public __dma{
		virtual int _prepare_dma();
		virtual int _do_transfer(u32,u32 *);
	} _otc_dma;

	struct __mdec_dma : public __dma{
		virtual int _prepare_dma();
		virtual int _do_transfer(u32,u32 *);
	} _mdec_dma[2];

	struct __cdrom : public ICpuTimerObj{
		IGame *_game;
		u8 *_mem,*_regs;

		union{
			struct{
				unsigned int _pidx:2;
				unsigned int _xafe:1;
				unsigned int _pfe:1;
				unsigned int _pff:1;
				unsigned int _rfe:1;
				unsigned int _dfe:1;
				unsigned int _busy:1;
				unsigned int _cr:12;
				unsigned int _cw:12;
				unsigned int _state:3;
				unsigned int _param:4;
				unsigned int _ack:1;
				unsigned int _fxas:1;

				union{
					struct{
						unsigned int _cdda:1;
						unsigned int _auto_pause:1;
						unsigned int _report:1;
						unsigned int _xa_filter:1;
						unsigned int _ignore_bit:1;
						unsigned int _sector_size:1;
						unsigned int _xdda:1;
						unsigned int _speed:1;
					};
					u8 _mode;
				};

				union{
					struct{
						unsigned int _error:1;
						unsigned int _spinning:1;
						unsigned int _seek:1;
						unsigned int _id:1;
						unsigned int _opened:1;
						unsigned int _reading:1;
						unsigned int _seeking:1;
						unsigned int _play:1;
					};
					u8 _status;
				};
			};
			u64 _control;
		};

		__cdrom();
		virtual ~__cdrom();
		int reset();
		int init(int,void *,void *);
		int write(u32,u32,PS1M &);
		int read(u32,u32 *);
		int Run(u8 *mem,int,void *);
		virtual int Query(u32,void *);
		int _load(IGame *);
		int update(u32);

		enum : u8{
			CMD_SYNC,
			CMD_NOP,
			CMD_SETLOC,
			CMD_PLAY,
			CMD_FORWARD,
			CMD_BACKWARD,
			CMD_READN,
			CMD_STANDBY,
			CMD_STOP,
			CMD_PAUSE,
			CMD_INIT,
			CMD_MUTE,
			CMD_DEMUTE,
			CMD_SETFILTER,
			CMD_SETMODE,
			CMD_GETMODE,
			CMD_GETLOCL,
			CMD_GETLOCP,
			CMD_READT,
			CMD_GETTN,
			CMD_GETTD,
			CMD_SEEKL,
			CMD_SEEKP,
			CMD_READS=27,
			CMD_RESET,
			CMD_READTOC=30
		};
		enum : u8{
			REG_IMASK=9,
			REG_WANT=12,
			REG_ISTAT
		};

		protected:
			int _execCommand(int);
			int _do_report(int);
			u8 *_buffer,*_params;
			u32 *_io_regs,_track;
			u64 _pos;

#pragma pack(push)
#pragma pack(1)
		struct __subq{
			unsigned char _track;
			unsigned char _index;
			unsigned char _rel[3];
			unsigned char _abs[3];
		} *_subq;
#pragma pop()
	} _cdrom;

	struct __mdec _mdec;
	struct __gte GTE;
	struct __CP0{
		u32 _regs[32];
		enum{
			SR=12,
			CAUSE,
			EPC,
			PRID
		};
		int _reset();
		int _rfe(u32 *);
	} CP0;

	struct __dma *_dmas[7]={&_mdec_dma[0],&_mdec_dma[1],&_gpu_dma,&__dmas[0],&_spu_dma,&__dmas[1],&_otc_dma};

	virtual int _enterIRQ(int n,u32 pc=0);
public:
	virtual int do_dma(struct __dma *);

	friend class PS1M;
};

};

#endif