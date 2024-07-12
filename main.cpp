#include "arkad.h"
#include "gui.h"
#include "settings.h"
#include "utils.h"

GUI gui;
IMachine *machine=NULL;
ICore *cpu=NULL;

u32 __cycles,__line,__frame,__address;
u64 __data;
int _error_level=1;//LOGLEVEL;

#ifdef __WIN32__
HINSTANCE hInst,hGdipLib=NULL;

LPGDIPBITMAPLOCKBITS pfnGdipBitmapLockBits;
LPGDIPBITMAPUNLOCKBITS pfnGdipBitmapUnlockBits;
LPGDIPDRAWIMAGERECTI pfnGdipDrawImageRectI;
LPGDIPCREATEBITMAPFROMHBITMAP pfnGdipCreateBitmapFromHBITMAP;
LPGDIPRELEASEDC pfnGdipReleaseDC;
LPGDIPDISPOSEIMAGE pfnGdipDisposeImage;
LPGDIPCREATEFROMHDC pfnGdipCreateFromHDC;
LPGDIPCREATEBITMAPFROMGDIDIB pfnGdipCreateBitmapFromGDIDIB;
LPGDIPDELETEGRAPHICS pfnGdipDeleteGraphics;
LPGDIPSTARTUP pfnGdipStartup;
LPGDIPSHUTDOWN pfnGdipShutDown;
LPGDIPLOADIMAGEFROMSTREAM pfnGdipLoadImageFromStream;

static ULONG token=0;

int WINAPI WinMain(HINSTANCE h, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow){
	INITCOMMONCONTROLSEX icc;
	GDIPSTARTUPINPUT st={1,NULL,FALSE,FALSE};
	LPVOID output;

	freopen ("stdout.txt","w",stdout);

	hInst = h;
	ZeroMemory(&icc,sizeof(INITCOMMONCONTROLSEX));
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);
	OleInitialize(NULL);

	if(hGdipLib == NULL)
       hGdipLib = LoadLibrary("gdiplus.dll");
	if(hGdipLib != NULL){
		pfnGdipStartup = (LPGDIPSTARTUP)GetProcAddress(hGdipLib,"GdiplusStartup");
		pfnGdipStartup(&token,&st,output);
		pfnGdipShutDown = (LPGDIPSHUTDOWN)GetProcAddress(hGdipLib,"GdiplusShutdown");
		pfnGdipCreateFromHDC = (LPGDIPCREATEFROMHDC)
			GetProcAddress(hGdipLib,"GdipCreateFromHDC");
		pfnGdipCreateBitmapFromHBITMAP = (LPGDIPCREATEBITMAPFROMHBITMAP)
			GetProcAddress(hGdipLib,"GdipCreateBitmapFromHBITMAP");
		pfnGdipBitmapLockBits = (LPGDIPBITMAPLOCKBITS)
			GetProcAddress(hGdipLib,"GdipBitmapLockBits");
		pfnGdipBitmapUnlockBits = (LPGDIPBITMAPUNLOCKBITS)
			GetProcAddress(hGdipLib,"GdipBitmapUnlockBits");
		pfnGdipDrawImageRectI = (LPGDIPDRAWIMAGERECTI)
			GetProcAddress(hGdipLib,"GdipDrawImageRectI");
		pfnGdipDisposeImage = (LPGDIPDISPOSEIMAGE)
			GetProcAddress(hGdipLib,"GdipDisposeImage");
		pfnGdipReleaseDC = (LPGDIPRELEASEDC)
			GetProcAddress(hGdipLib,"GdipReleaseDC");
		pfnGdipCreateBitmapFromGDIDIB = (LPGDIPCREATEBITMAPFROMGDIDIB)
			GetProcAddress(hGdipLib,"GdipCreateBitmapFromGdiDib");
		pfnGdipDeleteGraphics = (LPGDIPDELETEGRAPHICS)
			GetProcAddress(hGdipLib,"GdipDeleteGraphics");
		pfnGdipLoadImageFromStream = (LPGDIPLOADIMAGEFROMSTREAM)
			GetProcAddress(hGdipLib,"GdipLoadImageFromStream");
   }
#else

int main(int argc, char *argv[]){
	//freopen ("myfile.txt","w",stderr);
    gtk_init(&argc, &argv);
#endif
	gui.Init();
	gui.Loop();
	if(machine)
		machine->Destroy();
#ifdef __WIN32__
	if(hGdipLib != NULL){
       if(token != 0){
           pfnGdipShutDown(token);
           token = 0;
       }
       FreeLibrary(hGdipLib);
       hGdipLib = NULL;
	}
	OleUninitialize();
#endif
	return 0;
}

#ifndef __WIN32__

extern "C" gboolean on_destroy(GtkWidget *widget,GdkEvent  *event, gpointer   user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(widget));
	if(name){
		u32 id = atoi(name);
		gui.OnCloseWindow(id,widget);
	}
	return 1;
}

extern "C" gboolean on_paint (GtkWidget* self,cairo_t* cr,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(self));
	if(name){
		u32 id = atoi(name);
		gui.OnPaint(id,GTK_WIDGET(self),cr);
	}
	return 1;
}

extern "C" void on_menuitem_select(GtkMenuItem* item,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(item));
	u32 id = atoi(name);
	//printf("%s %x\n",__FUNCTION__,id);
	GtkWidget *p=gtk_menu_item_get_submenu(item);
	if(!p){
		if(GTK_IS_CHECK_MENU_ITEM(item)){
			if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
				return;
		}
		gui.OnMenuItemSelect(id,item);
	}
	else
		gui.OnPopupMenuInit(id,GTK_MENU_SHELL(p));
}

extern "C" gboolean on_mouse_up (GtkWidget* self, GdkEventButton *event,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(self));
	if(name){
		u32 id = atoi(name);
		//gui.OnMouseMove(id,self,event->state,(int)event->x,(int)event->y);
	}
	return 0;
}

extern "C" gboolean on_mouse_move (GtkWidget* self, GdkEventMotion *event,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(self));
	if(name){
		u32 id = atoi(name);
		gui.OnMouseMove(id,self,event->state,(int)event->x,(int)event->y);
	}
	return 0;
}

extern "C" gboolean on_mouse_down (GtkWidget* self, GdkEventButton *event,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(self));
	if(name){
		u32 id = atoi(name);

		if(event->type == GDK_2BUTTON_PRESS)
			return gui.OnLDblClk(id,self);
		else if(event->type != GDK_BUTTON_PRESS)
			return 0;
		if(event->button==3)
			gui.OnRButtonDown(id,self);
		else if(event->button==1)
			gui.OnLButtonDown(id,self);
	}
	return 0;
}

extern "C" void on_show_menu (GtkMenuToolButton* self,  gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(self));
	u32 id = atoi(name);
	GtkWidget *w = gtk_menu_tool_button_get_menu(self);
	printf("mii %u %p\n",id,w);
}

extern "C" void on_menu_init (GtkWidget  *item, GtkWidget *p,  gpointer   user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(item));
	u32 id = atoi(name);
	gui.OnPopupMenuInit(id,GTK_MENU_SHELL(p));
}

extern "C" void on_button_clicked(GtkButton *button,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(button));
	u32 id = atoi(name);
	gui.OnButtonClicked(id,GTK_WIDGET(button));
}

extern "C" void on_command(GtkWidget* item,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(item));
	u32 id = atoi(name);

	GdkEvent *e=gtk_get_current_event();
	gui.OnCommand(id,e->type,item);
	if(e)
		gdk_event_free(e);
}

extern "C" gboolean on_move_resize(GtkWidget *window, GdkEvent *event, gpointer data) {
	if(!((GdkEventConfigure *)event)->send_event) return 0;
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(window));
	u32 id = atoi(name);
	gui.OnMoveResize(id,GTK_WIDGET(window),((GdkEventConfigure *)event)->width,((GdkEventConfigure *)event)->height);
	return 0;
}

extern "C" gboolean on_scroll_event(GtkWidget *range,GdkEventScroll *e,gpointer user_data){
	GtkScrollType scroll;
	gpointer v;

	if(user_data){
		g_signal_emit_by_name(G_OBJECT(user_data),"scroll-event",e,&v);
		return FALSE;
	}
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(range));
	u32 id = atoi(name);
	gui.OnScroll(id,GTK_WIDGET(range),scroll);
	return FALSE;
}

extern "C" gboolean on_scroll_change(GtkRange *range,GtkScrollType scroll,gdouble value,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(range));
	u32 id = atoi(name);
	if(!GTK_IS_SCROLLBAR(range) && GTK_IS_BIN(range))
		range = (GtkRange *)gtk_bin_get_child((GtkBin *)range);
	gui.OnScroll(id,GTK_WIDGET(range),scroll);
	return FALSE;
}

extern "C" void on_select_item (GtkTreeView  *item, GtkTreePath *path,  GtkTreeViewColumn *column, gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(item));
	u32 id = atoi(name);
	gui.OnCommand(id,GDK_BUTTON_PRESS,GTK_WIDGET(item));
}

extern "C" gboolean on_change_page(GtkNotebook *notebook,GtkWidget *,gint arg1, gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(notebook));
	u32 id = (u32)-1;
	if(name)
		id=atoi(name);
	u32 command=0;
	GdkEvent *e=gtk_get_current_event();
	if(e) command=e->type;
	command |= SL(arg1,16);
	gui.OnCommand(id,command,GTK_WIDGET(notebook));
	if(e)
		gdk_event_free(e);
	return FALSE;
}

extern "C" gboolean on_key_event (GtkWidget* w,GdkEventKey event,  gpointer user_data){
	if(event.type==GDK_KEY_PRESS)
		return gui.OnKeyDown(w,event.keyval,event.state);
	if(event.type==GDK_KEY_RELEASE)
		return gui.OnKeyUp(w,event.keyval,event.state);
	return 0;
}

extern "C" void on_row_activaated (GtkTreeView  *widget, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(widget));
	u32 id = (u32)-1;
	if(name)
		id=atoi(name);
	u32 command=0;
	GdkEvent *e=gtk_get_current_event();
	if(e)
		command=e->type;
	//command |= SL(arg1,16);
	gui.OnCommand(id,command,GTK_WIDGET(widget));
	if(e)
		gdk_event_free(e);
}

#endif

void EnterDebugMode(u64 v,u64 f){
//	if(!v) *((u64 *)0)=1;
	gui.DebugMode(1,v);
}

void OnMemoryUpdate(u32 a,u32 b,void *c){
	gui.OnMemoryUpdate(a,b,(ICore *)c);
}

void _log_printf(int level,const char *fmt,...){
	va_list arg;
	FILE *fp;

	fp = level < 0 ? stderr : stdout;
	//fp=stdout;
	level = abs(level);
	va_start(arg, fmt);
    if (level > _error_level)
		goto A;
    fprintf(fp,"%u ",GetTickCount());
    vfprintf(fp, fmt, arg);
    va_end(arg);
    fflush(fp);
A:
	va_start(arg, fmt);
	gui.putf(level,fmt,arg);
    va_end(arg);
   // GLOG(level,fmt);
}

u64 isDebug(u64 v){
	return BT(gui._getStatus(),v);
}