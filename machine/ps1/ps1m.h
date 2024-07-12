#include "arkad.h"
#include "machine.h"
#include "ps1gpu.h"
#include "ps1bios.h"
#include "ps1rom.h"
#include "ps1spu.h"

#ifndef __PS1MH__
#define __PS1MH__

namespace ps1{

#define PS1IOREG8_(a,b) ((u8*)b + RMAP_IO(a))
#define PS1IOREG_(a,b) ((u32 *)b)[SR(RMAP_IO((a)),2)]
#define PS1IOREG(a) PS1IOREG_(a,_io_regs)

class PS1M : public Machine,public PS1BIOS{
public:
	PS1M();
	virtual ~PS1M();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return PS1GPU::Draw(cr);};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);
	virtual int LoadSettings(void * &v);

	virtual s32 fn_write_io(u32,void *,void *,u32);
	virtual s32 fn_read_io(u32,void *,void *,u32);
	virtual s32 fn_write_io_timer(u32,void *,void *,u32);
	virtual s32 fn_read_io_timer(u32,void *,void *,u32);
	virtual s32 fn_write_io_dma(u32,void *,void *,u32);
	virtual s32 fn_write_io_gp(u32,void *,void *,u32);
	virtual s32 fn_read_io_gp(u32,void *,void *,u32);
	virtual s32 fn_write_io_mdec(u32,void *,void *,u32);
	virtual s32 fn_read_io_mdec(u32,void *,void *,u32);
	virtual s32 fn_write_io_spu(u32,void *,void *,u32);
protected:
	virtual int OnJump(u32);
	virtual int OnCop(RSZU,RSZU,RSZU,RSZU *ss);
	virtual int OnException(u32,u32);
};

};

#endif