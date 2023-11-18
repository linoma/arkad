#include "ccore.h"

CCore::CCore(void *p){
	_tobj._first=NULL;
	_filename=NULL;
	_portfnc_write=_portfnc_read=NULL;
	_regs=NULL;
	_dumpMode=0;
	_dumpAddress=0;
	_machine=p;
	_attributes=0;
	_status=0;
	_idx=0;
	InitializeCriticalSection(&_cr);
}

CCore::~CCore(){
}

int CCore::Exec(u32 status){
	int ret;

	if(BT(_status,S_QUIT))
		return 0;
	EXECCHECKBREAKPOINT(status,u32);
	ret = _exec(status);
	//fixme
	_cycles += ret;
	if(/*f==0 || (m-f)>1000000 ||*/ _cycles >= _freq){
		_cycles -= _freq;
		return -1;
	}
	return ret;
}

int CCore::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_CPU_INDEX:
			*((u32 *)pv)=(u32)_idx;
			return 0;
		case ICORE_QUERY_NEW_WAITABLE_OBJECT:
			if(_machine)
				return ((IObject *)_machine)->Query(what,pv);
			return -1;
		case ICORE_QUERY_CURRENT_SCANLINE:
			return -1;
		case ICORE_QUERY_CPU_ACTIVE_INDEX:
			if(_machine)
				return ((IObject *)_machine)->Query(what,pv);
			*((u32 *)pv)=0;
			return 0;
		case ICORE_QUERY_SET_STATUS:
			_status = (_status & ~0xffffffff)|*((u32 *)pv);
			return 0;
		case ICORE_QUERY_NEXT_STEP:
			if(_machine)
				return ((IObject *)_machine)->Query(what,pv);
			return -1;
		case ICORE_QUERY_ADDRESS_INFO:
			if(_machine)
				return ((IObject *)_machine)->Query(what,pv);
			return -1;
		case ICORE_QUERY_SET_MACHINE:
			_machine=pv;
			return 0;
		case ICORE_QUERY_CPU_FREQ:
			*((u32 *)pv)=(u32)_freq;
			return 0;
		case ICORE_QUERY_CPU_TICK:
			*((u32 *)pv)=(u32)_cycles;
			return 0;
		case ICORE_QUERY_CPUS:{
			char *p = new char[200];
			if(!p) return -1;
			((void **)pv)[0]=p;
			memset(p,0,200);
			strcpy(p,"CPU 0");
		}
			return 0;
		case ICORE_QUERY_CPU_INTERFACE:{
			if(_machine)
				return ((IObject *)_machine)->Query(what,pv);
			u32 *p=(u32 *)pv;

			if(!*p){
				((void **)pv)[0]=this;
				return 0;
			}
		}
			return -1;
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
			LOGI("INT Breakpoint add %x\n",*p);
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
			if(_machine)
				return ((IObject *)_machine)->Query(what,pv);
			_dumpAddress=(u32)((u64)pv & 0xFFFFFFFF);
			return 0;
		case ICORE_QUERY_DBG_DUMP_FORMAT:
			//if(_machine)
				//return ((IObject *)_machine)->Query(what,pv);
			_dumpMode=(u32)((u64)pv & 0xFFFFFFFF);
			return 0;
		case ICORE_QUERY_DISASSEMBLE:
			{
				u64 *p=(u64 *)pv;
				return Disassemble((char *)p[0],(u32 *)p[1]);
			}
		case ICORE_QUERY_CPU_MEMORY:
			*((u64 *)pv)=(u64)_mem;
			return 0;
		case ICORE_QUERY_MEMORY_ACCESS:
			{
				int i=0;
				u32 *p=(u32 *)pv;
#ifdef _DEBUG
				for (auto it = _bk.begin();it != _bk.end(); ++it){
					u64 v=(u64)*it;
					if(!(v&0x8000000000000000))
						continue;
					if((u32)v != *p)
						continue;
					u32 vv=SR(v,32);
					if((vv&0x8007)!=0x8007)
						continue;
					u32 rs = 0;//rs = _regs[SR(vv,3)&0x1f];
					u32 rd = SR(vv&0xf8,3);
				/*	if(vv & 0x40000000)
						rd |= SR(vv&0x0fffff00,3);
					else
						rd=((u32 *)_regs)[rd];*/
					switch(SR(vv&0x30000000,28)){
						case 0:
							if((p[1] & 1))
								continue;
						break;
						case 1:
							if(!(p[1] & 1))
								continue;
						break;
					}

					switch(SR(vv,6)&0x3){//size
					}

					switch(SR(vv,28)&0x7){//condition
					}
					p[2]=_pc;
					p[3]=_idx;
					return 2;
				}
#endif
			}
			return 0;
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
	p->cyc=0;//_tobj._cyc;
	if(!_tobj._first){
		_tobj._first=p;
		_tobj._cyc=0;
	}
	else
		pp->next=p;
	//printf("AddTimerObj %p %u %d\n",obj,_elapsed,res);
A:
	p->elapsed=_elapsed;
	if(_tobj._edge == 0 || _elapsed<_tobj._edge)
		_tobj._edge=_elapsed;
	LOGD("%s %p %u %d\n",__FUNCTION__,obj,_elapsed,res);
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

LPCPUTIMEROBJ CCore::GetTimerObj(ICpuTimerObj *obj){
	LPCPUTIMEROBJ pp,p;

	p=_tobj._first;
	do{
		if(!(pp=p))
			break;
		if(p->obj==obj){
			return p;
		}
		p=p->next;
	}while(p);
	return NULL;
}

int CCore::Restart(){
	_cycles=0;
	_status&=~S_QUIT;
	_cstk.clear();
	_lastAccessAddress=(u32)-1;

	_tobj._cyc=0;
	ResetWaitableObject();

	//fixme resetWaitableObject
	//_tobj._first=0;
	//_tobj._edge=0;
	//DestroyWaitableObject();
	return 0;
}

int CCore::Reset(){
	Restart();
	ResetBreakpoint();
	return 0;
}

int CCore::Stop(){
	_status |= S_QUIT;
	return 0;
}

int CCore::Destroy(){
	DestroyWaitableObject();
	if(_filename)
		delete []_filename;
	_filename=NULL;
	return 0;
}

int CCore::_dumpCallstack(char *p){
	char *cc,pp[1024];

	cc=&pp[768];
	*((u64 **)pp)=0;
	for (auto it = _cstk.rbegin();it != _cstk.rend(); ++it){
		*cc=0;
		sprintf(cc,"%08x\n",*it);
		strcat(p,cc);
	}
	return 0;
}

int CCore::_dumpRegisters(char *p){
	sprintf(p,"PC: %08X",_pc);
	return 0;
}

int CCore::_dumpMemory(char *p,u8 *mem,u32 adr,u32 sz){
	char *cc,pp[1024];
	u32 i,n;

	cc=&pp[768];
	*((u64 **)pp)=0;
	if(!mem)
		mem=&_mem[adr];
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
	LPCPUTIMEROBJ p=_tobj._first;
	while(p){
		LPCPUTIMEROBJ pp=p;
		p=p->next;
		delete pp;
	}

	memset(&_tobj,0,sizeof(_tobj));
	return 0;
}

int CCore::ResetWaitableObject(){
	LPCPUTIMEROBJ p=_tobj._first;
	while(p){
		p->cyc=0;
		p=p->next;
	}

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

int CCore::SaveState(IStreamer *p){
#ifdef _DEVELOP
	u32 v;

	p->Write(&_pc,sizeof(u32),0);
	v=0;
	for(LPCPUTIMEROBJ p=_tobj._first;p;p=p->next)
		v++;
//	v=0;
	p->Write(&v,sizeof(u32));
	for(LPCPUTIMEROBJ o=_tobj._first;o && v;o=o->next){
		u32 id;

		if(((IObject *)o->obj)->Query(ICORE_QUERY_OID,&id)) id=0;
		p->Write(&id,sizeof(u32));
		p->Write(&o->elapsed,sizeof(u32));
		p->Write(&o->cyc,sizeof(u32));
		printf("ss %x\n",id);

		if(OID_SERIALIZE(id)){
			ISerializable *l;

			if(!((IObject *)o->obj)->Query(ICORE_QUERY_ISERIALIZE,(void *)&l))
				l->Serialize(p);
		}
	}
#endif
	return 0;
}

int CCore::LoadState(IStreamer *p){
#ifdef _DEVELOP
	u32 v;

	p->Read(&_pc,sizeof(u32),0);
	p->Read(&v,sizeof(u32),0);
//	DestroyWaitableObject();
printf("obj found %d\n",v);
	for(;v;v--){
		LPCPUTIMEROBJ o;
		u32 elapsed,cyc,id;

		p->Read(&id,sizeof(u32));
		p->Read(&elapsed,sizeof(u32));
		p->Read(&cyc,sizeof(u32));printf("id %x\n",id);
		if(!id) continue;
		for(o=_tobj._first;o;o=o->next){
			u32 iid;

			if(!((IObject *)o->obj)->Query(ICORE_QUERY_OID,&iid) && (u16)iid==(u16)id){
				o->elapsed=elapsed;
				o->cyc=cyc;
				break;
			}
		}

		printf("ccore loadstate %s found %x %u %x %u %u",o?"OK":"NOT",_pc,v,id,elapsed,cyc);

		if(!o){
			u32 d[10]={0};

			IObject *i = (IObject *)(_machine ? _machine : this);
			d[0]=id;
			if(!i->Query(ICORE_QUERY_NEW_WAITABLE_OBJECT,d)){
				ICpuTimerObj *pp= (ICpuTimerObj *)*((void **)&d[1]);
				pp->Query(ICORE_QUERY_OID,&d[6]);

				AddTimerObj(pp,elapsed);
				o=GetTimerObj(pp);
				o->cyc=cyc;
				printf(" p:%p ID:%x", *((void **)&d[1]),d[6]);
			}
		}

		if(o && o->obj && OID_SERIALIZE(id)){
			ISerializable *l;
			printf("lino\n");
			if(!((IObject *)o->obj)->Query(ICORE_QUERY_ISERIALIZE,(void *)&l)){
		//	if((u16)id == 0xf){
				//l=dynamic_cast<ISerializable *>(o->obj);
				//if(l)
				l->Unserialize(p);
			}
		//}
		}
		printf("\n");
	}
#endif
	return 0;
}

FileStream::FileStream(){
	fp=NULL;
}

FileStream::~FileStream(){
	Close();
}

int FileStream::Read(void *buf,u32 sz,u32 *osz){
	u32 ret;
	int res;

	res=-1;
	ret=0;
	if(!fp)
		goto Z;;
	ret=fread(buf,1,sz,fp);
	//printf("%s  %u %u\n",__FUNCTION__,sz,ret);
	res=0;
Z:
	if(osz) *osz=ret;
	return res;
}

int FileStream::Write(void *buf,u32 sz,u32 *osz){
	if(!fp)
		return -1;
	fwrite(buf,1,sz,fp);
	return 0;
}

int FileStream::Close(){
	if(fp){
		fclose(fp);
		fp=NULL;
	}
	return 0;
}

int FileStream::Seek(s64 p,u32 w){
	if(!fp)
		return -1;
	fseek(fp,p,w);
	return 0;
}

int FileStream::Open(char *p,u32 attr){
	fp = fopen(p,attr &1? "wb+" : "rb+");
	return fp != 0 ? 0 : -1;
}

MemoryStream::MemoryStream(u32 dw) : vector<void *>(){
	dwBufferSize = dw;
}

MemoryStream::~MemoryStream(){
	Close();
}

MemoryStream::buffer *MemoryStream::AddBuffer(){
   buffer *tmp;

   tmp = new buffer(dwBufferSize);
   if(tmp == NULL || tmp->buf == NULL)
       return NULL;
   push_back(tmp);
   dwSize += tmp->dwSize;
   return tmp;
}

int MemoryStream::Read(void *lpBuffer,u32 dwBytes,u32 *o){
	buffer *tmp;
	u32 dwRead,dwpos,dw;

	if(size() == 0 || lpBuffer == NULL || dwBytes < 1)
		return -1;
  /*
   if((dwpos = (u32)CercaItem(dwIndex)) == 0)
       return 0;
   if((tmp = (buffer *)(((LList::elem_list *)dwpos)->Ele)) == NULL)
       return 0;
   */
   if(dwIndex >= size())
	return -2;
	tmp = (buffer *)at(dwIndex-1);
	dwRead = 0;
	while(1){
       dw = tmp->dwBytesWrite - tmp->dwPos + 1;
       if(dwBytes < dw)
           dw = dwBytes;
       memcpy(lpBuffer,&tmp->buf[tmp->dwPos-1],dw);
       lpBuffer = ((u8 *)lpBuffer) + dw;
       dwBytes -= dw;
       dwRead += dw;
       dwPos += dw;
       tmp->dwPos += dw;
       if(dwBytes == 0 || (dwIndex+1) >= size())
           break;
       tmp->dwPos = 1;
       tmp = (buffer *)at(dwIndex++);
       currentBuffer = tmp;
	}
	if(o)
		*o=dwRead;
	return 0;
}

int MemoryStream::Write(void *lpBuffer,u32 dwBytes,u32 *o){
	buffer *tmp;
	u32 dwWrite,dw;

   if(size() == 0 || lpBuffer == NULL || dwBytes < 1)
       return -1;
   tmp = currentBuffer;
   dwWrite = 0;
   while(dwBytes != 0){
       if(tmp == NULL || tmp->dwSize == (tmp->dwPos - 1)){
           if(dwIndex == size()){
               if((tmp = AddBuffer()) == NULL)
                   break;
               dwIndex = size();
           }
           else{
               tmp = (buffer *)at(++dwIndex);
               tmp->dwPos = 1;
           }
           currentBuffer = tmp;
       }
       dw = tmp->dwSize - (tmp->dwPos - 1);
       if(dwBytes < dw)
           dw = dwBytes;
       memcpy(&tmp->buf[tmp->dwPos-1],lpBuffer,dw);
       lpBuffer = ((u8 *)lpBuffer) + dw;
       if(tmp->dwPos >= tmp->dwBytesWrite)
           tmp->dwBytesWrite += dw;
       tmp->dwPos += dw;
       dwBytes -= dw;
       dwWrite += dw;
       dwPos += dw;
   }
   if(o) *o=dwWrite;
   return 0;
}

int MemoryStream::Seek(s64 dwDistanceToMove,u32 dwMoveMethod){
 /*  u32 dw,dw1,dw2,dwRes;
   buffer *tmp;
   LONG lEnd;

   if(size() == 0)
       return -1;
   dwRes = 0xFFFFFFFF;
   switch(dwMoveMethod){
       case SEEK_SET:
           lEnd = dwDistanceToMove;
       break;
       case SEEK_END:
           lEnd = (LONG)((LONG)Size() + dwDistanceToMove);
       break;
       case SEEK_CUR:
           lEnd = ((LONG)dwPos + dwDistanceToMove - 1);
       break;
       default:
           return dwRes;
   }
	if(lEnd < 0)
		lEnd = 0;
	else if(lEnd > (LONG)Size())
		lEnd = (LONG)Size();
	dw1 = 0;
	dw2 = 1;
	for(auto it=begin();it!=end();it++){
		tmp=(buffer *)(*it);
       if((LONG)(dw1 + tmp->dwBytesWrite) >= lEnd){
           dwRes = dwPos = (u32)lEnd;
           dwPos++;
           currentBuffer = tmp;
           tmp->dwPos = lEnd - dw1 + 1;
           dwIndex = dw2;
           break;
       }
       dw1 += tmp->dwBytesWrite;
       dw2++;
	}*/
	return 0;
}

int MemoryStream::Open(char *,u32 a){
	if(size() > 0)
       return 0;
   dwSize = 0;
   if((currentBuffer = AddBuffer()) == NULL)
       return -11;
   dwPos = 1;
   dwIndex = 1;
   return 0;
}

MemoryStream::buffer::buffer(u32 dw){
   buf = (u8 *)::calloc(1,dw);
   if(buf == NULL)
       return;
   dwSize = dw;
   dwPos = 1;
   dwBytesWrite = 0;
}

int MemoryStream::Close(){
   for(auto it=begin();it!=end();it++){
	   buffer *p= (buffer *)(*it);
	   delete p;
	}
	clear();
	return 0;
}

Runnable::Runnable(){
	_status=0;
	_thread=0;
	_sleep=250*1000;
	InitializeCriticalSection(&_cr);
}

Runnable::Runnable(int n) : Runnable(){
	_sleep=n;
}

Runnable::~Runnable(){
}

void Runnable::Run(){
	//printf("runnable %u\n",_sleep);
	while(!(_status & QUIT)){
		usleep(_sleep);
		OnRun();
	}
}

void Runnable::_set_time(u32 n){
	_sleep=n;
}

int Runnable::Create(){
	if(_thread)
		return 1;
	if(pthread_create(&_thread,NULL,(void * (*)(void *))thread1_func,this) != 0)
		return -1;
	return 0;
}

int Runnable::Destroy(){
	_status |= QUIT;
	_status &= ~PLAY;
	if(_thread){
		pthread_join(_thread,0);
		_thread=0;
	}
	return 0;
}