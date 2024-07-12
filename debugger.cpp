#include "debugger.h"
#include "gui.h"
#include "resource.h"
#include "ccore.h"
#include "machine.h"

using namespace std;
extern ICore *cpu;

DebugWindow::DebugWindow() : DebugLog(){
	_window=NULL;
	_nItems=0;
	_searchWord=NULL;
	thread1=0;
	InitializeCriticalSection(&_cr);
	Reset();
}

DebugWindow::~DebugWindow(){
	//printf("DebugWWindow Destroy\n");
	if(thread1)
		pthread_join(thread1,NULL);
	if(_searchWord)
		delete []_searchWord;
	_searchWord=NULL;
}

void DebugWindow::_calc_lb_item_height(HWND win){
#ifdef __WIN32__
#else
	GtkTreeViewColumn *column;
	int x,y,width,i;

	column = gtk_tree_view_get_column(GTK_TREE_VIEW(win),0);
	gtk_tree_view_column_cell_get_size(column,NULL,&x,&y,&width,&_itemHeight);

	gtk_widget_style_get(GTK_WIDGET(win),"expander-size", &i,NULL);
	x = 1;
	//i=0;
	if(i >= _itemHeight){
		_itemHeight = i;
		x = 2;
	}
	i = 0;
	gtk_widget_style_get(GTK_WIDGET(win),"vertical-separator", &i,NULL);
	i *= x;
	_itemHeight += i;
#endif
}

void DebugWindow::OnMoveResize(u32 id,HWND,int w,int h){
	HWND win;
#ifdef __WIN32__
#else
	GtkAllocation al;

	if(!_window)
		return;
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_LB_DIS))))
		return;
	_calc_lb_item_height(win);

	gtk_widget_get_allocation(win,&al);
	_nItems=ceil((float)(h-10)/ (float)_itemHeight / 2.0f);
	//win=GetDlgItem(_window,RES(5890));
	//int i=gtk_paned_get_position(GTK_PANED(win));
	//gtk_widget_get_allocation(win,&al);
	//printf("onmoveresize %d %d %d %d\n",_itemHeight,_nItems,al.height,e->height);
	//memset(&_views,0,sizeof(_views));
#endif
	_memPages._reset();
	_invalidateView();
	_adjustCodeViewport((u32)-1,_iCurrentView);
}

int DebugWindow::_create_treeview_model(HWND win){
#ifdef __WIN32__
#else
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *list_store;
	int height;

	list_store = gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(win),GTK_TREE_MODEL(list_store));
  	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(win), FALSE);
  	gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(win),TRUE);
	column = gtk_tree_view_column_new();
	g_object_set(column,"sizing",GTK_TREE_VIEW_COLUMN_FIXED,NULL);
	gtk_tree_view_column_set_title(column,"Text");

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer,"ypad",0,"yalign",0.5,"family","Monospace","family-set",true,NULL);
	gtk_cell_renderer_set_fixed_size(renderer,-1,12);
	gtk_tree_view_column_pack_start(column,renderer,TRUE);

	gtk_tree_view_column_set_attributes(column,renderer,"text",0,NULL);
	gtk_tree_view_append_column((GtkTreeView *)win, column);

	gtk_widget_get_preferred_height(win,NULL,&height);
	_calc_lb_item_height(win);

	_nItems=ceil((float)(height-10)/(float)_itemHeight);
	g_object_unref (list_store);
#endif
	return 0;
}

int DebugWindow::Init(HWND w){
	HWND win;
#ifdef __WIN32__
#else
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *list_store;

	_window=w;
	gtk_widget_hide_on_delete(_window);
	DebugLog::Init(w);
	pthread_create(&thread1, NULL, thread1_func, (void*) this);

	win=GetDlgItem(_window,RES(IDC_DEBUG_LB_DIS));
	_create_treeview_model(win);

	win=GetDlgItem(_window,RES(IDC_DEBUG_LB_BP));
	list_store = gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(win),GTK_TREE_MODEL(list_store));
  	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(win), FALSE);
  	gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(win),TRUE);
	column = gtk_tree_view_column_new();
	g_object_set(column,"sizing",GTK_TREE_VIEW_COLUMN_FIXED,NULL);
	gtk_tree_view_column_set_title(column,"Text");
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer,"ypad",0,"yalign",0.5,"family","Monospace","family-set",true,NULL);
	//gtk_cell_renderer_set_fixed_size(renderer,-1,12);
	gtk_tree_view_column_pack_start(column,renderer,TRUE);

	gtk_tree_view_column_set_attributes(column,renderer,"text",0,NULL);
	gtk_tree_view_append_column((GtkTreeView *)win, column);

#ifdef _DEBUG
	GtkWidget *m,*mm;

	win=gtk_menu_bar_new();
	gtk_widget_set_name(win,RES(IDC_DEBUG_MENUBAR));

	w=gtk_bin_get_child(GTK_BIN(_window));
	gtk_container_add(GTK_CONTAINER(w),win);
	gtk_box_reorder_child(GTK_BOX(w),win,0);

	m=gtk_menu_new();
	w=gtk_menu_item_new_with_label ("Debug");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), m);
	gtk_menu_shell_append (GTK_MENU_SHELL (win), w);

	w = gtk_menu_item_new_with_label ("Break on...");
	gtk_menu_shell_append (GTK_MENU_SHELL (m), w);

	mm=gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), mm);
	w = gtk_check_menu_item_new_with_label ("IRQ");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,5));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

	w = gtk_check_menu_item_new_with_label ("BREAK");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,6));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

	w = gtk_check_menu_item_new_with_label ("DEVICE");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,7));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

	w = gtk_check_menu_item_new_with_label ("VBLANK");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,8));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

	w = gtk_menu_item_new_with_label ("Level");
	gtk_menu_shell_append (GTK_MENU_SHELL (m), w);

	mm=gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), mm);

	w = gtk_radio_menu_item_new_with_label (NULL,"Off");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,1));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
	GSList *group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(w));

	w = gtk_radio_menu_item_new_with_label (group,"On");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,2));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	//gtk_radio_menu_item_set_group(GTK_RADIO_MENU_ITEM(w),group);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

	w = gtk_check_menu_item_new_with_label ("DEV");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,9));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);

	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
	w = gtk_menu_item_new_with_label ("Notes");
	gtk_menu_shell_append (GTK_MENU_SHELL (m), w);

	mm=gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), mm);
	w = gtk_menu_item_new_with_label ("Load");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,100));
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

	w = gtk_menu_item_new_with_label ("Save");
	gtk_menu_shell_append (GTK_MENU_SHELL (mm), w);
	gtk_widget_show(w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

	w = gtk_menu_item_new_with_label ("Close");
	gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_MENUBAR,IDC_DEBUG_CLOSE));
	gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
	g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
/*
	m=gtk_viewport_new(NULL,NULL);
	w=gtk_text_view_new();
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(w),true);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(w),true);
	gtk_widget_set_name(w,RES(IDC_DEBUG_TV1));
	g_signal_connect(G_OBJECT(w),"populate-popup",G_CALLBACK(on_menu_init),0);
	gtk_container_add (GTK_CONTAINER (m),w);
	w = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_add (GTK_CONTAINER (w),m);
	m=gtk_label_new ("Notes");
	gtk_notebook_append_page(GTK_NOTEBOOK (GetDlgItem(_window,RES(IDC_DEBUG_TAB1))), w, m);

	mm=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
	m=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
	w=gtk_combo_box_text_new();
	gtk_widget_set_name(w,RES(IDC_DEBUG_LAYERS_CB));
	gtk_container_add (GTK_CONTAINER (m),w);
	{
		GtkAdjustment *adjustment;

		adjustment = gtk_adjustment_new (50.0, 0.0, 100.0, 1.0, 5.0, 0.0);

		w = gtk_spin_button_new (adjustment, 1.0, 0);
		gtk_container_add (GTK_CONTAINER (m),w);
	}
	gtk_container_add (GTK_CONTAINER (mm),m);

	m=gtk_viewport_new(NULL,NULL);
	w=gtk_drawing_area_new();
	gtk_widget_set_hexpand(w,true);
	gtk_widget_set_vexpand(w,true);
	gtk_widget_set_name(w,RES(IDC_DEBUG_DA1));
	g_signal_connect(G_OBJECT(w),"draw",G_CALLBACK(on_paint),0);
	gtk_container_add (GTK_CONTAINER (m),w);
	w = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_add (GTK_CONTAINER (w),m);
	gtk_container_add (GTK_CONTAINER (mm),w);

	m=gtk_label_new ("Layers");
	gtk_notebook_append_page(GTK_NOTEBOOK (GetDlgItem(_window,RES(IDC_DEBUG_TAB1))), mm, m);

	mm=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
	m=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);

	w=gtk_combo_box_text_new();
	gtk_widget_set_name(w,RES(IDC_DEBUG_PALETTE_CB));
	gtk_container_add (GTK_CONTAINER (m),w);
	gtk_container_add (GTK_CONTAINER (mm),m);

	w=gtk_drawing_area_new();
	gtk_widget_set_hexpand(w,true);
	gtk_widget_set_vexpand(w,true);
	gtk_widget_set_name(w,RES(IDC_DEBUG_DA2));
	g_signal_connect(G_OBJECT(w),"draw",G_CALLBACK(on_paint),0);
	gtk_container_add (GTK_CONTAINER (mm),w);

	m=gtk_label_new ("Palette");
	gtk_notebook_append_page(GTK_NOTEBOOK (GetDlgItem(_window,RES(IDC_DEBUG_TAB1))), mm, m);*/
#endif

#endif
	return 0;
}

void DebugWindow::Update(){
#ifdef _DEBUG
	int i,nn;
	char *p,*pp;
	HWND win;

	if(!_window || !cpu)
		return;
	//return;
	p=(char *)&_memPages;
	i=cpu->Dump(&p);
	//p=NULL;
	if(!p || i< 1)
		goto A;
	pp=p;
	while(*pp){
#ifdef __WIN32__
		win = GetDlgItem(_window,atoi(pp));
#else
		win = GetDlgItem(_window,pp);
#endif
		pp += strlen(pp)+5;
		nn = strlen(pp);

		if(win){
#ifdef __WIN32__

#else
			GtkTextBuffer *b= gtk_text_view_get_buffer((GtkTextView *)win);
			if(*pp){
				char *s,*ss;

				s=pp;
				gtk_text_buffer_set_text(b,s,0);

				while((ss=strtok(s,"\\"))){
					GtkTextIter ei;
					char tag[2];

					tag[0]=tag[1]=0;
					if((u64)ss - (u64)pp)
						tag[0]=*ss++;
					gtk_text_buffer_get_end_iter(b,&ei);
					switch(tag[0]){
						case 'b':
						case 'u':
							gtk_text_buffer_insert_with_tags_by_name(b,&ei,ss,-1,tag,NULL);
							break;
						default:
							gtk_text_buffer_insert(b,&ei,ss,-1);
							break;
					}

					s=NULL;
				}
			}
			else
				gtk_text_buffer_set_text(b,"",-1);
#endif
		}
		pp += nn+1;
	}
A:

	{
		u32 pc;

		cpu->Query(ICORE_QUERY_PC,&pc);
		_adjustCodeViewport(pc,DBV_RUN);
		_updateAddressInfo(pc);
	}
	win=GetDlgItem(_window,RES(IDC_DEBUG_LB_BP));
#ifdef __WIN32__

#else
	GtkListStore *model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)win);
	if(model != NULL)
		gtk_list_store_clear(model);
	{
		u64 *bp;
		char c[330];
		HWND w=NULL;

		if(!cpu->Query(ICORE_QUERY_BREAKPOINT_ENUM,&bp) && bp){
			pp=(char *)bp;
			for(;*bp;bp++){
				HWND e;
				char s='*',ec[][3]={"!=","==","<",">"},*p__;

				if(!w){
					w = gtk_menu_tool_button_get_menu(GTK_MENU_TOOL_BUTTON(GetDlgItem(_window,RES(5517))));
					if(w){
						GList *children = gtk_container_get_children(GTK_CONTAINER(w));
						for(i=0;children;children = g_list_next(children),i++){
							gtk_container_remove(GTK_CONTAINER(w),GTK_WIDGET(children->data));
						}
					}
					else
						w=gtk_menu_new();
				}

				if(!(*bp & 0x8000000000000000ULL))
					s=32;
				sprintf(c,"%c 0x%08X",s,(u32)*bp);
				e=gtk_menu_item_new_with_label (c);
				gtk_menu_shell_append (GTK_MENU_SHELL (w), e);

				if(ISMEMORYBREAKPOINT(*bp)){//memory
					char s_[][4]={"R","W","R/W"};
					char s__[][3]={"B","W","L","*"};

					sprintf(c,"%c %c 0x%X %s %s %s",s,SR(*bp,34) & 1 ?'M': 'm',(u32)*bp,s__[SR(*bp,38) & 3],s_[SR(*bp,60)&3],ec[SR(*bp,35) & 7]);
				}
				else if(*bp & 0x800000000000ULL){
					u32 v=SR(*bp,32);
					if(v&0x40000000)
						sprintf(c,"%c   0x%X  r%d %s %x",s,(u32)*bp,SR(v,3) & 0x1f, ec[v&7],SR(v,8)&0x7f|SR(v&0x3fff0000,9));
					else
						sprintf(c,"%c   0x%X  r%d %s r%d",s,(u32)*bp,SR(v,3) & 0x1f, ec[v&7],SR(v,8)&0x1f);
				}
				else
					sprintf(c,"%c   0x%X  %5d %5d",s,(u32)*bp,(u16)SR(*bp,32),(u16)(SR(*bp,48) & 0x7fff));
				if(!_getNoteFromAddress((u32)*bp,&p__)){
					for(i=strlen(c);*p__ && *p__!=10;p__++,i++)
						c[i]=*p__;
					c[i]=0;
				}
				HTREEITEM pi=AddListItem(win,c);
				delete pi;
			}
			free(pp);
			if(w){
				gtk_widget_show_all(w);
				gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(GetDlgItem(_window,RES(5517))),w);
			}
		}
	}
	win= GetDlgItem(_window,RES(7101));
	if(win){
		char c[20];

		sprintf(c,"LA: %x",(u32)_memPages._lastAccessAddress);
		gtk_label_set_text(GTK_LABEL(win),c);
	}
	win= GetDlgItem(_window,RES(7100));
	if(win){
		char c[20];

		sprintf(c,"EA: %x",(u32)_memPages._editAddress);
		gtk_label_set_text(GTK_LABEL(win),c);
	}
#endif
	DebugLog::Update();
	DebugWindow::_updateToolbar();
	if(p)
		delete []p;
#endif
}

int DebugWindow::_getNoteFromAddress(u32 pc,char **p){
	char *pp;
	int nn;
	HWND w;
#ifdef _DEBUG

#ifdef __WIN32__
#else
	GtkTextIter s,e;
	GtkTextBuffer *b;

	if(!(w=GetDlgItem(_window,RES(IDC_DEBUG_TV1))))
		return -1;
	if(!(b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w))))
		return -2;
	gtk_text_buffer_get_start_iter(b,&s);
	gtk_text_buffer_get_end_iter(b,&e);
	if(!(pp=gtk_text_buffer_get_text(b,&s,&e,false)))
		return -3;
	nn=0;
	while(*pp && nn==0){
		char *c=pp;
		int n=0,i=0;
		while(c[n]){
			if(i==0 && c[n]=='\t'){
				if(n){
					u32 d;
					sscanf(c,"%x",&d);
					if(d==pc){
						*p=&c[n];
						nn=n;
						goto A;
					}
				}
				i=1;
			}
			n++;
			if(c[n]=='\n'){
				n++;
				break;
			}
		}
		pp+=n;
	}
	return -4;
A:
#endif

#endif
	return 0;
}

int DebugWindow::_updateAddressInfo(u32 pc){
	char *p;

	if(_getNoteFromAddress(pc,&p)) return -1;
	ShowToastMessage(p);
	return 0;
}

void DebugWindow::_adjustCodeViewport(u32 adr,int v){
	HWND win;
	u32 _data[10];
	LPDISVIEW pv;

	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_SB))) || !GTK_IS_WIDGET(win) || !cpu)
		return;
	if(adr==(u32)-1)
		cpu->Query(ICORE_QUERY_PC,&adr);
	if(v==-1)
		v=_iCurrentView;
	if(v==-1)
		v=DBV_VIEW;
	pv=&_views[v];

	if(adr<pv->uAdr || adr > (pv->uAdr+pv->uSize) || _iCurrentView != v){
		_data[0]=adr;
		*((u64 *)&_data[1])=(u64)&_data[3];
		if(cpu->Query(ICORE_QUERY_ADDRESS_INFO,_data))
			return;
		pv->uAdr=_data[3];
		pv->uSize=_data[4];
		_data[9]=(u32)-1;
		if(cpu->Query(ICORE_QUERY_NEXT_STEP,&_data[9]))
			_data[9]=2;
		//printf("_adjustCodeViewport: %d %x %x %d %x\n",v,_data[3],_data[4],_data[9],adr);
#ifdef __WIN32__
#else
		gtk_range_set_range(GTK_RANGE(win),0,_data[4]-_nItems*_data[9]);
		gtk_range_set_increments(GTK_RANGE(win),_data[9],_nItems*_data[9]);
#endif
		if(v != DBV_VIEW)
			memcpy(&_views[DBV_VIEW],pv,sizeof(DISVIEW));
	}

#ifdef __WIN32__
#else
	gtk_range_set_value(GTK_RANGE(win),adr & (pv->uSize-1));
#endif
	_fillCodeWindow(adr,v);
}

void DebugWindow::_fillCodeWindow(u32 adr,int cv){
	HWND win;
	LPDISVIEW pv;
	char *p;

	if(!cpu || !(win=GetDlgItem(_window,RES(IDC_DEBUG_LB_DIS))))
		return;
	if(!(p = new char[1000]))
		return;
#ifdef __WIN32__
#else
	g_object_freeze_notify(G_OBJECT(win));
A:
	pv = &_views[cv];

	if(adr<=pv->uStart || adr > pv->uEnd || _iCurrentView!=cv){
		u64 *bp;

		_iCurrentView=cv;
		GtkListStore  *model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)win);
		if(model != NULL)
			gtk_list_store_clear(model);
		pv->uStart=adr;

		if(!cpu || cpu->Query(ICORE_QUERY_BREAKPOINT_ENUM,&bp))
			bp=NULL;
	//	printf("fi %x %d ",adr,_nItems);
		for(int i=0;i<_nItems && cpu;i++){
			u64 *pb,d[]={(u64)&p[4],(u64)&adr};

			*((u64 *)p)=0x2020202020;
			for(pb=bp;pb && *pb;pb++){
				if((u32)*pb != adr)
					continue;
				p[0]=!(*pb & 0x8000000000000000) ? '+' : '*';
				break;
			}

			cpu->Query(ICORE_QUERY_DISASSEMBLE,d);

			HTREEITEM pi=AddListItem(win,p);
			if(pi)
				delete pi;
		}
		//printf(" %x\n",adr);
		pv->uEnd=adr-1;
	}

	GtkTreeSelection *select = gtk_tree_view_get_selection((GtkTreeView *)win);

	if(select != NULL && _iCurrentView==DBV_RUN &&& cpu){
		GtkListStore *model;
		GtkTreeIter iter;

		model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)win);
		if(model != NULL){
			char *string;
			int n,i;

			cpu->Query(ICORE_QUERY_PC,&_pc);
			n=0;
			i = gtk_tree_model_get_iter_first((GtkTreeModel *)model,&iter);
			while(i){
				n=1;
				gtk_tree_model_get((GtkTreeModel *)model, &iter, 0, &string, -1);
				sscanf(string+4,"%x",&i);
				free(string);
				if((u32)i == _pc){
					gtk_tree_selection_select_iter(select,&iter);
					n=2;
					break;
				}
				i=gtk_tree_model_iter_next((GtkTreeModel *)model,&iter);
			}

			if(n==1 && _pc){
				//printf("lino %x %x %d\n\n",adr,_pc,i);
				adr=_pc;
				memset(pv,0,sizeof(DISVIEW));
				goto A;
			}
		}
	}

	g_object_thaw_notify(G_OBJECT(win));
	//printf("fl %x %x\n",pv->uStart,pv->uEnd);
#endif
	delete []p;
}

HTREEITEM DebugWindow::AddListItem(HWND listbox, char *sText){
	HTREEITEM iter;
#ifdef __WIN32__

#else
	GtkListStore *model;

  //  g_object_freeze_notify(G_OBJECT(listbox));

	model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)listbox);
	if(model == NULL)
		return NULL;
	iter = new GtkTreeIter[1];
	if(iter == NULL)
		return NULL;
	gtk_list_store_append(model,iter);
	gtk_list_store_set(model,iter,0,sText, -1);
	gtk_widget_queue_resize_no_redraw(listbox);
	//g_object_thaw_notify(G_OBJECT(listbox));
#endif
	return iter;
}

HTREEITEM DebugWindow::GetListItem(HWND w,u32 n){
	HTREEITEM piter;
	u32 i;
#ifdef __WIN32__

#else
	GtkListStore *model;

	model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)w);
	if(model == NULL)
		return NULL;
	if((piter = new GtkTreeIter[1]) == NULL)
		return NULL;
	gtk_tree_model_get_iter_first((GtkTreeModel *)model,piter);
	for(i=1;i<=n;i++){
		if(!gtk_tree_model_iter_next((GtkTreeModel *)model,piter))
			goto _err;
	}
	return piter;
_err:
	delete piter;
#endif
	return NULL;
}

HTREEITEM DebugWindow::GetSelectedListItem(HWND w){
	HTREEITEM piter;

#ifdef __WIN32__

#else
	GtkListStore *model;
	GtkTreeSelection *select;

	select = gtk_tree_view_get_selection((GtkTreeView *)w);
	if(select == NULL)
		return NULL;
	if((piter = new GtkTreeIter[1]) == NULL)
		return NULL;
	if(!gtk_tree_selection_get_selected(select,(GtkTreeModel **)&model,piter))
		goto _err;
	return piter;
_err:
	delete piter;
#endif
	return NULL;
}

u32 DebugWindow::GetSelectedListItemIndex(HWND w){
	u32 res;
#ifdef __WIN32__

#else
	GtkListStore *model;
	GtkTreeSelection *select;
	GtkTreeIter *piter,iter;
	GtkTreePath *path,*path1;

	path=path1=NULL;
	select = gtk_tree_view_get_selection((GtkTreeView *)w);
	if(select == NULL)
		return res;
	if((piter = new GtkTreeIter[1]) == NULL)
		return res;
	if(!gtk_tree_selection_get_selected(select,(GtkTreeModel **)&model,piter))
		goto E;
	path = gtk_tree_model_get_path((GtkTreeModel *)model,piter);
	if(path == NULL)
		goto E;
	if(!gtk_tree_model_get_iter_first((GtkTreeModel *)model,&iter))
		goto E;
	res = 0;
	do{
		path1 = gtk_tree_model_get_path((GtkTreeModel*)model,&iter);
		if(path1 != NULL){
			int x = gtk_tree_path_compare(path,path1);
			gtk_tree_path_free(path1);
			if(x == 0)
				goto A;
		}
		if(!gtk_tree_model_iter_next((GtkTreeModel *)model,&iter))
			break;
		res++;
	}while(1);
E:
	res=(u32)-1;
A:
	if(piter)
		delete piter;
	if(path)
		gtk_tree_path_free(path);
#endif
	return res;
}

void DebugWindow::_adjustScrollViewport(int v,HWND w,double *r,u32 attr){
#ifdef __WIN32__
#else
	double d[6];
	int i;
	GtkAdjustment *control;

	if(!w || (control = gtk_range_get_adjustment(GTK_RANGE(w))) == NULL)
		return;
	i=1;
	if(v<0){
		i*=-1;
		v*=-1;
	}
	g_object_get((gpointer)control,"lower",&d[0],"upper",&d[1],"value",&d[2],"page-increment",&d[3],"step-increment",&d[4],NULL);
	d[5]=(d[2]+d[v]*i);
	if(r)
		*r=d[5];
	if(attr&1)
		gtk_range_set_value(GTK_RANGE(w),d[5]);
#endif
}

void DebugWindow::OnKeyScrollEvent(int v,u32 id,HWND w){//fixme
	double d;
	u32 i;

#ifdef __WIN32__
#else
	if(!w) w=GetDlgItem(_window,RES(IDC_DEBUG_SB));
	if(id==IDC_DEBUG_LB_DIS){
		_adjustScrollViewport(v,w,&d,0);
		_adjustCodeViewport((u32)d+_views[DBV_VIEW].uAdr,DBV_VIEW);
	}
	else{
		w=gtk_window_get_focus(GTK_WINDOW(_window));
		i = (u32)(u64)g_object_get_data(G_OBJECT(w),"type");
		if(i==2){
			_adjustScrollViewport(v,_memPages._sb);
		//	g_signal_emit_by_name((gpointer)_memPages._sb,"scroll-event",0,d,&d);
			if(!_memPages._onScroll(id,_memPages._sb,GTK_SCROLL_END))
				Update();
		}
	}
#endif
}

int DebugWindow::OnKeyDown(int key,u32 wParam){
	u32 id=(u32)-1;
#ifdef __WIN32__
#else
	HWND w=gtk_window_get_focus(GTK_WINDOW(_window));
	if(w){
		char *name = (char *)gtk_widget_get_name(w);
		if(name)
			id=atoi(name);
	}
	switch(id){
		case IDC_DEBUG_DA1*10:{
			int v;

			switch(key){
				case GDK_KEY_Page_Down:
					v=3;
				break;
				case GDK_KEY_Page_Up:
					v=-3;
				break;
				case GDK_KEY_Down:
					v=4;
				break;
				case GDK_KEY_Up:
					v=-4;
				break;
			}
			_adjustScrollViewport(v,w,0);
			gtk_container_set_focus_child(GTK_CONTAINER(_window),w);
		}
		return 0;
	}
	switch(key){
		case GDK_KEY_Page_Down:
			OnKeyScrollEvent(3,id);
		break;
		case GDK_KEY_Page_Up:
			OnKeyScrollEvent(-3,id);
		break;
		case GDK_KEY_Down:
			OnKeyScrollEvent(4,id);
		break;
		case GDK_KEY_Up:
			OnKeyScrollEvent(-4,id);
		break;
		case GDK_KEY_F12://Run framee
			DebugMode(8);
		break;
		case GDK_KEY_F3://Stop
			if(id==3102)
				SearchWord();
			else
				DebugMode(1);
			break;
		case GDK_KEY_F6:
			DebugMode(6);
			break;
		case GDK_KEY_F5://Go
			DebugMode(5);
			break;
		case GDK_KEY_F4:
			DebugMode(3);//Step In
			break;
		case GDK_KEY_F7://Run to cuursor
			DebugMode(4);
			break;
		case GDK_KEY_F8:
			DebugMode(7);//Step Over
		break;
		case GDK_KEY_F9:
			if(id==IDC_DEBUG_LB_BP){
				u32 d[2];

				if((d[0]=GetSelectedListItemIndex(w)) != (u32)-1){
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_SET_BREAKPOINT_STATE,d)){
						_invalidateView();
						Update();
					}
				}
			}
			else{
				u32 adr,d[2];

				if(!GetAddressFromSelectedItem(GetDlgItem(_window,RES(IDC_DEBUG_LB_DIS)),&adr)){
					d[0]=adr;
					d[1]=0;
					if(!cpu->Query(ICORE_QUERY_BREAKPOINT_ADD,(void *)d)){
						Update();
						w=GetDlgItem(_window,RES(IDC_DEBUG_TAB1));
						int i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (w))-5;
						gtk_notebook_set_current_page(GTK_NOTEBOOK(w),i);

						w=GetDlgItem(_window,RES(IDC_DEBUG_LB_BP));
						HTREEITEM im=GetListItemFromAddress(w,adr);
						if(im){
							GtkTreeSelection *select = gtk_tree_view_get_selection((GtkTreeView *)w);
							gtk_tree_selection_select_iter(select,im);
							delete im;
						}
					}
				}
			}
		break;
		case GDK_KEY_Delete:
			//printf("f %d %d\n",id,key);
			if(id==IDC_DEBUG_LB_BP){
			//cpu->Query(ICORE_QUERY_BREAKPOINT_DEL,(void *)adr);
				u32 d[2];
				d[0]=GetSelectedListItemIndex(w);
				d[1]=(u32)-1;
				if(!cpu->Query(ICORE_QUERY_BREAKPOINT_DEL,d)){
					_invalidateView();
					Update();
				}
			}
		break;
	}
#endif
	return 0;
}

int DebugWindow::DebugMode(int m,u64 v,u64 vv){
#ifdef _DEBUG
	switch(m){
		case 0:
			BC(_status,S_DEBUG|S_PAUSE);
			if(machine)
				machine->OnEvent(ME_LEAVEDEBUG);
			gtk_widget_hide(_window);
		break;
		case 1:
			if(!v || BT(_status,v)){
				BS(_status,S_DEBUG|S_PAUSE);
				BC(_status,S_DEBUG_NEXT|S_DEBUG_NEXT_FRAME);
				_pc=0;
#ifdef __WIN32__
#else
				gtk_widget_show_all(_window);
				gtk_notebook_set_current_page(GTK_NOTEBOOK(GetDlgItem(_window,RES(IDC_DEBUG_TAB1))),0);
#endif
				OnChangeCpu(vv);
				if(machine) machine->OnEvent(ME_ENTERDEBUG);
				Update();
				FlashWindow(_window,2);
			}
		break;
		case 5://Run
			if(!BT(_status,S_DEBUG))
				return -1;
			BS(_status,S_DEBUG_NEXT);
			_pc=0x1000000000000002;
			DebugWindow::_updateToolbar();
		break;
		case 2:
			BC(_status,S_PAUSE);
			DebugWindow::_updateToolbar();
		break;
		case 3:{//next   F4
			u32 n[2]={2,0};

			if(!BT(_status,S_DEBUG))
				return -1;
			BS(_status,S_DEBUG_NEXT);
		//	n=(u32)2;
			if(cpu->Query(ICORE_QUERY_NEXT_STEP,n)){

				n[0]=2;
				//cpu->Query(ICORE_QUERY_PC,&i);
				//n+=i;
			}
			//printf("ICORE_QUERY_NEXT_STEP %x\n",n[0]);
			_pc = (u64)n[0];
			DebugWindow::_updateToolbar();
		}
		break;
		case 4://run to cursor	F7
			if(!BT(_status,S_DEBUG))
				return -1;
			BS(_status,S_DEBUG_NEXT);
			{
				u32 i;

				if(!GetAddressFromSelectedItem(GetDlgItem(_window,RES(IDC_DEBUG_LB_DIS)),&i))
					_pc = 0x8000000000000000|(u32)i;
			}
			DebugWindow::_updateToolbar();
		break;
		case 6://set pc
			if(!BT(_status,S_DEBUG))
				return -1;
			{
				u32 i;

				if(!GetAddressFromSelectedItem(GetDlgItem(_window,RES(IDC_DEBUG_LB_DIS)),&i)){
					cpu->Query(ICORE_QUERY_SET_PC,(void *)(u64)&i);
				}
			}
			DebugWindow::_updateToolbar();
		break;
		case 7://step over F8
			if(!BT(_status,S_DEBUG))
				return -1;
			BS(_status,S_DEBUG_NEXT);
			{
				u32 i,n[2]={1,23};

				cpu->Query(ICORE_QUERY_PC,&i);//25aa4f
				n[0]=(u32)1;
				if(cpu->Query(ICORE_QUERY_NEXT_STEP,n))
					n[0]=2;
				i += n[0];//fixme
				_pc = 0x8000000000000000|(u32)i;
			}
			DebugWindow::_updateToolbar();
		break;
		case 8://f12
			if(!BT(_status,S_DEBUG))
				return -1;
			BS(_status,S_DEBUG_NEXT_FRAME);
			BC(_status,S_PAUSE);
			DebugWindow::_updateToolbar();
		break;
	}
#endif
	return 0;
}

HTREEITEM DebugWindow::GetListItemFromAddress(HWND w,u32 n){
	HTREEITEM piter;
	u32 i,a;
	char *string;
#ifdef __WIN32__

#else
	GtkListStore *model;

	model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)w);
	if(model == NULL)
		return NULL;
	if((piter = new GtkTreeIter[1]) == NULL)
		return NULL;
	i=0;
	gtk_tree_model_get_iter_first((GtkTreeModel *)model,piter);
	goto B;
	for(;;i++){
		if(!gtk_tree_model_iter_next((GtkTreeModel *)model,piter))
			goto _err;
B:
		gtk_tree_model_get((GtkTreeModel *)model, piter, 0, &string, -1);
		sscanf(string+4,"%x",&a);
		free(string);
		if(a==n)  return piter;
	}
_err:
	delete piter;
#endif
	return NULL;
}

int DebugWindow::GetAddressFromSelectedItem(HWND win,u32 *adr){
	int res=-1;
#ifdef _DEBUG
	HTREEITEM piter = GetSelectedListItem(win);
	if(piter){
#ifdef __WIN32__
#else
		GtkListStore *model;

		res--;
		model = (GtkListStore *)gtk_tree_view_get_model((GtkTreeView *)win);
		if(model != NULL){
			char *string;
			u32 i;

			res--;
			gtk_tree_model_get((GtkTreeModel *)model, piter, 0, &string, -1);
			sscanf(string+4,"%x",&i);
			*adr=i;
			free(string);
			res=0;
		}
#endif
	}
#endif
	return res;
}

void DebugWindow::OnButtonClicked(u32 id,HWND w){
#ifdef _DEBUG
	switch(id){
		case 5518:{
			int i;
			HWND win;

			win=GetDlgItem(_window,RES(IDC_DEBUG_TAB));
			_memPages._addPage(win,0,0,&i);
#ifdef __WIN32__
#else
			gtk_notebook_set_current_page(GTK_NOTEBOOK (win),i);
			//i=gtk_combo_box_get_active(GTK_COMBO_BOX(w));
#endif
		}
		break;
		case 5600:
		case 5601:
		case 5602:
		case 5603:
		case 5604:
		case 5605:
		case 5606:
			DebugLog::OnButtonClicked(id,w);
		break;
		case 5501:
			DebugMode(3);
		break;
		case 5502:
			DebugMode(8);
		break;
		case IDC_DEBUG_RUN:
			DebugMode(5);
		break;
		case IDC_DEBUG_STOP:
			DebugMode(1);
		break;
		case 5506://jump to address
			{
				u32 a;
				char c[30];
				HWND ww,e;
#ifdef __WIN32__
#else
				ww=GetDlgItem(_window,RES(5599));
				sscanf(gtk_entry_get_text(GTK_ENTRY(ww)),"%x",&a);
				_adjustCodeViewport(a,DBV_VIEW);
				{
					u32 n;

					cpu->Query(ICORE_QUERY_CPU_ACTIVE_INDEX,&n);
					gtk_notebook_set_current_page(GTK_NOTEBOOK(GetDlgItem(_window,RES(IDC_DEBUG_TAB1))),n);
				}
				ww = gtk_menu_tool_button_get_menu(GTK_MENU_TOOL_BUTTON(GetDlgItem(_window,RES(5506))));
				if(!ww)
					ww=gtk_menu_new();
				sprintf(c,"0x%08X",a);
				e=gtk_menu_item_new_with_label (c);
				gtk_menu_shell_append (GTK_MENU_SHELL (ww), e);

				//printf("%u\n",a);
				/*ww=GetDlgItem(_window,RES(IDC_DEBUG_SB));
				gtk_range_set_value((GtkRange *)ww,a&(_endAddress-1));
				d=a;
				g_signal_emit_by_name((gpointer)ww,"change-value",0,d,&d);*/
#endif
			}
		break;
		case 5510:
//			if(!LoadBreakpoints())
	//			Update();
		break;
		default:
			switch(SR(id,16)){
				case 4:
					if(!_memPages._close()){
						HWND win=GetDlgItem(_window,RES(IDC_DEBUG_TAB));
#ifdef __WIN32__
#else
						gint n=gtk_notebook_get_current_page(GTK_NOTEBOOK (win));
						gtk_notebook_set_current_page(GTK_NOTEBOOK (win),_memPages._iPage);
						gtk_notebook_remove_page(GTK_NOTEBOOK (win),n);
#endif
					}
				break;
				case 1:{
					HWND ww;
					char c[20],*p;

					sprintf(c,"%d",(u16)id +0x20000);
#ifdef __WIN32__
#else
					if((ww=GetDlgItem(_window,c)) && (p=(char *)gtk_entry_get_text(GTK_ENTRY(ww)))){
						u32 u;

						sscanf(p,"%x",&u);
						_memPages._setPageAddress(u);
						Update();
					}
#endif
				}
				break;
			}
			break;
	}
#endif
}

void DebugWindow::OnScroll(u32 id,HWND w,GtkScrollType t){
	double d,d1,d2,d3;
#ifdef _DEBUG

#ifdef __WIN32__
#else
	GtkAdjustment *control;

	if((control = gtk_range_get_adjustment(GTK_RANGE(w))) == NULL)
		return;
	g_object_get((gpointer)control,"lower",&d,"upper",&d1,"value",&d2,"page-increment",&d3,NULL);
	switch(id){
		case IDC_DEBUG_SB:
	//printf("sb %.2f %.2f %.2f\n",d,d1,d2);

		_adjustCodeViewport((u32)d2+_views[DBV_VIEW].uAdr,DBV_VIEW);
		break;
		case IDC_DEBUG_DA1*10:{
			char c[20];

			HWND ww=GetDlgItem(_window,RES(CONCAT(IDC_DEBUG_DA1,00)));
			sprintf(c,"%u",(u32)d2);
			gtk_label_set_text(GTK_LABEL(ww),c);
			gtk_widget_queue_draw(GetDlgItem(_window,RES(IDC_DEBUG_DA1)));
		}
		break;
		default:
			switch(SR(id,16)){
				case 5:
					if(!_memPages._onScroll(id,w,t))
						Update();
					break;
			}
		break;
	}
#endif
#endif
}

int DebugWindow::Reset(){
	DebugLog::Reset();
	ShowToastMessage(0);
	_pc=0;
	memset(&_views,0,sizeof(_views));
	_iCurrentView=DBV_VIEW;
	_memPages._reset();
//	_memPages._setPageAddress(0x8008a078);
	BC(_status,S_DEBUG_NEXT);
	return 0;
}

int DebugWindow::LoadBreakpoints(char *fn){
#ifdef _DEBUG
	u64 bp;
	FILE *fp;
	u32 p[5],ac;
	ICore *pu;
	char c[1025];

	if(!fn || !*fn) return -1;
	*((u64 *)c)=0;
	if(fn)
		strcpy(c,fn);
	strcat(c,".bk");
	if(!cpu || !(fp=fopen(c,"rb")))
		return -2;
	pu=cpu;
	pu->Query(ICORE_QUERY_CPU_ACTIVE_INDEX,&ac);
//printf("%s %s %s\n",__FILE__,__FUNCTION__,c);

	for(u32 i=0;;i++){
		*((u64 *)p)=0;
		p[0]=i;
		if(pu->Query(ICORE_QUERY_CPU_INTERFACE,&p))
			break;
		pu = (ICore *)*((void **)p);
		pu->Query(ICORE_QUERY_BREAKPOINT_CLEAR,NULL);
		while(!feof(fp)){
			if(!fread(&bp,1,sizeof(bp),fp))
				break;
			if(bp ==(u64)-1)
				break;
			p[0]=(u32)bp;
			p[1]=SR(bp,32);
			pu->Query(ICORE_QUERY_BREAKPOINT_ADD,(void *)p);
		}
		pu->Query(ICORE_QUERY_BREAKPOINT_LOAD,(void *)0);
	}
	fclose(fp);

	p[0]=ac;
	cpu->Query(ICORE_QUERY_CPU_INTERFACE,&p);
#endif
	return 0;
}

int DebugWindow::SaveBreakpoints(char *fn){
	int res=-1;

#ifdef _DEBUG
	u64 *bp;
	char *pp;
	FILE *fp;
	ICore *pu;
	char c[1025];

	if(!fn || !*fn) return -1;
	*((u64 *)c)=0;
	if(fn)
		strcpy(c,fn);
	strcat(c,".bk");

	if(!cpu || !(fp=fopen(c,"wb")))
		return -2;
	pu=cpu;
	for(u32 i=0;;i++){
		u32 p[5];

		pp=0;
		res=-2;
		*((u64 *)p)=0;
		p[0]=i;
		if(pu->Query(ICORE_QUERY_CPU_INTERFACE,&p))
			break;
		pu = (ICore *)*((void **)p);
		if(pu->Query(ICORE_QUERY_BREAKPOINT_ENUM,&bp) || !bp)
			goto B;
		pp=(char *)bp;
		for(;*bp;bp++){
			fwrite(bp,sizeof(u64),1,fp);
		}
		res=0;
B:
		if(pp)
			free(pp);
		p[0]=p[1]=0xFFFFFFFF;
		fwrite(p,sizeof(u64),1,fp);
	}
	fclose(fp);
#endif
	return res;
}

int DebugWindow::Loop(){
	int ret=0;
#ifdef _DEBUG
	if(BT(_status,S_DEBUG_NEXT)){
		switch(SR(_pc,63)){
			case 0:{
					u32 i=_pc&0xfffffff;
					if(--i == 0){
						switch(SR(_pc,60)&7){
							case 0:
								BC(_status,S_DEBUG_NEXT);
								Update();
								//ShowToastMessage((char *)"Breakpoint reached.stopped");
								//FlashWindow(_window,2);
							break;
							case 1:
								BC(_status,S_PAUSE|S_DEBUG_NEXT);
								DebugWindow::_updateToolbar();
							break;
						}
						_pc=0;
						return -1;
					}
					_pc = (_pc & 0xF000000000000000)|i;
				}
				ret=0;
			break;
			case 1:{
				u32 __pc,i=_pc&0x7fffffffffffffff;

				if(!cpu->Query(ICORE_QUERY_PC,&__pc) && __pc==i){
					ShowToastMessage((char *)"Reached cursor.");
					_pc=0;
					BC(_status,S_DEBUG_NEXT);
					Update();
					FlashWindow(_window,2);
					return -1;
				}
			}
			ret=0;
			break;
		}
		return 0;
	}
#endif
	if(BT(_status,S_PAUSE|S_DEBUG_UPDATE))
		ret=-1;
	BC(_status,S_DEBUG_UPDATE);
	return ret;
}

int DebugWindow::OnDisableCore(){
	HWND win;

#ifdef _DEBUG

#ifdef __WIN32__
#else
	win=GetDlgItem(_window,RES(IDC_DEBUG_TAB));
	int i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (win))-1;///remove allpages withoutLog
	for(;i>0;i--){
		//GtkWidget *p=gtk_notebook_get_nth_page(GTK_NOTEBOOK (win),0);
		gtk_notebook_remove_page(GTK_NOTEBOOK (win),0);
		//	if(p)
		//	gtk_widget_destroy(p);
	}
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_TAB1))))//tab on top
		goto B;
	i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (win))-6;
	for(;i>0;i--){
		//GtkWidget *p=gtk_notebook_get_nth_page(GTK_NOTEBOOK (win),0);
		gtk_notebook_remove_page(GTK_NOTEBOOK (win),0);
		//	if(p)
		//	gtk_widget_destroy(p);
	}
B:
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_MENUBAR))))
		goto A;
	{
		GList *children = gtk_container_get_children(GTK_CONTAINER(win));
		for(int i=0;children;children = g_list_next(children),i++){
			if(!i) continue;
			gtk_container_remove(GTK_CONTAINER(win),GTK_WIDGET(children->data));
		}
	}
A:
#endif

#endif
	return 0;
}

int DebugWindow::OnEnableCore(){
#ifdef _DEBUG
	HWND win,w,e,a;
	LPDEBUGGERPAGE p=NULL;
	u32 id;
	char *s;
	int i;

	if(!_window || !cpu || cpu->Query(ICORE_QUERY_CPUS,&s) || !s)
		goto B;
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_TAB1))))
		goto B;

	for(char *ss=s,i=0;*ss;i++){
#ifdef __WIN32__
#else
		if(i!=0){
			a=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);

		/*	e=gtk_tree_view_new();
			gtk_box_pack_start(GTK_BOX(a),(HWND)e,TRUE,TRUE,0);
			w=gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL,0);
			gtk_widget_set_name(w,IDC_DEBUG_SB);
			g_signal_connect (w, "change-value", G_CALLBACK (::on_scroll_change), NULL);
			gtk_box_pack_start(GTK_BOX(a),(HWND)w,FALSE,FALSE,0);
			gtk_widget_set_name(e,IDC_DEBUG_LB_DIS+i);
		}*/
			gtk_widget_show_all(a);
			w = gtk_label_new (ss);
			gtk_notebook_insert_page(GTK_NOTEBOOK (win), a, w,i);
	//	if(i==0)
		//	_create_treeview_model(e);
		}
		else{
			if((a=gtk_notebook_get_nth_page(GTK_NOTEBOOK(win),0)))
				gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(win),a,ss);
		}
#endif
		ss += strlen(ss)+1+sizeof(u32);
	}
	delete []s;

B:
	if(!cpu || cpu->Query(ICORE_QUERY_DBG_MENU,&s) || !s)
		goto A;
#ifdef __WIN32__
#else
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_MENUBAR))))
		goto A;

	if(!*s)  goto E;
	e=gtk_menu_new();
	w=gtk_menu_item_new_with_label ("Machine");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), e);
	gtk_menu_shell_append (GTK_MENU_SHELL (win), w);

	for(char *ss=s;*ss;){
		char *c=ss;
		ss += strlen(ss)+1;
		u32 *v=(u32 *)ss;
		ss+=sizeof(u32)*2;
		switch((u8)v[1]){
			case 2:
				a = gtk_check_menu_item_new_with_label (c);
				if((u8)SR(v[1],8))
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(a),true);
			break;
			case 0:
				a=NULL;
			break;
			default:
				a = gtk_menu_item_new_with_label (c);
			break;
		}
		if(a){
			gtk_widget_set_name(a,MAKELONG(IDC_DEBUG_MENUBAR,(v[0]&0xfff)|SL(v[1] & 7,12)|0x8000));
			gtk_menu_shell_append (GTK_MENU_SHELL (e), a);
			gtk_widget_show(a);
			g_signal_connect (a, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		}
		switch((u8)SR(v[1],16)){
			case 1:{
				char cc[10];

				a=GetDlgItem(_window,RES(IDC_DEBUG_LAYERS_CB));
				sprintf(cc,"%u",v[0]);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(a),cc,c);
			}
			break;
		}
	}
E:
	a=GetDlgItem(_window,RES(IDC_DEBUG_OAM_CB));
	for(i=0;i<129;i++){
		char c[10];

		sprintf(c,"OAM %d",i);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(a),NULL,c);
	}
#endif
	delete []s;
A:
	if(!cpu || cpu->Query(ICORE_QUERY_DBG_PAGE,&p) || !p)
		return -1;
#ifdef __WIN32__
#else
	i=1;
	for(LPDEBUGGERPAGE pp=p;pp->size;pp++){
		sscanf(pp->name,"%d",&id);
		e=0;
		if(pp->group)
			goto C;
		win=GetDlgItem(_window,RES(IDC_DEBUG_TAB));
		switch(pp->type){
			case 3:
			case 2:{
				char c[30];

				HWND cb,mb,b=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
				a=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);

				e=gtk_entry_new();
				sprintf(c,"%d",id|0x20000);
				gtk_widget_set_name(e,c);
				gtk_container_add (GTK_CONTAINER (a),e);

				e=gtk_button_new_from_icon_name("gtk-jump-to",GTK_ICON_SIZE_MENU);
				sprintf(c,"%d",id|0x10000),
				gtk_widget_set_name(e,c);
				g_signal_connect(G_OBJECT(e),"clicked",G_CALLBACK(on_button_clicked),0);
				gtk_container_add (GTK_CONTAINER (a),e);

				e=gtk_combo_box_text_new();
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"8 bits");
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"16 bits");
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"32 bits");
				gtk_combo_box_set_active(GTK_COMBO_BOX(e),0);
				sprintf(c,"%d",id|0x30000),
				gtk_widget_set_name(e,c);
				g_signal_connect(G_OBJECT(e),"changed",G_CALLBACK(::on_command),0);
				gtk_container_add (GTK_CONTAINER (a),e);

				if(pp->type==2){
					e=gtk_button_new_from_icon_name("gtk-add",GTK_ICON_SIZE_MENU);
					sprintf(c,"%d",5518),
					gtk_widget_set_name(e,c);
					g_signal_connect(G_OBJECT(e),"clicked",G_CALLBACK(on_button_clicked),0);
					gtk_container_add (GTK_CONTAINER (a),e);

					cb=gtk_button_new_from_icon_name("gtk-close",GTK_ICON_SIZE_MENU);
					sprintf(c,"%d",id|0x40000),
					gtk_widget_set_name(cb,c);
					gtk_widget_set_sensitive(cb,false);
					g_signal_connect(G_OBJECT(cb),"clicked",G_CALLBACK(on_button_clicked),0);
					gtk_container_add (GTK_CONTAINER (a),cb);

					mb=gtk_menu_button_new();
					sprintf(c,"%d",id|0x60000),
					gtk_widget_set_name(mb,c);
					//g_signal_connect(G_OBJECT(e),"clicked",G_CALLBACK(on_button_clicked),0);
					gtk_container_add (GTK_CONTAINER (a),mb);
				}

				gtk_box_pack_start(GTK_BOX(b),(HWND)a,false,false,0);

				e=gtk_text_view_new();
				gtk_text_view_set_monospace(GTK_TEXT_VIEW(e),true);
				gtk_text_view_set_editable(GTK_TEXT_VIEW(e),false);
				gtk_widget_set_name(e,id);
				//
				GtkTextBuffer  *bu = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e));
				gtk_text_buffer_create_tag(bu, "u","foreground", "red", NULL);
				//gtk_text_buffer_create_tag(bu, "n","background", "white", NULL);

				g_object_set_data(G_OBJECT(e),"id",(gpointer)(u64)id);
				g_object_set_data(G_OBJECT(e),"type",(gpointer)(u64)pp->type);
				if(pp->popup)
					g_signal_connect(G_OBJECT(e),"populate-popup",G_CALLBACK(on_menu_init),0);
				//if(pp->editable)
					//g_signal_connect(G_OBJECT(e),"populate-popup",G_CALLBACK(on_menu_init),0);
				if(pp->clickable)
					g_signal_connect(G_OBJECT(e), "button-press-event", G_CALLBACK(::on_mouse_down), NULL);

				a=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
				gtk_box_pack_start (GTK_BOX (a),e,TRUE,TRUE,0);

				w = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL,NULL);
				sprintf(c,"%d",id|0x50000),
				gtk_widget_set_name(w,c);
			//	gtk_range_set_slider_size_fixed(GTK_RANGE(w),true);
				//gtk_range_set_round_digits(GTK_RANGE(w),0);
				g_signal_connect(G_OBJECT(w),"change-value",G_CALLBACK(on_scroll_change),0);
				gtk_box_pack_start (GTK_BOX (a),w,0,0,0);

				g_signal_connect (e, "scroll-event", G_CALLBACK (::on_scroll_event), (gpointer)w);

				gtk_box_pack_start(GTK_BOX(b),a,TRUE,TRUE,0);

				_memPages._connect(id,b,e,w,cb,mb);
				_memPages._addPage(0,pp->attributes,&e);
				gtk_container_add (GTK_CONTAINER (e),b);
			}
			break;
			case 1:
				{
					GtkWidget *a;//=gtk_viewport_new(NULL,NULL);
					if(!(a=gtk_text_view_new()))
						continue;
					gtk_text_view_set_monospace(GTK_TEXT_VIEW(a),true);
					gtk_text_view_set_editable(GTK_TEXT_VIEW(a),false);
					gtk_widget_set_name(a,pp->name);
					g_object_set_data(G_OBJECT(a),"id",(gpointer)(u64)id);
					g_object_set_data(G_OBJECT(a),"type",(gpointer)(u64)pp->type);

					GtkTextBuffer  *bu = gtk_text_view_get_buffer(GTK_TEXT_VIEW(a));
					gtk_text_buffer_create_tag(bu, "u","foreground", "red", NULL);
					gtk_text_buffer_create_tag(bu, "b","weight", PANGO_WEIGHT_BOLD, NULL);
					if(pp->popup)
						g_signal_connect(G_OBJECT(a),"populate-popup",G_CALLBACK(on_menu_init),0);
					if(pp->clickable)
						g_signal_connect(G_OBJECT(a), "button-press-event", G_CALLBACK(::on_mouse_down), NULL);

					//if(pp->editable)
						//g_signal_connect(G_OBJECT(e),"populate-popup",G_CALLBACK(on_menu_init),0);

					//gtk_container_add(GTK_CONTAINER (a),e);
					e = gtk_scrolled_window_new(NULL,NULL);
					if(e && a)
						gtk_container_add(GTK_CONTAINER (e),a);
			}
			break;
		}
		goto D;
C:
D:
		if(e){
			gtk_widget_show_all(e);
			w = gtk_label_new (pp->title);
		//	int i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (win));
			//g_object_set_data(G_OBJECT(e),"type",(gpointer)(u64)pp->type);
			int z=gtk_notebook_insert_page(GTK_NOTEBOOK (win), e, w,i++-1);
			if(pp->type==2)	_memPages._iPage=z;

			//gtk_notebook_prepend_page(GTK_NOTEBOOK (win), e, w);
		}
	}

	if(p)
		free(p);
	//gtk_notebook_reorder_child(GTK_NOTEBOOK(win),gtk_notebook_get_nth_page(GTK_NOTEBOOK(win),0),-1);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(win),0);
	/*
	a=gtk_viewport_new(NULL,NULL);
	e=gtk_text_view_new();
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(e),true);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(e),true);
	gtk_widget_set_name(e,"8888");

	g_object_set_data(G_OBJECT(e),"id",(gpointer)(u64)8888);
	/*if(pp->popup)
		g_signal_connect(G_OBJECT(e),"populate-popup",G_CALLBACK(on_menu_init),0);
	if(pp->clickable)
		g_signal_connect(G_OBJECT(e), "button-press-event", G_CALLBACK(::don_mouse_down), NULL);
	//if(pp->editable)
		//g_signal_connect(G_OBJECT(e),"populate-popup",G_CALLBACK(on_menu_init),0);*/

	/*gtk_container_add(GTK_CONTAINER (a),e);

	e = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_add(GTK_CONTAINER (e),a);
	gtk_widget_show_all(e);
	w = gtk_label_new ("Console");

	gtk_notebook_append_page(GTK_NOTEBOOK (win), e, w);
	*/
#endif
	_memPages._reset();
#endif
	return 0;
}

void DebugWindow::OnCommand(u32 id,u32 type,HWND w){
#ifdef _DEBUG

#ifdef __WIN32__
#else
	switch(id){
		case IDC_DEBUG_LB_BP:
		case IDC_DEBUG_LB_DIS:{
			u32 a;

			if(!GetAddressFromSelectedItem(w,&a))
				_updateAddressInfo(a);
		}
			break;
		case IDC_DEBUG_TAB:
			switch((u16)type){
				case GDK_BUTTON_PRESS:
				case GDK_BUTTON_RELEASE:{
					HWND e = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w),SR(type,16));
					if((u32)(u64)g_object_get_data(G_OBJECT(e),"type") == 2){
						_memPages._switchPage((u32)(u64)g_object_get_data(G_OBJECT(e),"index"),e);
						Update();
					}
				}
				break;
			}
			break;
		case IDC_DEBUG_TAB1:
			switch((u16)type){
				case GDK_BUTTON_PRESS:{
					int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(w));
					if(SR(type,16) < n - 3)
						OnChangeCpu(SR(type,16));
				}
				break;
			}
		break;
		case 5609:
			DebugLog::OnCommand(id,type,w);
		break;
		case IDC_DEBUG_LAYERS_CB:{
			u8 *data;
			u32 params[10]={0};

			data = (u8 *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(w));
			params[0] = stoi((char *)data);
			data=(u8 *)&params;
			if(!cpu || cpu->Query(ICORE_QUERY_DBG_LAYER,&data)){
				data=NULL;
			}
			HWND win = GetDlgItem(_window,RES(CONCAT(IDC_DEBUG_DA1,0)));
			GtkAdjustment *d=gtk_range_get_adjustment(GTK_RANGE(win));
			gtk_adjustment_configure(GTK_ADJUSTMENT(d),0,0,params[2],1,LOWORD(params[1]),256);
		}
		break;
		case IDC_DEBUG_OAM_CB:
		break;
		default:
			switch(SR(id,16)){
				case 3:
					int i=gtk_combo_box_get_active(GTK_COMBO_BOX(w));
					_memPages._setPageDumpMode(i*2);
					Update();
				break;
			}
			break;
	}
#endif

#endif
}

int DebugWindow::OnChangeCpu(u32 c){
#ifdef _DEBUG
	HWND w,win,page,a;
	u32 p[5];

	p[0]=c;
	if(!cpu || cpu->Query(ICORE_QUERY_CPU_INTERFACE,&p))
		return -1;
#ifdef __WIN32__
#else
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_TAB1))))
		return -2;

	if(cpu == (ICore *)*((void **)p) && gtk_notebook_get_current_page(GTK_NOTEBOOK(win)) == c)
		goto A;

	if(!(page=gtk_notebook_get_nth_page(GTK_NOTEBOOK(win),c)))
		return -3;

	if((w = GetDlgItem(win,RES(IDC_DEBUG_LB_DIS)))){
		g_object_ref(w);

		if((a=gtk_widget_get_parent(w)))
			gtk_container_remove(GTK_CONTAINER(a),w);
		gtk_box_pack_start(GTK_BOX(page),w,TRUE,TRUE,0);

		g_object_unref(w);
	}

	if((w = GetDlgItem(_window,RES(IDC_DEBUG_SB)))){
		g_object_ref(w);
		if((a=gtk_widget_get_parent(w)))
			gtk_container_remove(GTK_CONTAINER(a),w);
		gtk_box_pack_start(GTK_BOX(page),w,FALSE,FALSE,0);
		g_object_unref(w);
	}
//printf("OnChangeCpu %p %p\n",cpu,*((void **)p));
	cpu = (ICore *)*((void **)p);
A:
	disable_gobject_callback(G_OBJECT(win),G_CALLBACK(::on_change_page),true);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(win),c);
	_invalidateView();
	_adjustCodeViewport((u32)-1,DBV_VIEW);
	disable_gobject_callback(G_OBJECT(win),G_CALLBACK(::on_change_page),false);
#endif
	Update();
#endif
	return 0;
}

void DebugWindow::OnLButtonDown(u32 id,HWND w){
	switch(id){
		case IDC_DEBUG_LB_BP:
			break;
	}
}

void DebugWindow::OnRButtonDown(u32 id,HWND ww){
#ifdef _DEBUG

#ifdef __WIN32__
#else
	switch(id){
		case IDC_DEBUG_LB_BP:
		{
			HWND w,m=gtk_menu_new();

			//GetAddressFromSelectedItem(ww,&adr);
			//d[0]=GetSelectedListItemIndex(w);

			w = gtk_menu_item_new_with_label ("Copy");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,10));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Toggle");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,3));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Go...");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,4));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Reset");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,5));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Delete");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,6));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Delete all");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,7));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Disable all");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,8));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Edit");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,9));
			gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			gtk_menu_popup_at_pointer(GTK_MENU(m),0);
		}
		break;
	}
#endif

#endif
}

void DebugWindow::OnPopupMenuInit(u32 id,HMENU menu){
	HWND w;
	char s[10];
#ifdef _DEBUG

#ifdef __WIN32__
#else
	{
		DEBUGGERPAGE pp;

		sprintf(s,"%d",id);
		w=GetDlgItem(_window,s);
		u32 i = (u32)(u64)g_object_get_data(G_OBJECT(w),"id");

		pp.size=sizeof(DEBUGGERPAGE);
		pp.id = i;
		pp.w=(GtkWidget *)menu;
		pp.event_id=ICORE_MESSAGE_POPUP_INIT;
		pp.res=0;
		if(cpu->Query(ICORE_QUERY_DBG_PAGE_EVENT,&pp))
			goto A;
		g_signal_connect(G_OBJECT(pp.res), "activate", G_CALLBACK (::on_menuitem_select), NULL);
	}
A:
	switch(id){
		case IDC_DEBUG_TV_LOG:
			w = gtk_menu_item_new_with_label ("Find");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_TV_LOG,7));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		break;
		case IDC_DEBUG_TV1:
			w = gtk_menu_item_new_with_label ("Go...");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_TV1,1));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		break;
		case 3105:
			w = gtk_menu_item_new_with_label ("Change...");
			gtk_widget_set_name(w,MAKELONG(id,5));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		break;
		case 3102:
			if(_memPages._editAddress){
				w = gtk_menu_item_new_with_label ("Change...");
				gtk_widget_set_name(w,MAKELONG(id,5));
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
				gtk_widget_show(w);
				g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

				w = gtk_menu_item_new_with_label ("...to watchpoint");
				gtk_widget_set_name(w,MAKELONG(id,6));
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
				gtk_widget_show(w);
				g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

				w = gtk_menu_item_new_with_label ("Reset");
				gtk_widget_set_name(w,MAKELONG(id,8));
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
				gtk_widget_show(w);
				g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
				w = gtk_menu_item_new_with_label ("Decrement");
				gtk_widget_set_name(w,MAKELONG(id,9));
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
				gtk_widget_show(w);
				g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
				w = gtk_menu_item_new_with_label ("Increment");
				gtk_widget_set_name(w,MAKELONG(id,10));
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
				gtk_widget_show(w);
				g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
			}
			w = gtk_menu_item_new_with_label ("Find");
			gtk_widget_set_name(w,MAKELONG(id,7));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		break;
		case 3100://0xc1c
		{
			w = gtk_menu_item_new_with_label ("Reset");
			gtk_widget_set_name(w,MAKELONG(3100,1));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
			w = gtk_menu_item_new_with_label ("Increment");
			gtk_widget_set_name(w,MAKELONG(3100,2));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Decrement");
			gtk_widget_set_name(w,MAKELONG(3100,3));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

			w = gtk_menu_item_new_with_label ("Change...");
			gtk_widget_set_name(w,MAKELONG(3100,4));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		}
		break;
	}
#endif

#endif
}

void DebugWindow::OnMenuItemSelect(u32 id,HMENUITEM item){
#ifdef _DEBUG
	char s[20],*c;
	HWND w;
	u32 d[7];
	DEBUGGERPAGE pp;
#ifdef __WIN32__
#else
	GtkTextIter ss,se;
	GtkTextBuffer *b;

	sprintf(s,"%d",(u16)id);
	w=GetDlgItem(_window,s);

	u32 i = (u32)(u64)g_object_get_data(G_OBJECT(w),"id");
	pp.size=sizeof(DEBUGGERPAGE);
	pp.id = i;
	pp.w=w;
	pp.event_id=ICORE_MESSAGE_MENUITEM_SELECT;
	pp.wParam=SR(id,16);
	pp.res=0;
	if(!cpu->Query(ICORE_QUERY_DBG_PAGE_EVENT,&pp))
		return;
	switch((u16)id){
		case IDC_DEBUG_TV1:
			switch(SR(id,16)){
				default:
					printf("lino\n");
				break;
			}
			break;
		case IDC_DEBUG_MENUBAR:
			id=SR(id,16);
			if(id & 0x8000){
				u8  t=SR(id&0x7000,12);
				id=(u32)id&0xfff;
				switch(t){
					case 2:
						id |= gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))?0x10000:0;
					break;
				}
				cpu->Query(ICORE_QUERY_DBG_MENU_SELECTED,&id);
				return;
			}
			switch(id){
				case 1:
					_error_level=0;
				break;
				case 2:
					_error_level=8;
				break;
				case 5:
					_status ^= DEBUG_BREAK_IRQ;
				break;
				case 6:
					_status ^= DEBUG_BREAK_OPCODE;
				break;
				case 7:
					_status ^= DEBUG_BREAK_DEVICE;
				break;
				case 8:
					_status ^= DEBUG_BREAK_VBLANK;
				break;
				case 9:
					_status ^= DEBUG_LOG_DEV;
				break;
				case 100:
					LoadNotes();
				break;
				case 101:
					SaveNotes();
				break;
				case IDC_DEBUG_CLOSE:
					DebugMode(0);
					break;
			}
		break;
		case IDC_DEBUG_LB_BP:
			switch(SR(id,16)){
				case 4:{
					if(!GetAddressFromSelectedItem(GetDlgItem(_window,RES(IDC_DEBUG_LB_BP)),d)){
						_adjustCodeViewport(*d,DBV_VIEW);
						gtk_notebook_set_current_page(GTK_NOTEBOOK(GetDlgItem(_window,RES(IDC_DEBUG_TAB1))),0);
					}
				}
				break;
				case 5:{
					u32 d[2];

					d[0]=GetSelectedListItemIndex(w);
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_BREAKPOINT_RESET,d))
						Update();
				}
				break;
				case 6:{
					u32 d[2];
					d[0]=GetSelectedListItemIndex(w);
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_BREAKPOINT_DEL,d)){
						_invalidateView();
						Update();
					}
				}
				break;
				case 7:
					if(!cpu->Query(ICORE_QUERY_BREAKPOINT_CLEAR,NULL))
						Update();
				break;
				case 8:{
					u32 d[2];

					if(!cpu->Query(ICORE_QUERY_BREAKPOINT_COUNT,d)){
						u32 c=d[0];
						for(u32 n=0;n<c;n++){
							d[0]=n;
							d[1]=0;
							cpu->Query(ICORE_QUERY_SET_BREAKPOINT_STATE,d);
						}
						Update();
					}
				}
				break;
				case 0xa:{
					if(!GetAddressFromSelectedItem(GetDlgItem(_window,RES(IDC_DEBUG_LB_BP)),d)){
						char s[20];

						sprintf(s,"%x",*d);
						gtk_clipboard_set_text(gtk_widget_get_clipboard(_window,GDK_SELECTION_CLIPBOARD),(gchar *)s,-1);
						ShowToastMessage((char *)"Address copied.");
					}
				}
				break;
				case 9://Edit
				{
					u32 d[3];

					d[0]=GetSelectedListItemIndex(w);
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_BREAKPOINT_INFO,d)){
						if(!ChangeBreakpointBox((char *)"Edit Breakpoint",(u64 *)&d[1])){
							cpu->Query(ICORE_QUERY_SET_BREAKPOINT,d);
							Update();
						}
					}
				}
				break;
				default:{
					u32 d[2];

					d[0]=GetSelectedListItemIndex(w);
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_SET_BREAKPOINT_STATE,d))
						Update();
				}
				break;
			}
		break;
		default:
			i = (u32)(u64)g_object_get_data(G_OBJECT(w),"type");
			switch(i){
				case 1:
					b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
					gtk_text_buffer_get_selection_bounds(b,&ss,&se);
					c=gtk_text_buffer_get_text(b,&ss,&se,FALSE);
					d[0]=-1;
					if(sscanf(c,"%c%d",s,&d[0]) != 2 || s[0] != 'r'){
						*((void **)&d[2])=c;
						d[0]=-1;
					}
					switch(SR(id,16)){
						case 1:
							d[1]=0;
							cpu->Query(ICORE_QUERY_SET_REGISTER,d);
							Update();
						break;
						case 2:
							cpu->Query(ICORE_QUERY_REGISTER,d);
							d[1]+=1;
							cpu->Query(ICORE_QUERY_SET_REGISTER,d);
							Update();
						break;
						case 3:
							cpu->Query(ICORE_QUERY_REGISTER,d);
							d[1]-=1;
							cpu->Query(ICORE_QUERY_SET_REGISTER,d);
							Update();
						break;
						case 4:
							cpu->Query(ICORE_QUERY_REGISTER,d);
							if(!ChangeLocationBox((char *)"Change Register",d,BV(0))){
								cpu->Query(ICORE_QUERY_SET_REGISTER,d);
								DebugWindow::Update();
							}
						break;
					}
				break;
				case 2:
					switch(SR(id,16)){
						case 6:{
							d[0]=_memPages._editAddress;
							d[1]=0x80008003;
							cpu->Query(ICORE_QUERY_BREAKPOINT_ADD,d);
							Update();
						}
						break;
						case 8:
						case 9:
						case 10:{
							b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
							if(gtk_text_buffer_get_selection_bounds(b,&ss,&se)){
								c=gtk_text_buffer_get_text(b,&ss,&se,FALSE);
							}

							d[0]=_memPages._editAddress;
							d[1]=0;
							d[2]=SR(id,16)-8;
							d[3]=id;
							_memPages._getCurrentMode(&d[4]);
							d[4]=(d[4]+1)*2;
							cpu->Query(ICORE_QUERY_SET_LOCATION,d);
							Update();
						}
						break;
						case 7:{
							if((c = new char[1000])){
								*((u64 *)d)=(u64)c;
								if(!ChangeLocationBox((char *)"Find...",d,2)){
									SearchWord(c);
								}
								delete []c;
							}
						}
						break;
						case 5:{
							d[0]=_memPages._editAddress;
							d[1]=0;
							d[2]=0;
							d[3]=id;
							//printf("de %x\n",id);
							if(!ChangeLocationBox((char *)"Change Memory...",d)){
								cpu->Query(ICORE_QUERY_SET_LOCATION,d);
								Update();
							}
						}
						break;
						case 0x400:{
							HWND ww;
							char c[20],*p;

							sprintf(c,"%d",(u16)id + 0x20000);
#ifdef __WIN32__
#else
							if((ww=GetDlgItem(_window,c)) && (p=(char *)gtk_entry_get_text(GTK_ENTRY(ww)))){
								u32 u;

								sscanf(p,"%x",&u);
								_memPages._addBookmark(u);
							}
#endif

						}
						break;
						case 0x401:
							_memPages._delBookmark();
						break;
						default:
							if(SR(id,16)>=0x440 && SR(id,16) < 0x500){
HWND ww;
							char c[20],*p;

							sprintf(c,"%d",(u16)id + 0x20000);
#ifdef __WIN32__
#else
							if((ww=GetDlgItem(_window,c)) && (p=(char *)gtk_menu_item_get_label(GTK_MENU_ITEM(item)))){
								gtk_entry_set_text(GTK_ENTRY(ww),p);
								sprintf(c,"%d",(u16)id + 0x10000);
								gtk_button_clicked(GTK_BUTTON(GetDlgItem(_window,c)));
							}
#endif
							}
						break;
					}
				break;
			}
		}
#endif

#endif
}

int DebugWindow::OnLDblClk(u32 id,HWND w){
	int res=-1;
#ifdef _DEBUG
	u32 adr;
	int x,y;
#ifdef __WIN32__
#else
	GtkTextIter ii,ie;
	DEBUGGERPAGE pp;
	gdouble dx,dy;
	GtkTextBuffer *tb;
	GdkEvent *e;

	e=NULL;
	res=-1;
	switch(id){
		case 7100:
		case 7101:
		{
			gchar *p=(gchar *)gtk_label_get_text(GTK_LABEL(w));
			if(p){
				while(*p && *p!=0x20) p++;
				if(*p){
					gtk_clipboard_set_text(gtk_widget_get_clipboard(_window,GDK_SELECTION_CLIPBOARD),p,-1);
					ShowToastMessage((char *)"Address copied.");
				}
			}
		}
			return 0;
		break;
		case IDC_DEBUG_LB_DIS:
		{
			gdouble dx,dy;

			e=gtk_get_current_event();
			gdk_event_get_coords(e,&dx,&dy);
			if(dx<20){

				u32 d[2];

				GetAddressFromSelectedItem(w,&d[0]);
				if(!cpu->Query(ICORE_QUERY_BREAKPOINT_INDEX,d)){
					d[0]=d[1];
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_SET_BREAKPOINT_STATE,d)){
						_invalidateView();
						Update();
					}
				}
			}
		}
			res=0;
			goto C;
		break;
		case IDC_DEBUG_LB_BP:
		{
			gdouble dx,dy;
			u32 adr;

			e=gtk_get_current_event();
			gdk_event_get_coords(e,&dx,&dy);
			if(dx<20){

				u32 d[2];

				GetAddressFromSelectedItem(w,&adr);
				d[0]=GetSelectedListItemIndex(w);
				d[1]=(u32)-1;
				if(!cpu->Query(ICORE_QUERY_SET_BREAKPOINT_STATE,d))
					Update();
			}
		}
			res=0;
			goto C;
		break;
		case 1001:{
			gdouble dx,dy;
			char c[30];
			if(!BT(_status,S_DEBUG))
				goto C;

			e=gtk_get_current_event();
			gdk_event_get_coords(e,&dx,&dy);
			sprintf(c,"%d %d",(int)dx,(int)dy);
			ShowToastMessage(c);
			goto C;
		}
		break;
		case IDC_DEBUG_DA1:
			printf("click\n");goto C;
	}
	e=gtk_get_current_event();
	gdk_event_get_coords(e,&dx,&dy);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(w),&ii,(int)dx,(int)dy);
	x=dx;y=dy;
	if(!GTK_IS_TEXT_VIEW(w))
		goto B;
	tb=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
	if(!gtk_text_buffer_get_selection_bounds(tb,0,0))
		goto C;
	y=gtk_text_iter_get_line(&ii);
	x = gtk_text_iter_get_line_offset(&ii);
B:
	pp.size=sizeof(DEBUGGERPAGE);
	pp.id = id;
	pp.w=w;
	pp.x=x;
	pp.y=y;
	pp.w=w;
	pp.event_id=GDK_2BUTTON_PRESS;
	pp.res=0;
	if(!cpu->Query(ICORE_QUERY_DBG_PAGE_COORD_TO_ADDRESS,&pp)){
		if(pp.message)
			adr = (u32)pp.res;
		goto A;
	}
	if(!GTK_IS_TEXT_VIEW(w))
		goto C;
	gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(tb),&ii,0);
	gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(tb),&ie,10);

	sscanf(gtk_text_iter_get_slice(&ii,&ie),"%x",&adr);
	{
		char s[15],e[]={3,5,9},ee[]={1,2,4};
		sprintf(s,"%d",GetDlgItemId(w)+0x30000);
		int i=gtk_combo_box_get_active(GTK_COMBO_BOX(GetDlgItem(_window,s)));
		adr += (y*16)+(((x-10)/e[i])*ee[i]);
	}
A:
	DEVF("mm %x %d %d\n",adr,x,y);
	_memPages._editAddress=adr;
	Update();
	res=0;
C:
	if(e)
		gdk_event_free(e);
#endif

#endif
	return res;
}

int DebugWindow::ChangeBreakpointBox(char *title,u64 *bp,u32 flags){
	int res=-1;
	char c[30],*p;
	u32 bd;
	volatile int i;
	HWND dialog,vb,e,w;
	u64 bkv;

	bkv = *bp;
	bd = SR(bkv,32);
#ifdef _DEBUG

#ifdef __WIN32__
#else
	GtkEntryBuffer *b;

	dialog = gtk_dialog_new_with_buttons (title,GTK_WINDOW(_window), GTK_DIALOG_DESTROY_WITH_PARENT, "_Ok",
                                              GTK_RESPONSE_ACCEPT,"_Cancel",GTK_RESPONSE_CANCEL, NULL);
	if(!dialog)
		return -1;

	vb = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vb),5);

	e=gtk_check_button_new_with_label("Enabled");
	gtk_container_add (GTK_CONTAINER (vb),e);
	gtk_widget_set_name(e,10);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e),ISBREAKPOINTENABLED(*bp) !=0);

	w=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,3);
	gtk_box_set_homogeneous(GTK_BOX(w),true);
	e=gtk_text_view_new();
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(e),true);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(e),false);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(e),false);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(e),4);
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(e),4);
	{
		GtkTextBuffer *x=gtk_text_view_get_buffer(GTK_TEXT_VIEW(e));
		sprintf(c,"%08X",(u32)*bp);
		gtk_text_buffer_set_text(x,c,-1);
	}
	gtk_widget_set_name(e,1);
	gtk_box_pack_start(GTK_BOX (w),e,true,true,3);
	//gtk_container_add (GTK_CONTAINER (w),e);
	if(ISMEMORYBREAKPOINT(*bp)){
		e=gtk_text_view_new();
		gtk_text_view_set_monospace(GTK_TEXT_VIEW(e),true);
		gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(e),4);
		gtk_text_view_set_top_margin(GTK_TEXT_VIEW(e),4);
		//gtk_container_add (GTK_CONTAINER (w),e);
		gtk_box_pack_start(GTK_BOX (w),e,true,true,3);
	}
	gtk_container_add (GTK_CONTAINER (vb),w);

	e=gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Memory");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Code");
	gtk_combo_box_set_active(GTK_COMBO_BOX(e),ISMEMORYBREAKPOINT(*bp) ? 0 :1);
	gtk_widget_set_sensitive(e,false);
	gtk_widget_set_name(e,2);
	gtk_container_add (GTK_CONTAINER (vb),e);

	e=gtk_combo_box_text_new();
	gtk_widget_set_name(e,3);
	if(ISMEMORYBREAKPOINT(*bp)){
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Read");
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Write");
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Read/Write");
		i=SR(bd & 0x30000000,28);
	}
	else{
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Counter");
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Complex");
		i = bd & 0x8000 ? 1 : 0;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(e),i);
	gtk_container_add (GTK_CONTAINER (vb),e);

	if(ISMEMORYBREAKPOINT(*bp)){
		e=gtk_combo_box_text_new();
		gtk_widget_set_name(e,5);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Byte");
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Word");
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Long");
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"*");
		i=SR(bd,6) & 3;

		gtk_combo_box_set_active(GTK_COMBO_BOX(e),i);
		gtk_container_add (GTK_CONTAINER (vb),e);

		e=gtk_check_button_new_with_label("Stop");
		gtk_widget_set_name(e,100);
		gtk_container_add (GTK_CONTAINER (vb),e);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e),SR(bd,2) & 1 !=0);
	}

	e=gtk_combo_box_text_new();
	gtk_widget_set_name(e,4);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Register");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Value");

	i=bd&0x40000000 ? 1 : 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(e),i);
	gtk_container_add (GTK_CONTAINER (vb),e);

	if(ISMEMORYBREAKPOINT(*bp)){
		i= SR(bd&0xf8,3);
		if(bd & 0x40000000){
			i |= SR(bd&0x0fffff00,3);
			sprintf(c,"%x",i);
		}
		else
			sprintf(c,"r%d",i);
		i=SR(bd,3)&7;
	}
	else{
		e=gtk_label_new("Counter");
		gtk_container_add (GTK_CONTAINER (vb),e);
		e=gtk_entry_new();
		gtk_widget_set_name(e,100);//first value
		if(bd&0x8000){//complex
			i = SR(bd,3) & 0x1f;
			sprintf(c,"r%d",i);
		}
		else{
			i=(int)(bd&0x7fff);
			sprintf(c,"%x",i);
		}
		i=bd&7;
		b=gtk_entry_get_buffer(GTK_ENTRY(e));
		gtk_entry_buffer_set_text(b,c,-1);
		gtk_container_add (GTK_CONTAINER (vb),e);
	}

	e=gtk_combo_box_text_new();
	gtk_widget_set_name(e,11);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"!=");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"==");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"<");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,">");

	gtk_combo_box_set_active(GTK_COMBO_BOX(e),i);
	gtk_container_add (GTK_CONTAINER (vb),e);

	e=gtk_label_new("Value");
	gtk_container_add (GTK_CONTAINER (vb),e);

	e=gtk_entry_new();
	gtk_widget_set_name(e,200);//value
	b=gtk_entry_get_buffer(GTK_ENTRY(e));

	if(ISMEMORYBREAKPOINT(*bp)){

	}
	else{
		if(bd&0x8000){//no counter
			i = SR(bd&0x7f00,8);
			if(bd & 0x40000000){
				i |= SR(bd&0x3fff0000,9);
				sprintf(c,"%x",i);
			}
			else
				sprintf(c,"r%d",i);
		}
		else{
			sprintf(c,"%x",SR(bd,16)&0x7fff);
		}
	}
	gtk_entry_buffer_set_text(b,c,-1);
	gtk_container_add (GTK_CONTAINER (vb),e);
	gtk_widget_show_all(dialog);
	res=1;
	if(gtk_dialog_run(GTK_DIALOG (dialog))!=GTK_RESPONSE_ACCEPT)
		goto A;

	e = GetDlgItem(dialog,RES(10));//enabled
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e)))
		bkv |= 0x8000000000000000ULL;
	else
		bkv &= ~0x8000000000000000ULL;
	e = GetDlgItem(dialog,RES(4));
	i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
	bkv= SL((u64)i,62)|(bkv & 0xBFFFFFFFFFFFFFFFULL);

	if(ISMEMORYBREAKPOINT(bkv)){
		e = GetDlgItem(dialog,RES(100));//stop
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e)))
			bkv |= 0x400000000ULL;
		else
			bkv &= ~0x400000000ULL;

		e = GetDlgItem(dialog,RES(3));//access
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
		bkv = SL((u64)i,60)|(bkv & 0xCFFFFFFFFFFFFFFFULL);//0x3

		e = GetDlgItem(dialog,RES(5));//size
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
		bkv = SL((u64)i,38)|(bkv & 0xFFFFFF3FFFFFFFFF);//

		e = GetDlgItem(dialog,RES(200));
		b=gtk_entry_get_buffer(GTK_ENTRY(e));
		p = (char *)gtk_entry_buffer_get_text(b);
		sscanf(p,"%x",&i);
	//	bkv = (bkv & ~0x7FFF7f0000000000ULL)|SL((u64)(i&0x7f),40);//800080FF

		e = GetDlgItem(dialog,RES(4));//register/value
		if(gtk_combo_box_get_active(GTK_COMBO_BOX(e))){
	//		bkv |= 0x4000000000000000ULL;
	//		bkv |= SL((u64)SR(i,7) & 0x3fff,48);
		}

		e = GetDlgItem(dialog,RES(11));//condition
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
		//i=7;
		bkv = (bkv & ~0x1800000000ULL) | SL((u64)i,35);
	}
	else{
		e = GetDlgItem(dialog,RES(3));
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
		bkv = SL((u64)i,47) | (bkv & ~0x800000000000ULL);
		bd=SR(bkv,32);
		if(bd&0x8000){//complex
			e = GetDlgItem(dialog,RES(100));//counter
			b=gtk_entry_get_buffer(GTK_ENTRY(e));
			p = (char *)gtk_entry_buffer_get_text(b);
			if(*p=='r') p++;
			sscanf(p,"%x",&i);
			bkv = (bkv & ~0xf800000000ULL)|SL((u64)i,35);

			e = GetDlgItem(dialog,RES(200));//counter
			b=gtk_entry_get_buffer(GTK_ENTRY(e));
			p = (char *)gtk_entry_buffer_get_text(b);
			sscanf(p,"%x",&i);
			bkv = (bkv & ~0x7FFF7f0000000000ULL)|SL((u64)(i&0x7f),40);
			e = GetDlgItem(dialog,RES(4));
			if(gtk_combo_box_get_active(GTK_COMBO_BOX(e))){
				bkv |= 0x4000000000000000ULL;
				bkv |= SL((u64)SR(i,7)&0x3fff,48);
			}
			e = GetDlgItem(dialog,RES(11));//condition
			i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
			bkv= SL((u64)i,32)|(bkv & ~0x300000000ULL);
		}
		else{
			e = GetDlgItem(dialog,RES(200));//value
			b=gtk_entry_get_buffer(GTK_ENTRY(e));
			p = (char *)gtk_entry_buffer_get_text(b);
			sscanf(p,"%x",&i);
			bkv = SL((u64)i,48)|(bkv & 0x8000FFFFFFFFFFFF);

			e = GetDlgItem(dialog,RES(100));//counter
			b=gtk_entry_get_buffer(GTK_ENTRY(e));
			p = (char *)gtk_entry_buffer_get_text(b);
			sscanf(p,"%x",&i);
			bkv = SL((u64)(i&0x7fff),32)|(bkv & 0xFFFF0000FFFFFFFF);
		}
	}
	*bp=bkv;
	res=0;
A:
	gtk_widget_destroy(dialog);
#endif

#endif
	return res;
}

int DebugWindow::ChangeLocationBox(char *title,u32 *d,u32 flags){
	int res=-1;
#ifdef _DEBUG
	HWND dialog, e0,e1,vb;
	char c[20];

#ifdef __WIN32__
#else
	dialog = gtk_dialog_new_with_buttons (title,GTK_WINDOW(_window), GTK_DIALOG_DESTROY_WITH_PARENT, "_Ok",
                                              GTK_RESPONSE_ACCEPT,"_Cancel",GTK_RESPONSE_CANCEL, NULL);
	if(!dialog)
		return -1;
	vb = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vb),5);
	e0=gtk_entry_new();
	gtk_container_add (GTK_CONTAINER (vb),e0);
	if(BVT(flags,1)==0){
		if(!BVT(flags,0))
			sprintf(c,"%x",d[0]);
		else if(d[0] != (u32)-1)
			sprintf(c,"r%d",d[0]);
		else
			strcpy(c,*((char **)&d[2]));
		gtk_entry_set_text(GTK_ENTRY(e0),c);

		e1=gtk_entry_new();
		sprintf(c,"%x",d[1]);
		gtk_entry_set_text(GTK_ENTRY(e1),c);
		gtk_container_add (GTK_CONTAINER (vb),e1);
	}
	gtk_widget_show_all(dialog);
	res=1;
	if(gtk_dialog_run(GTK_DIALOG (dialog))!=GTK_RESPONSE_ACCEPT)
		goto A;
	if(BVT(flags,1)==0)
		sscanf(gtk_entry_get_text(GTK_ENTRY(e1)),"%x",&d[1]);
	else{
		strcpy((char *)*((u64 *)d),gtk_entry_get_text(GTK_ENTRY(e0)));
	}
	res=0;
A:
	gtk_widget_destroy(dialog);
#endif

#endif
	return res;
}

int DebugWindow::OnMemoryUpdate(u32 adr,u32 flags,ICore *core){
	u32 d[5];

#ifdef _DEBUG
	if(!BT(_status,S_DEBUG))
		return 0;
	//if(BT(_status,S_PAUSE))
		//goto A;
	//if(adr == 0x4026AC83)
		//EnterDebugMode();
	//adr=0x2000014;
	d[0]=adr;
	d[1]=flags;//4026AC83
	if(!core) core=cpu;
	if(core->Query(ICORE_QUERY_MEMORY_ACCESS,d) <= 0)
		goto A;
	//cpu->Query(ICORE_QUERY_PC,&d[2]);
	putf(0,"Breakpoint reached at location %x %x at PC:%x CPU:%d\n",adr,flags,d[2],(int)d[3]);
	//beep(600,80,128);
	if(d[4]){
		DebugMode(1,0,d[3]);
		ShowToastMessage((char *)"Memory breakpoint reached. Stopped");
	}
A:
	if(!BT(_status,S_PAUSE))
		goto B;
	_memPages._lastAccessAddress=adr;
	//printf("mu %x %x\n",adr,flags);
B:
#endif
	return 0;
}

int DebugWindow::SearchWord(char *c){
	u32 d[10],dd[10];
	int i;

	if(!c){
		if(!_searchWord)
			return -1;
		c=_searchWord;
	}
	if(!c[0])
		return -2;
	i=strlen(c);
	d[0]=_memPages._editAddress;
	*((u64 *)&d[1])=(u64)&d[3];
	cpu->Query(ICORE_QUERY_ADDRESS_INFO,d);

	*((u64 *)dd)=(u64)c;
	dd[2]=_memPages._editAddress;
	dd[3]=d[4]-(_memPages._editAddress-d[3]);

	//printf("lino %x %x\n",dd[2],dd[3]);
	if(!cpu->Query(ICORE_QUERY_MEMORY_FIND,dd)){
		_memPages._editAddress=dd[2];
		_memPages._dumpAddress=_memPages._editAddress;
		_memPages._editAddress+=i;
		Update();
	}
	if(c!=_searchWord){
		if(!_searchWord || i != strlen(_searchWord)){
			if(_searchWord)
				delete []_searchWord;
			_searchWord=new char[i+1];
			_searchWord[0]=0;
			strcpy(_searchWord,c);
		}
	}
	return -1;
}

int DebugWindow::LoadNotes(char *fn){
	int res=-1;
#ifdef _DEBUG
	FILE *fp;
	u32 n;
#ifdef __WIN32__
#else
	GtkTextBuffer *b;
	char *p,fileName[1024],ext[1024];
	HWND w;

	*((u64 *)fileName)=0;
	*((u64 *)ext)=0;
	p=NULL;
	//GUI::ShowOpen((char *)"Open Notes File",(char *)ext,_window,fileName,1,0);
	if(!(fp=fopen("notes.txt","rb")))
		goto A;
	fseek(fp,0,SEEK_END);
	n=ftell(fp);
	if(!(p=new char[n+5]))
		goto B;
	fseek(fp,0,SEEK_SET);
	fread(p,1,n,fp);
	p[n]=0;
	if((w=GetDlgItem(_window,RES(IDC_DEBUG_TV1)))){
		b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
		gtk_text_buffer_set_text(b,p,-1);
	}
B:
	if(p)
		delete []p;
	if(fp)
		fclose(fp);
	res=0;
#endif

#endif
A:
	return res;
}

int DebugWindow::SaveNotes(char *fn){
	int res=-1;
#ifdef _DEBUG

	char *pp;
	FILE *fp;

#ifdef __WIN32__
	pp=NULL;
#else
	GtkTextIter s,e;
	GtkTextBuffer *b;

	res--;
	b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(GetDlgItem(_window,RES(IDC_DEBUG_TV1))));
	gtk_text_buffer_get_start_iter(b,&s);
	gtk_text_buffer_get_end_iter(b,&e);
	pp=gtk_text_buffer_get_text(b,&s,&e,false);
	res--;
#endif
	if(!pp || !(fp=fopen("notes.txt","wb")))
		goto A;
	fwrite(pp,1,strlen(pp),fp);
	fclose(fp);
	res=0;
A:
#endif
	return res;
}

void DebugWindow::_invalidateView(int i){
	if(i==-1 || i == DBV_RUN)
		memset(&_views[DBV_RUN],0,sizeof(DISVIEW));
	if(i==-1 || i == DBV_VIEW)
		memset(&_views[DBV_VIEW],0,sizeof(DISVIEW));
}

void DebugWindow::Threaad1Update(){
	EnterCriticalSection(&_cr);
	for (auto it = _quCmd.begin();it != _quCmd.end(); ++it){
		u64 v = *it;
		if(v&0x8000000000000000)
			continue;
		if(!(v&0x4000000000000000))
			continue;
		*it |=0x8000000000000000;
		//printf("tu:%lx\n",v);
		switch((u16)v){
			case 2:
				ShowToastMessage(0);
			break;
		}
	}
	LeaveCriticalSection(&_cr);
	//DebugLog::Update();
}

void DebugWindow::TimerThread(){
	vector<u32> del;

	while(!(DebugWindow::_status & S_QUIT)){
		usleep(95000);
		EnterCriticalSection(&_cr);
		u32 i=0;
		for (auto it = _quCmd.begin();it != _quCmd.end(); ++it,i++){
			u64 v = *it;
			if(v&0x8000000000000000){
				del.push_back(i);
				continue;
			}
			switch((u16)v){
				case 1:
				{
					u64 n = SR(v,16);
					if(n>99)
						n=2;
					else
						n = SL(n+1,16)|1;
					*it=n;
				}
				break;
				default:
					if(!(v&0x4000000000000000)){
						*it |=0x4000000000000000;
#ifdef __WIN32__
#else
						gdk_threads_add_idle(thread1_worker, this);
#endif
					}
					break;
			}
		}
		for (auto it = del.rbegin();it != del.rend(); ++it){
			_quCmd.erase(_quCmd.begin()+*it);
		}
		del.clear();
		LeaveCriticalSection(&_cr);
	}
}

int DebugWindow::ShowToastMessage(char *p){
#ifdef _DEBUG
	char s[1000];
	HWND w;
	int i;
#ifdef __WIN32__
#else
	w=GetDlgItem(_window,RES(7102));
	if(!w)
		return -1;
	for(i=0;p && *p && *p!='\n';i++)
		s[i]=*p++;
	s[i]=0;
	gtk_label_set_text(GTK_LABEL(w),s);
#endif
	if(i)
		AddCommandQueue(1);
#endif
	return 0;
}

int DebugWindow::AddCommandQueue(u64 cmd){
	EnterCriticalSection(&_cr);
	_quCmd.push_back(cmd);
	LeaveCriticalSection(&_cr);
	return 0;
}

int DebugWindow::_updateToolbar(){
#ifdef _DEBUG
	BOOL b = BT(_status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE;

	EnableWindow(GetDlgItem(_window,RES(IDC_DEBUG_RUN)),b);
	EnableWindow(GetDlgItem(_window,RES(IDC_DEBUG_RUN_CURSOR)),b);
	EnableWindow(GetDlgItem(_window,RES(IDC_DEBUG_RUN_NEXT)),b);
	EnableWindow(GetDlgItem(_window,RES(IDC_DEBUG_STOP)),!b);
#endif
	return 0;
}

int DebugWindow::OnPaint(u32 id,HWND w,HDC cr){
	switch(id){
		case IDC_DEBUG_DA1:
			UpdateLayersView(w,cr);
		break;
		case IDC_DEBUG_DA2:
			UpdatePaletteView(w,cr);
		break;
		case IDC_DEBUG_DA3:
			UpdateOAMView(w,cr);
		break;
		default:
			printf("%s %d\n",__FUNCTION__,id);
		break;
	}
	return 0;
}

int DebugWindow::UpdatePaletteView(HWND win,HDC cr){
	u8 *data;
	HBITMAP bit;
	HDC ca;

	data=NULL;
	bit=NULL;
#ifdef __WIN32__
#else
	int w = gtk_widget_get_allocated_width (win);
	int h = gtk_widget_get_allocated_height (win);
#endif
	if(!cpu || cpu->Query(ICORE_QUERY_DBG_PALETTE,&data)){
		data=NULL;
	}
#ifdef __WIN32__
#else
	if(!data){
		cairo_set_source_rgb(cr,0.0,0.0,0.0);
		cairo_rectangle(cr,0,0,w,h);
		cairo_fill (cr);
	}
	else{
		cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
		ca = cairo_create (surface);

		cairo_set_source_rgb(ca,0.0,0.0,0.0);
		cairo_rectangle(ca,0,0,w,h);
		cairo_fill (ca);

		int rw,rh,y,xs;

		rw= SR(w,4);
		rh=rw;
		y = h - (((((u32 *)data)[1] * rw) / w) * rh);
		if(y<0)
			y =0;
		xs = SR(w-(16*rw),1);

		for(int i=0,x=xs;i<((u32 *)data)[1];i++){
			u32 v = ((u32 *)data)[0x10+i];

			cairo_set_source_rgb(ca,((u8)SR(v,16))/255.0f,((u8)SR(v,8)) / 255.0f,((u8)v) / 255.0f);
			cairo_rectangle(ca,x,y,rw,rh);
			cairo_fill (ca);
			x+=rw;
			if(x>w-rw){
				y+=rh;
				if(y>h-rh)
					break;
				x=xs;
			}
		}
		bit=gdk_pixbuf_get_from_surface(surface,0,0,w,h);

		cairo_surface_destroy (surface);
		cairo_destroy (ca);
		gdk_cairo_set_source_pixbuf (cr, bit, 0,0);
		cairo_paint(cr);
		cairo_fill (cr);
	}
#endif
	if(data)
		free(data);
	if(bit)
		DeleteObject(bit);
	return 0;
}

int DebugWindow::UpdateLayersView(HWND win,HDC cr){
	u8 *data;
	HBITMAP bit;
	HDC ca;
	u32 params[10];

	data=NULL;
	bit=NULL;
#ifdef __WIN32__
#else
	int w = gtk_widget_get_allocated_width (win);
	int h = gtk_widget_get_allocated_height (win);

	data = (u8 *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(GetDlgItem(_window,RES(IDC_DEBUG_LAYERS_CB))));
	params[0] = data ? stoi((char *)data) : 0;
	params[1] = w;
	params[2] = h;
	HWND ww=GetDlgItem(_window,RES(CONCAT(IDC_DEBUG_DA1,0)));
	params[3]=gtk_range_get_value(GTK_RANGE(ww));
#endif
	{
		data=(u8 *)&params;
		if(!cpu || cpu->Query(ICORE_QUERY_DBG_LAYER,&data)){
			data=NULL;
		}
	}
#ifdef __WIN32__
#else
	if(!data){
		cairo_set_source_rgb(cr,0.0,0.0,0.0);
		cairo_rectangle(cr,0,0,w,h);
	}
	else{
		s32 rw,rh,wo,ho,xp,yp;
		u16 *p;

		cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
		ca = cairo_create (surface);

		cairo_set_source_rgb(ca,0.0,0.0,0.0);
		cairo_rectangle(ca,0,0,w,h);
		cairo_fill (ca);

		rw=rh=NFXD(1);

		wo=LOWORD(((u32 *)data)[1]);
		ho=HIWORD(((u32 *)data)[1]);
		if(ho < h || wo < w){
//			s32 n = h < w ? h : w;

			//h=w=n;
		}
	//	rh=h*4096/ho;
		//rw=w*4096/wo;
		xp=SR(NFXD(w)-wo*rw,1);
		yp=SR(NFXD(h)-ho*rh,1);
		xp=yp=0;
		p=(u16 *)&((u32 *)data)[10];

		for(int y=0;y<ho;y++){
			for(int x=0;x<wo;x++){

				u32 v=p[x+y*wo];
				cairo_set_source_rgb(ca,(v & 31)/31.0f,(SR(v,5)&31) / 31.0f,(SR(v,10)&31) / 31.0f);

				cairo_rectangle(ca,DFXD(xp+x*rh),DFXD(yp+y*rw),DFXD(rw),DFXD(rh));
				cairo_fill (ca);
			}
		}

		cairo_set_source_rgb(ca,1,1,1);
		cairo_set_line_width(ca,0.5);
	//	cairo_set_dash(ca,
	/*	for(int x=0;x<wo;x+=LOWORD(((u32 *)data)[4])){
			cairo_move_to(ca,xp+x,yp+0);
			cairo_line_to(ca,xp+x,yp+ho);
		}
		for(int y=0;y<ho;y+=HIWORD(((u32 *)data)[4])){
			cairo_move_to(ca,xp,yp+y);
			cairo_line_to(ca,xp+wo,yp+y);
		}*/
		cairo_stroke (ca);

		bit=gdk_pixbuf_get_from_surface(surface,0,0,w,h);

		cairo_surface_destroy (surface);
		cairo_destroy (ca);
		gdk_cairo_set_source_pixbuf (cr, bit, 0,0);
		cairo_paint(cr);
	}
	cairo_fill (cr);
#endif
	if(data) free(data);
	if(bit) DeleteObject(bit);
	return 0;
}

int DebugWindow::UpdateOAMView(HWND win,HDC cr){
	u8 *data;
	HBITMAP bit;
	HDC ca;
	int idx;

	data=NULL;
	bit=NULL;
#ifdef __WIN32__
#else
	int w = gtk_widget_get_allocated_width (win);
	int h = gtk_widget_get_allocated_height (win);

	idx =gtk_combo_box_get_active(GTK_COMBO_BOX(GetDlgItem(_window,RES(IDC_DEBUG_OAM_CB))));
#endif
	{
		data=(u8 *)&idx;
		if(!cpu || cpu->Query(ICORE_QUERY_DBG_OAM,&data)){
			data=NULL;
		}
	}
#ifdef __WIN32__
#else
	if(!data){
		cairo_set_source_rgb(cr,0.0,0.0,0.0);
		cairo_rectangle(cr,0,0,w,h);
	}
	else{
		int rw,rh,wo,ho,xp,yp;
		void *p;
		cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
		ca = cairo_create (surface);

		cairo_set_source_rgb(ca,0.0,0.0,0.0);
		cairo_rectangle(ca,0,0,w,h);
		cairo_fill (ca);

		rw=rh=2;
		wo=LOWORD(((u32 *)data)[0]);
		ho=HIWORD(((u32 *)data)[0]);
		xp=SR(w-wo*rw,1);
		yp=SR(h-ho*rh,1);
		p=(void *)&data[((u32 *)data)[1]];

		for(int y=0;y<ho;y++){
			for(int x=0;x<wo;x++){
				u32  v;
				float r,g,b;

				if(((u32 *)data)[2]){
					v=((u32 *)p)[x+y*wo];
					b=(u8)SR(v,16) / 255.0f;
					g=(u8)SR(v,8) / 255.0f;
					r=(u8)v / 255.0f;
				//	if(SR(v,24) != 255) r=g=b=0;
				}
				else{
					v=((u16 *)p)[x+y*wo];
					b=(v & 31) /31.0f;
					g=(SR(v,5)&31) / 31.0f;
					r=(SR(v,10)&31) /31.0f;
				}
				cairo_set_source_rgb(ca,r,g,b);
				cairo_rectangle(ca,xp+x*rh,yp+y*rw,rw,rh);
				cairo_fill (ca);
			}
		}

		bit=gdk_pixbuf_get_from_surface(surface,0,0,w,h);

		cairo_surface_destroy (surface);
		cairo_destroy (ca);
		gdk_cairo_set_source_pixbuf (cr, bit, 0,0);
		cairo_paint(cr);
	}
	cairo_fill (cr);
#endif
	if(data) free(data);
	if(bit) DeleteObject(bit);
	return 0;
}

DebugWindow::__memView::__memView(){
	iCurrentPage=-1;
	_view=NULL;
	_sb=NULL;
	_iPage=(u32)-1;
	_reset();
}

DebugWindow::__memView::~__memView(){
}

int DebugWindow::__memView::_reset(){
	size=sizeof(DEBUGGERDUMPINFO);
	_lastAccessAddress=-1;
	_dumpLength=0x1000;
	_dumpLines=3;
	iCurrentPage=-1;
#ifdef __WIN32__
#else
	if(_view && GTK_IS_WIDGET(_view)){
			GtkAllocation al;
		GtkTextAttributes *p;
		PangoContext *context;
		PangoFontMetrics *font_metrics;

		p=gtk_text_view_get_default_attributes(GTK_TEXT_VIEW(_view));
		gtk_widget_get_allocation(GTK_WIDGET(_view),&al);
		context = gtk_widget_get_pango_context(_view);
		font_metrics = pango_context_get_metrics(context,p->font,NULL);
		_charSz[1]=ceil((pango_font_metrics_get_descent(font_metrics) + pango_font_metrics_get_ascent(font_metrics)) / (float)PANGO_SCALE) +1;
		_charSz[0]=ceil(pango_font_metrics_get_approximate_digit_width(font_metrics) / (float)PANGO_SCALE) +1;
		_dumpLines=al.height/_charSz[1];
	}
#endif
	_switchPage(0);
	return 0;
}

int DebugWindow::__memView::_addPage(HWND win,u32 attr,HWND *wr,int *ir){
	HWND w,e;
	char c[40];
	int i;
	struct __mempage p;
#ifdef __WIN32__
#else
	e=gtk_frame_new(0);
	g_object_set_data(G_OBJECT(e),"type",(gpointer)(u64)2);
	g_object_set_data(G_OBJECT(e),"index",(gpointer)(u64)_items.size());
	gtk_widget_show_all(e);
	if(win){
		sprintf(c,"Memory %u",(u32)_items.size());
		w = gtk_label_new (c);
		i=gtk_notebook_insert_page(GTK_NOTEBOOK (win), e, w,_items.size()+1);
	}
#endif
	if(wr) *wr=e;
	if(ir) *ir=i;
	p._uEnd=KB(512);
	p._win=e;
	p._uMode=_dumpMode;
	_items.push_back(p);
	return  0;
}

int DebugWindow::__memView::_connect(u32 id,...){
	va_list  arg;

	va_start(arg,id);

	_id=id;
	_box=va_arg(arg,HWND);;
	_view=va_arg(arg,HWND);
	_sb=va_arg(arg,HWND);
	_xb=va_arg(arg,HWND);
	_mb=va_arg(arg,HWND);
#ifdef __WIN32__
#else
	if(_view){
		GtkAllocation al;
		GtkTextAttributes *p;
		PangoContext *context;
		PangoFontMetrics *font_metrics;

		p=gtk_text_view_get_default_attributes(GTK_TEXT_VIEW(_view));
		gtk_widget_get_allocation(GTK_WIDGET(_view),&al);
		context = gtk_widget_get_pango_context(_view);
		font_metrics = pango_context_get_metrics(context,p->font,NULL);
		_charSz[1]=ceil((pango_font_metrics_get_descent(font_metrics) + pango_font_metrics_get_ascent(font_metrics)) / (float)PANGO_SCALE) +1;
		_charSz[0]=ceil(pango_font_metrics_get_approximate_digit_width(font_metrics) / (float)PANGO_SCALE) +1;
		_dumpLines=al.height/_charSz[1];
	}
#endif
	va_end(arg);

	return 0;
}

int DebugWindow::__memView::_delBookmark(u32 a){
	if(iCurrentPage == -1 || iCurrentPage >= _items.size())
		return -1;
	if(a==-1)
		_items[iCurrentPage]._bookmark.clear();
	else{
		if(a >= _items[iCurrentPage]._bookmark.size()) return -2;
		_items[iCurrentPage]._bookmark.erase(_items[iCurrentPage]._bookmark.begin()+a);
	}
	return _update_bookmarks_menu();
}

int DebugWindow::__memView::_addBookmark(u64 a){
	if(iCurrentPage == -1 || iCurrentPage >= _items.size())
		return -1;
	for(auto it=_items[iCurrentPage]._bookmark.begin();it !=_items[iCurrentPage]._bookmark.end();it++){
		if(a==(*it))  return -2;
	}
	_items[iCurrentPage]._bookmark.push_back(a);
	_update_bookmarks_menu();
	return 0;
}

int DebugWindow::__memView::_setPageAddress(u64 a){
	int res;
	u32 d[10];

	res=-1;
	_dumpAddress=SL(SR(a,4),4);
	if(iCurrentPage == -1 || iCurrentPage >= _items.size())
		goto W;
	d[0]=a;
	*((u64 *)&d[1])=(u64)&d[3];
	if(!cpu->Query(ICORE_QUERY_ADDRESS_INFO,d)){
		_items[iCurrentPage]._uStart=d[3];
		_items[iCurrentPage]._uEnd=d[3]+d[4];
		_items[iCurrentPage]._uAdr=_dumpAddress;
		_adjustViewport();
	}
	res=0;
W:
	return res;
}

int DebugWindow::__memView::_getCurrentMode(u32 *r){
	if(!r) return -1;
	*r=0;
	if(iCurrentPage == -1 || iCurrentPage >= _items.size()) return 0;
	*r=_items[iCurrentPage]._uMode;
	return  0;
}

int DebugWindow::__memView::_setPageDumpMode(u32 a){
	_dumpMode=a;
	if(iCurrentPage == -1 || iCurrentPage >= _items.size())
		return 0;
	_items[iCurrentPage]._uMode=_dumpMode;
	return 0;
}

int DebugWindow::__memView::_switchPage(u32 i,HWND nbw){
	if(i>=_items.size())
		return -1;
	if(i==iCurrentPage)
		return 0;
	if(iCurrentPage!=-1){
		_items[iCurrentPage]._uAdr=_dumpAddress;
		_items[iCurrentPage]._uMode=_dumpMode;
	}
	iCurrentPage=i;
	_dumpAddress=_items[i]._uAdr;
	_dumpMode=_items[i]._uMode;
#ifdef __WIN32__
#else
	if(nbw && _box){
		//printf("type %x\n",(u32)(u64)g_object_get_data(G_OBJECT(nbw),"type"));
		if((u32)(u64)g_object_get_data(G_OBJECT(nbw),"type") == 2){
			HWND a;

			g_object_ref(_box);
			if((a=gtk_widget_get_parent(_box)))
				gtk_container_remove(GTK_CONTAINER(a),_box);
			gtk_container_add (GTK_CONTAINER (nbw),_box);
			g_object_unref(_box);
			if(_xb)
				gtk_widget_set_sensitive(_xb,i> 0);
		}
	}
#endif
	_adjustViewport();
	_update_bookmarks_menu();
	return  0;
}

int DebugWindow::__memView::_adjustViewport(){
#ifdef __WIN32__
#else
	if(!_sb || !GTK_IS_WIDGET(_sb)) return -1;
	GtkAdjustment *d=gtk_range_get_adjustment(GTK_RANGE(_sb));
	double v = _items[iCurrentPage]._uAdr-_items[iCurrentPage]._uStart;
	gtk_adjustment_configure(GTK_ADJUSTMENT(d),v,0,_items[iCurrentPage]._uEnd-_items[iCurrentPage]._uStart,16,16,16*_dumpLines);
#endif
	return 0;
}

int DebugWindow::__memView::_close(int i){
	if(i==-1)
		i=iCurrentPage;
	if(i >=_items.size() || i < 0)
		return -1;
	if(_box)
		_switchPage(0,_items[0]._win);
	_items.erase(_items.begin()+i);
	return 0;
}

int DebugWindow::__memView::_onScroll(u32 id,HWND w,GtkScrollType t){
	double d[10];
#ifdef __WIN32__
#else
	GtkAdjustment *control;


	if((control = gtk_range_get_adjustment(GTK_RANGE(w))) == NULL)
		return -1;
	g_object_get((gpointer)control,"lower",&d[0],"upper",&d[1],"value",&d[2],"page-increment",&d[3],"step-increment",&d[4],"page-size",&d[5],NULL);
#endif
	_dumpAddress=iCurrentPage != -1 ? _items[iCurrentPage]._uStart:0;
	_dumpAddress +=  floor(d[2]/16.0f) *16;
	return 0;;
}

int DebugWindow::__memView::_update_bookmarks_menu(){
	HMENU m;
	HWND mi;
	u32 i;

	if(!_mb) return -1;
#ifdef __WIN32__
#else
	if(!(m=(HMENU)gtk_menu_button_get_popup(GTK_MENU_BUTTON(_mb)))){
		m=(HMENU)gtk_menu_new();
	}
	GList *children = gtk_container_get_children(GTK_CONTAINER(m));
	for(;children;children = g_list_next(children))
		gtk_container_remove(GTK_CONTAINER(m),GTK_WIDGET(children->data));
	i=0x440;
	for(auto it=_items[iCurrentPage]._bookmark.begin();it !=_items[iCurrentPage]._bookmark.end();it++){
		char c[20];

		sprintf(c,"%08X",(u32)(*it));
		mi=gtk_menu_item_new_with_label(c);
		gtk_menu_shell_append(GTK_MENU_SHELL(m),mi);
		gtk_widget_set_name(mi,MAKELONG(_id,i++));
		g_signal_connect (mi, "activate", G_CALLBACK (::on_menuitem_select), NULL);
	}
	mi=gtk_menu_item_new_with_label("Add...");
	gtk_widget_set_name(mi,MAKELONG(_id,0x400));
	g_signal_connect (mi, "activate", G_CALLBACK (::on_menuitem_select), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(m),mi);
	mi=gtk_menu_item_new_with_label("Delete all");
	gtk_widget_set_name(mi,MAKELONG(_id,0x401));
	g_signal_connect (mi, "activate", G_CALLBACK (::on_menuitem_select), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(m),mi);
	gtk_widget_show_all(GTK_WIDGET(m));
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(_mb),GTK_WIDGET(m));
#endif
	return 0;
}