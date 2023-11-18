#include "gui.h"
#include "utils.h"
#include "resource.h"

extern GUI gui;
#ifdef __WIN32__
HWND Settings::hwndPropertySheet=0;
#endif

Settings::Settings(){
	_items.insert({"antialias","1"});
	u16 a[]={GDK_KEY_Up,GDK_KEY_Down,GDK_KEY_Left,GDK_KEY_Right,'a','s','q','p','w','z','x','o','l','k','m','n','1','2','3','4',0};
	_items.insert({"keys",string((char *)a,sizeof(a))});
	_items.insert({"frameskip","-1"});
	_items.insert({"pcm_buffer_size","3"});
	_items.insert({"pcm_sync","1"});
	_items.insert({"fps_sync","1"});
	_items.insert({"width","384"});
	_items.insert({"height","224"});
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
	HWND w,nb,b;
#ifdef __WIN32__
	PROPSHEETPAGE psp[2]={0};
	PROPSHEETHEADER psh={0};
	int i;

   i = 0;
   psp[i].dwSize = sizeof(PROPSHEETPAGE);
   psp[i].dwFlags = PSP_DEFAULT;
   psp[i].hInstance = hInst;
   psp[i].pszTemplate = MAKEINTRESOURCE(IDD_DIALOG16);
   psp[i].pfnDlgProc = (DLGPROC)DlgProc16;
   psp[i++].lParam = (LPARAM)this;

  /* psp[i].dwSize = sizeof(PROPSHEETPAGE);
   psp[i].dwFlags = PSP_DEFAULT;
   psp[i].hInstance = hInst;
   psp[i].pszTemplate = MAKEINTRESOURCE(IDD_DIALOG22);
   psp[i].pfnDlgProc = (DLGPROC)DlgProc22;
   psp[i++].lParam = (LPARAM)this;*/

   psh.dwSize = sizeof(PROPSHEETHEADER);
   psh.dwFlags = PSH_NOAPPLYNOW|PSH_USEICONID|PSH_PROPSHEETPAGE|PSH_DEFAULT|PSH_USECALLBACK;
   psh.pfnCallback = PropSheetProc;
   psh.pszCaption = "Properties...";
   psh.pszIcon = MAKEINTRESOURCE(2);
   psh.hInstance = hInst;
   psh.hwndParent = gui._getWindow();
   psh.nPages = i;
   psh.ppsp = psp;
   if(PropertySheet(&psh) == -1)
       return -1;

#else
	GtkWidget *dlg=CreateDialogFromResource("102");

	gtk_dialog_add_buttons(GTK_DIALOG(dlg),"Ok",GTK_RESPONSE_OK,
				"Cancel",GTK_RESPONSE_CANCEL,NULL);
	nb=GetDlgItem(dlg,RES(100));
	{
		int i = stoi(_items["antialias"]);
		w = GetDlgItem(dlg,RES(1000));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),i);
	}
	{
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;
		GtkListStore *model;
		int height;

		w = GetDlgItem(nb,RES(2000));
		model = gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_INT);
		gtk_tree_view_set_model(GTK_TREE_VIEW(w),GTK_TREE_MODEL(model));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(w), FALSE);
		gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(w),TRUE);
		column = gtk_tree_view_column_new();
		g_object_set(column,"sizing",GTK_TREE_VIEW_COLUMN_FIXED,NULL);
		gtk_tree_view_column_set_title(column,"Text");

		renderer = gtk_cell_renderer_text_new();
		g_object_set(renderer,"ypad",0,"yalign",0.5,"family","Monospace","family-set",true,NULL);
		gtk_cell_renderer_set_fixed_size(renderer,-1,12);
		gtk_tree_view_column_pack_start(column,renderer,TRUE);

		gtk_tree_view_column_set_attributes(column,renderer,"text",0,NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(w), column);

		u16 *k = (u16 *)_items["keys"].c_str();
		model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)w);
		for(int i=0;model && k && *k;k++,i++){
			char c[200];
			HTREEITEM iter;

			if((iter = new GtkTreeIter[1])){
				gtk_list_store_append(model,iter);
				char *p=gdk_keyval_name(*k);
				if(p)
					sprintf(c,"%2d %s",i+1,p);
				else
					sprintf(c,"%2d %04X %c",i+1,*k,isprint(*k) ? *k :0x20);
				gtk_list_store_set(model,iter,0,c, -1);
				gtk_widget_queue_resize_no_redraw(w);
				//g_object_thaw_notify(G_OBJECT(listbox));
			}
		}
	}

	if(gtk_dialog_run(GTK_DIALOG(dlg)) != GTK_RESPONSE_OK)
		goto Z;
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
Z:
	gtk_widget_destroy(dlg);
#endif
	return 0;
}

#ifdef __WIN32__

int CALLBACK Settings::PropSheetProc(HWND hwndDlg,UINT uMsg,LPARAM lParam){
	if(uMsg == PSCB_INITIALIZED)
		Settings::hwndPropertySheet = hwndDlg;
	return 0;
}

BOOL CALLBACK Settings::DlgProc16(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
	switch(msg){
		case WM_INITDIALOG:
			SetWindowLong(hWnd,GWLP_USERDATA,lParam);
		break;
		case WM_NOTIFY:
           if(((LPNMHDR)lParam)->code == PSN_APPLY){
		  }
		break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
			}
        break;
	}
	return FALSE;
}
#endif