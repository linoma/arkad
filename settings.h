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
	std::map<string,string> _items;
};

#endif