#ifndef __ICOREH__
#define __ICOREH__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef void *pvoid;

typedef struct{
	virtual int Query(u32,void *) = 0;
} IObject;

typedef struct : IObject{
	virtual int Exec(u32) = 0;
	virtual int Dump(char **)=0;
	//virtual int Disassemble(char *,u32 *)=0;
	virtual int Init()=0;
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
	u32 elapsed;
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

#define IDEVICE_OK			0
#define IDEVICE_CONNECTED	5

#define IDEVICE_QUERY_CONNECT			0
#define IDEVICE_QUERY_SET_ENABLE		0x20002

#define ICORE_QUERY_IO_PORT				3
#define ICORE_QUERY_PC					10
#define ICORE_QUERY_MEMORY_INFO			41
#define ICORE_QUERY_MEMORY_ACCESS		40
#define ICORE_QUERY_MEMORY_FIND			42
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
#define ICORE_QUERY_CPU_INFO			22

#define ICORE_QUERY_DBG_PAGE			0x101
#define ICORE_QUERY_DBG_PAGE_INFO		0x104
#define ICORE_QUERY_DBG_PAGE_EVENT		0x105
#define ICORE_QUERY_DBG_PAGE_COORD_TO_ADDRESS		0x106

#define ICORE_QUERY_DBG_DUMP_ADDRESS	0x122
#define ICORE_QUERY_DBG_DUMP_FORMAT		0x123

#define ICORE_QUERY_GPIO_PINS			0x201
#define ICORE_QUERY_GPIO_STATUS			0x202
#define ICORE_QUERY_GPIO_STATUS			0x202

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

#define ICORE_QUERY_SET_LOCATION			0x1fe15
#define ICORE_QUERY_SET_LAST_ADDRESS		0x1fe17

#define ICORE_MESSAGE_POPUP_INIT			GDK_EVENT_LAST+1
#define ICORE_MESSAGE_MENUITEM_SELECT		GDK_EVENT_LAST+2

#define DEBUG_BREAK_IRQ		0x8000000000000000
#define DEBUG_BREAK_OPCODE	0x1000000000000000
#define DEBUG_LOG_DEV		0x0100000000000000

#define AM_BYTE		0x20
#define AM_WORD 	0x10
#define AM_DWORD 	8
#define AM_READ 	0
#define AM_WRITE 	1


#endif