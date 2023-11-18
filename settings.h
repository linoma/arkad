#include "arkad.h"
#include <string>
#include <map>

#ifndef __SETTINGSH__
#define __SETTINGSH__

using namespace std;

class Settings{
public:
	Settings();
	virtual ~Settings();
	virtual std::string & operator[] (const std::string& k);
	virtual int Load(char *f=NULL);
	virtual int Save(char *f=NULL);
	virtual int Show(HWND);
protected:
#ifdef __WIN32__
	static BOOL CALLBACK DlgProc16(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static int CALLBACK PropSheetProc(HWND hwndDlg,UINT uMsg,LPARAM lParam);

	static HWND hwndPropertySheet;
#endif
	std::map<string,string> _items;
};

#endif