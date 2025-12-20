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
	virtual int Run(u8 *,int,void *);
protected:
	int Load(char *);
	int _free_mem(u32);
	u32 _alloc_mem(u32,u32);
	int _biosCall();
	virtual int _enterIRQ(int n,u32 pc=0);
	int ReturnFromCall();

private:
	struct __calls{
		struct __handler{
			u32 _attr,_adr;
			RSZU _regs[36];

			__handler(u32,u32,RSZU *r=0,u32 a=0);
		};
		vector<__handler> cb;

		int _doCall(u32 *pc,RSZU *r);
		int _returnFromCall(u32 *pc,RSZU *r);
		int _addCall(u32,u32,RSZU *r=0);
		int _isEmpty(){return cb.size()==0;};
		int _reset();
	} _cb;

	RSZU _regs_copy[38],_regsh_copy[38];

	struct ThreadParam{
		int status;
		u32 func; //function to execute when thread begins
		u32 stack;
		int stack_size;
		u32 gp_reg;
		int initial_priority;
		int current_priority;
		u32 attr;
		u32 option;
	};

	struct SemaParam {
		int count, //used by WaitSema and SignalSema
			max_count,
			init_count, //initial value for count
			wait_threads; //number of threads associated with this semaphore
		u32 attr, //not used by kernel
			option; //not used by kernel
	};

	struct TCB{
		u32 prev;
		u32 next;
		int status;
		u32 func;
		u32 current_stack;
		u32 gp_reg;
		short current_priority;
		short init_priority;
		int wait_type; //0=not waiting, 1=sleeping, 2=waiting on semaphore
		int sema_id;
		int wakeup_count;
		int attr;
		int option;
		u32 _func; //???
		int argc;
		u32 argv;
		u32 initial_stack;
		int stack_size;
		u32 root; //function to return to when exiting thread?
		u32 heap_base;
	};

	struct thread_context{
		u32 sa_reg;  // Shift amount register
		u32 fcr_reg;  // FCR[fs] (fp control register)
		u32 unkn;
		u32 unused;
		u128 at, v0, v1, a0, a1, a2, a3;
		u128 t0, t1, t2, t3, t4, t5, t6, t7;
		u128 s0, s1, s2, s3, s4, s5, s6, s7, t8, t9;
		u64 hi0, hi1, lo0, lo1;
		u128 gp, sp, fp, ra;
		u32 fp_regs[32];
	};

	struct sema{
		u32 free; //pointer to empty slot for a new semaphore
		int count;
		int max_count;
		int attr;
		int option;
		int wait_threads;
		u32 wait_next, wait_prev;
	};
};

};

#endif