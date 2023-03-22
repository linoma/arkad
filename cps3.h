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

#ifndef __CPS3H__
#define __CPS3H__

typedef GtkWidget 	*HWND;
typedef GdkPixbuf 	*HBITMAP;
typedef GdkEvent	MSG;
typedef MSG *		LPMSG;

typedef unsigned char BOOL;
typedef unsigned long LONG;
typedef u32 DWORD;

#define NOARG

#define MAKEWORD(a,b) (((b)<<16)|(a))
#define MAKEHWORD(a,b) (((b)<<8)|(a))
#define MAKEBYTE(a,b) (((b)<<4)|(a))

#define LOWORD(a) ((u16)a)
#define HIWORD(a) ((u16)SR(a,16))

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

#define AWORD(a,b) *((u32 *)&((u8 *)a)[b])
#define AHWORD(a,b) *((u16 *)&((u8 *)a)[b])
#define ABYTE(a,b) *((u8 *)&((u8 *)a)[b])

#define HIWORD(a) ((u16)SR(a,16))
#define LOWORD(a) ((u16)a)

#define S_QUIT		BV(0)
#define S_INIT		BV(1)
#define S_RUN		BV(2)
#define I_PAUSE		3
#define S_PAUSE		BV(I_PAUSE)
#define S_DEBUG_NEXT BV(4)

#define S_DEBUG		BV(8)
#define S_LOAD		BV(16)

extern u32 __cycles;
extern u8 _portb;
extern int _error_level;
extern ICore *cpu;

//typedef int(*CoreCallback)(int);

#ifdef __cplusplus
extern "C" {
#endif

void _log_printf(int level,const char *fmt,...);
void on_menuiteem_select(GtkMenuItem* item,gpointer user_data);
gboolean on_mouse_down (GtkWidget* self, GdkEventButton *event,gpointer user_data);
void on_menu_init (GtkWidget  *item, GtkWidget *p,  gpointer   user_data);
void on_button_clicked(GtkButton *button,gpointer user_data);
void on_command(GtkWidget* item,gpointer user_data);

#ifdef __cplusplus
}
#endif

void EnterDebugMode(u64 v=0);
void OnMemoryUpdate(u32 a,u32 b);

#ifdef _DEBUG
	#ifndef LOGLEVEL
		#define LOGLEVEL	9
	#endif
	#define DEVF(fmt,...) LOG(9,(fmt),## __VA_ARGS__)

	#define ONMEMORYUPDATE(a,c)	OnMemoryUpdate((a),c)
#else
	#define DEVF(fmt,...)
	#ifndef LOGLEVEL
		#define LOGLEVEL	5
	#endif

	#define ONMEMORYUPDATE(a,c)
#endif

#define LOG(level,fmt,...) _log_printf(level,(fmt),## __VA_ARGS__)
#define LOGV(fmt,...) LOG(5,(fmt),## __VA_ARGS__)
#define LOGI(fmt,...) LOG(6,(fmt),## __VA_ARGS__)
#define LOGD(fmt,...) LOG(8,(fmt),## __VA_ARGS__)
#define LOGE(fmt,...) LOG(-1,(fmt),## __VA_ARGS__)

#define STR_IMPL_(x) #x      //stringify argument
#define STR(x) STR_IMPL_(x)
#define RES(x) STR(x)

#define EXIT(fmt,...) printf((fmt),## __VA_ARGS__);exit(-1);


#endif