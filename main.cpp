#include "cps3.h"
#include "gui.h"
#include "cps3m.h"
#include "utils.h"

GUI gui;
Machine *machine=NULL;
u32 __cycles;
int _error_level=8;//LOGLEVEL;
ICore *cpu=NULL;

int main(int argc, char *argv[]){
	int i;

	freopen ("myfile.txt","w",stderr);
    gtk_init(&argc, &argv);
	gui.Init();

	gui.LoadBinary("");
	gui.Loop();
	delete machine;

	return 0;
}

extern "C" void on_destroy(){
	gui.Quit();
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

void EnterDebugMode(u64 v){
	gui.DebugMode(1,v);
}

void OnMemoryUpdate(u32 a,u32 b){
	gui.OnMemoryUpdate(a,b);
}

void _log_printf(int level,const char *fmt,...){
	va_list arg;
	FILE *fp;

	fp = level < 0 ? stderr : stdout;
	fp=stdout;
	level = abs(level);
	va_start(arg, fmt);
    if (level > _error_level)
		goto A;
    fprintf(fp,"%u ",GetTickCount());
    vfprintf(fp, fmt, arg);
    va_end(arg);
A:
	va_start(arg, fmt);
	gui.putf(level,fmt,arg);

    va_end(arg);
   // GLOG(level,fmt);
}