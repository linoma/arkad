#include "utils.h"
#include <alsa/asoundlib.h>

extern "C" volatile char _binary_res_glade_start[];

void InitializeCriticalSection(u32 *cs){
	*cs=0;
}

void EnterCriticalSection(u32 *cs){
	while(!TryEnterCriticalSection(cs))
		pthread_yield();
}

int TryEnterCriticalSection(u32 *cs){
	u32 __new=1,ret=0;

	__asm__ __volatile__(
		"xorl %%eax,%%eax\n"
		"lock cmpxchg %2,%1\n"
		"setzb %0\n"
		: "=m"(ret) : "m" (*cs),"r"(__new) : "rax"
	);
	return ret;
}

void LeaveCriticalSection(u32 *cs){
	*cs=0;
}

u32 InterlockedExchange(u32 *Target,u32 Value){
    __asm__ __volatile__(
        "mov %0,%%eax\n"
        "lock xchg %%eax,%1\n"
        "mov %%eax,%0\n"
        : "=m" (Value) : "m" (Target) : "rax"
    );
    return Value;
}

DWORD GetTickCount(){
	struct timeval t;

	gettimeofday( &t, NULL );
	return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

u64 _micros(){
	struct timeval t;

	gettimeofday( &t, NULL );
	return (t.tv_sec * 1000000) + t.tv_usec;
}

u32 _log2(u32 val){
	u32 v;

	__asm__ __volatile__(
		"bsfl %1,%%eax\n"
		"mov %%eax,%0\n"
		: "=m" (v) : "m" (val) : "rax"
	);
	return v;
}

GtkWidget* GetDlgItem(GtkWidget* parent, const gchar* name){
	if(!parent)
		return 0;
	gchar *s=(gchar *)gtk_widget_get_name((GtkWidget*)parent);
	//if(s) printf("gi: %s\n",s);
	if (s && strcasecmp(s, (gchar*)name) == 0)
		return parent;
	if (GTK_IS_BIN(parent)) {
		GtkWidget *child = gtk_bin_get_child(GTK_BIN(parent));
		if((child=GetDlgItem(child, name)))
			return child;
	}
	if (GTK_IS_CONTAINER(parent)) {
		GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
		for(;children;children = g_list_next(children)){
			GtkWidget* widget = GetDlgItem((GtkWidget *)children->data, name);
			if (widget != NULL)
				return widget;
		}
	}
	if(GTK_IS_MENU_ITEM(parent)){
		GtkWidget *child=gtk_menu_item_get_submenu(GTK_MENU_ITEM(parent));
		if((child=GetDlgItem(child, name)))
			return child;
	}
	return NULL;
}

u32 GetDlgItemId(GtkWidget *w){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(w));
	if(!name) return (u32)-1;
	return atoi(name);
}

void gtk_widget_set_name(GtkWidget *w,u32 id){
	char c[20];

	sprintf(c,"%u",id);
	gtk_widget_set_name(w,c);
}

GtkWidget *CreateDialogFromResource(const char *id,void *h){
	GtkBuilder *_builder = gtk_builder_new();
	gtk_builder_add_from_string (_builder,(gchar *)_binary_res_glade_start,-1,NULL);
	GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(_builder, id));
	gtk_builder_connect_signals(_builder, h);
	//gtk_widget_show(w);
	g_object_unref(_builder);
	return w;
}

void disable_gobject_callback(GObject *obj, GCallback cb,gboolean _block){
  gulong hid;

  if (!obj || !cb) {
    return;
  }
  hid = g_signal_handler_find(obj, G_SIGNAL_MATCH_FUNC,0, 0, NULL,(gpointer) cb, NULL);
  if(_block)
	g_signal_handler_block(obj, hid);
else
	g_signal_handler_unblock(obj, hid);
}

void FlashWindow(GtkWidget *hwnd,u32 uCount){
	u32 i;
	MSG m;

	for(i=0;i<uCount;i++){
		EnableWindow(hwnd,(i & 1) ? TRUE : FALSE);
		//gdk_window_beep(gtk_widget_get_window(hwnd));
		while(PeekMessage(&m))
			DispatchMessage(&m);
		Sleep(150);
	}
}

void Sleep(u32 dwMilliseconds){
	usleep(dwMilliseconds*1000);
}

void EnableWindow(GtkWidget *hwnd,int enable){
	gtk_widget_set_sensitive(hwnd,enable);
}

DWORD DispatchMessage(LPMSG lpMsg){
#ifndef __WIN32__
	//gtk_main_do_event(lpMsg);
	gtk_main_iteration_do (false);
	return 0;
#else
	return ::DispatchMessage(lpMsg);
#endif
}

BOOL PeekMessage(LPMSG lpMsg){
#ifndef __WIN32__
	HWND hwnd;

	gtk_main_iteration_do (false);
	hwnd = gtk_grab_get_current();
	if(hwnd != NULL){
		if(GTK_IS_MENU_SHELL(hwnd))
			return TRUE;
	}
	return gtk_events_pending();
#else
   return ::PeekMessage(lpMsg,NULL,0,0,PM_REMOVE);
#endif
}

int beep(int freq,int duration,int vol){
	int err,res;
    unsigned int i;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;
	char *buf;

    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		return -1;
	res=-2;
	buf=NULL;
    if ((err = snd_pcm_set_params(handle,SND_PCM_FORMAT_U8,SND_PCM_ACCESS_RW_INTERLEAVED,
                      1,11025,1, 500000)) < 0) goto A;
	i=11025.0f / freq;
	if(!(buf= new char[11025]))
		goto A;
	vol = std::max(255,vol);
   while(duration){
	   int x,n,m = std::min(duration,1000);
	   duration -= m;
	   x = (11.025f*m)-i;
	   	for(n=0;n<=x;){
			int ii;

			for(ii=0;ii<i/2;ii++)
				buf[n++]=255;
			for(;ii<i;ii++)
				buf[n++]=0;
			//printf("\t %d %d\n",ii,n);
		}

		//printf("%d %d %d dur:%d\n",n,i,x,m);
		for(x=n;x<11025;x++)
			buf[x]=0;
        frames = snd_pcm_writei(handle, buf, n);
        if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
        if (frames < 0) {
           // printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
            break;
        }
        //if (frames > 0 && frames < (long)11025)
          //  printf("Short write (expected %li, wrote %li)\n", (long)11025, frames);
    }
	res=0;
A:
	err = snd_pcm_drain(handle);
    snd_pcm_close(handle);
    if(buf)
		delete []buf;
    return res;
}