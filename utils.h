#include "arkad.h"

#ifndef __UTILSH__
#define __UTILSH__


#pragma pack(push)
#pragma pack(1)

typedef struct{
  u8  riff[4];
  u32 len;
  u8  cWavFmt[8];
  u32 dwHdrLen;
  u16 wFormat;
  u16 wNumChannels;
  u32 dwSampleRate;
  u32 dwBytesPerSec;
  u16 wBlockAlign;
  u16 wBitsPerSample;
  u8  cData[4];
  u32 dwDataLen;
} WAVHDR, *PWAVHDR, *LPWAVHDR;

typedef struct{
   	u16 bfType;
   	u32 bfSize;
   	u16 bfReserved1;
   	u16 bfReserved2;
   	u32 bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER, *LPBITMAPFILEHEADER;

#pragma pack(pop)

typedef struct{
   	u32 biSize;
   	u32 biWidth;
   	u32 biHeight;
   	u16 biPlanes;
   	u16 biBitCount;
   	u32 biCompression;
   	u32 biSizeImage;
   	u32 biXPelsPerMeter;
   	u32 biYPelsPerMeter;
   	u32 biClrUsed;
   	u32 biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;

typedef struct {
  	u8 rgbBlue;
  	u8 rgbGreen;
  	u8 rgbRed;
  	u8 rgbReserved;
} RGBQUAD, *LPRGBQUAD;

typedef struct tagBITMAPINFO{
   	BITMAPINFOHEADER bmiHeader;
   	RGBQUAD          bmiColors[1];
} BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO;

DWORD GetTickCount();
u64 _micros();
void EnterCriticalSection(u32 *cs);
int TryEnterCriticalSection(u32 *cs);
void LeaveCriticalSection(u32 *cs);
void InitializeCriticalSection(u32 *cs);
GtkWidget* GetDlgItem(GtkWidget* parent, const gchar* name);
GtkWidget *CreateDialogFromResource(const char *id,void *h=0);
void disable_gobject_callback(GObject *obj, GCallback cb,gboolean _block);
u32 _log2(u32 v);
u32 GetDlgItemId(GtkWidget *w);
void gtk_widget_set_name(GtkWidget *w,u32 id);
void FlashWindow(GtkWidget *hwnd,u32 uCount);
void Sleep(u32 dwMilliseconds);
void EnableWindow(GtkWidget *hwnd,int enable);
BOOL PeekMessage(LPMSG lpMsg);
DWORD DispatchMessage(LPMSG lpMsg);
int beep(int freq,int duration,int vol);
int SaveBitmap(char *fn,int width,int height,int nc,int bp,u8 *data);

#endif