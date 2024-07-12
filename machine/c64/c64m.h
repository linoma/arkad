#include "machine.h"
#include "ccore.h"

#ifndef __C64MH__
#define __C64MH__

namespace c64{

class m6502 :public CCore{
public:
	m6502();
	virtual ~m6502();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(void *);
	virtual int _enterIRQ(int n,u32 pc=0);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
	virtual int Disassemble(char *dest,u32 *padr);
	virtual int _dumpRegisters(char *p);
	virtual int OnException(u32,u32);
protected:
	int _exec(u32);
};

class c64m : public Machine,m6502{
public:
	c64m();
	virtual ~c64m();
	virtual int Load(IGame *,char *);
	virtual int Destroy();
	virtual int Reset();
	virtual int Init();

	virtual int OnEvent(u32,...);
	virtual int Draw(HDC cr=NULL){return -1;};
	virtual int Query(u32 what,void *pv);
	virtual int Exec(u32);
	virtual int Dump(char **);

};

};

#endif