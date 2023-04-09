#include "machine.h"

Machine::Machine(u32 sz){
	_memory=NULL;
	_memsize=sz;
	_status=0;
}

Machine::~Machine(){
	printf("Destroy %s\n",__FILE__);
	Destroy();
}

int Machine::Init(){
	if(!(_memory=new u8[_memsize]))
		return -1;
	return 0;
}

int Machine::Reset(){
	if(_memory)
		memset(_memory,0,_memsize);
	_status=0;
	return 0;
}

int Machine::Destroy(){
	if(_memory)
		delete []_memory;
	_memory=NULL;
	return 0;
}