#include "machine.h"
#include "utils.h"
#include "gui.h"

extern GUI gui;
Machine::Machine(u32 sz){
	_memory=NULL;
	_memsize=sz;
	_status=0;
}

Machine::~Machine(){
	//printf("Destroy %s\n",__FILE__);
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
	_last_update=_last_time;
	_last_frame=_frame;
	return 0;
}

int Machine::Destroy(){
	if(_memory)
		delete []_memory;
	_memory=NULL;
	return 0;
}

int Machine::OnVblank(s32 v){
	BS(_status,1);
	switch(v){
		case 1:
			BS(Machine::_status,2);
		break;
	}
	return 0;
}

int Machine::OnEvent(u32,...){
	return 0;
}

int Machine::OnFrame(){
	u32 i,u,uu;

	if(++_frame>=(_last_frame+10) || _frame>59){
		OnVblank(1);
		u=(_frame-_last_frame)*(4096*1000/60);
		_last_frame = _frame;
		uu=GetTickCount();
		i = (uu - _last_time)*4096;
		if(u>i){
			//printf("sleep %u %u\n",u,i);
			u-=i;
			if(u>4096){
			//	usleep(SR(u,2));
				uu=GetTickCount();
			}
		}
		_last_time=uu;
		if((uu-_last_update)>1000){
			//printf("NF %u %u\n",(uu-_last_update),_frame);
			//uu-=_last_update;

			char s[1024];

			sprintf(s,"NF %u %u",(uu-_last_update),_frame);
			gui.PushStatusMessage(s);
			_last_update=uu;
			_last_frame=_frame=0;
		}
	}
	else
		OnVblank(0);
	if(!_frame_skip--){
		_frame_skip=10;
		return 0;
	}
	return 1;
}

int Machine::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	m.insert({"width","384"});
	m.insert({"height","224"});
	return 0;
}