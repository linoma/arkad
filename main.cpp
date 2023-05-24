#include "arkad.h"
#include "gui.h"
#include "cps3m.h"
#include "utils.h"

GUI gui;
IMachine *machine=NULL;
u32 __cycles,__data;
int _error_level=1;//LOGLEVEL;
ICore *cpu=NULL;

int main(int argc, char *argv[]){
	int i;

	freopen ("myfile.txt","w",stderr);
    gtk_init(&argc, &argv);
	gui.Init();
#ifdef _DEVELOPa
	gui.LoadBinary("roms/m68k/lino.bin");
#endif
	gui.Loop();

	if(machine)
		machine->Destroy();

	return 0;
}

extern "C" gboolean on_destroy(GtkWidget *widget,GdkEvent  *event, gpointer   user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(widget));
	u32 id = atoi(name);
	gui.OnCloseWWindow(id,widget);
	return 1;
}

extern "C" gboolean on_paint (GtkWidget* self,cairo_t* cr,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(self));
	u32 id = atoi(name);
	gui.OnPaint(id,GTK_WIDGET(self),cr);
	return 1;
}

extern "C" void on_menuiteem_select(GtkMenuItem* item,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(item));
	u32 id = atoi(name);
	GtkWidget *p=gtk_menu_item_get_submenu(item);
	if(!p)
		gui.OnMenuItemSelect(id,item);
	else
		gui.OnPopupMenuInit(id,GTK_MENU_SHELL(p));
}

extern "C" gboolean on_mouse_down (GtkWidget* self, GdkEventButton *event,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(self));
	u32 id = atoi(name);
	if(event->type == GDK_2BUTTON_PRESS)
		return gui.OnLDblClk(id,self);
	else if(event->type != GDK_BUTTON_PRESS)
		return 0;
	if(event->button==3)
		gui.OnRButtonDown(id,self);
	else if(event->button==1)
		gui.OnLButtonDown(id,self);
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
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(window));
	u32 id = atoi(name);
	gui.OnMoveResize(id,GTK_WIDGET(window),(GdkEventConfigure *)event);
	return 0;
}

extern "C" gboolean on_scroll_change(GtkRange *range,GtkScrollType scroll,gdouble value,gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(range));
	u32 id = atoi(name);
	if(!GTK_IS_SCROLLBAR(range))
		range = (GtkRange *)gtk_bin_get_child((GtkBin *)range);
	gui.OnScroll(id,GTK_WIDGET(range),scroll);
	return FALSE;
}

extern "C" gboolean on_change_page (GtkNotebook *notebook,gint arg1, gpointer user_data){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(notebook));
	u32 id = atoi(name);

	GdkEvent *e=gtk_get_current_event();
	gui.OnCommand(id,e->type,GTK_WIDGET(notebook));
	if(e)
		gdk_event_free(e);
	return FALSE;
}

extern "C" gboolean on_key_event (GtkWidget* self,GdkEventKey event,  gpointer user_data){
	if(event.type==GDK_KEY_PRESS)
		return gui.OnKeyDown(self,event.keyval,event.state);
	if(event.type==GDK_KEY_RELEASE)
		return gui.OnKeyUp(self,event.keyval,event.state);
	return 0;
}

void EnterDebugMode(u64 v){
	gui.DebugMode(1,v);
}

void OnMemoryUpdate(u32 a,u32 b){
	gui.OnMemoryUpdate(a,b);
}

int isDebug(u64 v){
	u64 vv = gui._getStatus();
	return BT(vv,v) !=0;
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