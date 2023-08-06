#include "arkad.h"
#include "utils.h"
#include <vector>

#ifndef __CCOREH__
#define __CCOREH__

#ifdef _DEBUG
	#define STORECALLLSTACK(a) _cstk.push_back(a);
	#define LOADCALLSTACK(a) \
	 while (!_cstk.empty()){\
		u32 b =_cstk.back();\
		_cstk.pop_back();\
		if(b==(a)) break;\
	}
#else
	#define STORECALLLSTACK(a)
	#define LOADCALLSTACK(a)
#endif

#define STOREPORT(){\
	u32 vx = (__cycles<<8)|(_portb);\
	EnterCriticalSection(&_cr);\
	_pins.push_back(vx);\
	LeaveCriticalSection(&_cr);\
}

#define EXECTIMEROBJLOOP(ret,a,b)\
if(_tobj._edge && (_tobj._cyc+=ret) >= _tobj._edge){\
	LPCPUTIMEROBJ p=_tobj._first;\
	while(p){\
		LPCPUTIMEROBJ pp = p->next;\
		int i = p->obj->Run(b,_tobj._cyc-p->cyc);\
		p->cyc=0;\
		if(i<0)\
			DelTimerObj(p->obj);\
		else if(i){\
			a;\
		}\
		p=pp;\
	}\
	while(_tobj._cyc>=_tobj._edge)\
		_tobj._cyc-=_tobj._edge;\
}

#define ISMEMORYBREAKPOINT(a) ((SR(a,32) & 0x8007) == 0x8007)
#define BK_VALUE(a,b,c) \
if(ISMEMORYBREAKPOINT(a)){\
	b= SR(a&0xf8,3);\
	if(a & 0x40000000)\
		b |= SR(a&0x0fffff00,3);\
	else\
		b=c[b];\
}\
else{\
	b = SR(a&0x7f00,8);\
	if(a & 0x40000000)\
		b |= SR(a&0x3fff0000,9);\
	else\
		b=c[rd];\
}

#ifdef _DEBUG
	#define CHECKBREAKPOINT()\
			u32 vv=SR(v,32);\
			if(vv&0x8000){\
				u32 rs,rd;\
				if(!(v&0x8000000000000000))\
					continue;\
				if((vv&7)==7)\
					continue;\
				rs = _regs[SR(vv,3)&0x1f];\
				rd = SR(vv&0x7f00,8);\
				if(vv & 0x40000000)\
					rd |= SR(vv&0x3fff0000,9);\
				else\
					rd=_regs[rd];\
				switch(vv&0x7){\
					case 0:\
					break;\
					case 1:\
						if(rd==rs)\
							continue;\
					break;\
				}\
			}\
			else{\
				int ret=(int)(vv&0x7fff)+1;\
				v=(v&~0xFFFF00000000)|(SL((u64)(ret&0x7fff),32));\
				*it=v;\
				if(!(v&0x8000000000000000))\
					continue;\
				int i=(int)SR(vv,16) & 0x7fff;\
				if(i && i != ret)\
					continue;\
			}
#else
	#define CHECKBREAKPOINT()
#endif

#define ISMEMORYBREAKPOINT(a) ((SR(a,32) & 0x8007) == 0x8007)
#define ISBREAKPOINTENABLED(a) ((a & 0x8000000000000000))
#define RESETBREAKPOINTCOUNTER(a) (a & 0x8FFF0000FFFFFFFF)
#define ISBREAKPOINTCOUNTER(a) ((a&0x800000000000)==0)

#define EXECCHECKBREAKPOINT(a,b)\
if(BT(a,S_DEBUG) && !BT(a,S_DEBUG_NEXT)){printf("bbk\n");\
	for (auto it = _bk.begin();it != _bk.end(); ++it){\
		u64 v=(u64)*it;\
		if((b)v != _pc)\
			continue;\
		CHECKBREAKPOINT();\
		return -2;\
	}\
}


#define __S(a,...)	sprintf(cc,STR(%s) STR(\x20) STR(a),## __VA_ARGS__);
#define __F(a,...) 	printf(STR(%08X %04X %s) STR(\x20) STR(a) STR(\n),_pc,_opcode,## __VA_ARGS__);

class CCore : public ICore{
public:
	CCore(void *p=NULL);
	virtual ~CCore();
	virtual int Reset();
	virtual int Destroy();
	virtual int _exec(u32)=0;
	int AddTimerObj(ICpuTimerObj *obj,u32 _elapsed=0,char *n=0);
	int DelTimerObj(ICpuTimerObj *obj);
	char *_getFilename(char *p=NULL){return _filename;};
	virtual int Query(u32,void *);
	virtual int Exec(u32);
	virtual int _dumpMemory(char *p,u8 *mem,u32 adr,u32 sz=0x400);
	virtual int _dumpRegisters(char *p);
	virtual int _dumpCallstack(char *p);
protected:
	int DestroyWaitableObject();
	int ResetWaitableObject();
	void ResetBreakpoint();
	virtual int Disassemble(char *dest,u32 *padr){return 0;};

	struct {
		u32 _cyc,_edge;
		LPCPUTIMEROBJ _first;
	} _tobj;

	u32 _cycles,_cr,_pc,_dumpAddress,_dumpMode,_lastAccessAddress,_freq,_attributes;
	std::vector<u32> _cstk;
	std::vector<u64> _bk;
	char *_filename;
	CoreMACallback *_portfnc_write,*_portfnc_read;
	u8 *_mem,*_regs;
	void *_machine;
private:
	u64 _status;
};

#endif