#include "machine.h"
#include "1943spu.h"
#include "1943gpu.h"
#include "game.h"

#ifndef __1943H__
#define __1943H__

namespace M1943{

class M1943 : public Machine,public Z80Cpu,public M1943Gpu,public M1943Spu{
public:
	M1943();
	virtual ~M1943();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();
	virtual int Exec(u32);

	virtual int Load(void *){return 0;};

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return M1943Gpu::Draw(cr);};
	virtual int Query(u32 what,void *pv);

	virtual int Dump(char **);

	virtual int LoadSettings(void * &);

	virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
	virtual int OnChangeIRQ(u32,u32 *){return -1;};

	const int BGRAM_BANKS=4;
	const int BGRAM_BANK_SIZE=0x1000;
private:
	u32 _last_frame,_active_cpu;
};

class M1943Game : public FileGame{
public:
	M1943Game();
	virtual ~M1943Game();
};

};

#endif