#include "amiga500dev.h"

namespace amiga{

amiga500dev::amiga500dev() : M68000Cpu(),Denise(),Paula(){
}

amiga500dev::~amiga500dev(){
}

int amiga500dev::Init(void *a,void *b){
	if(M68000Cpu::Init(a))
		return -2;
	M68000Cpu::_io_regs=(u16 *)b;
	if(Denise::Init(a,b))
		return -3;
	if(Paula::Init(a,b))
		return -4;
	if(_gary.init(0,a,b))
		return -5;
	if(_agnus.init(0,a,b))
		return -6;
	for(int i=0;i<sizeof(_cia)/sizeof(__cia);i++){
		if(_cia[i].init(i,a,b))
			return -7;
	}
	return 0;
}

int amiga500dev::Reset(){
		memset(M68000Cpu::_io_regs,0xff,0x200);
	M68000Cpu::Reset();
	_gary.reset();
	_agnus.reset();
	for(int i=0;i<sizeof(_cia)/sizeof(__cia);i++)
		_cia[i].reset();
	//M68000Cpu::_io_regs[REG_INTENAR]=0;

	return 0;
}

int amiga500dev::__gary::reset(){
	return 0;
}

int amiga500dev::__gary::init(int,void *a,void *b){
	_io_regs=(u16 *)b;
	_mem=(u8 *)a;
	return 0;
}

int amiga500dev::__gary::write(u32,u16){
	return 0;
}

int amiga500dev::__gary::read(u32,u16 *){
	return 0;
}

int amiga500dev::__fat_agnus::reset(){
	return 0;
}

int amiga500dev::__fat_agnus::init(int,void *a,void *b){
	_io_regs=(u16 *)b;
	_mem=(u8 *)a;
	return 0;
}

int amiga500dev::__fat_agnus::write(u32,u16){
	return 0;
}

int amiga500dev::__fat_agnus::read(u32,u16 *){
	return 0;
}

int amiga500dev::__cia::reset(){
	memset(_regs,0,sizeof(_regs));
	_io_regs[REG_SERDATR] = SERDATR_RXD | SERDATR_TSRE | SERDATR_TBE;
	return 0;
}

int amiga500dev::__cia::init(int,void *a,void *b){
	_io_regs=(u16 *)b;
	_mem=(u8 *)a;
	return 0;
}

int amiga500dev::__cia::write(u32 a,u16 v){
	switch(a&0xf){
		case 1:
		break;
		default:
			printf("cia w %x %x\n",a,v);
		break;
	}
	return 0;
}

int amiga500dev::__cia::read(u32 a,u16 *m){
	switch(a&0xf){
		case 1:
		break;
		default:
			printf("cia r %x\n",a);
		break;
	}
	return 0;
}


};