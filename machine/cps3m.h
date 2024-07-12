#include "machine.h"
#include "sh2.h"
#include "cps3gpu.h"
#include "cps3dac.h"
#include "cps3dev.h"

#ifndef __CPS3MH__
#define __CPS3MH__

namespace cps3{

class CPS3M : public Machine,public SH2Cpu,public CPS3GPU,public CPS3DAC,public CPS3DEV{
public:
	CPS3M();
	virtual ~CPS3M();
	virtual int Query(u32,void *);

	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();
	virtual int Exec(u32);
	virtual int Draw(HDC cr=NULL){return CPS3GPU::Draw(cr);};
	virtual int OnEvent(u32,...);
	virtual int Dump(char **p);

	virtual int LoadSettings(void * &);
	virtual int SaveState(IStreamer *);
	virtual int LoadState(IStreamer *);

	virtual s32 fn_device(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_flash_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_flash_r(u32 a,pvoid,pvoid,u32);

	virtual s32 fn_crypted_rom_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_crypted_rom_r(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gfx_device_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gfx_device_r(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gpu_device_w(u32 a,pvoid,pvoid,u32);
	virtual s32 fn_gpu_device_r(u32 a,pvoid,pvoid,u32);
protected:
	int _do_dma(int ch);

};

};

#endif