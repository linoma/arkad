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
	int height;

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
#endif

#endif
	return 0;
}

void DebugWindow::Update(){
#ifdef _DEBUG
	int len,nn;
	char *p,*pp;
	HWND win;

	if(!_window || !cpu)
		return;
	//return;
	p=NULL;
	len=cpu->Dump(&p);
	//p=NULL;
	if(!p || len < 1)
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
		char c[30];

		HWND w=NULL;

		if(!cpu->Query(ICORE_QUERY_BREAKPOINT_ENUM,&bp) && bp){
			pp=(char *)bp;
			for(;*bp;bp++){
				HWND e;
				char s='*',ec[][3]={"!=","==","<",">"};

				if(!w){
					w = gtk_menu_tool_button_get_menu(GTK_MENU_TOOL_BUTTON(GetDlgItem(_window,RES(5517))));
					if(w){
						GList *children = gtk_container_get_children(GTK_CONTAINER(w));
						for(int i=0;children;children = g_list_next(children),i++){
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

					sprintf(c,"%c M 0x%X %s %s %s",s,(u32)*bp,s__[SR(*bp,38) & 3],s_[SR(*bp,60)&3],ec[SR(*bp,35) & 7]);
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

		sprintf(c,"LA: %x",_lastAccessAddress);
		gtk_label_set_text(GTK_LABEL(win),c);
	}
	win= GetDlgItem(_window,RES(7100));
	if(win){
		char c[20];

		sprintf(c,"EA: %x",_editAddress);
		gtk_label_set_text(GTK_LABEL(win),c);
	}
#endif
	DebugLog::Update();
	DebugWindow::_updateToolbar();
	if(p)
		delete []p;
#endif
}

int DebugWindow::_updateAddressInfo(u32 _pc){
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
		return 21;
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
					if(d==_pc){
						ShowToastMessage(&c[n]);
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
A:
#endif

#endif
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
A:
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

HTREEITEM DebugWindow::AddListItem (HWND listbox, char *sText){
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
	u32 i;

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

void DebugWindow::OnKeyScroolEvent(int v){//fixme
	HWND w;
	double d[6];
#ifdef __WIN32__
#else
	GtkAdjustment *control;
	int i;

	w=GetDlgItem(_window,RES(IDC_DEBUG_SB));
	if((control = gtk_range_get_adjustment((GtkRange *)w)) == NULL)
		return;
	i=1;
	if(v<0){
		i*=-1;
		v*=-1;
	}

	g_object_get((gpointer)control,"lower",&d[0],"upper",&d[1],"value",&d[2],"page-increment",&d[3],"step-increment",&d[4],NULL);
	d[5]=(d[2]+d[v]*i);
	//printf("kd %x %u %u %x \n",(u32)d[2],(u32)d[3],(u32)d[4],(u32)(d[5]+_views[DBV_VIEW].uAdr));
	//d[5]+=_startAddress;
	//gtk_range_set_value(GTK_RANGE(w),d[5]);
	//g_signal_emit_by_name((gpointer)w,"change-value",0,d[5],&d[5]);
#endif
	_adjustCodeViewport((u32)d[5]+_views[DBV_VIEW].uAdr,DBV_VIEW);
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
	switch(key){
		case GDK_KEY_Page_Down:
			OnKeyScroolEvent(3);
		break;
		case GDK_KEY_Page_Up:
			OnKeyScroolEvent(-3);
		break;
		case GDK_KEY_Down:
			OnKeyScroolEvent(4);
		break;
		case GDK_KEY_Up:
			OnKeyScroolEvent(-4);
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
						int i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (w))-3;
						gtk_notebook_set_current_page(GTK_NOTEBOOK(w),i);
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
				BC(_status,S_DEBUG_NEXT);
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
			_pc=0x10000002;
			DebugWindow::_updateToolbar();
		break;
		case 2:
			BC(_status,S_PAUSE);
			DebugWindow::_updateToolbar();
		break;
		case 3:{//next   F4
			u32 n;

			if(!BT(_status,S_DEBUG))
				return -1;
			BS(_status,S_DEBUG_NEXT);
			n=(u32)2;
			if(cpu->Query(ICORE_QUERY_NEXT_STEP,&n)){
				u32 i;

				n=2;
				//cpu->Query(ICORE_QUERY_PC,&i);
				//n+=i;
			}
			_pc = (u32)n;
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
					_pc = 0x80000000|(u32)i;
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
				u32 i,n;

				cpu->Query(ICORE_QUERY_PC,&i);//25aa4
				n=(u32)1;
				if(cpu->Query(ICORE_QUERY_NEXT_STEP,&n))
					n=2;
				i+=n;//fixme
				_pc = 0x80000000|(u32)i;
			}
			DebugWindow::_updateToolbar();
		break;
	}
#endif
	return 0;
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
			DebugMode(4);
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
				double d;
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
			if(!LoadBreakpoints())
				Update();
		break;
		default:
			if(id>=0x10000 && id < 0x19000){
				HWND ww;
				char c[20],*p;

				sprintf(c,"%d",id+0x10000);
#ifdef __WIN32__
#else
				if((ww=GetDlgItem(_window,c)) && (p=(char *)gtk_entry_get_text(GTK_ENTRY(ww)))){
					u32 u;

					sscanf(p,"%x",&u);
					if(!cpu->Query(ICORE_QUERY_DBG_DUMP_ADDRESS,(void *)(u64)u))
						Update();
				}
#endif
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

	if((control = gtk_range_get_adjustment((GtkRange *)w)) == NULL)
		return;
	g_object_get((gpointer)control,"lower",&d,"upper",&d1,"value",&d2,"page-increment",&d3,NULL);
	//printf("sb %.2f %.2f %.2f\n",d,d1,d2);
#endif
	_adjustCodeViewport((u32)d2+_views[DBV_VIEW].uAdr,DBV_VIEW);
#endif
}

int DebugWindow::Reset(){
	DebugLog::Reset();
	ShowToastMessage(0);
	_pc=0;
	memset(&_views,0,sizeof(_views));
	_iCurrentView=DBV_VIEW;
	_startAddress=_endAddress=0;
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

	*((u64 *)c)=0;
	if(fn)
		strcpy(c,fn);
	strcat(c,".bk");
	if(!cpu || !(fp=fopen(c,"rb")))
		return -1;
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

	*((u64 *)c)=0;
	if(fn)
		strcpy(c,fn);
	strcat(c,".bk");

	if(!cpu || !(fp=fopen(c,"wb")))
		return -1;
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
A:
	fclose(fp);
#endif
	return res;
}

int DebugWindow::Loop(){
	int ret=0;
#ifdef _DEBUG
	if(BT(_status,S_DEBUG_NEXT)){
		switch(SR(_pc,31)){
			case 0:{
					u32 i=_pc&0xfffffff;
					if(--i == 0){
						switch(SR(_pc&0x70000000,28)){
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
					_pc = (_pc & 0xF0000000)|i;
				}
				ret=0;
			break;
			case 1:{
				u32 __pc,i=_pc&0x7fffffff;

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
A:
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
	int i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (win))-1;
	for(;i>0;i--){
		//GtkWidget *p=gtk_notebook_get_nth_page(GTK_NOTEBOOK (win),0);
		gtk_notebook_remove_page(GTK_NOTEBOOK (win),0);
		//	if(p)
		//	gtk_widget_destroy(p);
	}
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_TAB1))))
		goto B;
	i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (win))-5;
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

	if(!_window || !cpu || cpu->Query(ICORE_QUERY_CPUS,&s) || !s)
		goto B;
	if(!(win=GetDlgItem(_window,RES(IDC_DEBUG_TAB1))))
		goto B;

	for(char *ss=s,i=0;*ss;i++){
		char *c=ss;
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
			default:
				a = gtk_menu_item_new_with_label (c);
			break;
		}
		gtk_widget_set_name(a,MAKELONG(IDC_DEBUG_MENUBAR,(v[0]&0xfff)|SL(v[1] & 7,12)|0x8000));
		gtk_menu_shell_append (GTK_MENU_SHELL (e), a);
		gtk_widget_show(a);
		g_signal_connect (a, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		switch((u8)SR(v[1],8)){
			case 1:
				a=GetDlgItem(_window,RES(IDC_DEBUG_LAYERS_CB));
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(a),NULL,c);
			break;
		}
	}
#endif
	delete []s;
A:
	if(!cpu || cpu->Query(ICORE_QUERY_DBG_PAGE,&p) || !p)
		return -1;
#ifdef __WIN32__
#else
	win=GetDlgItem(_window,RES(IDC_DEBUG_TAB));
	for(LPDEBUGGERPAGE pp=p;pp->size;pp++){
		sscanf(pp->name,"%d",&id);
		e=0;
		switch(pp->type){
			case 2:
				{
					char c[30];

					GtkWidget *b=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
					GtkWidget *a=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
					e=gtk_entry_new();
					sprintf(c,"%d",id+0x20000);
					gtk_widget_set_name(e,c);
					gtk_container_add (GTK_CONTAINER (a),e);

					e=gtk_button_new_from_icon_name("gtk-jump-to",GTK_ICON_SIZE_MENU);
					sprintf(c,"%d",id+0x10000),
					gtk_widget_set_name(e,c);
					g_signal_connect(G_OBJECT(e),"clicked",G_CALLBACK(on_button_clicked),0);

					gtk_container_add (GTK_CONTAINER (a),e);
					e=gtk_combo_box_text_new();
					gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"8 bits");
					gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"16 bits");
					gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"32 bits");
					gtk_combo_box_set_active(GTK_COMBO_BOX(e),0);
					sprintf(c,"%d",id+0x30000),
					gtk_widget_set_name(e,c);
					g_signal_connect(G_OBJECT(e),"changed",G_CALLBACK(::on_command),0);
					gtk_container_add (GTK_CONTAINER (a),e);
					gtk_box_pack_start(GTK_BOX(b),(HWND)a,false,false,0);

					a=gtk_viewport_new(NULL,NULL);
					e=gtk_text_view_new();
					gtk_text_view_set_monospace(GTK_TEXT_VIEW(e),true);
					gtk_text_view_set_editable(GTK_TEXT_VIEW(e),false);
					gtk_widget_set_name(e,pp->name);
					//
					GtkTextBuffer  *bu = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e));
					gtk_text_buffer_create_tag(bu, "u","foreground", "red", NULL);
					//gtk_text_buffer_create_tag(bu, "n","background", "white", NULL);

					g_object_set_data(G_OBJECT(e),"id",(gpointer)(u64)id);
					if(pp->popup)
						g_signal_connect(G_OBJECT(e),"populate-popup",G_CALLBACK(on_menu_init),0);
					//if(pp->editable)
						//g_signal_connect(G_OBJECT(e),"populate-popup",G_CALLBACK(on_menu_init),0);
					if(pp->clickable)
						g_signal_connect(G_OBJECT(e), "button-press-event", G_CALLBACK(::on_mouse_down), NULL);
					gtk_container_add (GTK_CONTAINER (a),e);
					e = gtk_scrolled_window_new(NULL,NULL);
					gtk_container_add (GTK_CONTAINER (e),a);

					gtk_box_pack_start(GTK_BOX(b),(HWND)e,TRUE,TRUE,0);

					e=b;
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
		if(e){
			w = gtk_label_new (pp->title);
			int i = gtk_notebook_get_n_pages(GTK_NOTEBOOK (win));
			gtk_notebook_insert_page(GTK_NOTEBOOK (win), e, w,i-1);
			//gtk_notebook_prepend_page(GTK_NOTEBOOK (win), e, w);
		}
	}
	gtk_widget_show_all(win);
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

#endif
	return 0;
}

void DebugWindow::OnCommand(u32 id,u32 type,HWND w){
#ifdef _DEBUG

#ifdef __WIN32__
#else
	switch(id){
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
		default:
			//case 3102:
			if(id>0x30000){
				int i=gtk_combo_box_get_active(GTK_COMBO_BOX(w));
				if(!cpu->Query(ICORE_QUERY_DBG_DUMP_FORMAT,(void *)(u64)(i*2)))
					Update();
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
	if(cpu->Query(ICORE_QUERY_CPU_INTERFACE,&p))
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
			u32 adr;

			//GetAddressFromSelectedItem(ww,&adr);
			//d[0]=GetSelectedListItemIndex(w);

			w = gtk_menu_item_new_with_label ("Copy");
			gtk_widget_set_name(w,MAKELONG(IDC_DEBUG_LB_BP,8));
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
			gtk_widget_set_name(w,MAKELONG(3105,5));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
			gtk_widget_show(w);
			g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);
		break;
		case 3102:
			if(_editAddress){
				w = gtk_menu_item_new_with_label ("Change...");
				gtk_widget_set_name(w,MAKELONG(id,5));
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
				gtk_widget_show(w);
				g_signal_connect (w, "activate", G_CALLBACK (::on_menuitem_select), NULL);

				w = gtk_menu_item_new_with_label ("...to breakpoint");
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
				case 4:
				{
					if(!GetAddressFromSelectedItem(GetDlgItem(_window,RES(IDC_DEBUG_LB_BP)),d)){
						_adjustCodeViewport(*d,DBV_VIEW);
						gtk_notebook_set_current_page(GTK_NOTEBOOK(GetDlgItem(_window,RES(IDC_DEBUG_TAB1))),0);
					}
				}
				break;
				case 5:
				{
					u32 d[2];

					d[0]=GetSelectedListItemIndex(w);
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_BREAKPOINT_RESET,d))
						Update();
				}
				break;
				case 6:
				{
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
				case 8:
				{
					if(!GetAddressFromSelectedItem(GetDlgItem(_window,RES(IDC_DEBUG_LB_BP)),d)){
						char s[20];

						sprintf(s,"%x",*d);
						gtk_clipboard_set_text(gtk_widget_get_clipboard(_window,GDK_SELECTION_CLIPBOARD),(gchar *)s,-1);
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
							cpu->Query(ICORE_QUERY_BREAKPOINT_SET,d);
							Update();
						}
					}
				}
				break;
				default:
				{
					u32 d[2];

					d[0]=GetSelectedListItemIndex(w);
					d[1]=(u32)-1;
					if(!cpu->Query(ICORE_QUERY_SET_BREAKPOINT_STATE,d))
						Update();
				}
				break;
			}
		break;
		case 3105:
		case 3102:
			switch(SR(id,16)){
				case 6:{
						d[0]=_editAddress;
						d[1]=0x80008007;
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

					d[0]=_editAddress;
					d[1]=0;
					d[2]=SR(id,16)-8;
					d[3]=id;
					d[4]=10;
					cpu->Query(ICORE_QUERY_SET_LOCATION,d);
					Update();
				}
				break;
				case 7:{
					c = new char[1000];
					*((u64 *)d)=(u64)c;
					if(!ChangeLocationBox((char *)"Find...",d,2)){
						SearchWord(c);
					}
					delete []c;
				}
				break;
				default:{
					d[0]=_editAddress;
					d[1]=0;
					d[2]=0;
					d[3]=id;
					//printf("de %x\n",id);
					if(!ChangeLocationBox((char *)"Change Memory...",d)){
						cpu->Query(ICORE_QUERY_SET_LOCATION,d);
						Update();
					}
				}
		}
		break;
		default:
			b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
			gtk_text_buffer_get_selection_bounds(b,&ss,&se);
			c=gtk_text_buffer_get_text(b,&ss,&se,FALSE);
			d[0]=-1;
			if(sscanf(c,"%c%d",s,&d[0]) != 2)
				*((void **)&d[2])=c;
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
		}
#endif

#endif
}

int DebugWindow::OnLDblClk(u32 id,HWND w){
	int res=-1;
#ifdef _DEBUG
	u32 adr,d[3];
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
				if(*p)
					gtk_clipboard_set_text(gtk_widget_get_clipboard(_window,GDK_SELECTION_CLIPBOARD),p,-1);
			}
		}
			return 0;
		break;
		case IDC_DEBUG_LB_DIS:
		{
			gdouble dx,dy;
			u32 adr;

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
	_editAddress=adr;
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
	HWND dialog,vb,e;
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

	e=gtk_text_view_new();
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(e),true);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(e),false);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(e),false);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(e),5);
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(e),5);
	{
		GtkTextBuffer *x=gtk_text_view_get_buffer(GTK_TEXT_VIEW(e));
		sprintf(c,"%08X",(u32)*bp);
		gtk_text_buffer_set_text(x,c,-1);
	}
	gtk_widget_set_name(e,1);
	gtk_container_add (GTK_CONTAINER (vb),e);


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
		i=SR(bd&0x30000000,28);
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
	}

	e=gtk_combo_box_text_new();
	gtk_widget_set_name(e,4);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Register");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(e),NULL,"Value");
	i=bd&0x40000000 ? 1 : 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(e),i);
	gtk_container_add (GTK_CONTAINER (vb),e);

	e=gtk_label_new("Counter");
	gtk_container_add (GTK_CONTAINER (vb),e);

	e=gtk_entry_new();
	gtk_widget_set_name(e,100);//first value

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
		if(bd&0x8000){//complex
			i = SR(bd,3) & 0x1f;
			sprintf(c,"r%d",i);
		}
		else{
			i=(int)(bd&0x7fff);
			sprintf(c,"%x",i);
		}
		i=bd&7;
	}
	b=gtk_entry_get_buffer(GTK_ENTRY(e));
	gtk_entry_buffer_set_text(b,c,-1);

	gtk_container_add (GTK_CONTAINER (vb),e);

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

	e = GetDlgItem(dialog,RES(4));
	i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
	bkv= SL((u64)i,62)|(bkv & 0xBFFFFFFFFFFFFFFFULL);

	if(ISMEMORYBREAKPOINT(bkv)){
		e = GetDlgItem(dialog,RES(3));
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
		bkv = SL((u64)i,60)|(bkv & 0xCFFFFFFFFFFFFFFF);

		e = GetDlgItem(dialog,RES(5));
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
		bkv = SL((u64)i,38)|(bkv & 0xFFFFFF3FFFFFFFFF);

		e = GetDlgItem(dialog,RES(4));
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));

		e = GetDlgItem(dialog,RES(200));
		b=gtk_entry_get_buffer(GTK_ENTRY(e));
		p = (char *)gtk_entry_buffer_get_text(b);
		sscanf(p,"%x",&i);

		e = GetDlgItem(dialog,RES(11));//condition
		i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
		//i=7;
		bkv = (bkv & ~0x3800000000ULL)|SL((u64)i,35);
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
			bkv = (bkv & ~0x40007f0000000000ULL)|SL((u64)(i&0x7f),40);;
			e = GetDlgItem(dialog,RES(4));
			if(gtk_combo_box_get_active(GTK_COMBO_BOX(e))){
				bkv |= 0x4000000000000000ULL;
			}

			e = GetDlgItem(dialog,RES(11));//condition
			i=gtk_combo_box_get_active(GTK_COMBO_BOX(e));
			bkv= SL((u64)i,32)|(bkv & ~0x700000000ULL);
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
	u32 d[4];

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
	DebugMode(1,0,d[3]);
	ShowToastMessage((char *)"Memory breakpoint reached.stopped");
A:
	if(!BT(_status,S_PAUSE))
		goto B;
	_lastAccessAddress=adr;
	cpu->Query(ICORE_QUERY_SET_LAST_ADDRESS,d);
	//printf("%x %x\n",adr,flags);
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
	d[0]=_editAddress;
	*((u64 *)&d[1])=(u64)&d[3];
	cpu->Query(ICORE_QUERY_ADDRESS_INFO,d);

	*((u64 *)dd)=(u64)c;
	dd[2]=_editAddress;
	dd[3]=d[4]-(_editAddress-d[3]);

	//printf("lino %x %x\n",dd[2],dd[3]);
	if(!cpu->Query(ICORE_QUERY_MEMORY_FIND,dd)){
		_editAddress=dd[2];
		if(!cpu->Query(ICORE_QUERY_DBG_DUMP_ADDRESS,(void *)(u64)_editAddress)){
			_editAddress+=i;
			Update();
		}
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

	char *pp,c[30];
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
		case IDC_DEBUG_DA1:{
#ifdef __WIN32__
#else
		int width = gtk_widget_get_allocated_width (w);
		int height = gtk_widget_get_allocated_height (w);

		cairo_set_source_rgb(cr,0.0,0.0,0.0);
		cairo_rectangle(cr,0,0,width,height);
		cairo_fill (cr);
#endif
		}
		break;
		default:
			printf("%s %d\n",__FUNCTION__,id);
		break;
	}
	return 0;
}