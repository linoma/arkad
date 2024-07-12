#include "machine.h"
#include "blktigerspu.h"
#include "blktigergpu.h"

#ifndef __BLACKKTIGERH__
#define __BLACKKTIGERH__

namespace blktiger{

class BlackTiger : public Machine,public Z80Cpu,public BlkTigerGpu,public BlkTigerSpu{
public:
	BlackTiger();
	virtual ~BlackTiger();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();
	virtual int Exec(u32);

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return BlkTigerGpu::Draw(cr);};
	virtual int Query(u32 what,void *pv);

	virtual int Dump(char **);

	virtual int LoadSettings(void * &);

	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32);

	virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
	virtual int OnChangeIRQ(u32,u32 *);

	const int BGRAM_BANKS=4;
	const int BGRAM_BANK_SIZE=0x1000;
private:
	u32 _active_cpu,_bank;
};

};

#endif