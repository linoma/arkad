#include "blacktiger.h"

namespace blktiger{

//#define __Z80F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE) __F(a,## __VA_ARGS__);

//#define __Z80F(a,...) printf(STR(%08X %04X %s) STR(\x20) STR(a) STR(\n),_pc,_opcode,## __VA_ARGS__);
#define __Z80F(a,...)

#define BLKTIGER_AUDIOCPU_STATE	8

static u8 _key_config[]={11,10,9,8,1,6,12,13};

BlackTiger::BlackTiger() : Machine(MB(30)),Z80Cpu(),BlkTigerGpu(),BlkTigerSpu(){
	Z80Cpu::_freq=MHZ(6);
	_keys=_key_config;
	_bank=0;
}

BlackTiger::~BlackTiger(){
}

int BlackTiger::Load(IGame *pg,char *path){
	int res;
	u32 u;

	if(!pg || pg->Open(path,0))
		return -1;
	Reset();

	pg->Read(_memory,889856,&u);
	res--;
	memcpy(_mem,_memory,0x8000+0x4000);
	//memcpy(&_mem[0xc000],&_memory[0x51000],0x1000);

	BlkTigerSpu::Load(&_memory[0x48000]);
	res=0;
A:
	return res;
}

int BlackTiger::Destroy(){
	Z80Cpu::Destroy();
	Machine::Destroy();
	BlkTigerSpu::Destroy();
	BlkTigerGpu::Destroy();
	return 0;
}

#define DSW1	4
int BlackTiger::Reset(){
	_active_cpu=0;

	Z80Cpu::Reset();
	Machine::Reset();
	BlkTigerSpu::Reset();
	BlkTigerGpu::Reset();

	Machine::_status&=~BLKTIGER_AUDIOCPU_STATE;

	_ioreg[3]=0x8f;
	_ioreg[1]=0xff;
	_ioreg[2]=0xff;
	_ioreg[0]=0xff;
	_ioreg[DSW1]=0xff;
	_ioreg[5]=1;//vblank
	return 0;
}

int BlackTiger::Init(){
	if(Machine::Init())
		return -1;
	if(Z80Cpu::Init(&_memory[MB(5)]))
		return -2;
	for(int i =0;i<15;i++)
		SetIO_cb(i,(CoreMACallback)&BlackTiger::fn_port_w);
	for(int i =0;i<0x1000;i++)
		SetMemIO_cb(i|0xc000,(CoreMACallback)&BlackTiger::fn_mem_w,(CoreMACallback)&BlackTiger::fn_mem_r);
	//SetIO_cb(7,(CoreMACallback)&BlackTiger::fn_port_w,(CoreMACallback)&BlackTiger::fn_port_r);
	if(BlkTigerSpu::Init(*this))
		return -4;
	_gpu_regs=_ioreg;
	_gpu_mem=&_memory[MB(20)];
	_pal_ram=&_mem[0xd800];
	_sprite_ram=&_mem[0xfe00];
	_char_ram=&_memory[0x59000];
	if(BlkTigerGpu::Init())
		return -5;
	AddTimerObj((BlkTigerGpu *)this,381);
	return 0;
}

int BlackTiger::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	BlkTigerGpu::LoadSettings(v);
	BlkTigerSpu::LoadSettings(v);
	return 0;
}

int BlackTiger::Exec(u32 status){
	int ret;

	ret=Z80Cpu::Exec(status);
	__cycles=Z80Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			return -ret;
	}
	_ioreg[7]=__data;
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0);,_ioreg);
	Machine::_status^=BLKTIGER_AUDIOCPU_STATE;
	if((Machine::_status & BLKTIGER_AUDIOCPU_STATE)){
		switch(BlkTigerSpu::Exec(status)){
			case -3:
				Machine::_status &= ~BLKTIGER_AUDIOCPU_STATE;
			//	EnterDebugMode();
				break;
			case -2:
				printf("bp\n\n\n");
				return 0x10000002;
		}
	}
	MACHINE_ONEXITEXEC(status,0);
}

int BlackTiger::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case (u32)-1:
			if(!OnFrame())
				BlkTigerGpu::Update();
			BlkTigerSpu::Update();
			return 0;
		case ME_MOVEWINDOW:
			{
				int x,y,w,h;

				va_start(arg, ev);
				x=va_arg(arg,int);
				y=va_arg(arg,int);
				w=va_arg(arg,int);
				h=va_arg(arg,int);
				CreateBitmap(w,h);
				Draw();

				va_end(arg);
			}
			return 0;
		case ME_REDRAW:
			BlkTigerGpu::Update();
			Draw();
			CALLEE(Machine::OnEvent,ev,return,arg);
		case ME_KEYUP:{
				u32 key;

				CALLEE(Machine::OnEvent,ev,key=,arg);
				if(key < 8)
					_ioreg[0] |= SL(1,key);
				else
					_ioreg[1] |= SL(1,key-8);

			}
			return 0;
		case ME_KEYDOWN:{
				u32 key;

				CALLEE(Machine::OnEvent,ev,key=,arg);
				if(key < 8)
					_ioreg[0] &= ~SL(1,key);
				else
					_ioreg[1] &= ~SL(1,key-8);
			}
			return 0;
		case 0:{
			va_start(arg, ev);
			u32 i=va_arg(arg,int);
			va_end(arg);
			ev=i&~0xff000000;
			if(i&0xff000000)
				return BlkTigerSpu::_enterIRQ(ev,0);
		}
		break;
		default:
			if(BVT(ev,31))
				return -1;
			{
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				if(i == 1){//insidde loop just set on it
					//BS(_gic._irq_pending,BV(ev));
					return 1;
				}
			}
		break;
	}
	switch(ev){
		case 1:
			Z80Cpu::_enterIRQ(1,0);
		break;
	}
	return 0;
}

int BlackTiger::Dump(char **pr){
	int res,i;
	char *c,*cc,*p,*pp;
	u8 *mem;
	u32 adr;
	DEBUGGERDUMPINFO di;

	_dump(pr,&di);
	if((c = new char[300000])==NULL)
		return -1;

	*((u64 *)c)=0;
	cc = &c[290000];
	*((u64 *)cc)=0;

	res = 0;
	p=c;
	strcpy(p,"3100");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	((CCore *)cpu)->_dumpRegisters(p);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3102");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	adr=di._dumpAddress;
	pp=&cc[900];

	//mem=_mem+adr;
	((CCore *)cpu)->_dumpMemory(p,NULL,&di);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3103");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	///pp=&cc[900];
	for(i=0;i<0x100;i++){
		*cc=0;
		sprintf(cc,"%02X:%02x   ",i,_ioreg[i]);
		strcat(p,cc);
		if((i&7)==7){
			strcat(p,"\n");
		}
	}
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3104");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	BlkTigerSpu::_dumpRegisters(p);
	res += strlen(p)+1;
	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3106");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	((CCore *)cpu)->_dumpCallstack(p);

	res += strlen(p)+1;

	*pr = c;
	return res;
}

int BlackTiger::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_CPU_ACTIVE_INDEX:
			*((u32 *)pv)=_active_cpu;
			return 0;
		case ICORE_QUERY_DBG_MENU_SELECTED:
			{
				u32 id =*((u32 *)pv);
				//printf("id %x\n",id);
				switch((u16)id){
					case 7:
						return -1;
					default:
						_layers[(u16)id-1]->Save();
						_layers[(u16)id-1]->_visible=id&0xFFFF0000?1:0;
						return 0;
					case 10:
					case 11:
						_ym[id&1]._enabled=id&0xFFFF0000?1:0;
						//printf("opn enabled %d %d\n",id&1,_ym[id&1]._enabled);
					break;
				}
			}
			return -1;
		case ICORE_QUERY_DBG_MENU:
			{
				char *p = new char[1000];
				((void **)pv)[0]=p;
				memset(p,0,1000);
				for(int i=0;i<4;i++){
					sprintf(p,"Layer %d",i);
					p+=strlen(p)+1;
					*((u32 *)p)=1+i;
					*((u32 *)&p[4])=0x102;
					p+=sizeof(u32)*2;
				}
				strcpy(p,"OPN 0");
				p+=strlen(p)+1;
				*((u32 *)p)=10;
				*((u32 *)&p[4])=0x102;
				p+=sizeof(u32)*2;
				strcpy(p,"OPN 1");
				p+=strlen(p)+1;
				*((u32 *)p)=11;
				*((u32 *)&p[4])=0x102;
				p+=sizeof(u32)*2;
				*((u64 *)p)=0;
			}
			return 0;
		case ICORE_QUERY_NEXT_STEP:{
			switch(*((u32 *)pv)){
				case 1:
					*((u32 *)pv)=3;
					return 0;
				default:
					*((u32 *)pv)=2;
					if(_active_cpu==1){
						if(Machine::_status & BLKTIGER_AUDIOCPU_STATE)
							*((u32 *)pv)=4;
					}
					return 0;
			}
		}
			return -1;
		case ICORE_QUERY_CPUS:{
			char *p = new char[500];
			if(!p) return -1;
			((void **)pv)[0]=p;
			memset(p,0,500);
			strcpy(p,"CPU");
			p+=4+sizeof(u32);
			strcpy(p,"CPU Audio");
		}
			return 0;
		case ICORE_QUERY_CPU_INTERFACE:{
			u32 *p=(u32 *)pv;

			switch(*p){
				case 0:
					_active_cpu=0;
					((void **)pv)[0]=(ICore *)this;
					return 0;
				case 1:
					_active_cpu=1;
					((void **)pv)[0]=(ICore *)&_cpu;
					return 0;
			}
		}
			return -1;
		case ICORE_QUERY_ADDRESS_INFO:
			{
				u32 adr,*pp,*p = (u32 *)pv;
				adr =*p++;
				pp=(u32 *)*((u64 *)p);
				switch(SR(adr,24)){
					case 0:
						pp[0]=0;
						pp[1]=KB(64);
						break;
					default:
						return -2;
				}
			}
			return 0;
		case ICORE_QUERY_DBG_PAGE:
			{
				LPDEBUGGERPAGE p;

				if(!pv)
					return -1;
				*((LPDEBUGGERPAGE *)pv)=NULL;
				if(!(p = (LPDEBUGGERPAGE)malloc(9*sizeof(DEBUGGERPAGE))))
					return -2;
				*((LPDEBUGGERPAGE *)pv)=p;
				memset(p,0,9*sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Registers");
				strcpy(p->name,"3100");
				p->type=1;
				p->popup=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Memory");
				strcpy(p->name,"3102");
				p->type=2;
				p->editable=1;
				p->popup=1;
				p->clickable=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"IO Ports");
				strcpy(p->name,"3103");
				p->type=1;
				p->editable=1;
				p->popup=0;
				p->clickable=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"SPU Registers");
				strcpy(p->name,"3104");
				p->type=1;
				p->editable=1;
				p->popup=0;
				p->clickable=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Call Stack");
				strcpy(p->name,"3106");
				p->type=1;
				p->popup=1;
			}
			return 0;
		default:
			return Z80Cpu::Query(what,pv);
	}
	return -1;
}

s32 BlackTiger::fn_port_r(u32 a,pvoid port,pvoid data,u32){
	switch((u8)a){
		case 0://in0
			//*((u8 *)data)=0x1c;
			//return 1;
		break;
		case 1://in1
			//*((u8 *)data)=0xff;
		break;
		case 2://in2
			//*((u8 *)data)=0xff;
		break;
		case 3://DSW0
			//*((u8 *)data)=0xff;
		break;
		case 4://DSW1
			//*((u8 *)data)=0x6f;
		break;
		case 5://DSW1
			//*((u8 *)data)=0x6f;
		break;
	}
	return 0;
}

s32 BlackTiger::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 0:
			//printf("\033[31;47m %c \033[0m",*(u8 *)data);
			//fflush(stdout);
			return BlkTigerSpu::fn_port_w(a,port,data,AM_WRITE|AM_BYTE);
		break;
		case 1:{
			u8 v=(*(u8 *)data) & 0xf;
			//printf("bank %d\n",v);
			memcpy(&_mem[0x8000],&_memory[0x8000+(v*0x4000)],0x4000);
		}
		break;
		case DSW1:
			BlkTigerSpu::fn_port_w(a,port,data,AM_WRITE);
			if(*((u8 *)data)  & 0x20){
				Machine::_status &= ~BLKTIGER_AUDIOCPU_STATE;
			//	*((u8 *)port) &= ~0x20;
			}
			__txlayers._visible = *((u8 *)data) & 0x80 ? 0 : 1;
		//EnterDebugMode();
		break;
		case 5://clear vvblank
			//printf("vblank  %x\n",_pc);
		case 6://whatcchdog
		//printf("watchdog  %x\n",_pc);
		break;
		case 8:
		case 9:{
			_ioreg[a]=*((u8 *)data);
			s16 v = *((s16 *)&_ioreg[8]);
			for(int i=0;i<sizeof(_layers)/sizeof(struct __tile *);i++){
				if(_layers[i] )
					((__layer *)_layers[i])->_scrollx = v;
			}
		}
		break;
		case 0xa:
		case 0xb:{
			_ioreg[a]=*((u8 *)data);
			//printf("scrolly %d %d\n",*((s16 *)&_ioreg[0x8]),*((s16 *)&_ioreg[0xa]));
			s16 v = *((s16 *)&_ioreg[0xa]);
			for(int i=0;i<sizeof(_layers)/sizeof(struct __tile *);i++){
				if(_layers[i] )
					((__layer *)_layers[i])->_scrolly = (s16)v;
			}
		}
		break;
		case 0xc:
			__layers[1]._visible =__layers[0]._visible = *((u8 *)data) & 2 ? 0 : 1;
			//oam
			__oams._visible = *((u8 *)data) & 4 ? 0 : 1;
		break;
		case 0xd:
			_bank= *((u8 *)data);
		break;
		case 0xe:
			for(int i=0;i<2;i++){
				if(_layers[i] )
					_layers[i]->_layout = *((u8 *)data) ? 1 : 0;
			}
		break;
		default:
			printf("IO Write %x %x pc:%x\n",a,((u8 *)data)[0],_pc);
		break;
	}
	return 0;
}

s32 BlackTiger::fn_mem_w(u32 a,pvoid mem,pvoid data,u32 f){
	((u8 *)_gpu_mem)[(a&0xfff)+((_bank&0x3)*BGRAM_BANK_SIZE)]=*((u8 *)data);
	return 0;
}

s32 BlackTiger::fn_mem_r(u32 a,pvoid mem,pvoid data,u32 f){
	*((u8 *)data) = ((u8 *)_gpu_mem)[(a&0xfff)+((_bank&0x3)*BGRAM_BANK_SIZE)];
	return 0;
}

int BlackTiger::OnChangeIRQ(u32,u32 *){
	return -1;
}

#include "z80.cpp.inc"

};