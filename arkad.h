#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo/cairo.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>
#include <cstdarg>
#include "icore.h"

#ifndef __ARKADH__
#define __ARKADH__

typedef GtkWidget 	*HWND;
typedef GdkPixbuf 	*HBITMAP;
typedef GdkEvent	MSG;
typedef MSG *		LPMSG;

typedef unsigned char BOOL;
typedef unsigned long LONG;
typedef u32 DWORD;
typedef void *LPVOID;

typedef struct __IGame : IObject{
	virtual int Open(char *)=0;
	virtual int Close()=0;
	virtual int Read(void *,u32,u32 *)=0;
	virtual int Seek(s64,u32)=0;
} IGame;

#define MACHINE_EVENT(a) (0x80000000|(a))

typedef struct __IMachine {
	virtual int OnEvent(u32,...)=0;
	virtual int Draw(cairo_t *)=0;

	virtual int Exec(u32)=0;
	virtual int Load(IGame *,char *)=0;
	virtual int Destroy()=0;
	virtual int Init()=0;
} IMachine;


#define NOARG
#define STR_IMPL_(x) #x
#define STR(x) STR_IMPL_(x)
#define RES(x) STR(x)

#ifdef __WIN32__
	#define DPC_PATH '\\'
	#define DPS_PATH STR(\\)
	#define DPC_PATH_INVERSE '/'
	#define DPS_PATH_INVERSE "/"
#else
	#define DPC_PATH '/'
	#define DPS_PATH STR(/)
	#define DPC_PATH_INVERSE '\\'
	#define DPS_PATH_INVERSE "\\"
#endif

#define MAKEWORD(a,b) (((b)<<16)|(a))
#define MAKEHWORD(a,b) (((b)<<8)|(a))
#define MAKEBYTE(a,b) (((b)<<4)|(a))

#define LOWORD(a) ((u16)a)
#define HIWORD(a) ((u16)SR(a,16))
#define HIBYTE(a) ((u8)SR(a,8))
#define LOBYTE(a) ((u8)a)

#define SR(a,b) ((a)>>(b))
#define SL(a,b) ((a)<<(b))

#define BV(a) SL(1,(a))
#define BC(a,b) ((a) &= ~(b))
#define BS(a,b) ((a) |= (b))
#define BT(a,b) ((a) & (b))
#define KB(a) ((a)*1024)
#define MB(a) (KB(a)*1024)
#define BVT(a,b) BT(a,BV(b))
#define BVC(a,b) BC(a,BV(b))
#define BVS(a,b) BS(a,BV(b))

#define AWORD(a,b) *((u32 *)&((u8 *)a)[b])
#define AHWORD(a,b) *((u16 *)&((u8 *)a)[b])
#define ABYTE(a,b) *((u8 *)&((u8 *)a)[b])

#define RGB(a,b,c) (SL((u8)c,16)|SL((u8)b,8)|(u8)a)
#define RGB555(a,b,c) (SL(c,10)|SL(b,5)|a)

#define S_QUIT			BV(0)
#define S_INIT			BV(1)
#define S_RUN			BV(2)
#define I_PAUSE			3
#define S_PAUSE			BV(I_PAUSE)
#define S_DEBUG_NEXT 	BV(4)

#define S_DEBUG			BV(8)
#define S_LOAD			BV(16)
#define S_DEBUG_UPDATE	BV(17)

extern u32 __cycles,__data;
extern int _error_level;
extern ICore *cpu;
extern IMachine *machine;


#ifdef __cplusplus
extern "C" {
#endif

void on_menuiteem_select(GtkMenuItem* item,gpointer user_data);
gboolean on_mouse_down (GtkWidget* self, GdkEventButton *event,gpointer user_data);
void on_menu_init (GtkWidget  *item, GtkWidget *p,  gpointer   user_data);
void on_button_clicked(GtkButton *button,gpointer user_data);
void on_command(GtkWidget* item,gpointer user_data);

#ifdef __cplusplus
}
#endif

int isDebug(u64);
void EnterDebugMode(u64 v=0);
void OnMemoryUpdate(u32 a,u32 b);
void _log_printf(int level,const char *fmt,...);


#define LOG(level,fmt,...) _log_printf(level,(fmt),## __VA_ARGS__)
#define LOGV(fmt,...) LOG(1,(fmt),## __VA_ARGS__)
#define LOGI(fmt,...) LOG(2,(fmt),## __VA_ARGS__)
#define LOGD(fmt,...) LOG(4,(fmt),## __VA_ARGS__)
#define LOGE(fmt,...) LOG(8,(fmt),## __VA_ARGS__)

#define EXIT(fmt,...) printf((fmt),## __VA_ARGS__);exit(-1);

#ifdef _DEBUG
	#ifndef LOGLEVEL
		#define LOGLEVEL	9
	#endif
	#define DEVF(fmt,...) LOG(16,(fmt),## __VA_ARGS__)
	#define LOGF(fmt,...) LOG(isDebug(DEBUG_LOG_DEV) ? -1 : 4,(fmt),## __VA_ARGS__)

	#define ONMEMORYUPDATE(a,c)	OnMemoryUpdate((a),c)
#else
	#define DEVF(fmt,...)
	#define LOGF(fmt,...)
	#ifndef LOGLEVEL
		#define LOGLEVEL	5
	#endif

	#define ONMEMORYUPDATE(a,c)
#endif

#endif