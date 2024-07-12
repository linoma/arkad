#include "amigapaula.h"
#include "amiga500dev.h"

namespace amiga {

#define SETCLR(a,b){ printf(" B:%x ",b);if (b & 0x8000){ a |= b & 0x7FFF;printf(" Ra:%x ",a);} else{ a &= ~b;printf(" Rb:%x ",a);}}

Paula::Paula() : PCMDAC(){
}

Paula::~Paula(){
}

int Paula::Init(void *a,void *b){
	_io_regs=(u16 *)b;
	_mem=(u8 *)a;
	return 0;
}

int Paula::write(u32 a,u16 v){
	switch(a){
		case REG_INTENA:
			printf("preg %x %x %x %x %x ",a&0x1ff,v,_io_regs[REG_INTENA],~v,_io_regs[REG_INTENA] & ~v);
			SETCLR(_io_regs[REG_INTENA],v);
			printf("%x\n",_io_regs[REG_INTENA]);
			_io_regs[REG_INTENAR]=_io_regs[REG_INTENA];
			return 1;
	}
	return 1;
}

int Paula::read(u32 a,u16 *v){
	return 0;
}

};
