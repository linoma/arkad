#include "arkad.h"
#include <map>
#include <vector>
#include <string>

#ifndef __MACHINEH__
#define __MACHINEH__

using namespace std;

#define MS_VBLANK				BV(16)
#define ME_KEYDOWN 				MACHINE_EVENT(1)
#define ME_REDRAW				MACHINE_EVENT(2)
#define ME_MOVEWINDOW			MACHINE_EVENT(3)
#define ME_KEYUP 				MACHINE_EVENT(4)

#define IMACHINE_QUERY_MEMORY_ACCESS		0x100fe17
#define IMACHINE_QUERY_SETTINGS				0x100fe18
#define IMACHINE_QUERY_VIDEO_SIZE			0x100fe19


class Machine : public IMachine{
public:
	Machine(u32 sz=0);
	virtual ~Machine();
	virtual int Init();
	virtual int Destroy();
	virtual int Reset();
	virtual int Load(IGame *,char *)=0;
	virtual int OnVblank(s32);
	virtual int OnEvent(u32,...);
	virtual int LoadSettings(void * &);
	virtual int OnFrame();
	u8 *_memory;
protected:
	u32 _memsize,_frame_skip,_frame,_last_time,_last_update,_last_frame;
	u64 _status;
};

#endif