#include "arkad.h"
#include "ccore.h"
#include "general_device.h"

#ifndef __PS1MDECH__
#define __PS1MDECH__

namespace ps1 {

class PS1DEV;

struct __mdec : ICpuTimerObj{
	u32 *_io_regs,*_in,*_tmp;
	u8 *_mem;
	u8 *_iqy,*_iquv,*_izz;
	s16 *_iscale,*_cr,*_cb;

	static u8 _zz[64];
	struct{
		u32 *p,end,current,size,pw,pr;
	} _out;
	union{
		u32 _status;
		u8 _status8[8];
		u64 _status64;
		struct{
			unsigned int _param:16;
			unsigned int _block:3;
			unsigned int _0:4;
			unsigned int _do15:1;
			unsigned int _dos:1;//24
			unsigned int _dod:2;
			unsigned int _dor:1;
			unsigned int _dir:1;
			unsigned int _busy:1;
			unsigned int _dif:1;
			unsigned int _dof:1;//bit 32/31
			unsigned int _1:1;//status8[4]
			unsigned int _cdo15:1;
			unsigned int _cdos:1;
			unsigned int _cdod:2;
			unsigned int _command:3;
			unsigned int _2:5;//status8[5]
			unsigned int _dma1:1;
			unsigned int _dma0:1;
			unsigned int _reset:1;
		};
	};
	__mdec();
	~__mdec();
	int reset();
	int reset_decoder();
	int init(int,void *,void *);
	int write(u32,u32,PS1DEV &);
	int read(u32,u32 *,PS1DEV &);
	virtual int Run(u8 *,int,void *);
	virtual int Query(u32,void *){return -1;};
	protected:

	u32 _unrle(u16 *in,u16 *out,u8 *iq);
	void _idct(s16 *);
	void _yuv_rgb(int,int,s32 *,s32 *,s16 *,s16 *);
	void _yuv_mono4(s32 *,s32 *);
	void _yuv_mono8(s32 *,s32 *);
	int _decode(u16 *);

	struct __command<u32> _fifo;

	friend class PS1DEV;
};

};


#endif
