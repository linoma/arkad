#include "arkad.h"
#include "debugger.h"
#include "utils.h"
#include "game.h"

#ifndef __GUIH__
#define __GUIH__

class GUI : public DebugWindow,private GameManager{
public:
	GUI();
	virtual ~GUI();
	int Init();
	void Loop();
	void Quit(){BS(_status,S_QUIT);};
	int LoadBinary(char *);
	virtual int OnPaint(u32,GtkWidget *,cairo_t *);
	HWND _getDisplay(){return GetDlgItem(_window,STR(1001));};
	int OnKeyDown(GtkWidget *w,int key,u32);
	int OnKeyUp(GtkWidget *w,int key,u32);
	int ChangeCore(int c);
	void OnMoveResize(u32 id,GtkWidget *w,GdkEventConfigure *e);
	void OnButtonClicked(u32 id,GtkWidget *w);
	void  OnCommand(u32 id,u32 c,GtkWidget *w);
	static BOOL ShowOpen(char *caption,char *lpstrFilter,HWND hWndParent,char *fileName,int nMaxFile,DWORD dwFlags);
	static int MessageBox(char *caption,char *message,HWND hWndParent,DWORD dwFlags);
	int OnLDblClk(u32 id,GtkWidget *w);
	void OnLButtonDown(u32 id,GtkWidget *w);
	void OnRButtonDown(u32 id,GtkWidget *w);
	void OnScroll(u32,GtkWidget *,GtkScrollType);
	void OnPopupMenuInit(u32 id,GtkMenuShell *menu);
	void OnMenuItemSelect(u32 id,GtkMenuItem *item);
	int OnCloseWWindow(u32,GtkWidget *);
protected:
	static gint OnKeyEvent(GtkWidget *w,GdkEventKey *event,gpointer func_data);
	int Load(u32,char *);
private:
	guint key_snooper;
	GtkWidget *_window;
	u16 _keys[20];
};

#endif