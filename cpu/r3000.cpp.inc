#include "r3000.h"


R3000Cpu::R3000Cpu() : CCore(){
	_regs=NULL;
	_freq=MHZ(33);
}

R3000Cpu::~R3000Cpu(){
	Destroy();
}

int R3000Cpu::Destroy(){
	CCore::Destroy();
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int R3000Cpu::Reset(){
	CCore::Reset();
	return 0;
}

int R3000Cpu::Init(void *m){
	if(!(_regs = new u8[32*sizeof(64)]))
		return -1;
	_mem=(u8 *)m;
	return 0;
}

int R3000Cpu::SetIO_cb(u32,CoreMACallback,CoreMACallback b){
	return -1;
}

int R3000Cpu::_exec(u32){
	int ret;

	ret=1;
	_opcode=0;
	switch((_opcode >> 24) & 0xFC){
		case 0x00:
			switch(_opcode & 0x3F){
			}
		break;
		case 0x4:
			switch((_opcode & 0x001F0000) >> 16){
			}
		break;
		case 0x44:
			switch(_opcode >> 24){
			}
		break;
		case 0x48:
			switch((_opcode >> 20) & 0x3F){
			}
		break;
		case 0x60:
           switch((_opcode >> 20) & 0x38){
           }
		break;
		case 0x64:
			switch((_opcode >> 20) & 0x38){
			}
		break;
		case 0x6C:
           switch((_opcode >> 20) & 0x38){
              }
       break;
       case 0x70:
           switch(((_opcode >> 12) & 0xE00) | (_opcode & 0x3f)){
           }
       break;
       case 0x7C:
           switch(_opcode & 0x3F){
               case 0x20:
                   switch(_opcode & 0x7FF){
                   }
               break;
           }
       break;
       case 0xD0:
           switch((_opcode >> 20) & 0x3F){
               case 0:
                   switch((_opcode >> 16) & 0xF){
                   }
               break;
               case 0x1:
                   switch((_opcode >> 16) & 0xF){
                   }
               break;
           }
       break;

       case 0xF0:
           switch((_opcode >> 20) & 0x38){
           }
       break;
       case 0xF1:
       	switch((_opcode >> 20) & 0xf){
           }
       break;
	}
	_pc+=4;
	return ret;
}