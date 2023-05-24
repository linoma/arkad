#include "ccore.h"

CCore::CCore(){
	_tobj._first=NULL;
	_filename=NULL;
	_portfnc_write=_portfnc_read=NULL;
	_dumpMode=0;
	_dumpAddress=0;
}

CCore::~CCore(){
}

int CCore::Exec(u32 status){
	int ret;

	EXECCHECKBREAKPOINT(status,u32);
	ret = _exec(status);
	__cycles+=ret;
	if(/*f==0 || (m-f)>1000000 ||*/ __cycles > _freq){
			//printf("elapsed %lu %u\n",m-f,__cycles);
			//f=m;
		__cycles -= __cycles < _freq ? __cycles : _freq;
		//printf("__cycles %u\n",__cycles);
		return -1;
	}
	return ret;
}

int CCore::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_SET_PC:
			_pc=*((u32 *)pv);
			return 0;
		case ICORE_QUERY_PC:
			*((u32 *)pv)=(u32)_pc;
			return 0;
		case ICORE_QUERY_FILENAME:
			if(!_filename)
				return -2;
			strcpy((char *)pv,_filename);
			return 0;
		case ICORE_QUERY_BREAKPOINT_INDEX:
			{
				u32 i=0,*p=(u32 *)pv;
				for (auto it = _bk.begin();it != _bk.end(); ++it,i++){
						if((u32)*it == p[0]){
							*((u64 *)&p[1])=i;
							return 0;
						}
				}
			}
			return -1;
		case ICORE_QUERY_BREAKPOINT_INFO:
			{
				u32 i=0,*p=(u32 *)pv;
				for (auto it = _bk.begin();it != _bk.end(); ++it,i++){
						if(i==p[0]){
							*((u64 *)&p[1])=*it;
							return 0;
						}
				}
			}
			return 1;
		case ICORE_QUERY_BREAKPOINT_SET:
			{
				u32 i=0,*p=(u32 *)pv;
				for (auto it = _bk.begin();it != _bk.end(); ++it,i++){
						if(i==p[0]){
							*it=*((u64 *)&p[1]);
							return 0;
						}
				}
			}
			return 1;
		case ICORE_QUERY_BREAKPOINT_ENUM:
			{
				u64 *p;

				if(!pv)
					return -1;
				*((LPDEBUGGERPAGE *)pv)=NULL;
				if(!(p = (u64 *)malloc((_bk.size()+1)*sizeof(u64))))
					return -2;
				*((u64 **)pv)=p;
				for (auto it = _bk.begin();it != _bk.end(); ++it){
					*p++=(u64)*it;
				}
				*p=0;
			}
			return 0;
		case ICORE_QUERY_BREAKPOINT_LOAD:
			ResetBreakpoint();
			return 0;
		case ICORE_QUERY_BREAKPOINT_RESET:
		{
			u32 *p=(u32 *)pv;
			u64 v=_bk[p[0]];

			if(ISBREAKPOINTCOUNTER(v)){
				_bk[p[0]]=RESETBREAKPOINTCOUNTER(v);
			}
		}
			return 0;
		case ICORE_QUERY_BREAKPOINT_CLEAR:
			_bk.clear();
			return 0;
		case ICORE_QUERY_BREAKPOINT_MEM_ADD:
			{
			}
			return 0;
		case ICORE_QUERY_BREAKPOINT_ADD:
		{
			u32 *p;

			p = (u32 *)pv;
			LOGD("INT Breakpoint add %x\n",*p);
			for (auto it = _bk.begin();it != _bk.end(); ++it){
				u32 v=(u32)*it;
				if(v==p[0] && (SR(*it,32) & 0x8007) != 0x8007)
					return -1;
			}
			u64 u = p[0]|SL((u64)p[1],32);
			_bk.push_back(u);
			return 0;
		}
		case ICORE_QUERY_BREAKPOINT_COUNT:
		{
			u32 *p,n,m;

			p = (u32 *)pv;
			n=0;
			m=*p;
			for (auto it = _bk.begin();it != _bk.end(); ++it){
				switch(*p){
					default:
						n++;
					break;
				}

			}
			p[0]=n;

			return 0;
		}
			return 0;
		case ICORE_QUERY_SET_BREAKPOINT_STATE:
			{
				u32 *p=(u32 *)pv;
				u64 a=_bk[p[0]];

				switch(p[1]){
					case -1:
						a^= 0x8000000000000000;
					break;
					case -2:
					break;
					case 1:
						a|= 0x8000000000000000;
					break;
					case 0:
						a&= ~0x8000000000000000;
					break;
				}
				_bk[p[0]]=a;
			}
			return 0;
		case ICORE_QUERY_BREAKPOINT_DEL:
			{
				u32 i=0,*p=(u32 *)pv;
				for (auto it = _bk.begin();it != _bk.end(); ++it,i++){
						if(i==p[0]){
							_bk.erase(it);
							return 0;
						}
				}
			}
			return -1;
		case ICORE_QUERY_BREAKPOINT_SAVE:
			return 0;
		case ICORE_QUERY_SET_LAST_ADDRESS:
		{
			u32 *p=(u32 *)pv;
			_lastAccessAddress=p[0];
		}
			return 0;
		case ICORE_QUERY_DBG_DUMP_ADDRESS:
			_dumpAddress=(u32)((u64)pv & 0xFFFFFFFF);
			return 0;
		case ICORE_QUERY_DBG_DUMP_FORMAT:
			_dumpMode=(u32)((u64)pv & 0xFFFFFFFF);
			return 0;
		case ICORE_QUERY_DISASSEMBLE:
			{
				u64 *p=(u64 *)pv;
				return Disassemble((char *)p[0],(u32 *)p[1]);
			}
		default:
			return -1;
	}
}

int CCore::AddTimerObj(ICpuTimerObj *obj,u32 _elapsed,char *n){
	LPCPUTIMEROBJ pp,p;
	int res;

	res=1;
	p=_tobj._first;
	do{
		if(!(pp=p))
			break;
		if(p->obj==obj)
			goto A;
		p=p->next;
	}while(p);
	res=-1;
	if(!(p=new CPUTIMEROBJ[1]))
		goto A;
	res=0;
	p->next=NULL;
	p->last=pp;
	p->obj=obj;
	p->cyc=_tobj._cyc;
	if(!_tobj._first){
		_tobj._first=p;
		_tobj._cyc=0;
	}
	else
		pp->next=p;
A:
	p->elapsed=_elapsed;
	if(_tobj._edge == 0 || _elapsed <  _tobj._edge)
		_tobj._edge=_elapsed;
	LOGD("AddTimerObj %p %u %d\n",obj,_elapsed,res);
	return res;
}

int CCore::DelTimerObj(ICpuTimerObj *obj){
	LPCPUTIMEROBJ pp,p;
	u32 _elapsed;

	p=_tobj._first;
	do{
		if(!(pp=p))
			break;
		if(p->obj==obj){
			if(p==_tobj._first)
				_tobj._first=p->next;
			if(p->next)
				p->next->last=p->last;
			if(p->last)
				p->last->next = p->next;
			delete p;
			if(!_tobj._first){
				_tobj._edge=0;
				_tobj._cyc=0;
			}
			goto A;
		}
		p=p->next;
	}while(p);
	LOGD("DelTimerObj: Not found!!!\n");
	return -1;
A:
	p=_tobj._first;
	_elapsed=(u32)-1;
	while(p){
		if(p->elapsed < _elapsed)
			_elapsed=p->elapsed;
		p=p->next;
	}
	if(_tobj._edge == 0 || _elapsed<_tobj._edge)
		_tobj._edge=_elapsed;
	return 0;
}

int CCore::Reset(){
	_cstk.clear();
	_lastAccessAddress=(u32)-1;
	_tobj._cyc=0;
	_tobj._first=0;
	_tobj._edge=0;
	InitializeCriticalSection(&_cr);
	DestroyWaitableObject();
	ResetBreakpoint();
	return 0;
}

int CCore::Destroy(){
	DestroyWaitableObject();
	if(_filename)
		delete []_filename;
	_filename=NULL;
	return 0;
}

int CCore::_dumpMemory(char *p,u8 *mem,u32 adr,u32 sz){
	char *cc,pp[1024];
	u32 i,n;

	cc=&pp[768];
	*((u64 **)pp)=0;
	if(!mem)
		return 0;
	for(i=0;i<sz;i++){
		*cc=0;
		if((i&0xf)==0){
			sprintf(cc,"%08X ",adr+i);
			pp[0]=0x20;
			pp[1]=0;
		}
		strcat(p,cc);

		switch(_dumpMode){
			case 4:
				sprintf(cc,"%08x ",*((u32 *)&mem[i]));
				i+=(n=3);
				break;
			case 2:
				sprintf(cc,"%04x ",*((u16 *)&mem[i]));
				i+=(n=1);
				break;
			default:
				sprintf(cc,"%02x ",mem[i]);
				n=0;
				break;
		}

		if(adr+i >=_lastAccessAddress && adr+i <(_lastAccessAddress+n+1)){
			strcat(p,"\\u");
			strcat(cc,"\\n");
		}
		strcat(p,cc);
		if(!_dumpMode){
			if(isprint(mem[i]))
				strncat(pp,(char *)&mem[i],1);
			else
				strcat(pp,".");
		}
		if((i&0xf) == 0xf){
			strcat(p,pp);
			strcat(p,"\n");
		}
	}
	return 0;
}

int CCore::DestroyWaitableObject(){
	if(_tobj._first){
		LPCPUTIMEROBJ p=_tobj._first;
		while(p){
			LPCPUTIMEROBJ pp=p;
			p=p->next;
			delete pp;
		}
	}
	memset(&_tobj,0,sizeof(_tobj));
	return 0;
}

void CCore::ResetBreakpoint(){
	for (auto it = _bk.begin();it != _bk.end(); ++it){
		u64 v=(u64)*it;
		if(ISMEMORYBREAKPOINT(v)){
		}
		else if(!ISBREAKPOINTCOUNTER(v)){
		}
		else{
			v=RESETBREAKPOINTCOUNTER(v);
		}
		*it=v;
	}
}

Runnable::Runnable(){
	_status=0;
	_thread=0;
	_sleep=250*1000;
	InitializeCriticalSection(&_cr);
}

Runnable::Runnable(int n):Runnable(){
	_sleep=n;
}

Runnable::~Runnable(){
}

void Runnable::Run(){
	//printf("runnable %u\n",_sleep);
	while(!(_status & 1)){
		usleep(_sleep);
		OnRun();
	}
	printf("runnable destroyy\n");
}

void Runnable::_set_time(u32 n){
	_sleep=n;
}

int Runnable::Create(){
	if(_thread) return 1;
	if(pthread_create(&_thread,NULL,(void * (*)(void *))thread1_func,this) != 0)
		return -1;
	return 0;
}

int Runnable::Destroy(){
	_status|=1;
	if(_thread){
		pthread_join(_thread,0);
		_thread=0;
	}
	return 0;
}