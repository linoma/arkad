#include "arkad.h"
#include "ccore.h"
#include "elf.h"
#include "game.h"

#ifndef __PS2DEVH__
#define __PS2DEVH__

namespace ps2{

class PS2M;

#include "r5900.h.inc"

#define ISIO(a) (SR(a,24) < 0x1d && (a&0xf0000000)== 0x10000000)
#define RMAP_IO(a) ( ((a) & 0xfff)|SR((a)&0x0f000000,12))

#define PS2IOREG(a) _io_regs[SR(RMAP_IO((a)),2)]


#define RIO(a,b,c,d) if(ISIO(a)){register u32 a__=RMAP_IO(a);\
	if(_portfnc_read[a__]) (((CCore *)this)->*_portfnc_read[a__])(a,b,&__data,d);\
}

#define WIO(a,b,c,d,e) if(ISIO(a)){register u32 a__ = RMAP_IO(a);\
	if(_portfnc_write[a__]) e (((CCore *)this)->*_portfnc_write[a__])(a,b,c,d);\
}

#define RMAP_(a,b,c)\
if(((a)>= 0x20000000 && (a)<=0x21FFFFFF) || ((a)>=0 && a <= 0x1FFFFFF)) b=&((u8 *)_mem)[(a)&0x1FFFFFF];\
else if(((a)>= 0x30100000 && (a)<=0x31FFFFFF)) b=&((u8 *)_mem)[(a)&0x1FFFFFF];\
else if((a) >=0x10800000 && (a)<=0x1f9fffff) b=(u8 *)&((u8 *)_io_regs)[RMAP_IO(a)];\
else b=NULL;

class RomStream : public ISOStream{
public:
	RomStream();
	RomStream(char *);
	virtual ~RomStream();
	virtual int Parse(IGame *,LElfFile **h=NULL);
	virtual int _getInfo(u32 *,void **);
	virtual int SeekToStart(s64 a=0);
protected:
	void *_data;
};

class PS2ROM : public Game{
public:
	PS2ROM();
	virtual ~PS2ROM();
	virtual int Open(char *,u32);
	virtual int Read(void *,u32,u32 *);
	virtual int Write(void *,u32,u32 *);
	virtual int Close();
	virtual int Query(u32 w,void *pv);
	virtual int Seek(s64,u32);
	virtual int Tell(u64  *);
protected:
	int _type;
};

class PS2DEV : public R5900Cpu{
public:
	PS2DEV();
	virtual ~PS2DEV();
	virtual int Reset();
	virtual int Init(PS2M &);
protected:

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
		u32 *_io_regs,_elapsed,_counter,_step;
		u8 *_mem;

		__timer();
		int reset();
		int init(int,void *,void *);
		int write(u32,u32);
		int read(u32,u32 *);
		virtual int Run(u8 *,int,void *);
		virtual int Query(u32,void *){return -1;};
		protected:
			u32 _count();
	} _timers[4];

	struct __dma{
		union{
			u64 _status;
			struct{
				unsigned int _idx:3;
				unsigned int _mode:2;
			};
		};
		u32 *_io_regs,_src,_dst,_count;
		u8 *_mem;

		__dma();
		int reset();
		int init(int,void *,void *);
		int write(u32,u32,PS2DEV &);
		virtual int _do_transfer(u32,u32 *);
		virtual int _do_dma(PS2DEV &);

		friend class PS2DEV;
	} __dmas[3];


	struct __dvdrom{
		u32 *_io_regs;
		u8 *_mem;

		__dvdrom();
		int reset();
		int init(int,void *,void *);
	} _dvdrom;

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

	struct __VFPU{
		union{
			float _f[32];
			double _d[32];
		} _regs;
		u32 _sr,_rr;

		int _reset();
	} CP1;

	//struct __dma *_dmas[7]={0};

	friend class PS2M;

	virtual int do_dma(struct __dma *);

};

};

#endif