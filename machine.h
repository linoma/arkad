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
#define IMACHINE_QUERY_MEMORY_ALLOC			0x100fe1d
#define IMACHINE_QUERY_MEMORY_FREE			0x100fe1e

#define CALLEE(a,b,c,d){va_start(d,b);c a(b,d);va_end(d);}
#define MACHINE_ONEXITEXEC(a,b)\
if(b) return b;\
if((BVT(Machine::_status,1) && BT(a,S_DEBUG))){BC(Machine::_status,1|2);return 4;}\
if(BT(Machine::_status,2)){	BC(Machine::_status,1|2);return 3;}\
return 0;

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
	virtual int AllocMemory(u32,u32,void **);

	u8 *_memory;
protected:
	virtual int OnEvent(u32,va_list);
	u32 _mem_size,_frame_skip,_frame,_last_time,_last_update,_last_frame,_max_frame_skip,_fps_sync,_max_frame,_cr_frame_skip,_mem_start;
	u64 _status;
	u8 *_keys;
private:
	struct __block{
		u32 adr,size;
		union{
			struct{
				unsigned int _deleted:1;
			};
			u32 attribute;
		};
	};
	vector<__block> _blocks;
};

#endif