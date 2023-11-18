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
#pragma pack(pop)

#ifndef __WIN32__

#pragma pack(push)

#pragma pack(1)
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

typedef size_t FILETIME,*LPFILETIME;

typedef struct _SYSTEMTIME {
    u16 wYear;
    u16 wMonth;
    u16 wDayOfWeek;
    u16 wDay;
    u16 wHour;
    u16 wMinute;
    u16 wSecond;
    u16 wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

DWORD GetTickCount();
void GetLocalTime(LPSYSTEMTIME lpSystemTime);
int SystemTimeToFileTime(SYSTEMTIME *lpSystemTime,LPFILETIME lpFileTime);
int DosDateTimeToFileTime(u16 wFatDate,u16 wFatTime,LPFILETIME lpFileTime);
int FileTimeToDosDateTime(FILETIME *lpFileTime,u16 * lpFatDate,u16 * lpFatTime);
int FileTimeToLocalFileTime(FILETIME *lpFileTime,LPFILETIME lpLocalFileTime);
int FileTimeToSystemTime(FILETIME *lpFileTime,LPSYSTEMTIME lpSystemTime);

void FlashWindow(HWND hwnd,u32 uCount);
void Sleep(u32 dwMilliseconds);
void EnableWindow(HWND hwnd,int enable);

HWND GetDlgItem(HWND parent, const gchar* name);
void EnterCriticalSection(CRITICAL_SECTION *cs);
int TryEnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
void InitializeCriticalSection(CRITICAL_SECTION *cs);

DWORD DispatchMessage(LPMSG lpMsg);

u32 GetTempFileName(char * lpPathName,char * lpPrefixString,u32 uUnique,char *lpTempFileName);
u32 GetTempPath(u32 nBufferLength,char *lpBuffer);


#else
int CGdiPlusLoadBitmapResource(LPCTSTR pName, LPCTSTR pType, HMODULE hInst,void **ret);
#endif

BOOL PeekMessage(LPMSG lpMsg);

HWND CreateDialogFromResource(const char *id,void *h=0);
void disable_gobject_callback(GObject *obj, GCallback cb,gboolean _block);
u32 _log2(u32 v);
u32 GetDlgItemId(HWND w);
void gtk_widget_set_name(HWND w,u32 id);

int beep(int freq,int duration,int vol);
int SaveBitmap(char *fn,int width,int height,int nc,int bp,u8 *data);

#endif