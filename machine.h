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
#define ME_ENDFRAME				MACHINE_EVENT(-1)
#define ME_ENTERDEBUG			MACHINE_EVENT(100)
#define ME_LEAVEDEBUG			MACHINE_EVENT(101)

#define IMACHINE_QUERY_MEMORY_ACCESS		0x100fe17
#define IMACHINE_QUERY_SETTINGS				0x100fe18
#define IMACHINE_QUERY_VIDEO_SIZE			0x100fe19

#define CALLEE(a,b,c,d){va_start(d,b);c a(b,d);va_end(d);}

class Machine : public IMachine{
public:
	Machine(u32 sz=0);
	virtual ~Machine();
	virtual int Init();
	virtual int Destroy();
	virtual int Reset();

	virtual int OnEvent(u32,...)=0;
	virtual int LoadSettings(void * &);
	virtual int OnFrame();

	virtual int SaveState(IStreamer *);
	virtual int LoadState(IStreamer *);
	u8 *_memory;
protected:
	virtual int OnEvent(u32,va_list);
	u32 _memsize,_frame_skip,_frame,_last_time,_last_update,_last_frame,_max_frame_skip,_fps_sync,_max_frame,_cr_frame_skip;
	u64 _status,_total_frames;
	u8 *_keys;
};

#endif