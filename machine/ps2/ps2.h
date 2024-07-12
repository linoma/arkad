#include "arkad.h"
#include "machine.h"
#include "ps2bios.h"

#ifndef __PS2MH__
#define __PS2MH__

namespace ps2{

class PS2M : public Machine,public PS2BIOS{
public:
	PS2M();
	virtual ~PS2M();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return -1;};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);
	virtual int LoadSettings(void * &v);

	virtual s32 fn_write_io(u32,void *,void *,u32);
protected:
	virtual int OnJump(u32);
	virtual int OnCop(RSZU,RSZU,RSZU,RSZU *ss);
	virtual int OnException(u32,u32);

};

};

#endif