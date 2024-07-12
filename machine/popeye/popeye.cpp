#include "popeye.h"
#include <map>
#include <vector>
#include <string>
#include <array>

namespace POPEYE{

//#define __Z80F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE) __F(a,## __VA_ARGS__);

#define __Z80F(a,...)

static u8 _key_config[]={2,3,1,0,4,8,9,10,11,12,13,14,15,16,17,18};

#define PORT_P1 		0
#define PORT_P2 		1
#define PORT_SYSTEM 	2

#define PORT_DSW0		3
#define PORT_DSW1		4

static u32 bitswap(int n,int val...){
	u8 p[32];
	va_list arg;
	u32 res;

	va_start(arg,val);
	for(int  i=0;i<n;i++)
		p[i]=(u8)va_arg(arg,int);
	va_end(arg);
	res=0;
	for(int i=0;i<n;i++){
		if(!(val & BV(i)))
			continue;
		for(int ii=0;ii<n;ii++){
			if(p[ii]==i){
				res |= BV((n-1)-ii);
				break;
			}
		}
	}
	return res;
}

Popeye::Popeye() : Machine(MB(30)),Z80Cpu(),PopeyeSpu(),PopeyeGpu(){
	_keys=_key_config;
	Z80Cpu::_freq=MHZ(4);
	//_dev.prot.val[0]=0;
	//_dev.prot.val[1]=0;
	//cout <<  sizeof(struct __a) << endl;
}

Popeye::~Popeye(){
}

int Popeye::Load(IGame *pg,char *path){
	int res;
	u32 u;

	if(!pg || pg->Open(path,0))
		return -1;
	Reset();
	pg->Read(_memory,MB(1),&u);
	//printf("load %u\n",u);
	res--;
	for (int i = 0; i < 0x8000; i++){
		int  n =  bitswap(16,i,15, 14, 13, 12, 11, 10, 8, 7, 6, 3, 9, 5, 4, 2, 1, 0);
		_mem[i]=bitswap(8,_memory[n^0x3f],3, 4, 2, 5, 1, 6, 0, 7);
	}
	PopeyeGpu::Load(&_memory[0x11000]);
	res=0;
	return res;
}

int Popeye::Destroy(){
	Z80Cpu::Destroy();
	Machine::Destroy();
	PopeyeSpu::Destroy();
	PopeyeGpu::Destroy();
	return 0;
}

int Popeye::Reset(){
	Z80Cpu::Reset();
	Machine::Reset();
	PopeyeSpu::Reset();
	PopeyeGpu::Reset();
	memset(&_dev,0,sizeof(_dev));
	//_dev._ports[0]=0xff;
	//_dev._ports[2]=0xbf;
	_dev._ports[PORT_DSW0]=0x4f;
	_dev._ports[PORT_DSW1]=0x3d;
	return 0;
}

int Popeye::Init(){
	if(Machine::Init())
		return -1;
	if(Z80Cpu::Init(&_memory[MB(5)]))
		return -2;
	for(int i =0;i<4;i++)
		SetIO_cb(i,(CoreMACallback)&Popeye::fn_port_w,(CoreMACallback)&Popeye::fn_port_r);

	SetMemIO_cb(0xe000,(CoreMACallback)&Popeye::fn_mem_w,(CoreMACallback)&Popeye::fn_mem_r);
	SetMemIO_cb(0xe001,(CoreMACallback)&Popeye::fn_mem_w,(CoreMACallback)&Popeye::fn_mem_r);
//	SetMemIO_cb(0x8800,(CoreMACallback)&Popeye::fn_mem_w);
	//for(int i =0;i<0x2000;i++)
		//SetMemIO_cb(0xc000+i,(CoreMACallback)&Popeye::fn_bgmem_w);
	if(PopeyeSpu::Init(*this))
		return -4;
	_ioreg=_dev._ports;
	_gpu_regs=_ioreg;
	_gpu_mem=&_mem[0xa000];
	_pal_ram=&_mem[0xa400];
	_sprite_ram=&_mem[0x8c00];
	_char_ram=&_memory[0x8000];
	if(PopeyeGpu::Init())
		return -5;
	_setBlankArea(448,480,0,-1,60,CCore::_freq);
	AddTimerObj((PopeyeGpu *)this,_scanline_cycles);
	return 0;
}

int Popeye::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	PopeyeGpu::LoadSettings(v);
	PopeyeSpu::LoadSettings(v);
	return 0;
}

int Popeye::Exec(u32 status){
	int ret;

	ret=Z80Cpu::Exec(status);
	__cycles=Z80Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			ret *= -1;
			goto A;
	}
//	_mem[0x8800]=3;//infinite live
	//_ioreg[7]=__data;
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0);,_ioreg);
	ret=0;
A:
	MACHINE_ONEXITEXEC(status,ret);
}

int Popeye::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case ME_ENDFRAME:
			if(!OnFrame())
				PopeyeGpu::Update();
			PopeyeSpu::Update();
			return 0;
		case ME_MOVEWINDOW:
			{
				int  w,h;

				va_start(arg, ev);
				w=va_arg(arg,int);
				w=va_arg(arg,int);
				w=va_arg(arg,int);
				h=va_arg(arg,int);
				CreateBitmap(w,h);
				Draw();
				va_end(arg);
			}
			return 0;
		case ME_REDRAW:
			PopeyeGpu::Update();
			Draw();
			CALLVA(Machine::OnEvent,ev,ev);
			return ev;
		case ME_KEYDOWN:{
				int key;

				CALLEE(Machine::OnEvent,ev,key=,arg);
				if(key < 8)
					_dev._ports[PORT_P1] |= SL(1,key);
				else
					_dev._ports[2] |= SL(1,key-8);
			}
			return 0;
		case ME_KEYUP:{
				int key;

				CALLEE(Machine::OnEvent,ev,key=,arg);
				if(key < 8)
					_dev._ports[PORT_P1] &= ~SL(1,key);
				else
					_dev._ports[2] &= ~SL(1,key-8);
			}
			return 0;
		case 0:{
			va_start(arg, ev);
			u32 i=va_arg(arg,int);
			va_end(arg);
			ev=i&~0xff000000;
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
		case 1:{
			u32 v;
			u8 *p;

			if(_dev.nmi)
				_enterNMI();
			_dev.nmi=0;
			v=_dev.wd.counter;
			if(_dev.wd.enabled){
				v=(v+1) & 15;
			}
			else
				v=0;
			//	if(((v^_dev.wd.counter)&4) != 0)
				//	PopeyeSpu::Reset();
			_dev.wd.counter=v;
			p=&_mem[0x8c00];
			_bglayer._scrollx=*((s16 *)p);
			_bglayer._scrolly=p[2];
			_setPaletteBank(p[3]);
			_dev._vblank ^= 1;
		}
		break;
	}
	return 0;
}

int Popeye::Dump(char **pr){
	int res,i;
	char *c,*cc,*p;
	//u8 *mem;
	//u32 adr;
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
//	adr=di._dumpAddress;
	//pp=&cc[900];

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

	//for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++){
		sprintf(cc,"AY8910 %d\n\n",i+1);
		strcat(p,cc);
/*		for(int n=0;n<sizeof(_pwms._regs);n++){
			sprintf(cc,"%02X:%02X  ",n,_pwms._regs[n]);
			strcat(p,cc);
			if((n&7) == 7)
				strcat(p,"\n");
		}*/
		strcat(p,"\n\n");
	//}
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

int Popeye::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_DBG_LAYER:{
			int res;
			u32 *params;

			res=-3;
			params=*((u32 **)pv);
			if(!params  || (int)params[0] < 0) return -1;

			if(params[1]==0 && params[2]==0){
				params[1]= MAKELONG(512,512);
				params[2]=512;
				res=0;
			}
			else{
				u32 *p=(u32 *)calloc(1,512*512*sizeof(u16)+10*sizeof(u32));
				if(p){
					*((void **)pv)=p;
					memcpy(p,params,10*sizeof(u32));
					p[3]=10*sizeof(u32);
					_layers[params[0]]->_drawToMemory((u16 *)&p[10],p,*this);
					res=0;
				}
			}
			printf("L %u\n",params[0]);
			return res;
		}
			return  -1;
		case ICORE_QUERY_DBG_OAM:{
			int res;

			res=-3;
			int idx = (int)**((int **)pv);
			if(idx <0)
				return 4;
			PopeyeGpu::__oam *o= new __oam();

			if(!o) return -2;
			u32 *p=(u32 *)calloc(1,16*16*sizeof(u16)+10*sizeof(u32));
			if(p){
				u32 params[10];
				params[0]=idx;
				*((void **)pv)=p;
				o->Init(0,*this);

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
				//	u16 idx=((u16 *)_palette)[i+0x180];
					u16 col =((u16 *)_palette)[i];
					u32 c = SL(col&31,19)|SL(col&0x3c0,6)|SR(col&0x7c00,7);
					p[i+0x10]=c;
				}
				return 0;
			}
			return -1;
		case ICORE_QUERY_DBG_MENU_SELECTED:
			{
				u32 id =*((u32 *)pv);
				//printf("id %x\n",id);
				switch((u16)id){
					case 7:
						return -1;
					default:
					//	_layers[(u16)id-1]->Save();
						_layers[(u16)id-1]->_visible=id&0xFFFF0000?1:0;
						return 0;

				}
			}
			return -1;
		case ICORE_QUERY_DBG_MENU:
			{
				char *p = new char[1000];
				((void **)pv)[0]=p;
				memset(p,0,1000);
				for(int i=0;i<3;i++){
					sprintf(p,"Layer %d",i);
					p+=strlen(p)+1;
					*((u32 *)p)=i;
					*((u32 *)&p[4])=i==1 ? 0x200 : 0x100;
					*((u32 *)&p[4])|= 02;
					p+=sizeof(u32)*2;
				}
				strcpy(p,"ay8910 0");
				p+=strlen(p)+1;
				*((u32 *)p)=10;
				*((u32 *)&p[4])=0x302;
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

int Popeye::OnChangeIRQ(u32 a,u32 *){
	u32 nmi=a&1;
//	printf("reg_i %x\n",a);
	if(nmi != _dev.nmi){
		_dev.nmi=nmi;
	}
	_dev.wd.enabled=SR(a&2,1);
	return -1;
}

s32 Popeye::fn_port_r(u32 a,pvoid port,pvoid data,u32){
	switch((u8)a){
		case PORT_SYSTEM:
		if(_dev._vblank)
			_dev._ports[PORT_SYSTEM] &= ~0x10;
		else
			_dev._ports[PORT_SYSTEM] |= 0x10;
		//break;
		case PORT_P1:
		case PORT_P2:
			*((u8 *)data) = _dev._ports[(u8)a];
		break;
		case PORT_DSW0:{
			u8 v;

			if(_pwms._reg==__ay8910::AY_PORTA){
				u8 dsw1=SR(_dev._ports[PORT_DSW1],
					SR(_pwms._regs[__ay8910::AY_PORTB] & 0xe,1));
				v=(_dev._ports[PORT_DSW0] & 0x7f) | SL(dsw1,7);
			}
			else
				_pwms.read(&v);
			*((u8 *)data)=v;
		}
		break;
		default:
			printf("%s %x %x pc:%x\n",__FUNCTION__,a,((u8 *)data)[0],_pc);
		break;
	}
	return 1;
}

s32 Popeye::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		default:
		break;
		case 0:
			_pwms._reg=*((u8 *)data);
		break;
		case 1:
			_pwms.write(*((u8 *)data));
		break;
	}
	return 0;
}

s32 Popeye::fn_mem_w(u32 a,pvoid mem,pvoid data,u32 f){
	//printf("%s %x\n",__FUNCTION__,a);
	switch(a){
		case 0xe000:
			_dev.prot.shift=*((u8 *)data) & 7;
		break;
		case 0xe001:
			_dev.prot.val[0]=_dev.prot.val[1];
			_dev.prot.val[1]=*((u8 *)data);
		break;
		case 0x8800:
			*((u8 *)data)=4;
			return 1;
	}
	//((u8 *)_gpu_mem)[(a&0xfff)+((_bank&0x3)*BGRAM_BANK_SIZE)]=*((u8 *)data);
	return 0;
}

s32 Popeye::fn_mem_r(u32 a,pvoid mem,pvoid data,u32 f){
	//printf("%s %x\n",__FUNCTION__,a);
	switch(a){
		case 0xe000:{
			u8 val = SL(_dev.prot.val[1],_dev.prot.shift);
			val |= SR(_dev.prot.val[0],8-_dev.prot.shift);
			*((u8 *)data)=val;
		}
		break;
	}
	//*((u8 *)data) = ((u8 *)_gpu_mem)[(a&0xfff)+((_bank&0x3)*BGRAM_BANK_SIZE)];
	return 0;
}

PopeyeGame::PopeyeGame() : FileGame(){
	char c[][16]={"C-7A","C-7B","C-7C","C-7E","V-5N","V-1E","V-1F","V-1J","V-1K","PROM-CPU.4A","PROM-CPU.3A","PROM-CPU.5B",
		"PROM-CPU.5A","PROM-VID.7J"};
	_name="Popeye";
	_machine="popeye";
	for(int i =0;i<sizeof(c)/sizeof(c[0]);i++)
		_files.push_back(__item(c[i]));
}

PopeyeGame::~PopeyeGame(){
}

#include "z80.cpp.inc"

};