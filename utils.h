#include "arkad.h"

#ifndef __UTILSH__
#define __UTILSH__

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


#endif