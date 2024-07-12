#include "arkad.h"
#include "utils.h"
#include "debuglog.h"

#ifndef __DEBUGWINDOWH__
#define __DEBUGWINDOWH__

#define DBV_RUN    		0
#define DBV_VIEW   		1
#define DBV_NULL   		0xFF

typedef struct{
	u32 uStart,uEnd,uSize,uAdr;
} DISVIEW,*LPDISVIEW;

class DebugWindow : public DebugLog{
public:
	DebugWindow();
	virtual ~DebugWindow();
	int Init(HWND w);
	void Update();
	int DebugMode(int m,u64 v=0,u64 vv=0);
	void OnMoveResize(u32 id,HWND,int w,int h);
	void OnScroll(u32,HWND w,GtkScrollType t);
	int OnKeyDown(int key,u32);
	int OnMemoryUpdate(u32 adr,u32 flags,ICore *core=NULL);
	int SearchWord(char *c=NULL);
	void TimerThread();
	void Threaad1Update();
	int ShowToastMessage(char *);
	int OnPaint(u32 id,HWND w,HDC cr);
protected:
	int _create_treeview_model(HWND);
	HTREEITEM AddListItem (HWND listbox, char *sText);
	HTREEITEM GetListItem(HWND w,u32 n);
	HTREEITEM GetSelectedListItem(HWND w);

	u32 GetSelectedListItemIndex(HWND w);
	int OnChangeCpu(u32);
	int UpdatePaletteView(HWND,HDC);
	int UpdateLayersView(HWND,HDC);
	int UpdateOAMView(HWND,HDC);
	void _calc_lb_item_height(HWND win);
	int Loop();
	void OnButtonClicked(u32 id,HWND w);
	void OnPopupMenuInit(u32 id,HMENU menu);
	void OnMenuItemSelect(u32 id,HMENUITEM item);
	void OnRButtonDown(u32 id,HWND w);
	void OnLButtonDown(u32 id,HWND w);
	int OnLDblClk(u32 id,HWND w);
	virtual void OnCommand(u32 id,u32 c,HWND w);
	virtual int Reset();
	void _fillCodeWindow(u32 _pc,int cv);
	void OnKeyScrollEvent(int v,u32 id=0,HWND win=0);
	void _adjustScrollViewport(int,HWND,double *r=0,u32 attr=1);
	int GetAddressFromSelectedItem(HWND ,u32 *adr);
	HTREEITEM GetListItemFromAddress(HWND w,u32 n);
	void _adjustCodeViewport(u32 a,int v=-1);
	void _invalidateView(int v=-1);
	virtual int OnDisableCore();
	virtual int OnEnableCore();
	int ChangeLocationBox(char *title,u32 *d,u32 flags=0);
	int ChangeBreakpointBox(char *title,u64 *bp,u32 flags=0);
	int SaveBreakpoints(char *fn=NULL);
	int LoadBreakpoints(char *fn=NULL);
	int SaveNotes(char *fn=NULL);
	int LoadNotes(char *fn=NULL);
	int AddCommandQueue(u64 cmd);
	int _updateAddressInfo(u32 _pc);
	virtual int _updateToolbar();
	int _getNoteFromAddress(u32,char **);
	static void *thread1_func(void *data){
		((DebugWindow *)data)->TimerThread();
		return NULL;
	}
private:
	static gboolean thread1_worker(gpointer data){
		((DebugWindow *)data)->Threaad1Update();
		return FALSE;
	};

	struct __mempage{
		u32 _uMode;
		u64 _uStart,_uEnd,_uAdr;
		HWND _win;
		vector<u64> _bookmark;

		__mempage(){
			_uStart=_uEnd=_uAdr=0;
			_uMode=0;
		};
		__mempage(u64 a,u64 b){
			_uStart=a;
			_uEnd=b;
			_uAdr=a;
			_uMode=0;
		};

		int _reset(){
			_uAdr=_uStart;
			_bookmark.clear();
			return 0;
		};
	};

	struct __memView : DEBUGGERDUMPINFO{
		vector<struct __mempage> _items;
		u32 iCurrentPage,_id,_iPage,_charSz[2];
		HWND _box,_view,_sb,_xb,_mb;

		__memView();
		~__memView();
		int _reset();
		int _addPage(HWND win,u32 attr=0,HWND *wr=0,int *ir=0);
		int _switchPage(u32,HWND nbw=0);
		int _addBookmark(u64);
		int _delBookmark(u32 a=-1);
		int _connect(u32 id,...);
		int _onScroll(u32 id,HWND w,GtkScrollType t);
		int _close(int i=-1);
		int _setPageAddress(u64);
		int _setPageDumpMode(u32);
		int _getCurrentMode(u32 *);
		protected:
			int _update_bookmarks_menu();
			int _adjustViewport();
	} _memPages;

	HWND _window;
	pthread_t thread1;
	int _itemHeight,_nItems,_iCurrentView;
	u64 _pc;
	CRITICAL_SECTION _cr;
	DISVIEW _views[2];
	char *_searchWord;
	vector<u64> _quCmd;
	HBITMAP _bit;
};
#endif