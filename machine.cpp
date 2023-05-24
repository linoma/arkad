#include "machine.h"
#include "utils.h"

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
	_frame=0;
	_frame_skip=0;
	_last_time=GetTickCount();
	return 0;
}

int Machine::Destroy(){
	if(_memory)
		delete []_memory;
	_memory=NULL;
	return 0;
}

int Machine::OnVblank(s32 v){
	BVS(_status,1);
	switch(v){
		case 1:
			BVS(Machine::_status,2);
		break;
	}
	return 0;
}