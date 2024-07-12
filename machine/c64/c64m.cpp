#include "c64m.h"
#include "gui.h"

namespace c64{

c64m::c64m():Machine(MB(10)),m6502(){
}

c64m::~c64m(){
}

int c64m::Load(IGame *,char *){return -1;}
int c64m::Destroy(){return -1;}
int c64m::Reset(){return -1;}
int c64m::Init(){return -1;}

int c64m::OnEvent(u32,...){return -1;}

int c64m::Query(u32 what,void *pv){
	switch(what){
		default:
			return -1;//CCore::Query(what,pv);
	}
}

int c64m::Exec(u32 status){
	int ret;

	//ret=M68000Cpu::Exec(status);
	//__cycles=M68000Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			return -ret;
	}
//	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}
int c64m::Dump(char **){return -1;}

m6502::m6502():CCore(){
	_regs=NULL;
}

m6502::~m6502(){
}

int m6502::Destroy(){
	if(_regs)
		delete []_regs;
	_regs=NULL;
	//return 0;
	CCore::Destroy();
	return 0;
}

int m6502::Reset(){
	return CCore::Reset();
}


int m6502::Init(void *m){
	_mem=(u8 *)m;
	return 0;
}

int m6502::_enterIRQ(int n,u32 pc){return -1;}
int m6502::SetIO_cb(u32,CoreMACallback,CoreMACallback b){return -1;}
int m6502::Disassemble(char *dest,u32 *padr){return -1;}
int m6502::_dumpRegisters(char *p){return -1;}
int m6502::OnException(u32,u32){return -1;}
int m6502::_exec(u32 status){
	return -1;
}

};