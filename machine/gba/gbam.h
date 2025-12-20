#include "arkad.h"
#include "machine.h"
#include "gbabios.h"

#ifndef __GBAMH__
#define __GBAMH__

namespace gba{


class gbam : public Machine,public gbabios{
public:
	using CCore::_mem;

	gbam();
	virtual ~gbam();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return gbagpu::Draw(cr);};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);

	virtual int LoadSettings(void * &);
	virtual int Run(u8 *,int cyc,void *obj);

	int OnException(u32,u32);
protected:
	s32 fn_iow(u32,void *,void *,u32);
	s32 fn_ior(u32,void *,void *,u32);
	s32 fn_timerw(u32,void *,void *,u32);
	s32 fn_timerr(u32,void *,void *,u32);
	s32 fn_write_dma(u32,void *,void *,u32);
	s32 fn_write_gpu(u32,void *,void *,u32);
	s32 fn_spuw(u32,void *,void *,u32);

};

};


#endif