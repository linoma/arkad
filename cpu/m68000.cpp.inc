#include "m68000.h"

namespace M68000{

M68000Cpu::M68000Cpu() : CCore(){
	_regs=NULL;
}
M68000Cpu::~M68000Cpu(){
	Destroy();
}

int M68000Cpu::Destroy(){
	return 0;
}
int M68000Cpu::Reset(){
	return 0;
}
int M68000Cpu::Init(){
	if(!(_regs = new u8[16*sizeof(u32)]))
		return -1
	return 0;
}
int M68000Cpu::SetIO_cb(u32,CoreMACallback,CoreMACallback b){
	return -1;
}

int M68000Cpu::_exec(u32){
	int ret;
	_opcode=0;
	return ret;


}
};