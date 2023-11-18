#include "gui.h"
#include "utils.h"
#include "cps3m.h"
#include "resource.h"
#include "blacktiger.h"
#include "cps2m.h"
#include "1943.h"
#include "tehkanwc.h"

#include <iostream>

extern GUI gui;
extern u8 arkad_logo_png[];
void *GUI::_logo=NULL;

GUI::GUI() : DebugWindow(),GameManager(),Settings(),SaveState(){
	_window=NULL;
#ifdef __WIN32__
	hImageList=NULL;
	hwndTool=NULL;
	hwndStatus=NULL;
#else
	key_snooper = 0;
#endif
}

GUI::~GUI(){
#ifndef __WIN32__
	if(key_snooper)
		gtk_key_snooper_remove(key_snooper);
	key_snooper = 0;
#else
	if(hImageList)
		ImageList_Destroy(hImageList);
	hImageList=NULL;
#endif
}

void GUI::Loop(){
	int ret;
	MSG msg;

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
				goto C;
			break;
		}
		continue;
A:

C:
		while(PeekMessage(&msg))
			DispatchMessage(&msg);
	}
	//pthread_join(thread1,NULL);
}

int GUI::Init(){
	HWND w;

	string s =_items.at("keys");
	u16 *b = (u16 *)s.c_str();
	memcpy(_keys,b,s.length());

#ifndef __WIN32__
	if(!(_window=CreateDialogFromResource("gui")))
		return -1;
	w=CreateDialogFromResource("101");
	gtk_window_set_transient_for(GTK_WINDOW(w),GTK_WINDOW(_window));
	gtk_widget_show_all(_window);

	while(PeekMessage(0))
		DispatchMessage(NULL);
	key_snooper = gtk_key_snooper_install(OnKeyEvent,this);
#else
	WNDCLASS wc = {0};

	wc.style 			= CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
	wc.lpfnWndProc 		= MainWndProc;
	wc.hInstance 		= hInst;
	wc.hCursor 			= ::LoadCursor(NULL,IDC_ARROW);
	wc.lpszClassName 	= "ARKAD";
	wc.hbrBackground	= (HBRUSH)::GetStockObject(GRAY_BRUSH);
	wc.hIcon			= ::LoadIcon(hInst,MAKEINTRESOURCE(2));
	wc.lpszMenuName  = MAKEINTRESOURCE(100);
	if((classId = RegisterClass(&wc)) == 0)
		return 1;
	_window = CreateWindowEx(0, "ARKAD", "arKad", WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT,
                        320, 200, NULL, NULL, hInst, NULL);
	hwndStatus = CreateWindowEx(0,STATUSCLASSNAME,
        (PCTSTR) NULL, SBARS_SIZEGRIP |  WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,  _window,  (HMENU) 1, hInst, NULL);
	SendMessage(hwndStatus,SB_SIMPLE,0,0);

	hwndTool=CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
                                      WS_CHILD |WS_VISIBLE|CCS_TOP, 0, 0, 0, 0,
                                      _window, (HMENU)2, hInst, NULL);
	hImageList = ImageList_LoadImage(hInst,MAKEINTRESOURCE(IDB_TOOLBAR_BITMAP),32,5,0,IMAGE_BITMAP,LR_LOADTRANSPARENT);
	if ( !hImageList) return -1;

	SendMessage(hwndTool, TB_SETIMAGELIST,(WPARAM)0, (LPARAM)hImageList);

	TBBUTTON tbButtons[] = {
        { MAKELONG(0, 0), ID_FILE_LOAD, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Open" },
		{ MAKELONG(1, 0), 2, TBSTATE_INDETERMINATE, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Open"},
        { MAKELONG(2, 0), 3, TBSTATE_INDETERMINATE, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Save"},
        { MAKELONG(3, 0), 4, TBSTATE_INDETERMINATE, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Open"},
        { MAKELONG(4, 0), 5, TBSTATE_INDETERMINATE, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Save"}
    };

    SendMessage(hwndTool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(hwndTool, TB_ADDBUTTONS,(WPARAM)sizeof(tbButtons)/sizeof(TBBUTTON), (LPARAM)&tbButtons);
    SendMessage(hwndTool, TB_AUTOSIZE, 0, 0);

    ShowWindow(_window, SW_SHOW);
	UpdateWindow(_window);
#endif
	DebugWindow::Init(w);
	GameManager::Init();

#if defined(_DEVELOP)
//	ChangeCore(1);
	//LoadBinary("roms/m68k/lino.bin");

	{
		u32 m,g;
		char fileName[1024];

		strcpy(fileName,"roms/redearthnocd/redearthn_asia_nocd.29f400.u2");
		strcpy(fileName,"roms/redearth/redearth_euro.29f400.u2");
		strcpy(fileName,"roms/sfiii3_japan_nocd.29f400.u2");
		strcpy(fileName,"roms/blktiger/bdu-01a.5e");
		strcpy(fileName,"roms/twcp/twc-1.bin");
		//strcpy(fileName,"roms/1944j/nffj.03"};
		strcpy(fileName,"roms/1943/bme01.12d");
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
	printf("load game %u\n",g);
	DebugMode(1);
	char *c=getCurrentGameFilename();
	LoadBreakpoints(p);
    LoadNotes(c);
    if(c) delete []c;
#endif
	return 0;
}

int GUI::ChangeCore(int c){
	if(GameManager::_machine == (c+1)){
		machine->Reset();
		goto A;
	}
	GameManager::_machine=0;
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
		case 3:
			machine= new M1943::M1943();
			cpu = (M1943::Z80Cpu *)(M1943::M1943 *)machine;
		break;
		case 4:
			machine=new tehkanwc::tehkanwc();
			cpu = (tehkanwc::Z80Cpu *)(tehkanwc::tehkanwc *)machine;
		break;
		default:
			return -1;
	}

	machine->Init();
	GameManager::_machine = c+1;

	DebugWindow::OnEnableCore();
A:
	machine->LoadSettings((void * &)_items);

	{
		int w=stoi(_items["width"]);
		int h=stoi(_items["height"]);
		_resize_drawing_area(w,h);
	}
	Reset();
	return 0;
}

int GUI::OnPaint(u32 id,HWND w,HDC cr){
	switch(id){
		case IDC_DEBUG_DA1:
			return DebugWindow::OnPaint(id,w,cr);
		case 1001:
			if(machine)
				machine->Draw(cr);
			else{
#ifdef __WIN32__
			RECT rc;

			GetClientRect(w,&rc);
			PatBlt(cr,0,0,rc.right,rc.bottom,BLACKNESS);
			if(!GUI::_logo)
				CGdiPlusLoadBitmapResource(MAKEINTRESOURCE(IDB_LOGO),RT_RCDATA,hInst,&GUI::_logo);
#else
		int width = gtk_widget_get_allocated_width (w);
		int height = gtk_widget_get_allocated_height (w);

		cairo_set_source_rgb(cr,0.0,0.0,0.0);
		cairo_rectangle(cr,0,0,width,height);
		cairo_fill (cr);
#endif
			}
		break;
	}
	return 0;
}

#ifndef __WIN32__
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
#endif
int GUI::OnKeyDown(HWND w,int key,u32 wParam){
	u32 id = (u32)-1;
#ifdef __WIN32__
	id=100;
#else
	char *name = (char *)gtk_widget_get_name(w);
	if(name)
		id=atoi(name);
#endif
	switch(key){
		case GDK_KEY_F2:
			OnButtonClicked(5005,NULL);
		break;
#ifdef _DEVELOP
		case GDK_KEY_F11:
		if(wParam & 1)
			LoadGameState(0);
		else
			SaveGameState(0);
		break;
#endif

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
						if(key!=_keys[i])
							continue;
						if(machine)
							machine->OnEvent(ME_KEYDOWN,i,wParam);
						break;
					}
				break;
				default:
					//LOGD("%x\n",gdk_keyval_to_unicode(key));
				break;
			}
		break;
	}
	return 0;
}

int GUI::OnKeyUp(HWND w,int key,u32 wParam){
	u32 id = (u32)-1;
#ifdef __WIN32__
	id=100;
#else
	char *name = (char *)gtk_widget_get_name(w);
	if(name)
		id=atoi(name);
#endif
	switch(id){
		case 100:
			for(int i=0;i<sizeof(_keys)/sizeof(u16);i++){
				if(key!=_keys[i])
					continue;
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

void GUI::OnMoveResize(u32 id,HWND w,int wi,int he){
	switch(id){
		case 101:
			DebugWindow::OnMoveResize(id,w,wi,he);
			break;
		case 1001:{
			RECT rc={0};

#ifdef __WIN32__
			SendMessage(hwndStatus, WM_SIZE, 0, 0);
			SendMessage(hwndTool, WM_SIZE, 0, 0);
#else
			rc.right=wi;
			rc.bottom=he;
#endif
			if(machine){
				GetClientSize(&rc);
				machine->OnEvent(ME_MOVEWINDOW,0,0,rc.right-rc.left,rc.bottom-rc.top);
			}
		}
		break;
	}
}

int GUI::OnCloseWindow(u32 id,HWND w){
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

void GUI::OnButtonClicked(u32 id,HWND w){
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

				if(ShowOpen((char *)"Open Binary File",(char *)ext,_window,fileName,sizeof(fileName),0)){
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
		case 5511:{
			char *p=getCurrentGameFilename();

			SaveBreakpoints(p);
			SaveNotes(p);
			if(p) delete []p;
		}
		break;
		default:
			DebugWindow::OnButtonClicked(id,w);
		break;
	}
}

int GUI::MessageBox(char *caption,char *message,HWND hWndParent,DWORD dwFlags){
	int res;
#ifdef __WIN32__
#else
	GtkButtonsType btns;
	GtkMessageType type;
	GtkWidget *w;

	btns = GTK_BUTTONS_OK;
	type=GTK_MESSAGE_ERROR;
	w = gtk_message_dialog_new (GTK_WINDOW(hWndParent),GTK_DIALOG_DESTROY_WITH_PARENT,
                                  type,btns, message,NULL);
    if(!w) return 0;
	gtk_window_set_title(GTK_WINDOW(w),caption);
	res = gtk_dialog_run(GTK_DIALOG (w));
	gtk_widget_destroy(w);
#endif
	return res;
}

BOOL GUI::ShowOpen(char *caption,char *lpstrFilter,HWND hWndParent,char *fileName,int nMaxFile,DWORD dwFlags){
	BOOL res;

	res=FALSE;
#ifdef __WIN32__
	OPENFILENAME ofn={0};
	char c[500],*p,*p1,*p2;
	int i;

	lstrcpy(c,fileName);
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWndParent;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = nMaxFile;
	ofn.lpstrInitialDir = c;
	*((long *)fileName) = 0;
	if(lpstrFilter != NULL){
       ofn.lpstrFilter = lpstrFilter;
       ofn.nFilterIndex = 1;
   }
   ofn.Flags = OFN_FILEMUSTEXIST|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY|OFN_EXPLORER|dwFlags;
	if(caption != NULL)
		ofn.lpstrTitle = caption;
	if(!GetOpenFileName(&ofn))
		goto Z;
	if((i = lstrlen(fileName)) < ofn.nFileOffset){
		if((p = (char *)LocalAlloc(LPTR,nMaxFile)) == NULL)
			goto Z;
       lstrcpy(p,fileName);
       if(p[i] != '\\')
           lstrcat(p,"\\");
       lstrcat(p,(p2 = &fileName[i+1]));
       p1 = p + lstrlen(p);
       *p1++ = 0;
       lstrcpy(p1,fileName);
       while(*p2++ != 0);
       if(p1[i] != '\\')
           lstrcat(p1,"\\");
       lstrcat(p1,p2);
       CopyMemory(fileName,p,nMaxFile);
       LocalFree(p);
   }
   res= TRUE;
#else
	HWND w;
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
#endif
Z:
	return res;
}

int GUI::OnLDblClk(u32 id,HWND w){
	switch(id){
		default:
			return DebugWindow::OnLDblClk(id,w);
	}
}

void GUI::OnMouseMove(u32 id,HWND,u32,int x,int y){
	switch(id){
		case IDC_DEBUG_DA1:
			break;
	}
}

void GUI::OnLButtonDown(u32 id,HWND w){
	switch(id){
		default:
			DebugWindow::OnLButtonDown(id,w);
			break;
	}
}

void GUI::OnRButtonDown(u32 id,HWND w){
	switch(id){
		default:
			DebugWindow::OnRButtonDown(id,w);
			break;
	}
}

void GUI::OnCommand(u32 id,u32 c,HWND w){
	switch(id){
		default:
			DebugWindow::OnCommand(id,c,w);
		break;
	}
}

void GUI::OnScroll(u32 id,HWND w,GtkScrollType t){
	switch(id){
		case IDC_DEBUG_SB:
			DebugWindow::OnScroll(id,w,t);
			break;
	}
}

void GUI::OnPopupMenuInit(u32 id,HMENU menu){
	int i;

	switch(id){
		default:
			DebugWindow::OnPopupMenuInit(id,menu);
		break;
	}
}

void GUI::OnMenuItemSelect(u32 id,HMENUITEM item){
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
		case ID_FILE_LOAD:
		case 5052:
			OnButtonClicked(id,NULL);
		break;
		case 40000:{
#ifdef __WIN32__
			DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUTDIALOG),_window,(DLGPROC)AboutDialogProc);
#else
			GtkWidget *dialog = gtk_about_dialog_new();
			if(dialog){
				gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "arKad Emulator");
				gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.1");
				gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog),"(c) Linoma 2022/2023");
				gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "arKad Emulator");
				gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://www.arkademu.it");
				gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "https://www.arkademu.it");
				if(!GUI::_logo)
					GUI::_logo=gdk_pixbuf_new_from_inline(-1,(guint8 *)arkad_logo_png,false,NULL);

				gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(dialog), (GdkPixbuf *)GUI::_logo);
				//g_object_unref(pixbuf),

				gtk_dialog_run(GTK_DIALOG (dialog));
				gtk_widget_destroy(dialog);
			}
#endif
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
		case 41001:
		case 41002:
		case 41003:{
				int x=id-41000;
				int w=stoi(_items["width"])*x;
				int h=stoi(_items["height"])*x;
				if(
#ifdef __WIN32__
			CheckMenuRadioItem((HMENU)item,41001,41003,id,MF_BYCOMMAND)
#else
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))
#endif
		){
				_resize_drawing_area(w,h);
			}
		}
		break;
	}
}

int GUI::_resize_drawing_area(int &w,int &h){
#ifdef __WIN32__
	RECT rcc,rcw,rc;

	GetWindowRect(_window,&rcw);
	GetClientRect(_window,&rcc);

	w+=(rcw.right-rcw.left)-(rcc.right-rcc.left);
	h+=(rcw.bottom-rcw.top)-(rcc.bottom-rcc.top);
	GetWindowRect(hwndStatus,&rc);
	h += rc.bottom-rc.top;
	GetWindowRect(hwndTool,&rc);
	h += rc.bottom-rc.top;
	SetWindowPos(_window,0,0,0,w,h,SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
#else
	GList *children = gtk_container_get_children(GTK_CONTAINER(_window));
	if(!children || !children->data)
		return -1;
	children = gtk_container_get_children(GTK_CONTAINER(children->data));
	for(;children;children = g_list_next(children)){
		if(!GTK_IS_DRAWING_AREA(GTK_WIDGET(children->data))){
			GtkAllocation al;

			gtk_widget_get_allocation(GTK_WIDGET(children->data),&al);
			h+=al.height;
		}
	}
	gtk_window_resize(GTK_WINDOW(_window),w,h);
#endif
	return 0;
}

int GUI::GetClientSize(LPRECT p){
#ifdef __WIN32__
	RECT rc;

	GetClientRect(_window,&rc);
	p->right=rc.right-rc.left;
	p->bottom=rc.bottom-rc.top;
	p->left=0;
	GetWindowRect(hwndTool,&rc);
	p->top=(rc.bottom-rc.top);
	GetWindowRect(hwndStatus,&rc);
	p->bottom -= rc.bottom-rc.top;
#else
	GtkAllocation al;

	gtk_widget_get_allocation(_window,&al);
	p->left=p->top=0;
	if(p->right==-1)
		p->right=al.width;
	if(p->bottom==-1)
		p->bottom=al.height;
#endif
	return 0;
}

int GUI::_updateToolbar(){
	return 0;
}

void GUI::PushStatusMessage(char *ss){
#ifdef __WIN32__
	if(hwndStatus)
		SendMessage(hwndStatus,SB_SETTEXT,0,(LPARAM)ss);
#else
	char *p = new char[strlen(ss)+1];
	if(!p)
		return;
	strcpy(p,ss);
	gdk_threads_add_idle(thread1_worker,p);
#endif
}

int GUI::LoadGameState(char *){
#ifdef _DEVELOP
	FileStream *p;

	if(!(p = new FileStream()))
		return -1;
	if(!p->Open((char *)"test")){
		u32 v;

		p->Read(&v,sizeof(u32));
		//cpu->Reset();
		cpu->Query(ICORE_QUERY_BREAKPOINT_LOAD,(void *)0);
		machine->LoadState(p);
		EnterDebugMode();
	}
	delete p;
#endif
	return 0;
}

int GUI::SaveGameState(char *){
#ifdef _DEVELOP
	FileStream *p;
	u32 v;

	if(!(p = new FileStream()))
		return -1;
	if(!(p->Open((char *)"test",1))){
		v=1;
		p->Write(&v,sizeof(u32));
		machine->SaveState(p);
	}
	delete p;
#endif
	return 0;
}

int GUI::Reset(){
	memset(&_mouse,0,sizeof(_mouse));
	return DebugWindow::Reset();
}

#ifdef __WIN32__

BOOL CALLBACK GUI::AboutDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
	switch(msg){
		case WM_INITDIALOG:
			return TRUE;
		case WM_DRAWITEM:
			if(wParam==IDB_LOGO){
				LPDRAWITEMSTRUCT p=(LPDRAWITEMSTRUCT)lParam;
				if(GUI::_logo){
					void *graphics;

					pfnGdipCreateFromHDC(p->hDC,&graphics);
					pfnGdipDrawImageRectI(graphics,GUI::_logo,0,0,p->rcItem.right,p->rcItem.bottom);
					pfnGdipDeleteGraphics(graphics);
				}
			}
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(hWnd, wParam);
                    return TRUE;
                break;
			}
        break;
	}
	return  FALSE;
}

LRESULT CALLBACK GUI::MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
	switch(msg){
		case WM_COMMAND:
			switch(HIWORD(wParam)){
				case 0:
				case 1:
					if(!lParam)
						gui.OnMenuItemSelect(LOWORD(wParam),(HMENUITEM)GetMenu(hWnd));
					else
						gui.OnButtonClicked(LOWORD(wParam),(HWND)lParam);
				break;
			}
		break;
		case WM_CLOSE:
			gui.OnCloseWindow(100,hWnd);
		break;
		case WM_KEYDOWN:
			gui.OnKeyDown(hWnd,isalpha(wParam) ? tolower(wParam) : wParam,0);
		break;
		case WM_KEYUP:
			gui.OnKeyUp(hWnd,isalpha(wParam) ? tolower(wParam) : wParam,0);
		break;
		case WM_SIZE:
			gui.OnMoveResize(1001,hWnd,LOWORD(lParam),HIWORD(lParam));
		break;
		case WM_PAINT:{
			PAINTSTRUCT ps;

			BeginPaint(hWnd,&ps);
			gui.OnPaint(1001,hWnd,ps.hdc);
			EndPaint(hWnd,&ps);
			return 0;
		}
		break;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

#else

gboolean GUI::thread1_worker(gpointer data){
	HWND w=GetDlgItem(gui._getWindow(),RES(7000));
	if(w)
		gtk_label_set_text(GTK_LABEL(w),(char *)data);
	if(data)
		delete []((char *)data);
	return FALSE;
}

#endif