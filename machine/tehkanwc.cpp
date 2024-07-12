#include "tehkanwc.h"

namespace tehkanwc{

#define __Z80F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE) __F(a,## __VA_ARGS__);

//#define __Z80F(a,...)

#define PORT_SYSTEM	0xf802
#define PORT_P1		0xf803
#define PORT_P2		0xf813
#define PORT_P1X	0xf800
#define PORT_P1Y	0xf801
#define PORT_DSW1	0xf870
#define PORT_DSW3	0xf850
#define PORT_DSW2	0xf840

static u8 _key_config[]={11,10,9,8,1,6,12,13};

tehkanwc::tehkanwc() : Machine(MB(80)),Z80CpuShared(),tehkanwcGpu(),tehkanwcSpu(){
	Z80CpuShared::_freq=MHZ(4.608);
	_keys=_key_config;
}

tehkanwc::~tehkanwc(){
}

int tehkanwc::Load(IGame *pg,char *path){
	int res;
	u32 u;

	if(!pg || pg->Open(path,0))
		return -1;
	Reset();

	pg->Read(_memory,KB(256));
	res--;
	memcpy(_mem,_memory,0xc000);
	//memcpy(&_mem[0xc000],&_memory[0x51000],0x1000);
	_cpu.Load(&_memory[0xc000]);
	tehkanwcSpu::Load(&_memory[0x14000]);
	res=0;
A:
	return res;
}

int tehkanwc::Destroy(){
	Machine::Destroy();
	Z80Cpu::Destroy();
	_cpu.Destroy();
	tehkanwcSpu::Destroy();
	tehkanwcGpu::Destroy();
	return 0;
}

int tehkanwc::Reset(){
	_active_cpu=0;
	Machine::Reset();
	Z80Cpu::Reset();
	_cpu.Reset();
	tehkanwcSpu::Reset();
	tehkanwcGpu::Reset();

	_mem[0xf802]=0xf;
	_mem[PORT_P1]=0x20;
	_mem[0xf806]=0xf;
	_mem[0xf810]=0x0;
	_mem[0xf811]=0x0;
	_mem[PORT_P2]=0x20;
	_mem[PORT_DSW2]=0xff;
	_mem[PORT_DSW3]=0xff;
	_mem[PORT_DSW1]=0xf|0x80;

	_mem[PORT_P1X]=0x80;
	_mem[PORT_P1Y]=0x80;

	_mem[0xda00]=0x80;
	return 0;
}

int tehkanwc::Init(){
	if(Machine::Init())
		return -1;
	if(Z80Cpu::Init(&_memory[MB(5)]))
		return -2;

	for(int i =0;i<4;i++){
		SetMemIO_cb(i|0xf800,(CoreMACallback)&tehkanwc::fn_mem_w,(CoreMACallback)&tehkanwc::fn_mem_r);
		SetMemIO_cb(i|0xf810,(CoreMACallback)&tehkanwc::fn_mem_w,(CoreMACallback)&tehkanwc::fn_mem_r);
		SetMemIO_cb(0xf840|(i*0x10),(CoreMACallback)&tehkanwc::fn_mem_w,(CoreMACallback)&tehkanwc::fn_mem_r);
	}
	SetMemIO_cb(0xf806,(CoreMACallback)&tehkanwc::fn_mem_w,(CoreMACallback)&tehkanwc::fn_mem_r);
	SetMemIO_cb(0xf820,(CoreMACallback)&tehkanwc::fn_mem_w,(CoreMACallback)&tehkanwc::fn_mem_r);

	if(_cpu.Init(&_memory[MB(10)]))
		return -3;
	_cpu._cpu=this;
	_cpu.Query(ICORE_QUERY_SET_MACHINE,(IObject *)(ICore *)this);

	//SetIO_cb(7,(CoreMACallback)&tehkanwc::fn_port_w,(CoreMACallback)&tehkanwc::fn_port_r);
	if(tehkanwcSpu::Init(*this))
		return -4;

	_gpu_regs=_mem;
	_gpu_mem=&_mem[0xd000];
	_pal_ram=&_mem[0xd800];
	_sprite_ram=&_mem[0xe800];

	_char_ram=&_memory[0x18000];

	if(tehkanwcGpu::Init())
		return -5;
	for(int i=0xc800;i<0xec00;i++){
		SetMemIO_cb(i,(CoreMACallback)&tehkanwc::fn_shared_mem_w);
		_cpu.SetMemIO_cb(i,(CoreMACallback)&tehkanwc::__cpu::fn_shared_mem_w);
	}
	SetMemIO_cb(0xda00,(CoreMACallback)&tehkanwc::fn_mem_w);
	_cpu.SetMemIO_cb(0xda00,(CoreMACallback)&tehkanwc::fn_mem_w);
	_cpu._ipc=_mem;
	_ipc=&_memory[MB(10)];
	_cpu.Stop();
	AddTimerObj((tehkanwcGpu *)this,381);
	return 0;
}

int tehkanwc::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]="256";
	m["height"]="240";
	tehkanwcGpu::LoadSettings(v);
	tehkanwcSpu::LoadSettings(v);
	return 0;
}

int tehkanwc::Exec(u32 status){
	int ret;

	ret=Z80Cpu::Exec(status);
	__cycles=Z80Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			ret *= -1;
			goto A;
	}
	//_ioreg[7]=__data;
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0);,_ioreg);
	ret=0;
A:
	switch(tehkanwcSpu::Exec(status)){
		case -3:
		//	EnterDebugMode();
			break;
		case -2:
			if(!ret) ret= 0x10000002;
			break;
	}
	switch(_cpu.Exec(status)){
		case -3:
		//	EnterDebugMode();
			break;
		case -2:
			if(!ret)
				return 0x20000002;
		break;
	}
	MACHINE_ONEXITEXEC(status,ret);
}

int tehkanwc::Query(u32 what,void *pv){
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
						//_layers[(u16)id-1]->Save();
						_layers[(u16)id-1]->_visible=id&0xFFFF0000?1:0;
						return 0;
					break;
				}
			}
			return -1;
		case ICORE_QUERY_DBG_MENU:
			{
				char *p = new char[1000];
				((void **)pv)[0]=p;
				memset(p,0,1000);
				for(int i=0;i<3;i++){
					switch(i){
						case 1:
						strcpy(p,"Sprites");
						break;
						case 0:
						sprintf(p,"Layer BG");
						break;
						case 2:
						sprintf(p,"Layer FG");
						break;
					}
					p+=strlen(p)+1;
					*((u32 *)p)=1+i;
					*((u32 *)&p[4])=0x102;
					p+=sizeof(u32)*2;
				}
				strcpy(p,"AY8910 0");
				p+=strlen(p)+1;
				*((u32 *)p)=10;
				*((u32 *)&p[4])=0x202;
				p+=sizeof(u32)*2;
				strcpy(p,"AY8910 1");
				p+=strlen(p)+1;
				*((u32 *)p)=11;
				*((u32 *)&p[4])=0x202;
				p+=sizeof(u32)*2;
				strcpy(p,"MSM5205");
				p+=strlen(p)+1;
				*((u32 *)p)=12;
				*((u32 *)&p[4])=0x202;
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
		case ICORE_QUERY_CPUS:{
			char *p = new char[500];
			if(!p)
				return -1;
			((void **)pv)[0]=p;
			//memset(p,0,500);
			strcpy(p,"CPU");
			p+=4+sizeof(u32);
			strcpy(p,"CPU Audio");
			p+=10+sizeof(u32);
			strcpy(p,"CPU Sub");
			p+=8+sizeof(u32);
			*((u64 *)p)=0;
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
					((void **)pv)[0]=(ICore *)&(tehkanwcSpu::_cpu);
					return 0;
				case 2:
					_active_cpu=2;
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
				strcpy(p->title,"PPU Registers");
				strcpy(p->name,"3105");
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

int tehkanwc::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case (u32)-1:
			if(!OnFrame())
				tehkanwcGpu::Update();
			tehkanwcSpu::Update();
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
			tehkanwcGpu::Update();
			Draw();
			CALLEE(Machine::OnEvent,ev,return,arg);
		case ME_KEYUP:{
				int key;

				CALLEE(Machine::OnEvent,ev,key=,arg);
				if(key < 8)
					_ioreg[0] |= SL(1,key);
				else
					_ioreg[1] |= SL(1,key-8);
			}
			return 0;
		case ME_KEYDOWN:
			{
				int key;

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
			switch(SR(i,24)){
				case 1:
					return tehkanwcSpu::_enterIRQ(ev,0);
				case 2:
					return _cpu._enterIRQ(ev,0);
		//		case 0:
			//		return Z80Cpu::_enterIRQ(ev,0);

			}
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
			if(!Z80Cpu::_enterIRQ(1,0)){
				_cpu._enterIRQ(1,0);
				tehkanwcSpu::_enterIRQ(1,0);
			}
		break;
	}
	return 0;
}

int tehkanwc::Dump(char **pr){
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
#ifdef _DEvELOP
	sprintf(cc,"L:%d C:%u",__line,__cycles);
	strcat(p,cc);
#endif
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
	tehkanwcSpu::_dumpRegisters(p);
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

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3105");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	sprintf(p,"EC00: %04X EC02: %04X F860: %02X F870: %02X\n",*((u16 *)((u8 *)_gpu_regs + 0xec00)),
		*((u16 *)((u8 *)_gpu_regs + 0xec02)),_mem[0xf860],_mem[0xf870]);

	res += strlen(p)+1;

	*pr = c;
	return res;
}

s32 tehkanwc::fn_mem_w(u32 a,pvoid mem,pvoid data,u32){
	if(a==0xda00) return 0;
	switch((u8)a){
		case 0:
		case 1:
		case 0x10://track
		case 0x11:
			_trackball[SR((u8)a,4)].old[a&1] = _trackball[SR((u8)a,4)].pos+ *((u8 *)data);
		case 2:
		case 3:
			return 1;
		break;
		case 0x20:
			//printf("write mem %x %x %x\n",_pc,a,*((u8 *)data));
			//EnterDebugMode();
			tehkanwcSpu::_writeLatch(*((u8 *)data));
		break;
		case 0x40://sub cpu reset
			//printf("write mem %x %x %x\n",_pc,a,*((u8 *)data));
			if(*((u8 *)data)){
				_cpu.Restart();
				//EnterDebugMode();
			}
			else
				_cpu.Stop();
			return 0;
		case 0x50://audio reset
		//	printf("write mem %x %x %x\n",_pc,a,*((u8 *)data));
			tehkanwcSpu::fn_port_w(0x50,0,data,AM_WRITE|AM_BYTE);
			return 0;
		case 0x60://flip_x
		case 0x70://flip_y
		break;
		default:
		//	printf("write mem %x %x\n",_pc,a);
		break;
	}
	return 0;
}

s32 tehkanwc::fn_mem_r(u32 a,pvoid,pvoid data,u32){
	switch((u8)a){
		case 0x0:
		case 0x1:
		case 0x10:
		case 0x11://trackball
			*((u8 *)data)=_trackball[SR((u8)a,4)].pos-_trackball[SR((u8)a,4)].old[a&1];
		break;
		case 0x20:
			*((u8 *)data)=tehkanwcSpu::_readLatch();
		//	EnterDebugMode();
		break;
		case 2:
		case 3:
		case 0x13:
		case 0x40:
		case 0x50:
		case 0x60:
		case 0x70:
		break;
		default:
			printf("read mem %x %x\n",_pc,a);
		break;
	}
	return 0;
}

int tehkanwc::__cpu::Exec(u32 status){
	int ret;

	switch((ret= Z80Cpu::Exec(status))){
		case -1:
		case -2:
			return ret;
	}
	EXECTIMEROBJLOOP(ret,_enterIRQ(i__,0);ret=-3;,_mem);
	return ret;
}

int tehkanwc::__cpu::Load(void *m){
	memcpy(_mem,m,0x8000);
	_mem[0xf860]=0xff;
	return 0;
}

#include "z80.cpp.inc"

TWCPGame::TWCPGame() : FileGame(){
	char c[][16]={"twc-1.bin","twc-2.bin",	"twc-3.bin","twc-4.bin","twc-6.bin",
		"twc-12.bin","twc-8.bin","twc-7.bin","twc-11.bin",
		"twc-9.bin","twc-5.bin"};
	_name="TWCP";
	_machine="twcp";
	for(int i =0;i<sizeof(c)/sizeof(c[0]);i++)
		_files.push_back({c[i],0});
}

TWCPGame::~TWCPGame(){
}

};