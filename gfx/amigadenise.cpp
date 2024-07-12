#include "amigadenise.h"
#include "amiga500dev.h"
namespace amiga{

Denise::Denise() : GPU(){
}

Denise::~Denise(){
}

int Denise::Run(u8 *,int cyc,void *obj){
	/*if((_cycles += cyc) < 2128)
		return 0;
	for(;_cycles>2128;_cycles -= 2128) if(++__line == 240){PS1GPUREG(0x14)&=~BV(31); return 1;}
	if(__line >= 314){
		machine->OnEvent(ME_ENDFRAME,0);
		PS1GPUREG(0x14) |= BV(31);
		__line=0;
		_cycles=0;
	}*/
	return 0;
}

int Denise::Init(void *a,void *b){
	_io_regs=(u16 *)b;
	_mem=(u8 *)a;
	return 0;
}

int Denise::write(u32 a,u16 v){
	switch(a){
		//case REG_INTENA:
			//_io_regs[REG_INTENAR]=v;
		//	return 1;
	}
	return 1;
}

int Denise::read(u32,u16 *){
	return 0;
}

};