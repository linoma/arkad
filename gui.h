#include "arkad.h"
#include "debugger.h"
#include "utils.h"
#include "game.h"
#include "settings.h"

#ifndef __GUIH__
#define __GUIH__

struct __setting_value;

class GUI : public DebugWindow,private GameManager,public Settings{
public:
	GUI();
	virtual ~GUI();
	int Init();
	void Loop();
	void Quit(){BS(_status,S_QUIT);};
	void PushStatusMessage(char *);
	int LoadBinary(char *);
	virtual int OnPaint(u32,HWND ,HDC);
	HWND _getDisplay(){return GetDlgItem(_window,STR(1001));};
	HWND _getWindow(){return _window;};

	int OnKeyDown(HWND w,int key,u32);
	int OnKeyUp(HWND w,int key,u32);
	int ChangeCore(int c);
	void OnMoveResize(u32 id,HWND w,GdkEventConfigure *e);
	void OnButtonClicked(u32 id,HWND w);
	void  OnCommand(u32 id,u32 c,HWND w);
	static BOOL ShowOpen(char *caption,char *lpstrFilter,HWND hWndParent,char *fileName,int nMaxFile,DWORD dwFlags);
	static int MessageBox(char *caption,char *message,HWND hWndParent,DWORD dwFlags);
	int OnLDblClk(u32 id,HWND w);
	void OnLButtonDown(u32 id,HWND w);
	void OnRButtonDown(u32 id,HWND w);
	void OnScroll(u32,HWND ,GtkScrollType);
	void OnPopupMenuInit(u32 id,GtkMenuShell *menu);
	void OnMenuItemSelect(u32 id,GtkMenuItem *item);
	int OnCloseWWindow(u32,HWND );

	using Settings::operator [];

protected:
	static gint OnKeyEvent(HWND w,GdkEventKey *event,gpointer func_data);
	int Load(u32,char *);
	virtual int _updateToolbar();
private:
	static gboolean thread1_worker(gpointer data);

	guint key_snooper;
	HWND _window;
	u16 _keys[20];
};

#endif