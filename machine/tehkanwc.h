#include "machine.h"
#include "tehkanwcspu.h"
#include "tehkanwcgpu.h"
#include "game.h"

#ifndef __TEHKANWCH__
#define __TEHKANWCH__

namespace tehkanwc{

class tehkanwc : public Machine,public Z80CpuShared,public tehkanwcGpu,public tehkanwcSpu{
public:
	tehkanwc();
	virtual ~tehkanwc();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();
	virtual int Exec(u32);

	virtual int LoadSettings(void * &);
	virtual int Query(u32 what,void *pv);

	virtual int Dump(char **);

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return tehkanwcGpu::Draw(cr);};

	virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
	virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);

private:
	struct __cpu : public Z80CpuShared{
		public:
		//	virtual s32 fn_mem_w(u32 a,pvoid,pvoid data,u32);
			//virtual s32 fn_mem_r(u32 a,pvoid,pvoid data,u32);
			/*virtual s32 fn_port_w(u32 a,pvoid,pvoid data,u32){return -1;};
			virtual s32 fn_port_r(u32 a,pvoid,pvoid data,u32){return -1;};*/

			virtual int Load(void *);
			virtual int Exec(u32);

			__cpu() : Z80CpuShared(){_idx=2;_freq=MHZ(4.608);};
			tehkanwc *_cpu;
	} _cpu;
	u32 _last_frame,_active_cpu;
private:
	struct __trackball{
		int old[2];
		int pos;;
	} _trackball[2];
};

class TWCPGame : public FileGame{
public:
	TWCPGame();
	virtual ~TWCPGame();

};


};

#endif