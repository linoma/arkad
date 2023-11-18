#include "arkad.h"
#include "utils.h"
#include <vector>
#include <string>

using namespace std;

#ifndef __DEBUGLOGH__
#define __DEBUGLOGH__

#define DEBUGLOG_SIZE	4000

class DebugLog{
public:
	DebugLog();
	virtual ~DebugLog();
	int putf(int level,const char *fmt,va_list ptr);
	int putf(int level,const char *fmt,...);
	virtual int Reset();
	virtual int Init(HWND w);
	virtual void Update();
	virtual void OnCommand(u32 id,u32 c,HWND w);
	I_INLINE u64 _getStatus(){return _status;};
	int Invalidate(){BS(_status,S_DEBUG_UPDATE);return 0;};
protected:
	void OnButtonClicked(u32 id,HWND w);

	void Save();

	vector<string> _logs;
	u64 _status;
private:
	void _clear();

	HWND _tv,_window;
	int _level;
	char *_tags,*_ctags;
};

#endif