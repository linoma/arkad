#ifndef __ICOREH__
#define __ICOREH__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef signed long s64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef void *pvoid;

#define SR(a,b) ((a)>>(b))
#define SL(a,b) ((a)<<(b))

#define BV(a) SL(1,(a))
#define BC(a,b) ((a) &= ~(b))
#define BS(a,b) ((a) |= (b))
#define BT(a,b) ((a) & (b))

#define BVT(a,b) BT(a,BV(b))
#define BVC(a,b) BC(a,BV(b))
#define BVS(a,b) BS(a,BV(b))

#define S_QUIT			BV(0)
#define S_INIT			BV(1)
#define S_RUN			BV(2)
#define S_PAUSE			BV(3)
#define S_DEBUG_NEXT 	BV(4)

#define S_DEBUG			BV(8)
#define S_LOAD			BV(16)
#define S_DEBUG_UPDATE	BV(17)

typedef struct{
	virtual int Query(u32,void *) = 0;
} IObject;

typedef struct : IObject{
	virtual int Exec(u32) = 0;
	virtual int Dump(char **)=0;
	virtual int Init(void *)=0;
	virtual int Destroy()=0;
	//virtual int Load(char *)=0;
	virtual int Reset()=0;
} ICore;


typedef struct{
	virtual int Run(u8 *,int)=0;
} ICpuTimerObj;

struct _timerobj;

typedef struct _timerobj{
	ICpuTimerObj *obj;
	u32 elapsed,cyc;
	struct _timerobj *last,*next;
} CPUTIMEROBJ,*LPCPUTIMEROBJ;

typedef s32(IObject::*CoreCallback)(s32);
typedef s32(IObject::*CoreMACallback)(u32,pvoid,pvoid,u32);

typedef struct{
	u32 size;
	union{
		u32 attributes;
		struct{
			int type:4;
			int popup:1;
			int editable:1;
			int clickable:1;
			int message:1;
		};
	};
	union{
		char title[20];
		struct{
			int event_id;
			union{
				struct{
					int x;
					int y;
				};
				struct{
					u32 wParam;
					u32 lParam;
				};
				u64 a;
			};
			GtkWidget *w;
			u64 res;
		};
	};
	union{
		char name[20];
		u32 id;
	};
} DEBUGGERPAGE,*LPDEBUGGERPAGE;

class Runnable{
public:
	Runnable();
	Runnable(int);
	virtual ~Runnable();
	virtual void OnRun()=0;
	virtual int Create();
	virtual int Destroy();
	virtual void Run();
	void addStatus(u64 v){_status |= v;};
	int hasStatus(u64 v){return (_status & v) != 0;};
	void clearStatus(u64 v){_status &=  ~v;}
	enum : u32{
		QUIT=S_QUIT,
		PLAY=S_RUN,
		PAUSE=S_PAUSE
	};
protected:
	void _set_time(u32);
	u32 _cr;
private:
	u64 _status;
	u32 _sleep;
	pthread_t _thread;

	static void *thread1_func(void *data){
		((Runnable *)data)->Run();
		return NULL;
	};
};

#define IDEVICE_OK			0
#define IDEVICE_CONNECTED	5

#define IDEVICE_QUERY_CONNECT			0
#define IDEVICE_QUERY_SET_ENABLE		0x20002

#define ICORE_QUERY_IO_PORT				3
#define ICORE_QUERY_PC					10
#define ICORE_QUERY_MEMORY_INFO			41
#define ICORE_QUERY_MEMORY_ACCESS		40
#define ICORE_QUERY_MEMORY_FIND			42
#define ICORE_QUERY_CPUS				50
#define ICORE_QUERY_CPU_INTERFACE		55
#define ICORE_QUERY_CPU_ACTIVE_INDEX	566
#define ICORE_QUERY_ADDRESS_INFO		12
#define ICORE_QUERY_CPU_FREQ			13
#define ICORE_QUERY_OPCODE				14
#define ICORE_QUERY_NEXT_STEP			15
#define ICORE_QUERY_PREVOIUS_STEP		16
#define ICORE_QUERY_REGISTER			17
#define ICORE_QUERY_DISASSEMBLE			18
#define ICORE_QUERY_DUMP				19
#define ICORE_QUERY_FILENAME			20
#define ICORE_QUERY_FILE_EXT			21
#define ICORE_QUERY_CPU_MEMORY			22
#define ICORE_QUERY_CPU_STATUS			23
#define ICORE_QUERY_ADD_WAITABLE_OBJ	80

#define ICORE_QUERY_DBG_PAGE				0x101
#define ICORE_QUERY_DBG_PAGE_INFO			0x104
#define ICORE_QUERY_DBG_PAGE_EVENT			0x105
#define ICORE_QUERY_DBG_PAGE_COORD_TO_ADDRESS		0x106

#define ICORE_QUERY_DBG_DUMP_ADDRESS		0x122
#define ICORE_QUERY_DBG_DUMP_FORMAT			0x123

#define ICORE_QUERY_DBG_MENU				0x180
#define ICORE_QUERY_DBG_MENU_SELECTED		0x181

#define ICORE_QUERY_GPIO_PINS				0x201
#define ICORE_QUERY_GPIO_STATUS				0x202


#define ICORE_QUERY_BREAKPOINT_ENUM 	0xff01
#define ICORE_QUERY_BREAKPOINT_ADD		0xff02
#define ICORE_QUERY_BREAKPOINT_DEL		0xff03
#define ICORE_QUERY_BREAKPOINT_STATE	0xff04
#define ICORE_QUERY_BREAKPOINT_INFO		0xff05
#define ICORE_QUERY_BREAKPOINT_INDEX	0xff06
#define ICORE_QUERY_BREAKPOINT_LOAD		0xff10
#define ICORE_QUERY_BREAKPOINT_SAVE		0xff11
#define ICORE_QUERY_BREAKPOINT_CLEAR	0xff12
#define ICORE_QUERY_BREAKPOINT_RESET	0xff13
#define ICORE_QUERY_BREAKPOINT_COUNT	0xff14
#define ICORE_QUERY_BREAKPOINT_MEM_ADD		0xff20

#define ICORE_QUERY_SET_DEVICES				0x10001
#define ICORE_QUERY_SET_GPIO_PINS			0x10040
#define ICORE_QUERY_SET_REGISTER			0x1fe01
#define ICORE_QUERY_SET_PC					0x1fe02
#define ICORE_QUERY_SET_BREAKPOINT_STATE	0x1fe03
#define ICORE_QUERY_BREAKPOINT_SET			0x1fe04
#define ICORE_QUERY_SET_STATUS				0x1fe05

#define ICORE_QUERY_SET_MACHINE				0x1Fe20
#define ICORE_QUERY_SET_LOCATION			0x1fe15
#define ICORE_QUERY_SET_LAST_ADDRESS		0x1fe17

#define ICORE_MESSAGE_POPUP_INIT			GDK_EVENT_LAST+1
#define ICORE_MESSAGE_MENUITEM_SELECT		GDK_EVENT_LAST+2

#define DEBUG_BREAK_IRQ		0x8000000000000000
#define DEBUG_BREAK_OPCODE	0x1000000000000000
#define DEBUG_LOG_DEV		0x0100000000000000

#define AM_BYTE		0x40
#define AM_WORD 	0x20
#define AM_DWORD 	0x10
#define AM_READ 	0
#define AM_WRITE 	0x1


#endif