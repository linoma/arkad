#include "arkad.h"
#include "debugger.h"
#include "utils.h"
#include "game.h"
#include "settings.h"
#include "savestate.h"

#ifndef __GUIH__
#define __GUIH__

class GUI : public DebugWindow,private GameManager,public Settings,SaveState{
public:
	GUI();
	virtual ~GUI();
	int Init();
	void Loop();
	void Quit(){BS(_status,S_QUIT);};
	void PushStatusMessage(char *);
	int LoadBinary(char *);
	virtual int OnPaint(u32,HWND ,HDC);
	virtual int Reset();
#ifdef __WIN32__
	I_INLINE HWND _getDisplay(){return _getWindow();};
	HDC _getDC(u32 flags=0);
#else
	I_INLINE HWND _getDisplay(){return _windowDrawArea;};
#endif
	I_INLINE HWND _getWindow(){return _window;};

	int OnKeyDown(HWND w,int key,u32);
	int OnKeyUp(HWND w,int key,u32);
	int ChangeCore(int c);
	void OnMoveResize(u32 id,HWND w,int wi,int he);
	void OnButtonClicked(u32 id,HWND w);
	void  OnCommand(u32 id,u32 c,HWND w);
	static BOOL ShowOpen(char *caption,char *lpstrFilter,HWND hWndParent,char *fileName,int nMaxFile,DWORD dwFlags);
	static int MessageBox(char *caption,char *message,HWND hWndParent,DWORD dwFlags);
	int OnLDblClk(u32 id,HWND w);
	void OnLButtonDown(u32 id,HWND w);
	void OnRButtonDown(u32 id,HWND w);
	void OnMouseMove(u32,HWND,u32,int,int);
	void OnScroll(u32,HWND ,GtkScrollType);
	void OnPopupMenuInit(u32 id,HMENU menu);
	void OnMenuItemSelect(u32 id,HMENUITEM item);
	int OnCloseWindow(u32,HWND);
	int GetClientSize(LPRECT);
	int LoadGameState(char *);
	int SaveGameState(char *);

	using Settings::operator [];
	static void *_logo;
protected:
	int Load(u32,char *);
	virtual int _updateToolbar();
private:
#ifdef __WIN32__
	ATOM classId;
	HWND hwndStatus,hwndTool;
	HIMAGELIST hImageList[2];
	HDC hDC;

	static gint OnKeyEvent(HWND w,WPARAM,LPARAM);
	static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK AboutDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#else
	static gboolean thread1_worker(gpointer data);
	static gint OnKeyEvent(HWND w,GdkEventKey *event,gpointer func_data);
#endif
	int _resize_drawing_area(int &w,int &h);
	guint key_snooper;
	HWND _window,_windowDrawArea;
	u16 _keys[50];
	struct{
		int x,y,ox,oy,oxd,oyd;
	} _mouse;
};

#endif