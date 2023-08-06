#include "settings.h"
#include "utils.h"

Settings::Settings(){
	_items.insert({"antialias","1"});
	u16 a[]={GDK_KEY_Left,GDK_KEY_Right,GDK_KEY_Up,GDK_KEY_Down,'a','s','p','q','w','z','x',0};
	_items.insert({"keys",string((char *)a,sizeof(a))});
}

Settings::~Settings(){
}

string & Settings::operator[] (const string& k){
	return _items[k];
}

int Settings::Load(char *fn){
	return -1;
}

int Settings::Save(char *fn){
	return -1;
}

int Settings::Show(HWND p){
	GtkWidget *dlg=CreateDialogFromResource("102");
	gtk_dialog_add_buttons(GTK_DIALOG(dlg),"Ok",GTK_RESPONSE_OK,
				"Cancel",GTK_RESPONSE_CANCEL,NULL);
	HWND w = GetDlgItem(dlg,STR(1000));
	{
		int i = stoi(_items["antialias"]);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),i);
	}
	if(gtk_dialog_run(GTK_DIALOG(dlg)) != GTK_RESPONSE_OK)
		goto Z;
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
Z:
	gtk_widget_destroy(dlg);
	return 0;
}