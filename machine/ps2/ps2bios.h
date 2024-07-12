#include "arkad.h"
#include "ps2dev.h"
#include <vector>

#ifndef __PS2BIOSH__
#define __PS2BIOSH__

namespace ps2{

using namespace std;

class PS2BIOS : public PS2DEV{
public:
	PS2BIOS();
	virtual ~PS2BIOS();
	virtual int Reset();
	virtual int Init(PS2M &);
protected:
	typedef struct{
		int type;
		void *object;
		u32 sleep_ticks;
		u32 reserved[4];
	} WAIT_OBJECT,*LPWAIT_OBJECT;

	class Thread : vector<WAIT_OBJECT>{
		public:
			Thread();
			virtual ~Thread();
			int _create();
		protected:
			u32 _stack,*_regs;
	};
	vector<Thread>  _threads;
};

};

#endif