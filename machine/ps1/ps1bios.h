#include "arkad.h"
#include "ps1dev.h"

#ifndef __PS1BIOSH__
#define __PS1BIOSH__

namespace ps1{

class PS1M;

class PS1BIOS : public PS1DEV{
public:
	PS1BIOS();
	virtual ~PS1BIOS();
	virtual int Init(PS1M &);
	virtual int Reset();
protected:
	virtual int ExecA0(u32);
	virtual int ExecB0(u32);
	virtual int ExecC0(u32);
	virtual int ReturnFromCall();
	virtual int _enterIRQ(int n,u32 pc=0);
private:
	u32 _malloc(u32,u32);
	int _free(u32);
	u32 _regs_copy[38],SysIntRP[8];
	void *_hook_irq;
	typedef struct {
		u32 desc;
		s32 status;
		s32 mode;
		u32 fhandler;
	} EvCB[32];

	EvCB *Event, *HwEV, *EvEV, *RcEV, *UeEV, *SwEV, *ThEV;

	struct __pads{
		struct{
			void *buf;
			u32 len;
		} _pad[2];
		void *_buf;
		int _reset();
	} _pads;

	struct __files{
		struct __file{
			union{
				struct{
					u64 _pos,_size,_start;
				};
				struct{
					char *_fn,*_mem;
				};
			};
			u8 _type;
		} _items[16];

		u32 _last;

		__files(){_last=0;_reset();};
		int _reset();
		int _open(char *,u32,u32,u32 *r=0);
		int _write(u32,void *,u32,u32 *r=0);
		int _read(IStreamer *,u32,void *,u32,u32 *r=0);
		int _close(u32);
		int _seek(u32,u32,u32);
		protected:
		int _find_free_slot(u32 *r);
	} _files;

	struct __calls{
		struct __handler{
			u32 _attr,_adr,_regs[36];

			__handler(u32,u32,u32 *r=0,u32 a=0);
		};
		vector<__handler> cb;

		int _doCall(u32 *pc,u32 *r);
		int _returnFromCall(u32 *pc,u32 *r);
		int _addCall(u32,u32,u32 *r=0);
		int _isEmpty(){return cb.size()==0;};
		int _reset();
	} _cb;

	struct  __heap{
		u32 addr,end,last;
	} _heap;

	struct DIRENTRY {
		char name[20];
		s32 attr;
		s32 size;
		u32 next;
		s32 head;
		char system[4];
	};
};

};

#endif