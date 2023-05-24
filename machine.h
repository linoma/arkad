#include "arkad.h"
#include <map>

#ifndef __MACHINEH__
#define __MACHINEH__

using namespace std;

#define MS_VBLANK		BV(16)
#define ME_KEYDOWN 		MACHINE_EVENT(1)
#define ME_REDRAW		MACHINE_EVENT(2)
#define ME_MOVEWINDOW	MACHINE_EVENT(3)
#define ME_KEYDOWN 		MACHINE_EVENT(1)
#define ME_KEYUP 		MACHINE_EVENT(4)
#define ME_REDRAW		MACHINE_EVENT(2)
#define ME_MOVEWINDOW	MACHINE_EVENT(3)

#define IMACHINE_QUERY_MEMORY_ACCESS		0x100fe17

class Machine : public IMachine{
public:
	Machine(u32 sz=0);
	virtual ~Machine();
	virtual int Init();
	virtual int Destroy();
	virtual int Reset();
	virtual int Load(IGame *,char *)=0;
	virtual int OnVblank(s32);
	u8 *_memory;
protected:
	u32 _memsize,_frame_skip,_frame,_last_time;
	u64 _status;
	map<string,string>_settings;
};

#endif