#include "arkad.h"
#include "ccore.h"
#include "elf.h"
#include "game.h"
#include "ps2gpu.h"
#include "pcmdac.h"
#include "general_device.h"

#ifndef __PS2DEVH__
#define __PS2DEVH__

namespace ps2{

class PS2M;

#include "r5900.h.inc"

#define RMAP_(a,b,c)\
if(((a)>=0 && a < MS_RAM) || ((a)>= 0x20000000 && (a)<=0x21FFFFFF)) b=&((u8 *)_mem)[(a)&(MS_RAM-1)];\
else if(((a)>= 0x30100000 && (a)<(0x30000000|MS_RAM))) b=&((u8 *)_mem)[(a)&(MS_RAM-1)];\
else if(((a) >=0x10000000 && (a)<=0x1f9fffff) || ((a) >=0xb0000000 && (a)<=0xbf9fffff)) b=(u8 *)&((u8 *)_ioreg)[RMAP_IO(a)];\
else if(((a) >=0x1FC00000 && (a)<=0x1fffffff) || ((a) >=0x80000000 && (a)<=0x9fffffff)) b=&((u8 *)_mem)[MI_BIOS+((a)& (MS_BIOS-1))];\
else if(((a) >=0x20000000 && (a)<=0x2fffffff)) b=&((u8 *)_mem)[MI_BIOS+((a)& (MS_BIOS-1))];\
else b=NULL;

#define ISIO(a) ((a&0x10000000))
#define RMAP_IO(a) ( ((u16)(a))|SR((a)&0x02000000,9))

#define PS2IOREG(a) _ioreg[SR(RMAP_IO((a)),2)]

#define MI_RAM	0
#define MS_RAM	MB(32)
#define MI_IO	(MI_RAM+MS_RAM)
#define MS_IO	MB(1)
#define MI_BIOS	(MI_IO+MS_IO)
#define MS_BIOS	MB(4)
#define MI_VRAM	(MI_BIOS+MS_BIOS)
#define MS_VRAM	MB(4)

#define RIO(a,b,c,d) if(ISIO(a)){u32 a__=RMAP_IO(a);\
	if(_portfnc_read[a__]) (((CCore *)this)->*_portfnc_read[a__])(a,b,&__data,d);\
}

#define WIO(a,b,c,d,e) if(ISIO(a)){u32 a__ = RMAP_IO(a);\
	if(_portfnc_write[a__]) e (((CCore *)this)->*_portfnc_write[a__])(a,b,c,d);\
}


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

class PS2DEV : public R5900Cpu,public ps2gpu,public PCMDAC{
public:
	PS2DEV();
	virtual ~PS2DEV();
	virtual int Reset();
	virtual int Init(PS2M &);

	virtual int Run(u8 *,int,void *);
	virtual int Query(u32 a,void *p);
	virtual s32 fn_timer_regs_w(u32,pvoid,pvoid,u32);
	virtual s32 fn_timer_regs_r(u32,pvoid,pvoid,u32);
	virtual s32 fn_dma_regs_w(u32,pvoid,pvoid,u32);
	virtual s32 fn_gif_regs_w(u32,pvoid,pvoid,u32);
	virtual s32 fn_ipu_regs_w(u32,pvoid,pvoid,u32);
	virtual s32 fn_gs_regs_w(u32,pvoid,pvoid,u32);
	virtual s32 fn_sif_regs_w(u32,pvoid,pvoid,u32);
	virtual int do_dma(struct __dma *);
protected:
	virtual int OnCop(RSZU cop,RSZU op,RSZU s,RSZU *d);
	virtual int _enterIRQ(int n,u32 pc=0);

	friend class PS2M;
};

};

#endif