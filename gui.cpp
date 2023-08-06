#include "gui.h"
#include "utils.h"
#include "cps3m.h"
#include "resource.h"
#include "blacktiger.h"
#include "cps2m.h"
#include <iostream>

extern GUI gui;

GUI::GUI() : DebugWindow(),GameManager(),Settings(){
	_window=NULL;
	key_snooper = 0;
}

GUI::~GUI(){
	if(key_snooper)
		gtk_key_snooper_remove(key_snooper);
	key_snooper = 0;
}

void GUI::Loop(){
	int ret;
	while(!BT(_status,S_QUIT)){
B:
		if(!BT(DebugWindow::_status,S_RUN))
			goto A;

		if(BT(DebugWindow::_status,S_DEBUG)){
			if(DebugWindow::Loop())
				goto C;
		}
		switch((u16)(ret = machine->Exec(_status))){
			case 0:
			break;
			case 2:
				DebugMode(1,0,SR(ret,28)&7);
			case 1:
			case 3:
			default:
				while(PeekMessage(NULL))
					DispatchMessage(NULL);
			break;
		}
		continue;
A:

C:
		while(PeekMessage(NULL))
			DispatchMessage(NULL);
	}
	//pthread_join(thread1,NULL);
}

int GUI::Init(){
	string s =_items.at("keys");
	u16 *b = (u16 *)s.c_str();
	memcpy(_keys,b,s.length());

#ifndef __WIN32__
	if(!(_window=CreateDialogFromResource("gui")))
		return -1;
	GtkWidget *w=CreateDialogFromResource("101");

	gtk_window_set_transient_for(GTK_WINDOW(w),GTK_WINDOW(_window));
	gtk_widget_show_all(_window);
	while(PeekMessage(0))
		DispatchMessage(NULL);
#endif
	DebugWindow::Init(w);
	GameManager::Init();
	key_snooper = gtk_key_snooper_install(OnKeyEvent,this);
#ifdef _DEVELOP
//	ChangeCore(1);
	//LoadBinary("roms/m68k/lino.bin");
	//LoadBinary("roms/z800/float.bin");

	{
		u32 m,g;
		//char fileName[]={"roms/redearthnocd/redearthn_asia_nocd.29f400.u2"};
		//char fileName[]={"roms/redearth/redearth_euro.29f400.u2"};
		//char fileName[]={"roms/sfiii3_japan_nocd.29f400.u2"};
		char fileName[]={"roms/blktiger/bdu-01a.5e"};
		if(!Find(fileName,&g,NULL))
			Load(g,fileName);
	}
#endif
	while(PeekMessage(0))
		DispatchMessage(NULL);
	return 0;
}

int GUI::LoadBinary(char *p){
	machine->Load(NULL,p);
	BS(_status,S_LOAD|S_RUN);//|S_DEBUG|S_PAUSE
#ifdef _DEVELOP
	DebugMode(1);
	LoadBreakpoints();
    LoadNotes();
#endif
	return 0;
}

int GUI::Load(u32 g,char *p){
	u32 mi;
	IGame *gg;

	if(!g && !p)
		return -1;
	Reset();
	gg=NULL;
	mi=0;
	if(g){
		gg = at(g-1);
		if(MachineIndexFromGame((IGame *)gg,&mi))
			return -2;
	}

	ChangeCore(mi);

	if(machine->Load((IGame *)gg,p))
		return -3;
	_game=g;

	BS(_status,S_LOAD|S_RUN);//|S_DEBUG|S_PAUSE

	_updateToolbar();
#ifdef _DEVELOP
	DebugMode(1);
	LoadBreakpoints();
    LoadNotes();
#endif
	return 0;
}

int GUI::ChangeCore(int c){
	if(_machine == (c+1))
		return 0;
	_machine=0;
	DebugWindow::OnDisableCore();
	if(machine){
		machine->Destroy();
		machine=NULL;
		cpu=NULL;
	}

	switch(c){
		case 0:
			machine = new cps3::CPS3M();
			cpu = (SH2Cpu *)(cps3::CPS3M *)machine;
		break;
		case 1:
			machine = new blktiger::BlackTiger();
			cpu = (blktiger::Z80Cpu *)(blktiger::BlackTiger *)machine;
		break;
		case 2:
			machine= new cps2::CPS2M();
			cpu = (cps2::M68000Cpu *)(cps2::CPS2M *)machine;
		break;
		default:
			return -1;
	}

	machine->Init();
	_machine = c+1;

	DebugWindow::OnEnableCore();

	machine->LoadSettings((void * &)_items);

	{
		int w=stoi(_items["width"]);
		int h=stoi(_items["height"]);
		gtk_window_resize(GTK_WINDOW(_window),w,h);
	//	printf("%s %d %d\n",__FUNCTION__,w,h);
	}
	Reset();
	return 0;
}

int GUI::OnPaint(u32 id,GtkWidget *w,cairo_t* cr){
	switch(id){
		case 1001:
			if(machine)
				machine->Draw(cr);
		break;
	}
	return 0;
}

gint GUI::OnKeyEvent(GtkWidget *w,GdkEventKey *event,gpointer func_data){
	if(event->type == GDK_KEY_PRESS){
		int key=event->keyval;

		if(event->keyval==113 && (event->state & 4))
			key=-2;
		return gui.OnKeyDown(w,key,event->state);
	}
	else if(event->type == GDK_KEY_RELEASE)
		return gui.OnKeyUp(w,event->keyval,event->state);
	return FALSE;
}

int GUI::OnKeyDown(GtkWidget *w,int key,u32 wParam){
	u32 id = (u32)-1;
	char *name = (char *)gtk_widget_get_name(w);
	if(name)
		id=atoi(name);
	switch(key){
		case GDK_KEY_F2:
			OnButtonClicked(5005,NULL);
		break;
#ifdef _DEBUG
		case -2:
			Quit();
			return 0;
#endif
		default:
			switch(id){
				case 101:
					return DebugWindow::OnKeyDown(key,wParam);
				case 100:
					for(int i=0;i<sizeof(_keys)/sizeof(u16);i++){
						if(key!=_keys[i]) continue;
						if(machine)
							machine->OnEvent(ME_KEYDOWN,i,wParam);
						break;
					}
				default:
					//LOGD("%x\n",gdk_keyval_to_unicode(key));
				break;
			}
		break;
	}
	return 0;
}

int GUI::OnKeyUp(GtkWidget *w,int key,u32 wParam){
	u32 id = (u32)-1;
	char *name = (char *)gtk_widget_get_name(w);
	if(name)
		id=atoi(name);
	switch(id){
		case 100:
			for(int i=0;i<sizeof(_keys)/sizeof(u16);i++){
						if(key!=_keys[i]) continue;
						if(machine)
								machine->OnEvent(ME_KEYUP,i,wParam);
						break;
				}
		default:
			//LOGD("%x\n",gdk_keyval_to_unicode(key));
		break;
	}
	return 0;
}

void GUI::OnMoveResize(u32 id,GtkWidget *w,GdkEventConfigure *e){
	if(!e->send_event)
		return;
	switch(id){
		case 101:
			DebugWindow::OnMoveResize(id,w,e);
			break;
		case 1001:
			if(machine)
				machine->OnEvent(ME_MOVEWINDOW,0,0,e->width,e->height);
			break;
	}
}

int GUI::OnCloseWWindow(u32 id,GtkWidget *w){
	switch(id){
		case 100:
			Quit();
		break;
		case 101:
			OnButtonClicked(IDC_DEBUG_CLOSE,w);
		break;
	}
	return 0;
}

void GUI::OnButtonClicked(u32 id,GtkWidget *w){
	//printf("OnButtonClicked %d\n",id);
	switch(id){
		case 5000:
		case 5001:
		case 5051:
			DebugMode(1);
		break;
		case 5002:
			DebugMode(2);
		break;
		case 5003:
			DebugMode(3);
		break;
		case 5004:
		break;
		case 5507:
		case 5005:
		{
			char s[1024];

			//_game = 2;
			if(_game){
				IGame *p=at(_game-1);
				p->Query(IGAME_GET_FILENAME,s);
				Load(_game,s);
			}
			else{
				cpu->Query(ICORE_QUERY_FILENAME,s);
				LoadBinary((char *)s);
			}
		}
		break;
		case 5010:
		   Quit();
		break;
		case 5050:
			{
				char fileName[1024],ext[1024];

				*((u64 *)fileName)=0;
				*((u64 *)ext)=0;
				if(!cpu || cpu->Query(ICORE_QUERY_FILE_EXT,ext) || !ext[0])
					memcpy(ext,"All Files (*.*)\0*.*;\0\0\0\0\0\0\0",26);

				if(ShowOpen((char *)"Open Binary File",(char *)ext,_window,fileName,1,0)){
					u32 m,g;

					if(!Find(fileName,&g,NULL))
						Load(g,fileName);
				}
			}
		break;
		case IDC_DEBUG_CLOSE:
			DebugMode(0);
		break;
		case 5520:
			if(machine)
				machine->OnEvent(ME_REDRAW);
		break;
		case 5052:
			Settings::Show(0);
		break;
		default:
			DebugWindow::OnButtonClicked(id,w);
		break;
	}
}

int GUI::MessageBox(char *caption,char *message,HWND hWndParent,DWORD dwFlags){
	GtkButtonsType btns;
	GtkMessageType type;
	GtkWidget *w;
	int res;

	btns = GTK_BUTTONS_OK;
	type=GTK_MESSAGE_ERROR;
	w = gtk_message_dialog_new (GTK_WINDOW(hWndParent),GTK_DIALOG_DESTROY_WITH_PARENT,
                                  type,btns, message,NULL);
    if(!w) return 0;
	gtk_window_set_title(GTK_WINDOW(w),caption);
	res = gtk_dialog_run(GTK_DIALOG (w));
	gtk_widget_destroy(w);
	return res;
}

BOOL GUI::ShowOpen(char *caption,char *lpstrFilter,HWND hWndParent,char *fileName,int nMaxFile,DWORD dwFlags){
	HWND w;
	BOOL res;
	char *filename,*p,*p1;
	GtkFileFilter *filter;
	string s;

	w = gtk_file_chooser_dialog_new(caption,(GtkWindow *)hWndParent,GTK_FILE_CHOOSER_ACTION_OPEN,"_Cancel",
		GTK_RESPONSE_CANCEL,"_Open", GTK_RESPONSE_ACCEPT, NULL);
	if(!w) return FALSE;
	//if(*fileName)
		gtk_file_chooser_set_current_folder((GtkFileChooser *)w,fileName);
	*((long *)fileName) = 0;
	if(lpstrFilter != NULL && lpstrFilter[0] != 0){
		p = lpstrFilter;
		while(*p != 0){
			filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter,p);
			p += strlen(p) + 1;
			p1 = p;
			while(*p1 != 0){
				s = "";
				while(*p1 != 0){
					if(*p1 == ';'){
						gtk_file_filter_add_pattern (filter, s.c_str());
						s = "";
						p1++;
						break;
					}
					s += *p1++;
				}
				if(!s.length())
					gtk_file_filter_add_pattern (filter, s.c_str());
			}
			p += strlen(p) + 1;
			gtk_file_chooser_add_filter((GtkFileChooser *)w,filter);
		}
	}
	gtk_widget_show (w);
	res = FALSE;
	if(gtk_dialog_run((GtkDialog *)w) == GTK_RESPONSE_ACCEPT){
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(w));
		strcpy(fileName,filename);
		g_free(filename);
		res = TRUE;
	}
	gtk_widget_destroy(w);
	return res;
}

int GUI::OnLDblClk(u32 id,GtkWidget *w){
	switch(id){
		default:
			return DebugWindow::OnLDblClk(id,w);
	}
}

void GUI::OnLButtonDown(u32 id,GtkWidget *w){
	switch(id){
		default:
			DebugWindow::OnLButtonDown(id,w);
			break;
	}
}

void GUI::OnRButtonDown(u32 id,GtkWidget *w){
	switch(id){
		default:
			DebugWindow::OnRButtonDown(id,w);
			break;
	}
}

void GUI::OnCommand(u32 id,u32 c,GtkWidget *w){
	switch(id){
		default:
			DebugWindow::OnCommand(id,c,w);
		break;
	}
}

void GUI::OnScroll(u32 id,GtkWidget *w,GtkScrollType t){
	switch(id){
		case IDC_DEBUG_SB:
			DebugWindow::OnScroll(id,w,t);
			break;
	}
}

void GUI::OnPopupMenuInit(u32 id,GtkMenuShell *menu){
	int i;

	switch(id){

		default:
			DebugWindow::OnPopupMenuInit(id,menu);
		break;
	}
}

void GUI::OnMenuItemSelect(u32 id,GtkMenuItem *item){
	switch(id){
		default:
			if(id>0x10000){
				switch((u16)id){
					default:
						DebugWindow::OnMenuItemSelect(id,item);
					break;
				}
			}
		break;
		case 40000:
		{
			GtkWidget *dialog = gtk_about_dialog_new();
			if(dialog){
		//gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), "Battery");
				gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.1 beta");
				gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog),"(c) Linoma 2022/2023");
				gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "arKad Emulator");
				gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://www.arkademu.it");
				gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "https://www.arkademu.it");
		//gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(dialog), pixbuf);
		//g_object_unref(pixbuf), pixbuf = NULL;
				gtk_dialog_run(GTK_DIALOG (dialog));
				gtk_widget_destroy(dialog);
			}
		}
		break;
		case 40001:
			Quit();
		break;
		case 40002:
		break;
		case 40003:
			OnButtonClicked(5005,NULL);
		break;
		case 40100:
		case 40101:
		break;
	}
}

int GUI::_updateToolbar(){
	return 0;
}

void GUI::PushStatusMessage(char *ss){

	char *p = new char[strlen(ss)+1];
	strcpy(p,ss);
	gdk_threads_add_idle(thread1_worker,p);
}

gboolean GUI::thread1_worker(gpointer data){
		GtkWidget *w=GetDlgItem(gui._getWindow(),STR(7000));
		if(w)
			gtk_label_set_text(GTK_LABEL(w),(char *)data);
		if(data) delete []((char *)data);
		return FALSE;
}