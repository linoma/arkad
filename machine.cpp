#include "machine.h"
#include "utils.h"
#include "gui.h"

extern GUI gui;

Machine::Machine(u32 sz){
	_memory=NULL;
	_keys=0;
	_mem_size=sz;
	_status=0;
	_cr_frame_skip = _max_frame_skip=10;
	_fps_sync=1;
	_max_frame=60;
	_mem_start=0;
}

Machine::~Machine(){
	//printf("Destroy %s\n",__FILE__);
	Destroy();
}

int Machine::Init(){
	if(!(_memory=new u8[_mem_size]))
		return -1;
	return 0;
}

int Machine::Reset(){
	if(_memory)
		memset(_memory,0,_mem_size);
	_status=0;
	_frame=0;
	_frame_skip=0;
	_last_time=GetTickCount();
	_last_update=_last_time;
	_last_frame=_frame;
	__frame=0;
	return 0;
}

int Machine::Destroy(){
	if(_memory)
		delete []_memory;
	_keys=_memory=NULL;
	_mem_size=_mem_start=0;
	return 0;
}

int Machine::OnEvent(u32 ev,...){
	return -1;
}

int Machine::OnEvent(u32 ev,va_list arg){
	switch(ev){
		case ME_KEYDOWN:
		case ME_KEYUP:
			int v=va_arg(arg,int);
			return _keys[v];
	}
	return -1;
}

int Machine::OnFrame(){
	u32 i,u,uu;

	__frame++;
	BS(_status,1);
	if(++_frame >= (_last_frame+10) || _frame > _max_frame){
		BS(Machine::_status,2);

		u=(_frame-_last_frame)*(4096*1000/_max_frame);
		_last_frame = _frame;
		uu=GetTickCount();
		i = (uu - _last_time)*4096;
		if(_fps_sync){
			if(i < u){
				//printf("sleep %u %u\n",u,i);
				u -= i;
				if(u > 4096){
					if(_cr_frame_skip > 1 && _max_frame_skip ==-1) _cr_frame_skip--;
					usleep(SR(u,2));
					uu=GetTickCount();
				}
			}
			else if(_cr_frame_skip < 10 && _max_frame_skip==-1) _cr_frame_skip++;
		}
		_last_time=uu;
		if((uu-_last_update)>999){
			//printf("NF %u %u\n",(uu-_last_update),_frame);
			//uu-=_last_update;

			char s[1024];

			sprintf(s,"NF %u %u %u",(uu-_last_update),_frame,_cr_frame_skip);
			gui.PushStatusMessage(s);
			_last_update=uu;
			_last_frame=_frame=0;
		}
	}

	if(!_frame_skip--){
		_frame_skip = _cr_frame_skip;
		return 0;
	}
	return _frame_skip;
}

int Machine::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	_max_frame_skip=stoi(m["frameskip"]);
	if((_cr_frame_skip=_max_frame_skip) == -1)
		_cr_frame_skip=10;

	_fps_sync=stoi(m["fps_sync"]);
#ifdef _DEVELOP
	_fps_sync=1;
	_max_frame_skip=-1;
#endif
	m.insert({"width","384"});
	m.insert({"height","224"});
	return 0;
}

int Machine::SaveState(IStreamer *p){
	p->Write(&_mem_size,sizeof(_mem_size),0);
	if(_memory)
		p->Write(_memory,_mem_size,0);
	return 0;
}

int Machine::LoadState(IStreamer *p){
	p->Read(&_mem_size,sizeof(_mem_size));
	if(_memory)
		delete []_memory;
	_memory=NULL;
	if(!_mem_size)
		return 0;
	if(!(_memory=new u8 [_mem_size]))
		return -2;
	p->Read(_memory,_mem_size);
	return 0;
}

int Machine::AllocMemory(u32 sz,u32 fl,void **o){
	u32 ms;

	if(!_memory || !_mem_size)
		return -1;
	if(!sz || !o)
		return -2;
	ms=_mem_start;
	for(auto it=_blocks.begin();it!=_blocks.end();it++){
	}
	if((_mem_size-ms) < sz) return -3;

	if(o){
		*o =&_memory[ms];
		if(ms==_mem_start)
			_mem_start+=sz;
	}
	return 0;
}