#ifndef __WIN32__
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo/cairo.h>
#include <pthread.h>
#include <sys/time.h>
#else
#include <windows.h>
#include <commctrl.h>
#endif

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cstdarg>
#include "icore.h"

#ifndef __ARKADH__
#define __ARKADH__

#ifndef I_FASTCALL
	#ifdef __GNUC__
      	#define I_FASTCALL __attribute__ ((regparm(2)))
  	#elif defined(__WATCOMC__)
  		#define I_FASTCALL __watcall
   #else
       #define I_FASTCALL __fastcall
   #endif
#endif

#ifndef I_STDCALL
	#ifdef __GNUC__
		#define I_STDCALL __attribute__ ((stdcall))
	#else
		#define I_STDCALL __stdcall
	#endif
#endif

#ifndef I_CDECL
	#ifdef __GNUC__
		#define I_CDECL __attribute__ ((cdecl))
	#else
		#define I_CDECL __cdecl
	#endif
#endif

#ifndef I_EXPORT
	#ifdef __WIN32__
		#define I_EXPORT __declspec(dllexport)
	#else
		#define I_EXPORT
	#endif
#endif

typedef struct __IGame : IObject,IStreamer{
	virtual int AddFile(char *,u32)=0;
	virtual int GetFile(u32,char *)=0;
} IGame;

typedef struct __ISerializable{
	virtual int Serialize(IStreamer *)=0;
	virtual int Unserialize(IStreamer *)=0;
} ISerializable;

#define MACHINE_EVENT(a) ((u32)(0x80000000|(a)))

typedef struct __IMachine {
	virtual int OnEvent(u32,...)=0;
	virtual int Draw(HDC)=0;

	virtual int Exec(u32)=0;
	virtual int Load(IGame *,char *)=0;
	virtual int Destroy()=0;
	virtual int Init()=0;
	virtual int Reset()=0;

	virtual int LoadSettings(void * &)=0;
	virtual int SaveState(IStreamer *)=0;
	virtual int LoadState(IStreamer *)=0;
} IMachine;

typedef struct{
   char fileName[1024];
   u32 dwSize,dwSizeCompressed,dwFlags;
} COMPRESSEDFILEINFO,*LPCOMPRESSEDFILEINFO;

typedef struct __ICompressedFile : public IStreamer{
	virtual void SetFileStream(IStreamer *pStream) = 0;
	virtual int AddCompressedFile(const char *lpFileName,const int iLevel = 9) =0;
	virtual int DeleteCompressedFile(u32 index) =0;
	virtual u32 ReadCompressedFile(void * buf,u32 dwByte) =0;
	virtual int OpenCompressedFile(u16 uIndex,int mode=0) =0;
	virtual u32 WriteCompressedFile(void * buf,u32 dwByte) =0;
	virtual void Rebuild() =0;
	virtual int get_FileCompressedInfo(u32 index,LPCOMPRESSEDFILEINFO p) =0;
	virtual u32 Count() =0;
} ICompressedFile;

#define NOARG

#define STR_(x)			#x
#define STR(x) 			STR_(x)

#ifdef __WIN32__
	#define DPC_PATH '\\'
	#define DPS_PATH "\\"
	#define DPC_PATH_INVERSE '/'
	#define DPS_PATH_INVERSE "/"

	#define RES(x) 			((int)x)
#else
	#define DPC_PATH '/'
	#define DPS_PATH  "/"
	#define DPC_PATH_INVERSE '\\'
	#define DPS_PATH_INVERSE "\\"

	#define RES(x) 			STR(x)

	#define RGB(a,b,c) (SL((u8)c,16)|SL((u8)b,8)|(u8)a)

	#define MAKELONG(a,b) (((b)<<16)|(a))
	#define MAKEWORD(a,b) (((b)<<8)|(a))

	#define LOWORD(a) ((u16)a)
	#define HIWORD(a) ((u16)SR(a,16))
	#define HIBYTE(a) ((u8)SR(a,8))
	#define LOBYTE(a) ((u8)a)
#endif

#define CONCAT_(a,b) a##b
#define CONCAT(a,b) CONCAT_(a,b)

#define MAKEHWORD(a,b) ((((u8)(b))<<8)|(((u8)a)))
#define MAKEBYTE(a,b) (((b)<<4)|(a))

#define AWORD(a,b) *((u32 *)&((u8 *)a)[b])
#define AHWORD(a,b) *((u16 *)&((u8 *)a)[b])
#define ABYTE(a,b) *((u8 *)&((u8 *)a)[b])

#define RGB555(a,b,c) (SL(c,10)|SL(b,5)|a)

#define KB(a) ((a)*1024)
#define MB(a) (KB(a)*1024)

#define KHZ(a) ((a)*1000)
#define MHZ(a) (KHZ((a))*1000)
#define GHZ(a) (MHZ((a))*1000)

#define EX_SIGN(a,b) SR(SL((s32)a,(32-b)),(32-b))
#define RADIANS(a) ((a) * (3.141592f / 180.0f))

#define SC_FXD	12
#define SM_FXD	(SL(1,SC_FXD)-1)
#define NFXD(a) SL((a),SC_FXD)
#define ONEFXD NFXD(1)
#define DFXD(a) ((s32)((a)/(float)ONEFXD))
#define IFXD(a) (SR((s32)(a),SC_FXD))

extern u32 __cycles,__line,__frame,__address;
extern u64 __data;
extern int _error_level;
extern ICore *cpu;
extern IMachine *machine;

#ifdef __WIN32__
extern HINSTANCE hInst;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __WIN32__

void on_menuitem_select(GtkMenuItem* item,gpointer user_data);
gboolean on_mouse_down (GtkWidget* self, GdkEventButton *event,gpointer user_data);
gboolean on_mouse_move (GtkWidget* self, GdkEventMotion *event,gpointer user_data);
void on_menu_init (GtkWidget  *item, GtkWidget *p,  gpointer   user_data);
gboolean on_scroll_event(GtkWidget *range,GdkEventScroll *e,gpointer user_data);
void on_button_clicked(GtkButton *button,gpointer user_data);
void on_command(GtkWidget* item,gpointer user_data);
gboolean on_scroll_change(GtkRange *range,GtkScrollType scroll,gdouble value,gpointer user_data);
gboolean on_change_page (GtkNotebook *notebook,GtkWidget *,gint arg1, gpointer user_data);
gboolean on_paint (GtkWidget* self,cairo_t* cr,gpointer user_data);

#else

extern LPGDIPBITMAPLOCKBITS pfnGdipBitmapLockBits;
extern LPGDIPBITMAPUNLOCKBITS pfnGdipBitmapUnlockBits;
extern LPGDIPDRAWIMAGERECTI pfnGdipDrawImageRectI;
extern LPGDIPCREATEBITMAPFROMHBITMAP pfnGdipCreateBitmapFromHBITMAP;
extern LPGDIPRELEASEDC pfnGdipReleaseDC;
extern LPGDIPDISPOSEIMAGE pfnGdipDisposeImage;
extern LPGDIPCREATEFROMHDC pfnGdipCreateFromHDC;
extern LPGDIPCREATEBITMAPFROMGDIDIB pfnGdipCreateBitmapFromGDIDIB;
extern LPGDIPDELETEGRAPHICS pfnGdipDeleteGraphics;
extern LPGDIPSTARTUP pfnGdipStartup;
extern LPGDIPSHUTDOWN pfnGdipShutDown;
extern LPGDIPLOADIMAGEFROMSTREAM pfnGdipLoadImageFromStream;

#endif

#ifdef __cplusplus
}
#endif


u64 isDebug(u64);
void EnterDebugMode(u64 v=0,u64 f=0);
void OnMemoryUpdate(u32 a,u32 b,void *c=NULL);
void _log_printf(int level,const char *fmt,...);

#define EXIT(fmt,...) {printf((fmt),## __VA_ARGS__);exit(-1);}

#define CALLVA(a,b,c){va_list arg__;va_start(arg__,b);(c)=a(b,arg__);va_end(arg__);}

#ifdef _DEBUG
	#ifndef LOGLEVEL
		#define LOGLEVEL	9
	#endif
	#define LOG(level,fmt,...) _log_printf(level,(fmt),## __VA_ARGS__)
	#define LOGV(fmt,...) LOG(1,(fmt),## __VA_ARGS__)
	#define LOGI(fmt,...) LOG(2,(fmt),## __VA_ARGS__)
#ifdef _DEVELOP
	#define LOGD(fmt,...) LOG(4,(fmt),## __VA_ARGS__)
	#define LOGE(fmt,...) LOG(8,(fmt),## __VA_ARGS__)
#else
	#define LOGD(fmt,...)
	#define LOGE(fmt,...)
#endif
	#define DEVF(fmt,...) LOG(16,(fmt),## __VA_ARGS__)
	#define LOGF(fmt,...) LOG(isDebug(DEBUG_LOG_DEV) ? -1 : 4,(fmt),## __VA_ARGS__)

	#define ONMEMORYUPDATE(a,b,c)	OnMemoryUpdate((a),b,c)

	#define _LOG(l,a,...){\
		u32 pc__;cpu->Query(ICORE_QUERY_PC,&pc__);\
		LOG(isDebug(DEBUG_LOG_DEV) ? -1 : l,STR(%08x %u %u %u\x09) a STR(\n),pc__,__cycles,__frame,__line,## __VA_ARGS__);}
	#define DLOG(a,...) _LOG(4,a,## __VA_ARGS__)
#else
	#define DEVF(fmt,...)
	#define LOGF(fmt,...)
	#ifndef LOGLEVEL
		#define LOGLEVEL	5
	#endif

	#define ONMEMORYUPDATE(a,b,c)

	#define LOG(level,fmt,...) _log_printf(level,(fmt),## __VA_ARGS__)
	#define LOGV(fmt,...)
	#define LOGI(fmt,...)
	#define LOGD(fmt,...)
	#define LOGE(fmt,...)

	#define DLOG(a,...)
#endif

#endif