#include "1943.h"
#include <map>
#include <vector>
#include <string>

namespace M1943{

//#define __Z80F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE) __F(a,## __VA_ARGS__);

#define __Z80F(a,...)

#define M1943_AUDIOCPU_STATE	8
#define PORT_DSWA	0xc003
#define PORT_DSWB	0xc004

static u8 _key_config[]={11,10,9,8,1,6,12,13};

M1943::M1943() : Machine(MB(30)),Z80Cpu(),M1943Gpu(),M1943Spu(){
	_keys=_key_config;
	Z80Cpu::_freq=MHZ(6);
	_active_cpu=0;
}

M1943::~M1943(){
}

int M1943::Load(IGame *pg,char *path){
	int res;
	u32 u;

	if(!pg || pg->Open(path,0))
		return -1;
	Reset();

	pg->Read(_memory,MB(1),&u);
	res--;
	memcpy(_mem,_memory,0x8000);
	//memcpy(&_mem[0xc000],&_memory[0x51000],0x1000);

	M1943Spu::Load(&_memory[0x28000]);
	M1943Gpu::Load(&_memory[0x31000+0xa8000]);
	res=0;
A:
	return res;
}

int M1943::Destroy(){
	Z80Cpu::Destroy();
	Machine::Destroy();
	M1943Spu::Destroy();
	M1943Gpu::Destroy();
	return 0;
}

int M1943::Reset(){
	Z80Cpu::Reset();
	Machine::Reset();
	M1943Spu::Reset();
	M1943Gpu::Reset();

	_mem[0xc000]=0xff;
	_mem[0xc001]=0xff;
	_mem[0xc002]=0xff;
	_mem[PORT_DSWA]=0xf8;
	_mem[PORT_DSWB]=0xff;

	return 0;
}

int M1943::Init(){
	if(Machine::Init())
		return -1;
	if(Z80Cpu::Init(&_memory[MB(5)]))
		return -2;
	//for(int i =0;i<15;i++)
		//SetIO_cb(i,(CoreMACallback)&M1943::fn_port_w);
	for(int i =0;i<8;i++){
		SetMemIO_cb(i|0xc000,(CoreMACallback)&M1943::fn_mem_w,(CoreMACallback)&M1943::fn_mem_r);
		SetMemIO_cb(i|0xc800,(CoreMACallback)&M1943::fn_mem_w,(CoreMACallback)&M1943::fn_mem_r);
	}
	for(int i =0;i<4;i++){
		SetMemIO_cb(i|0xd800,(CoreMACallback)&M1943::fn_mem_w);
	}
	SetMemIO_cb(0xd806,(CoreMACallback)&M1943::fn_mem_w,(CoreMACallback)&M1943::fn_mem_r);
	//SetIO_cb(7,(CoreMACallback)&M1943::fn_port_w,(CoreMACallback)&M1943::fn_port_r);
	if(M1943Spu::Init(*this))
		return -4;
	_ioreg=&_mem[0xc000];
	_gpu_regs=_ioreg;
	_gpu_mem=&_memory[0x98000];
	_pal_ram=&_mem[0xd400];
	_sprite_ram=&_mem[0xf000];
	_char_ram=&_memory[0x31000];
	if(M1943Gpu::Init())
		return -5;
	AddTimerObj((M1943Gpu *)this,381);
	return 0;
}

int M1943::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	return 0;
}

int M1943::Exec(u32 status){
	int ret;

	ret=Z80Cpu::Exec(status);
	__cycles=Z80Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			ret *= -1;
			goto A;
	}
	_ioreg[7]=__data;
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0);,_ioreg);
	ret=0;
A:
	Machine::_status^=M1943_AUDIOCPU_STATE;
	if((Machine::_status & M1943_AUDIOCPU_STATE)){
		switch(M1943Spu::Exec(status)){
			case -3:
				Machine::_status &= ~M1943_AUDIOCPU_STATE;
			//	EnterDebugMode();
				break;
			case -2:
				if(!ret) return 0x10000002;
		}
	}
	MACHINE_ONEXITEXEC(status,ret);
}

int M1943::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case (u32)-1:
			if(!OnFrame())
				M1943Gpu::Update();
			M1943Spu::Update();
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
			M1943Gpu::Update();
			Draw();
			CALLVA(Machine::OnEvent,ev,ev);
			return ev;
		case ME_KEYUP:
			{
				int key;

				va_start(arg, ev);
				key=va_arg(arg,int);
				key=_keys[key];
				if(key < 8)
					_ioreg[0] |= SL(1,key);
				else
					_ioreg[1] |= SL(1,key-8);
				va_end(arg);
			}
			return 0;
		case ME_KEYDOWN:
			{
				int key;

				va_start(arg, ev);
				key=va_arg(arg,int);
				key=_keys[key];
				if(key < 8)
					_ioreg[0] &= ~SL(1,key);
				else
					_ioreg[1] &= ~SL(1,key-8);
				va_end(arg);
			}
			return 0;
		case 0:{
			va_start(arg, ev);
			u32 i=va_arg(arg,int);
			va_end(arg);
			ev=i&~0xff000000;
			//printf("irq %x\n",i);
			if(i&0xff000000)
				return M1943Spu::_enterIRQ(ev,0);
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
			M1943Spu::_enterIRQ(1,0);
		break;
	}
	return 0;
}

int M1943::Dump(char **pr){
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
	M1943Spu::_dumpRegisters(p);
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

int M1943::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_DBG_LAYER:{
			int res;
			u32 *params;

			res=-3;
			params=*((u32 **)pv);
			if(!params  || (int)params[0] < 0) return -1;

			if(params[1]==0 && params[2]==0){
				params[1]= MAKELONG(256,256);
				params[2]=65536;
				res=0;
			}
			else{
				u32 *p=(u32 *)calloc(1,256*256*sizeof(u16)+10*sizeof(u32));
				if(p){
					*((void **)pv)=p;
					p[0]= MAKELONG(256,256);
					p[1]=10*sizeof(u32);
					_layers[params[0]]->_drawToMemory((u16 *)&p[10],params,*this);
					res=0;
				}
			}
			printf("L %u\n",params[0]);
			return res;
		}
			return  -1;
		case ICORE_QUERY_DBG_OAM:{
			int res;
			u32 *params;

			res=-3;
			params= *((u32 **)pv);
			int idx = params[0];
			if(idx <0)
				return 4;
			M1943Gpu::__oam *o= new __oam();

			if(!o) return -2;
			u32 *p=(u32 *)calloc(1,16*16*sizeof(u16)+10*sizeof(u32));
			if(p){
				*((void **)pv)=p;
				o->Init(0,*this);
			//printf("azzo %d %d\n",o->_char_height,idx);
				p[0]= MAKELONG(o->_char_width,o->_char_height);
				p[1]=10*sizeof(u32);
				o->_drawToMemory((u16 *)&p[10],params,*this);
				res=0;
			}
			delete o;
			return res;
		}
			return -1;
		case ICORE_QUERY_DBG_PALETTE:{
				u32 *data,*p= (u32 *)calloc(sizeof(u32),0x400);
				data=p;
				*((void **)pv)=data;
				*data++=0x10;
				*data++=0x100;
				for(int i=0;i<0x100;i++){
					u16 idx=((u16 *)_palette)[i+0x180];
					u16 col =((u16 *)_palette)[idx];
					u32 c = SL(col&31,19)|SL(col&0x3c0,6)|SR(col&0x7c00,7);
					p[i+0x10]=c;
				}
				return 0;
			}
			return -1;
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
						_layers[(u16)id-1]->_enabled=id&0xFFFF0000?1:0;
						return 0;
					case 10:
					case 11:
						_ym[id&1]._enabled=id&0xFFFF0000?1:0;
						printf("opn enabled %d %d\n",id&1,_ym[id&1]._enabled);
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
					((u32 *)p)[0]=1+i;
					((u32 *)p)[1]=0x102;
					p+=sizeof(u32)*2;
				}
				strcpy(p,"OPN 0");
				p+=strlen(p)+1;
				((u32 *)p)[0]=10;
				((u32 *)p)[1]=0x02;
				p+=sizeof(u32)*2;
				strcpy(p,"OPN 1");
				p+=strlen(p)+1;
				((u32 *)p)[0]=11;
				((u32 *)p)[1]=0x02;
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
						if(Machine::_status & M1943_AUDIOCPU_STATE)
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

s32 M1943::fn_port_r(u32 a,pvoid port,pvoid data,u32){
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

s32 M1943::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 0:
			//printf("\033[31;47m %c \033[0m",*(u8 *)data);
			//fflush(stdout);
			return M1943Spu::fn_port_w(a,port,data,AM_WRITE|AM_BYTE);
		break;
		case 1:{
			u8 v=(*(u8 *)data) & 0xf;
			//printf("bank %d\n",v);
			memcpy(&_mem[0x8000],&_memory[0x8000+(v*0x4000)],0x4000);
		}
		break;
		case PORT_DSWA:
			//M1943Spu::fn_port_w(a,port,data,AM_WRITE);
			if(*((u8 *)data)  & 0x20){
				Machine::_status &= ~M1943_AUDIOCPU_STATE;
			//	*((u8 *)port) &= ~0x20;
			}
			__fglayers._visible = *((u8 *)data) & 0x80 ? 0 : 1;
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
			__bglayers[1]._visible =__bglayers[0]._visible = *((u8 *)data) & 2 ? 0 : 1;
			//oam
			__oams._visible = *((u8 *)data) & 4 ? 0 : 1;
		break;
		case 0xd:
			//_bank= *((u8 *)data);
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

s32 M1943::fn_mem_w(u32 a,pvoid mem,pvoid data,u32 f){
	switch(a){
		case 0xc800:
			M1943Spu::fn_mem_w(a,mem,data,f);
		//	EnterDebugMode();
			//printf("c800 %x\n",*(u8 *)data);
		break;
		case 0xc804:
			memcpy(&_mem[0x8000],&_memory[0x8000+((SR(*(u8 *)data,2)&7)*0x4000)],0x4000);
			_layers[3]->_visible=SR(*(u8 *)data,7);
			return 1;
		case 0xd806:
			_layers[0]->_visible=SR(*(u8 *)data,5);
			_layers[1]->_visible=SR(*(u8 *)data,4);
			_layers[2]->_visible=SR(*(u8 *)data,6);
			return 1;
		case 0xd800:
		case 0xd801:
		case 0xd802:
			__bglayers[1]._scrollx=*((u16 *)(&_mem[0xd800]));
			__bglayers[1]._scrolly=_mem[0xd802];
			return 1;
		case 0xd803:
		case 0xd804:
			__bglayers[0]._scrollx=*((u16 *)(&_mem[0xd803]));
			return 1;
	}
	//((u8 *)_gpu_mem)[(a&0xfff)+((_bank&0x3)*BGRAM_BANK_SIZE)]=*((u8 *)data);
	return 0;
}

s32 M1943::fn_mem_r(u32 a,pvoid mem,pvoid data,u32 f){
	//printf("%s %x\n",__FUNCTION__,a);
	//*((u8 *)data) = ((u8 *)_gpu_mem)[(a&0xfff)+((_bank&0x3)*BGRAM_BANK_SIZE)];
	return 0;
}

M1943Game::M1943Game() : FileGame(){
	char c[][16]={"bme01.12d","bme02.13d","bme03.14d",
		          "bm05.4k","bm.7k","bm04.5h","bm15.10f",
		          "bm16.11f","bm17.12f","bm18.14f",
		          "bm19.10j","bm20.11j","bm21.12j",
		          "bm22.14j","bm24.14k","bm25.14l","bm06.10a",
		          "bm07.11a","bm08.12a","bm09.14a","bm10.10c",
		          "bm11.11c","bm12.12c","bm13.14c","bm14.5f",
		          "bm23.8k","bm1.12a","bm2.13a","bm3.14a","bm5.7f","bm10.7l", "bm9.6l",
		          "bm12.12m","bm11.12l","bm8.8c","bm7.7c","bm4.12c", "bm6.4b"};
	_name="1943";
	_machine="1943";
	for(int i =0;i<sizeof(c)/sizeof(c[0]);i++)
		_files.push_back({c[i],0});
}

M1943Game::~M1943Game(){
}

#include "z80.cpp.inc"

};