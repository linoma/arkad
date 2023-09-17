#include "blacktiger.h"

namespace blktiger{

#define __Z80F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE) __F(a,## __VA_ARGS__);

#define BLKTIGER_AUDIOCPU_STATE	8

static u8 _keys[]={11,10,9,8,1,6,12,13};

BlackTiger::BlackTiger() : Machine(MB(30)),Z80Cpu(),BlkTigerGpu(),BlkTigerSpu(){
	Z80Cpu::_freq=MHZ(6);
	_active_cpu=0;
}

BlackTiger::~BlackTiger(){
}

int BlackTiger::Load(IGame *pg,char *path){
	FILE *fp;
	int res;
	u32 u;

	if(!pg || pg->Open(path))
		return -1;
	Reset();

	pg->Read(_memory,889856,&u);
	res--;
	memcpy(_mem,_memory,0x8000+0x4000);
	//memcpy(&_mem[0xc000],&_memory[0x51000],0x1000);
	if(Z80Cpu::Load(0))
		goto A;
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
	Z80Cpu::Reset();
	Machine::Reset();
	BlkTigerSpu::Reset();
	BlkTigerGpu::Reset();

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

	m["width"]="256";
	m["height"]="224";
	return 0;
}

int BlackTiger::Exec(u32 status){
	int ret;

	ret=Z80Cpu::Exec(status);
	__cycles=Z80Cpu::_cycles;
	switch(ret){
		case -1:
			return 1;
		case -2:
			return 2;
	}
	_ioreg[7]=__data;
	EXECTIMEROBJLOOP(ret,OnEvent(i,0);,_ioreg);
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
	if((BVT(Machine::_status,1) && BT(status,S_DEBUG)) || BT(Machine::_status,2)){
		BC(Machine::_status,1|2);
		return 3;
	}
	return 0;
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
			return CALLVA(Machine::OnEvent,ev);
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

	if(pr)
		*pr=0;
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
	adr=_dumpAddress;
	pp=&cc[900];

	//mem=_mem+adr;
	((CCore *)cpu)->_dumpMemory(p,NULL,adr);
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
			__oams[0]._visible = *((u8 *)data) & 4 ? 0 : 1;
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

Z80Cpu::Z80Cpu(void *m) : CCore(m){
	_regs=NULL;
	_ioreg=NULL;
	_freq=MHZ(3.579545);
	_portfnc_write_mem=_portfnc_read_mem=NULL;
}

Z80Cpu::~Z80Cpu(){
	Destroy();
}

int Z80Cpu::Destroy(){
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int Z80Cpu::Reset(){
	CCore::Reset();
	_pc=0;
	_opcode=0;
	_irq_pending=0;
	if(_regs)
		memset(_regs,0,0x120);
	return 0;
}

int Z80Cpu::Init(void *m){
	u32 n=(0x120) + (0x100*sizeof(CoreMACallback)*2) + (0x10000*sizeof(CoreMACallback)*2);
	if(!(_regs=new u8[n]))
		return -1;
	_ioreg=_regs+0x20;
	_portfnc_write = (CoreMACallback *)&_ioreg[0x100];
	_portfnc_read = &_portfnc_write[0x100];
	_portfnc_write_mem=&_portfnc_read[0x100];
	_portfnc_read_mem=&_portfnc_write_mem[0x10000];
	memset(_regs,0,n);
	_mem=(u8 *)m;
	return 0;
}

int Z80Cpu::SetIO_cb(u32 adr,CoreMACallback a,CoreMACallback b){
	if(_portfnc_write)
		_portfnc_write[(u8)adr]=a;
	if(_portfnc_read)
		_portfnc_read[(u8)adr]=b;
	return 0;
}

int Z80Cpu::SetMemIO_cb(u32 adr,CoreMACallback a,CoreMACallback b){
	if(_portfnc_write_mem)
		_portfnc_write_mem[(u16)adr]=a;
	if(_portfnc_read_mem)
		_portfnc_read_mem[(u16)adr]=b;
	return 0;
}

#define Z80LOGD(a,...)	__S(a,## __VA_ARGS__);

static char pr[][3]={"BC","DE","HL","SP"};
static char prs[][3]={"B","C","D","E","H","L","","A"};
static char prp[][3]={"BC","DE","HL","AF"};
static char jpc[][3]={"NZ","Z","NC","C","PO","PE","P","M"};

#define JMPcc(a)\
int i=0;\
switch(SR(_opcode & 0x38,3)){\
	case 0:\
		if(!(REG_F & Z_BIT))\
			i=1;\
	break;\
	case 1:\
		if(REG_F & Z_BIT)\
			i=1;\
	break;\
	case 2:\
		if(!(REG_F & C_BIT))\
			i=1;\
	break;\
	case 3:\
		if(REG_F & C_BIT)\
			i=1;\
	break;\
	default:\
		printf("JPU %x\n\n\n",SR(_opcode,3) & 7);\
		EnterDebugMode();\
	break;\
	case 0x6:\
		if(!(REG_F & S_BIT))\
			i=1;\
	break;\
	case 7:\
		if(REG_F & S_BIT)\
			i=1;\
	break;\
}

//e880
int Z80Cpu::_exec(u32 status){
	int res=1;
	u32 ipc=1;

	RLPC(_pc,_opcode);
	switch((u8)_opcode){
		case 0:
			_opcode=(u8)_opcode;
			__Z80F(NOARG,"NOP");
		break;
		case 0x1:
		case 0x11:
		case 0x21:
		case 0x31:{
			//LD BC,DE,HL,SP
			u16 v;

			_opcode &= 0xFFFFFF;
			v=(u16)SR(_opcode,8);
			ipc+=2;
			res++;
			u8 i = SR(_opcode&0x30,3);
			if(i==6)
				i=REGI_SP;
			REG16_(i)=v;
			__Z80F(%s\x2c%04XH,"LD",pr[SR(_opcode,4)&3],v);
		}
		break;
		case 2:
			_opcode=(u8)_opcode;
			WB(REG16_(REGI_BC),REG_A);
			__Z80F([BC]\x2c A,"LD");
		break;
		case 3:
		case 0x13:
		case 0x23:
		case 0x33:{
			_opcode=(u8)_opcode;
			u8 i = SR(_opcode&0x30,3);
			REG16_(i) = REG16_(i)+1;
			__Z80F(%s,"INC",pr[i/2]);
		}
		break;
		case 0x4:
		case 0xC:
		case 0x14:
		case 0x1C:
		case 0x24:
		case 0x2c:
		case 0x3c:{
			u8 v,vv;

			_opcode=(u8)_opcode;
			v=REG_(SR(_opcode,3)&7);
			vv=1;
			STATUSFLAGS("addb %0,%%cl",v,vv,v);
			REG_(SR(_opcode,3)&7)=v;
			REG_F &= ~N_BIT;
			__Z80F(%s,"INC",prs[SR(_opcode,3)&7]);
		}
		break;
		case 0x12:{
			u8 v;
			_opcode=(u8)_opcode;
			RB(REG16_(REGI_DE),v);
			REG_A=v;
			__Z80F([DE]\x2c A,"LD");
		}
		break;
		case 0x34:{
			u8 v,mm;

			_opcode=(u8)_opcode;
			res+=2;
			RB(REG16_(REGI_HL),v);
			mm=1;
			STATUSFLAGS("addb %0,%%cl",v,v,mm);
			WB(REG16_(REGI_HL),v);
			REG_F &= ~N_BIT;
			__Z80F([HL],"INC");
		}
		break;
		case 0x35:{
			u8 v,mm;

			_opcode=(u8)_opcode;
			res+=2;
			RB(REG16_(REGI_HL),v);
			mm=1;
			STATUSFLAGS("subb %0,%%cl",v,v,mm);
			WB(REG16_(REGI_HL),v);
			REG_F |=N_BIT;
			__Z80F([HL],"DEC");
		}
		break;
		case 0x6:
		case 0xe:
		case 0x16:
		case 0x1e:
		case 0x26:
		case 0x2e:
		case 0x3e:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			REG_(SR(_opcode,3)&7) = SR(_opcode,8);
			res++;
			__Z80F(%s\x2c%XH,"LD",prs[SR(_opcode,3)&7],SR(_opcode,8));
		}
		break;
		case 7:{
			u8  v;

			REG_F &= ~(A_BIT|N_BIT);
			_opcode=(u8)_opcode;
			v=REG_A;
			REG_A=SL(v,1)|SR(v,7);
			if(v&0x80)
				REG_F |= C_BIT;
			else
				REG_F &= ~C_BIT;
			__Z80F(NOARG,"RLCA");
		}
		break;
		case 0x8:{
			u16 v=REG16_(REGI_AF+REG_I2);
			REG16_(REGI_AF+REG_I2) = REG16_(REGI_AF);
			REG16_(REGI_AF)=v;
			__Z80F(AF\x2c AF\x27,"EX");
		}
		break;
		case 0xa:{
			u8 v;

			_opcode=(u8)_opcode;
			RB(REG16_(REGI_BC),v);
			REG_A=v;
			__Z80F(A\x2c[BC],"LD");
		}
		break;
		case 0xF:{
			u8 v = REG_A;
			REG_A=SR(v,1)|SL(v,7);
			if(v&1)
				REG_F |= C_BIT;
			else
				REG_F &=~C_BIT;
			REG_F &= ~(A_BIT|N_BIT);
			_opcode = (u8)_opcode;
			__Z80F(NOARG,"RRCA");
		}
		break;
		case  0x18:{
			s8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			ipc += v;
			__Z80F(%XH,"JR",_pc+ipc);
		}
		break;
		case 0x9:
		case 0x19:
		case 0x29:
		case 0x39:{
			u16 r;
			u8 i = SR(_opcode&0x30,3);
			if(i==6)
				i=REGI_SP;
			STATUSFLAGS16("addw %0,%%cx\n",r,REG16_(i),REG16_(REGI_HL));
			REG16_(REGI_HL) = r;
			REG_F &=~N_BIT;
			res+=2;
			_opcode=(u8)_opcode;
			__Z80F(HL\x2c%s,"ADD",pr[SR(_opcode&0x30,4)]);
		}
		break;
		case 0x10:{
			s8  d;

			REG_(REGI_B) = REG_(REGI_B)-1;
			ipc++;
			_opcode=(u16)_opcode;
			d=SR(_opcode,8);
			__Z80F(%XH,"DJNZ",_pc+d+2);
			res+=2;
			if(REG_(REGI_B))
				ipc += d;
		}
		break;
		case 0x1a:{
			u8 v;

			_opcode=(u8)_opcode;
			RB(REG16_(REGI_DE),v);
			REG_A=v;
			res+=2;
			__Z80F(A\x2c [DE],"LD");
		}
		break;
		case 0x22:{
			u16 v;

			_opcode = _opcode&0xFFFFFF;
			ipc+=2;
			res+=2;
			v = (u16)SR(_opcode,8);
			WW(v,REG16_(REGI_HL));
			__Z80F([%04XH]\x2cHL,"LD",v);
		}
		break;
		case 0x20:{
			s8 d;

			ipc++;
			_opcode=(u16)_opcode;
			d=(s8)SR(_opcode,8);
			res++;
			__Z80F(NZ\x2c%XH,"JR",_pc+d- 2);
			if(!(REG_F & Z_BIT)){
				res++;
				ipc+=d;
			}
		}
		break;
		case  0x27:{
			int tmp = REG_A;

			if ( ! ( REG_F & N_BIT ) ) {
				if ( ( REG_F & A_BIT ) || ( tmp & 0x0F ) > 9 )
					tmp += 6;
				if ( ( REG_F & C_BIT ) || tmp > 0x9F )
					tmp += 0x60;
			}
			else {
				if ( REG_F & A_BIT ) {
					tmp -= 6;
					if (!( REG_F & C_BIT ) )
						tmp &= 0xFF;
				}
				if ( REG_F & C_BIT )
					tmp -= 0x60;
			}
			REG_F &= ~(A_BIT|Z_BIT|C_BIT);
			if(tmp & 0x100)
				REG_F |= C_BIT;
			if(!(REG_A = tmp))
				REG_F |= Z_BIT;
			_opcode=(u8)_opcode;
			__Z80F(NOARG,"DAA");
		}
		break;
		case 0x28:{
			s8 d;

			_opcode=(u16)_opcode;
			ipc++;
			d=(s8)SR(_opcode,8);
			__Z80F(Z\x2c%XH,"JR",_pc+d+ipc);
			if(REG_F & Z_BIT){
				ipc += d;
				res+=4;
			}
		}
		break;
		case 0x2a:{
			u16 v =(u16)SR(_opcode,8);
			ipc+=2;
			res+=2;
			_opcode &= 0xFFFFFF;
			RW(v,REG16_(REGI_HL));
			__Z80F(HL\x2c[%04XH],"LD",v);
		}
		break;
		case 0x2F:
			_opcode=(u8)_opcode;
			REG_A=~REG_A;
			REG_F |= N_BIT|A_BIT;
			__Z80F(NOARG,"CPL");
		break;
		case 5:
		case 0xd:
		case 0x15:
		case 0x1d:
		case 0x25:
		case 0x2d:
		case 0x3d:{
			u8 mm,*p;

			_opcode=(u8)_opcode;
			mm=1;
			p=&REG_(SR(_opcode,3)&7);
			STATUSFLAGS("subb %0,%%cl",mm,*p,mm);
			*p=mm;
			REG_F |=N_BIT;
			__Z80F(%s,"DEC",prs[SR(_opcode,3)&7]);
		}
		break;
		case 0xb:
		case 0x1b:
		case 0x2b:
		case 0x3b:{
			u8 i=SR(_opcode&0x30,3);
			if(i==6) i =REGI_SP;
			REG16_(i)=REG16_(i) -1;
			_opcode=(u8)_opcode;
			__Z80F(%s,"DEC",pr[SR(_opcode&0x30,4)]);
		}
		break;
		case 0x30:{
			s8 d;

			_opcode=(u16)_opcode;
			ipc++;
			d=(s8)SR(_opcode,8);
			__Z80F(NC\x2c%XH,"JR",_pc+d + 2);
			if(!(REG_F & C_BIT)){
				ipc += d;
				res+=4;
			}
		}
		break;
		case 0x32:
			_opcode &= 0xFFFFFF;
			ipc+=2;
			WB(SR(_opcode,8),REG_A);
			__Z80F([%XH]\x2c A,"LD",SR(_opcode,8));
		break;
		case 0x36:
			res+=2;
			_opcode=(u16)_opcode;
			ipc++;
			WB(REG16_(REGI_HL),SR(_opcode,8));
			__Z80F([HL]\x2c %XH,"LD",SR(_opcode,8));
		break;
		case 0x37:
			REG_F|=C_BIT;
			_opcode=(u8)_opcode;
			__Z80F(NOARG,"SCF");
		break;
		case 0x38:{
			s8 d;

			_opcode=(u16)_opcode;
			ipc++;
			d=(s8)SR(_opcode,8);
			__Z80F(C\x2c%XH,"JR",_pc+d + 2);
			if((REG_F & C_BIT)){
				ipc += d;
				res+=4;
			}
		}
		break;
		case 0x3a:
			ipc+=2;
			_opcode&=0xffffff;
			RB(SR(_opcode,8),REG_A);
			__Z80F(A\x2c [%xH],"LD",SR(_opcode,8));
		break;
		case 0x46:
		case 0x4e:
		case 0x56:
		case 0x5e:
		case 0x66:
		case 0x6e:
		case 0x76:
		case 0x7e:{
			u8 v;

			_opcode=(u8)_opcode;
			RB(REG16_(REGI_HL),v);
			REG_(SR(_opcode,3)&7)=v;
			__Z80F(%s\x2c[HL],"LD",prs[SR(_opcode,3)&7]);
		}
		break;
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x77:
			_opcode=(u8)_opcode;
			WB(REG16_(REGI_HL),REG_(_opcode&7));
			__Z80F([HL]\x2c%s,"LD",prs[_opcode&7]);
		break;
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x87:{
			u8 mm;

			_opcode=(u8)_opcode;
			mm=REG_(_opcode&7);
			STATUSFLAGS("addb %0,%%cl",mm,REG_A,mm);
			REG_A=mm;
			REG_F &= ~N_BIT;
			__Z80F(A\x2c %s,"ADD",prs[_opcode&7]);
		}
		break;
		case 0x86:{
			u8 v;

			RB(REG16_(REGI_HL),v);
			STATUSFLAGS("addb %0,%%cl",v,REG_A,v);
			REG_A=v;
			REG_F &=~N_BIT;
			res++;
			_opcode=(u8)_opcode;
			__Z80F(A\x2c [HL],"ADD");
		}
		break;
		case 0x88:
		case 0x89:
		case 0x8a:
		case 0x8b:
		case 0x8c:
		case 0x8d:
		case 0x8f:{
			ipc++;
			_opcode=(u16)_opcode;
			__Z80F(NOARGG,"UNK");
			EnterDebugMode();
		}
		break;
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x97:{
			u8 mm;

			mm=REG_(_opcode&7);
			STATUSFLAGS("subb %0,%%cl",mm,REG_A,mm);
			REG_A=mm;
			REG_F |= N_BIT;
			_opcode=(u8)_opcode;
			__Z80F(%s,"SUB",prs[_opcode&7]);
		}
		break;
		case 0xa0:
		case 0xa1:
		case 0xa2:
		case 0xa3:
		case 0xa4:
		case 0xa5:
		case 0xa7:{
			u8 mm;

			_opcode=(u8)_opcode;
			mm=REG_(_opcode&7);
			STATUSFLAGS("andb %0,%%cl",mm,REG_A,mm);
			REG_A=mm;
			__Z80F(%s,"AND",prs[_opcode&7]);
		}
		break;
		case  0xa6:{
			u8 v;

			_opcode=(u8)_opcode;
			res+=2;
			RB(REG16_(REGI_HL),v);
			STATUSFLAGS("andb %0,%%cl",v,REG_A,v);
			REG_A=v;
			__Z80F([HL],"AND");
		}
		break;
		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
		case 0xac:
		case 0xad:
		case 0xaf:{
			u8 v;

			_opcode=(u8)_opcode;
			v=REG_(_opcode&7);
			STATUSFLAGS("xorb %0,%%cl",v,REG_A,v);
			REG_A=v;
			__Z80F(%s,"XOR",prs[_opcode&7]);
		}
		break;
		case 0xae:{
			u8 v;

			_opcode=(u8)_opcode;
			RB(REG16_(REGI_HL),v);
			STATUSFLAGS("xorb %0,%%cl",v,REG_A,v);
			REG_A=v;
			__Z80F([HL],"XOR");
		}
		break;
		case 0xb0:
		case 0xb1:
		case 0xb2:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb7:{
			u8 v;

			_opcode=(u8)_opcode;
			v=REG_(_opcode&7);
			STATUSFLAGS("orb %0,%%cl",v,REG_A,v);
			REG_A=v;
			__Z80F(%s,"OR",prs[_opcode&7]);
		}
		break;
		case 0xb6:{
			u8 v;

			_opcode=(u8)_opcode;
			RB(REG16_(REGI_HL),v);
			STATUSFLAGS("orb %0,%%cl",v,REG_A,v);
			REG_A=v;
			__Z80F([HL],"OR");
		}
		break;
		case 0xbe:{
			u8 mm;

			_opcode=(u8)_opcode;
			res++;
			RB(REG16_(REGI_HL),mm);
			STATUSFLAGS("subb %0,%%cl",mm,REG_A,mm);
			__Z80F([HL],"CP");
		}
		break;
		case 0xb8:
		case 0xb9:
		case 0xba:
		case 0xbb:
		case 0xbd:
		case 0xbc:
		case 0xbf:{
			u8 mm;

			_opcode=(u8)_opcode;
			mm=REG_(_opcode & 7);
			STATUSFLAGS("subb %0,%%cl",mm,REG_A,mm);
			__Z80F(%s,"CP",prs[_opcode&7]);
		}
		break;
		case 0xcb:
			_opcode = (u16)_opcode;
			ipc++;
			switch(SR(_opcode,8)){
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 7:{//rlc
					u8 v =REG_(SR(_opcode,8)&7);
					REG_F &= ~(N_BIT|A_BIT|Z_BIT|C_BIT|S_BIT);
					if(!(REG_(SR(_opcode,8)&7)= SL(v,1)|SR(v,7)))
						REG_F|=Z_BIT;
					if(v&0x80)
						REG_F |= C_BIT;
					if((v=REG_(SR(_opcode,8)&7)) & 0x80)
						REG_F |= S_BIT;
					PARITYFLAG(v);
					res++;
					__Z80F(%s,"RLC",prs[SR(_opcode,8)&7]);
				}
				break;
				case 0x16:{
					u8 vv,v;

					RB(REG16_(REGI_HL),v);
					REG_F &= ~(N_BIT|A_BIT|Z_BIT|S_BIT);
					if(!(vv= SL(v,1)|(REG_F & C_BIT)))
						REG_F|=Z_BIT;
					if(!(v&0x80))
						REG_F &= ~C_BIT;
					else
						REG_F |= C_BIT;
					if(vv&0x80)
						REG_F |= S_BIT;
					res++;
					WB(REG16_(REGI_HL),vv);
					__Z80F([HL],"RL");
				}
				break;
				case 0x18:
				case 0x19:
				case 0x1a:
				case 0x1b:
				case 0x1c:
				case 0x1d:
				case 0x1f:{
					u8 v = REG_(SR(_opcode,8)&7);
					REG_F&=~(N_BIT|A_BIT|Z_BIT|S_BIT);
					REG_(SR(_opcode,8)&7) = SR(v,1)|(REG_F&C_BIT ? 0x80 : 0);
					if(!(v&1))
						REG_F &= ~C_BIT;
					else
						REG_F |= C_BIT;
					if(!(v=REG_(SR(_opcode,8)&7)))
						REG_F |= Z_BIT;
					if(v&0x80)
						REG_F |= S_BIT;
					res++;
					__Z80F(%s,"RR",prs[SR(_opcode,8)&7]);
				}
				break;
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x13:
				case 0x14:
				case 0x15:
				case 0x17:{//RL
						u8 v =REG_(SR(_opcode,8)&7);
						REG_F &= ~(N_BIT|A_BIT|Z_BIT|S_BIT);

						if(!(REG_(SR(_opcode,8)&7)= SL(v,1)|(REG_F & C_BIT)))
							REG_F|=Z_BIT;

						if(!(v&0x80))
							REG_F &= ~C_BIT;
						else
							REG_F |= C_BIT;
						if(REG_(SR(_opcode,8)&7) & 0x80)
							REG_F |= S_BIT;
						res++;
						__Z80F(%s,"RL",prs[SR(_opcode,8)&7]);
					}
				break;
				case 0x20:
				case 0x21:
				case 0x22:
				case 0x23:
				case 0x24:
				case 0x25:
				case 0x27:{//SLA
						u8 v = REG_(SR(_opcode,8)&7);
						REG_F &= ~(A_BIT|C_BIT|Z_BIT|N_BIT|S_BIT);
						if((v&0x80))
							REG_F |= C_BIT;
						v=REG_(SR(_opcode,8)&7)=SL((s8)v,1);
						if(!v)
							REG_F|=Z_BIT;
						else if(v&0x80)
							REG_F|=S_BIT;
						res++;
						__Z80F(%s,"SLA",prs[SR(_opcode,8)&7]);
					}
				break;
				case 0x38:
				case 0x39:
				case 0x3a:
				case 0x3b:
				case 0x3c:
				case 0x3d:
				case 0x3f:{//srl
					u8 v = REG_(SR(_opcode,8)&7);
					REG_F &= ~(A_BIT|C_BIT|Z_BIT|N_BIT|S_BIT);
					if((v&1))
						REG_F |= C_BIT;
					v=REG_(SR(_opcode,8)&7)=SR(v,1);
					if(!v)
						REG_F |= Z_BIT;
					res++;
					__Z80F(%s,"SRL",prs[SR(_opcode,8)&7]);
				}
				break;
				case 0x46:{
					u8 v;

					RB(REG16_(REGI_HL),v);
					if(BT(v,BV(SR(_opcode&0x3800,11))))
						REG_F &= ~Z_BIT;
					else
						REG_F |= Z_BIT;
					REG_F &= ~N_BIT;
					REG_F |= A_BIT;
					__Z80F(%d\x2c [HL],"BIT",SR(_opcode&0x3800,11));
				}
				break;
				case 0xc6:
				case 0xce:
				case 0xd6:
				case 0xde:
				case 0xe6:
				case 0xee:
				case 0xf6:
				case 0xfe:
					EnterDebugMode();
				break;
				default:
					if(SR(_opcode,8) >= 0x3f && SR(_opcode,8) < 0x80 && (SR(_opcode,8) & 7)  != 6){
						if(BT(REG_(SR(_opcode,8)&7),BV(SR(_opcode&0x3800,11))))
							REG_F &= ~Z_BIT;
						else
							REG_F |= Z_BIT;
						REG_F &= ~N_BIT;
						REG_F |= A_BIT;
						__Z80F(%d\x2c%s,"BIT",SR(_opcode&0x3800,11),prs[SR(_opcode,8)&7]);
					}
					else if(SR(_opcode,8) > 0x7f && SR(_opcode,8) < 0xc0 && (SR(_opcode,8) & 7)  != 6){
						REG_(SR(_opcode,8)&7) &= ~BV(SR(_opcode&0x3800,11));
						__Z80F(%d\x2c%s,"RES",SR(_opcode&0x3800,11),prs[SR(_opcode,8)&7]);
					}
					else if(SR(_opcode,8) > 0xbf && SR(_opcode,8) < 0x100 && (SR(_opcode,8) & 7)  != 6){
						REG_(SR(_opcode,8)&7) |= BV(SR(_opcode&0x3800,11));
						__Z80F(%d\x2c%s,"SET",SR(_opcode&0x3800,11),prs[SR(_opcode,8)&7]);
					}
					else{
						printf("UNK %x\n",_opcode);
						EnterDebugMode();
					}
				break;
			}
		break;
		case 0xce:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			v+=(REG_F & C_BIT?1:0);
			STATUSFLAGS("addb %0,%%cl",v,REG_A,v);
			REG_A=v;
			__Z80F(A\x2c%02XH,"ADC",v);
		}
		break;
		case 0xcd:
			res += 4;
			REG_SP -= 2;
			WW(REG_SP,_pc+3);
			STORECALLLSTACK(_pc+3);
			ipc=0;
			_opcode &= 0xFFFFFF;
			__Z80F(%04XH,"CALL",SR(_opcode,8));
			_pc=SR(_opcode,8);
		break;
		case 0xd3:{//_opcode=(u8)_opcode;
			u8 v,w;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			__data=REG_A;
			w=0;
			if(_portfnc_write[v]){
				if((((Z80Cpu *)this)->*_portfnc_write[v])(v,&_ioreg[v],&__data,AM_WRITE|AM_BYTE))
					w=1;
			}
			if(w)
				_ioreg[v]=__data;
			__Z80F([%XH]\x2c A,"OUT",v);
		}
		break;
		case  0xd6:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			STATUSFLAGS("subb %0,%%cl",v,REG_A,v);
			REG_A=v;
			REG_F |= N_BIT;
			__Z80F(%02x,"SUB",v);
			res++;
		}
		break;
		case 0xc0:
		case 0xc8:
		case 0xd0:
		case 0xd8:
		case 0xe0:
		case 0xe8:
		case 0xf0:
		case 0xf8:{
			_opcode=(u8)_opcode;
			__Z80F(%s,"RET",jpc[SR(_opcode,3)&7]);
			JMPcc(SR(_opcode,3)&7);
			if(i){
				RW(REG_SP,_pc);
				REG_SP+=2;
				ipc--;
				res+=2;
				LOADCALLSTACK(_pc+ipc);
			}
		}
		break;
		case 0xc1:
		case 0xd1:
		case 0xe1:
		case 0xf1:{
			//POP BC DE HL AF
			RW(REG_SP,REG16_(SR(_opcode&0x30,3)));
			REG_SP+=2;
			res+=2;
			_opcode=(u8)_opcode;
			__Z80F(%s,"POP",prp[SR(_opcode&0x30,4)]);
		}
		break;
		case 0xc3:
			ipc=0;
			_opcode&=0xffffff;
			res+=2;
			_pc=SR(_opcode,8);
			__Z80F(%XH,"JP",_pc);
		break;
		case 0xc4:
		case 0xcc:
		case 0xd4:
		case 0xdc:
		case 0xe4:
		case 0xec:
		case 0xf4:
		case 0xfc:{
			_opcode&=0xffffff;
			ipc+=2;
			res+=2;
			JMPcc(SR(_opcode,3)&7);
			if(i){
				REG_SP -= 2;
				WW(REG_SP,_pc+3);
				STORECALLLSTACK(_pc+3);
				ipc=0;
				res+=2;
				_pc=SR(_opcode,8);
			}
			__Z80F(%s %XH,"CALL",jpc[SR(_opcode,3)&7],SR(_opcode,8));
		}
		break;
		case 0xc5:
		case 0xd5:
		case 0xe5:
		case 0xf5:{
			//PUSH BC DE HL AF
			REG_SP -= 2;
			u8 i = SR(_opcode&0x30,3);
			WW(REG_SP,REG16_(i));
			res+=2;
			_opcode = (u8)_opcode;
			__Z80F(%s,"PUSH",prp[i/2]);
		}
		break;
		case 0xc6:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			STATUSFLAGS("addb %0,%%cl",v,REG_A,v);
			REG_A=v;
			REG_F &=~N_BIT;
			__Z80F(A\x2c %02x,"ADD",SR(_opcode,8));
		}
		break;
		case 0xc7:
		case 0xcf:
		case 0xd7:
		case 0xdf:
		case 0xe7:
		case 0xef:
		case 0xf7:
		case 0xFF:
			_opcode=(u8)_opcode;
			REG_SP -= 2;
			WW(REG_SP,_pc+1);
			ipc=0;
			STORECALLLSTACK(_pc+1);
			_pc=_opcode & 0x38;
			res+=2;
			__Z80F(%XH,"RST",SR(_opcode,3)&7);
		break;
		case 0xc9:
			ipc--;
			_opcode=(u8)_opcode;
			__Z80F(NOARG,"RET");
			RW(REG_SP,_pc);
			REG_SP+=2;
			LOADCALLSTACK(_pc+ipc);
		break;
		case 0xd9:{
			u16 v;

			_opcode=(u8)_opcode;
			v=REG16_(REGI_BC);
			REG16_(REGI_BC)=REG16_(REG_I2+REGI_BC);
			REG16_(REG_I2+REGI_BC) = v;

			v=REG16_(REGI_DE);
			REG16_(REGI_DE)=REG16_(REG_I2+REGI_DE);
			REG16_(REG_I2+REGI_DE) = v;

			v=REG16_(REGI_HL);
			REG16_(REGI_HL)=REG16_(REG_I2+REGI_HL);
			REG16_(REG_I2+REGI_HL) = v;
			__Z80F(NOARG,"EXX");
		}
		break;
		case 0xdb:{//IN
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			__data=_ioreg[v];
			if(_portfnc_read[v]){
				if((((Z80Cpu *)this)->*_portfnc_read[v])(v,&_ioreg[v],&__data,AM_WRITE|AM_BYTE))
					_ioreg[v]=__data;
			}
			REG_A=__data;
			__Z80F(A\x2c[%XH]  %x,"IN",v,REG_A);
		}
		break;
		case 0xdd://IX
			ipc++;
			switch((u8)SR(_opcode,8)){
				default:
					__Z80F(%x,"UNK",_opcode);
					EnterDebugMode();
				break;
				case 9:
				case 0x19:
				case 0x29:
				case 0x39:{
					u16 r;
					u8 i;

					res+=3;
					_opcode=(u16)_opcode;
					i=SR(_opcode & 0x3000,11);
					if(i==6)
						i=REG_SP;
					STATUSFLAGS16("addw %0,%%cx\n",r,REG16_(i),REG_IX);
					REG_IX = r;
					REG_F &=~N_BIT;
					__Z80F(IX\x2c %s,"ADD",pr[SR(_opcode&0x3000,12)]);
				}
				break;
				case 0x21:
					res+=4;
					ipc+=2;
					REG_IX=SR(_opcode,16);
					__Z80F(IX\x2c%XH,"LD",SR(_opcode,16));
				break;
				case 0x22:
					ipc+=2;
					res+=3;
					WW(SR(_opcode,16),REG_IX);
					__Z80F([%XH]\x2c IX,"LD",SR(_opcode,16));
				break;
				case 0x2a:{
					u16 v;

					RW(SR(_opcode,16),v);
					REG_IX=v;
					res += 5;
					ipc+=2;
					__Z80F(IX\x2c[%XH],"LD",SR(_opcode,16));
				}
				break;
				case 0x34:{
					u8 v,d,mm;
					_opcode &=0xffffff;
					ipc++;
					res+=5;
					d=SR(_opcode,16);
					RB(REG_IX+d,v);
					mm=1;
					STATUSFLAGS("addb %0,%%cl",v,v,mm);
					WB(REG_IX+d,v);
					REG_F &=~N_BIT;
					__Z80F([IX+%02x],"INC",d);
				}
				break;
				case 0x35:{
					u8 v,d,mm;
					_opcode &=0xffffff;
					ipc++;
					res+=5;
					d=SR(_opcode,16);
					RB(REG_IX+d,v);
					mm=1;
					STATUSFLAGS("subb %0,%%cl",v,v,mm);
					WB(REG_IX+d,v);
					REG_F |=N_BIT;
					__Z80F([IX+%02x],"DEC",d);
				}
				break;
				case 0x36:
					res+=3;
					ipc+=2;
					WB(REG_IX+SR(_opcode&0xFF0000,16),SR(_opcode,24));
					__Z80F([IX+%XH]\x2c %Xh,"LD",SR(_opcode&0xFF0000,16),SR(_opcode,24));
				break;
				case 0x70:
				case 0x71:
				case 0x72:
				case 0x73:
				case 0x74:
				case 0x75:
				case 0x77:{
					s8 d;

					_opcode &=0xffffff;
					ipc++;
					d=SR(_opcode,16);
					WB(REG16_(REGI_IX)+d,
						REG_(SR(_opcode & 0x700,8)));
					res+=4;
					__Z80F([IX+%02xH]\x2c%s,"LD",(s8)d,prs[SR(_opcode&0x700,8)]);
				}
				break;
				case 0x46:
				case 0x4e:
				case 0x56:
				case 0x5e:
				case 0x76:
				case 0x7e:
				case 0x66:
				case 0x6e:{
					u8 v,i;

					_opcode&=0xFFFFFF;
					RB(REG16_(REGI_IX)+SR(_opcode,16),v);
					i = SR(_opcode,11)&7;
					REG_(i)=v;
					res+=4;
					ipc++;
					__Z80F(%s\x2c[IX+%02x],"LD",prs[SR(_opcode,11)&7],SR(_opcode,16));
				}
				break;
				case 0x86:{
					u8 mm;

					_opcode&=0xFFFFFF;
					ipc++;
					RB(REG_IX+SR(_opcode,16),mm);
					STATUSFLAGS("addb %0,%%cl",mm,REG_A,mm);
					REG_A=mm;
					res+=4;
					REG_F &=~N_BIT;
					__Z80F([IX+%xh],"ADD",SR(_opcode,16));
				}
				break;
				case 0x96:{
					u8 mm;

					_opcode&=0xFFFFFF;
					ipc++;
					RB(REG_IX+SR(_opcode,16),mm);
					STATUSFLAGS("subb %0,%%cl",mm,REG_A,mm);
					REG_A=mm;
					res+=4;
					REG_F |=N_BIT;
					__Z80F([IX+%xh],"SUB",SR(_opcode,16));
				}
				break;
				case 0xa6:{
					u8 mm;

					_opcode&=0xFFFFFF;
					ipc++;
					RB(REG_IX+SR(_opcode,16),mm);
					STATUSFLAGS("andb %0,%%cl",mm,REG_A,mm);
					REG_A=mm;
					res+=4;
					__Z80F([IX+%xh],"AND",SR(_opcode,16));
				}
				break;
				case 0xb6:{
					u8 mm;

					_opcode&=0xFFFFFF;
					ipc++;
					RB(REG_IX+SR(_opcode,16),mm);
					STATUSFLAGS("orb %0,%%cl",mm,REG_A,mm);
					REG_A=mm;
					res+=4;
					__Z80F([IX+%xh],"OR",SR(_opcode,16));
				}
				break;
				case 0xbe:{
					u8 mm;

					_opcode&=0xFFFFFF;
					ipc++;
					res+=4;
					RB(REG_IX+SR(_opcode,16),mm);
					STATUSFLAGS("subb %0,%%cl",mm,REG_A,mm);
					REG_F |=N_BIT;
					__Z80F([IX+%02x],"CP",SR(_opcode,16));
				}
				break;
				case 0xcb:{
					u8 v,d;
					u16 b;

					ipc+=2;
					b=(u16)SR(_opcode,16);
					RB(REG16_(REGI_IX)+(u8)b,v);
					switch(SR(b,8)){
						case 0x16:{
							u8 c = v&0x80;
							v = SL(v,1)|(REG_F&C_BIT);
							if(c)
								REG_F |= C_BIT;
							else
								REG_F&=~C_BIT;
							if(v)
								REG_F &= ~Z_BIT;
							else
								REG_F |= Z_BIT;
							__Z80F([IX+%02x],"RL",(u8)b);
						}
						break;
						case 0x1e:{
							u8 c = v&1;
							v = SR(v,1)|(REG_F&C_BIT ? 0x80 : 0);
							if(c)
								REG_F |= C_BIT;
							else
								REG_F&=~C_BIT;
							if(v)
								REG_F &= ~Z_BIT;
							else
								REG_F |= Z_BIT;
							__Z80F([IX+%02x],"RR",(u8)b);
						}
						break;
						case 0x26:{
							u8 c = v&0x80;
							v=SL((s8)v,1);
							if(c)
								REG_F |= C_BIT;
							else
								REG_F&=~C_BIT;
							__Z80F([IX+%02x],"SLA",(u8)b);
						}
						break;
						case 0x46:
						case 0x4e:
						case 0x56:
						case 0x5e:
						case 0x66:
						case 0x6e:
						case 0x76:
						case 0x7e:
							if(!BT(v,SL(1,SR(b,11) & 7)))
								REG_F |= Z_BIT;
							else
								REG_F &= ~Z_BIT;
							REG_F &= ~N_BIT;
							REG_F |= A_BIT;
							__Z80F(%XH\x2c[IX+%02x],"BIT",SR(b,11) & 7,(u8)b);
						break;
						case 0xc6:
						case 0xce:
						case 0xd6:
						case 0xde:
						case 0xe6:
						case 0xee:
						case 0xf6:
						case 0xfe:
							v |= BV(SR(b,11)&7);
							res+=5;
							__Z80F(%XH\x2c[IX+%02x],"SET",SR(b,11) & 7,(u8)b);
						break;
						case 0x86:
						case 0x8e:
						case 0x96:
						case 0x9e:
						case 0xa6:
						case 0xae:
						case 0xb6:
						case 0xbe:
							v &= ~BV(SR(b,11)&7);
							res+=5;
							__Z80F(%XH\x2c[IX+%02x],"RES",SR(b,11) & 7,(u8)b);
						break;
						default:
							__Z80F(%x %x %x,"UNK",_opcode,SR(b,8),(u8)b);
							EnterDebugMode();
						break;
					}
					WB(REG16_(REGI_IX)+(u8)b,v);
				}
				break;
				case 0xe1:
					RW(REG_SP,REG16_(REGI_IX));
					REG_SP+=2;
					res+=2;
					_opcode=(u16)_opcode;
					__Z80F(IX,"POP");
				break;
				case 0xe3:{
					u16 v;

					RW(REG_SP,v);
					REG_IX=v;
					res+=5;
					_opcode = (u16)_opcode;
					__Z80F(NOARG,"EX [SP], IX");
				}
				break;
				case 0xe5:
					REG_SP -= 2;
					WW(REG_SP,REG16_(REGI_IX));
					res+=2;
					_opcode=(u16)_opcode;
					__Z80F(IX,"PUSH");
				break;
			}
		break;
		case 0xeb:{
			u16 v;

			_opcode=(u8)_opcode;
			v=REG16_(REGI_DE);
			REG16_(REGI_DE)=REG16_(REGI_HL);
			REG16_(REGI_HL)=v;
			__Z80F(DE\x2c HL,"EX");
		}
		break;
		case 0xee:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			STATUSFLAGS("xorb %0,%%cl",v,REG_A,v);
			REG_A=v;
			__Z80F(%XH,"XOR",v);
		}
		break;
		case 0xfd://IY
			ipc++;
			switch((u8)SR(_opcode,8)){
				default:
					__Z80F(%XH,"UNK",_opcode);
					EnterDebugMode();
				break;
				case 9:
				case 0x19:
				case 0x29:
				case 0x39:{
					u16 r;
					u8 i;

					i=SR(_opcode & 0x3000,11);
					if(i==6)
						i=REG_SP;
					res+=3;
					_opcode=(u16)_opcode;
					STATUSFLAGS16("addw %0,%%cx\n",r,REG16_(i),REG_IY);
					REG_IY=r;
					REG_F &=~N_BIT;
					__Z80F(IY\x2c %s,"ADD",pr[SR(_opcode&0x3000,12)]);
				}
				break;
				case 0x21:
					res+=4;
					ipc+=2;
					REG_IY=SR(_opcode,16);
					__Z80F(IY\x2c%XH,"LD",SR(_opcode,16));
				break;
				case 0x34:{
					u8 v,d,mm;
					_opcode &=0xffffff;
					ipc++;
					d=SR(_opcode,16);
					RB(REG_IY+d,v);
					mm=1;
					STATUSFLAGS("addb %0,%%cl",v,v,mm);
					WB(REG_IY+d,v);
					REG_F &=~N_BIT;
					__Z80F([IX+%02x],"INC",d);
				}
				break;
				case 0x35:{
					u8 v,d,mm;
					_opcode &=0xffffff;
					ipc++;
					d=SR(_opcode,16);
					RB(REG_IY+d,v);
					mm=1;
					STATUSFLAGS("subb %0,%%cl",v,v,mm);
					WB(REG_IY+d,v);
					REG_F |=N_BIT;
					__Z80F([IX+%02x],"DEC",d);
				}
				break;
				case 0x36:
					res+=3;
					ipc+=2;
					WB(REG_IY+SR(_opcode&0xFF0000,16),SR(_opcode,24));
					__Z80F([IY+%Xh]\x2c %Xh,"LD",SR(_opcode&0xFF0000,16),SR(_opcode,24));
				break;
				case 0x86:{
					u8 d,v;

					ipc++;
					res+=4;
					_opcode&=0xFFFFFF;
					d=SR(_opcode,16);
					RB(REG_IY+d,v);
					STATUSFLAGS("addb %0,%%cl",v,REG_A,v);
					REG_A = v;
					REG_F &=~N_BIT;
					__Z80F(A\x2c[IY+%XH],"ADD",d);
				}
				break;
				case 0x70:
				case 0x71:
				case 0x72:
				case 0x73:
				case 0x74:
				case 0x75:
				case 0x77:{
					s8 d;

					_opcode &=0xffffff;
					ipc++;
					d=SR(_opcode,16);
					res+=4;
					WB(REG16_(REGI_IY)+d,
						REG_(SR(_opcode & 0x700,8)));
					res+=4;
					__Z80F([IY+%02x]\x2c%s,"LD",d,prs[SR(_opcode&0x700,8)]);
				}
				break;
				case 0x46:
				case 0x4e:
				case 0x56:
				case 0x5e:
				case 0x76:
				case 0x7e:
				case 0x66:
				case 0x6e:{
					u8 v,d;

					_opcode&=0xffffff;
					ipc++;
					RB(REG16_(REGI_IY)+SR(_opcode,16),v);
					d = (SR(_opcode,11)&7);
					if(d==6)
						d=7;
					REG_(d)=v;
					res+=4;
					__Z80F(%s\x2c[IY+%02x],"LD",prs[SR(_opcode&0x3800,11)],SR(_opcode,16));
				}
				break;
				case 0x8e:{
					u8 d,v;
					u16 mm;

					ipc++;
					_opcode&=0xffffff;
					d=SR(_opcode,16);
					RB(REG_IY+d,v);
					mm=REG_F & C_BIT;
					PRE_SF(REG_A);
					asm volatile("btw $0,%0\n"
								"adc %1,%%cl\n"
								: : "m"(mm),"m" (v) : "rcx","memory");
					AFT_SF(v);
					REG_A=v;
					REG_F &= ~N_BIT;
					res+=4;
					__Z80F(A\x2c[IY+%XH],"ADC",d);
				}
				break;
				case 0x96:{
					u8 v,d;

					ipc++;
					_opcode  &= 0xffffff;
					RB(REG16_(REGI_IY)+SR(_opcode,16),v);
					STATUSFLAGS("subb %0,%%cl",v,REG_A,v);
					REG_A=v;
					REG_F |= N_BIT;
					res+=4;
					__Z80F([IY+%02x],"SUB",SR(_opcode,16));
				}
				break;
				case 0x9e:{
					u8 r,d;
					u16 mm;

					ipc++;
					_opcode  &= 0xffffff;
					mm = (REG_F & C_BIT);
					RB(REG16_(REGI_IY)+SR(_opcode,16),r);
					PRE_SF(REG_A);
					asm volatile("btw $0,%0\n"
									"sbb %1,%%cl\n"
						: : "m"(mm),"m" (r) : "rcx","memory");
					AFT_SF(r);
					REG_A=r;
					REG_F |= N_BIT;
					__Z80F(A\x2c[IY+%02x],"SBC",SR(_opcode,16));
				}
				break;
				case 0xae:{
					u8 v;

					_opcode&=0xffffff;
					RB(REG16_(REGI_IY)+SR(_opcode,16),v);
					STATUSFLAGS("xorb %0,%%cl",v,REG_A,v);
					REG_A=v;
					res+=4;
					ipc++;
					__Z80F([IY+%02x],"XOR",SR(_opcode,16));
				}
				break;
				case 0xb6:{
					u8 mm;

					_opcode&=0xFFFFFF;
					ipc++;
					RB(REG_IY+SR(_opcode,16),mm);
					STATUSFLAGS("orb %0,%%cl",mm,REG_A,mm);
					REG_A=mm;
					res+=4;
					__Z80F([IY+%xh],"OR",SR(_opcode,16));
				}
				break;
				case 0xbe:{
					u8 mm;

					_opcode&=0xFFFFFF;
					ipc++;
					res+=44;
					RB(REG_IY+SR(_opcode,16),mm);
					STATUSFLAGS("subb %0,%%cl",mm,REG_A,mm);
					REG_F |=N_BIT;
					__Z80F([IY+%02x],"CP",SR(_opcode,16));
				}
				break;
				case 0xcb:{
					u8 v;
					u16 b;

					ipc+=2;
					b=(u16)SR(_opcode,16);
					RB(REG16_(REGI_IY)+(u8)b,v);
					switch(SR(b,8)){
						case 0x16:{
							u8 c = v&0x80;
							v = SL(v,1)|(REG_F&C_BIT);
							if(c)
								REG_F |= C_BIT;
							else
								REG_F &= ~C_BIT;
							if(v)
								REG_F &= ~Z_BIT;
							else
								REG_F |= Z_BIT;
							__Z80F([IY+%02x],"RL",(u8)b);
						}
						break;
						case 0x1e:{
							u8 c = v & 1;
							v=SR(v,1)|(REG_F & C_BIT ? 0x80:0);
							if(c)
								REG_F |= C_BIT;
							else
								REG_F &= ~C_BIT;

							if(v)
								REG_F &= ~Z_BIT;
							else
								REG_F |= Z_BIT;

							res+=5;
							__Z80F([IY+%XH] %XH,"RR",(u8)b,REG16_(REGI_IY)+(u8)b);
						}
						break;
						case 0x26:{
							u8 c = v&0x80;
							v=SL((s8)v,1);
							if(c)
								REG_F |= C_BIT;
							else
								REG_F&=~C_BIT;
							__Z80F([IY+%02x],"SLA",(u8)b);
						}
						case 0x3e:
							v=SR(v,1);
							res+=5;
							__Z80F([IY+%XH],"SRL",(u8)b);
						break;
						case 0x46:
						case 0x4e:
						case 0x56:
						case 0x5e:
						case 0x66:
						case 0x6e:
						case 0x76:
						case 0x7e:
							if(!BT(v,SL(1,SR(b,11) & 7)))
								REG_F  |= Z_BIT;
							else
								REG_F &= ~Z_BIT;
							REG_F &= ~N_BIT;
							REG_F |= A_BIT;
							__Z80F(%XH\x2c[IY+%02x],"BIT",SR(b,11) & 7,(u8)b);
						break;
						case 0x86:
						case 0x8e:
						case 0x96:
						case 0x9e:
						case 0xa6:
						case 0xae:
						case 0xb6:
						case 0xbe:
							v = v & ~BV(SR(b,11)&7);
							res+=5;
							__Z80F(%XH\x2c[IY+%02x],"RES",SR(b,11)&7,(u8)b);
						break;
						case 0xc6:
						case 0xce:
						case 0xd6:
						case 0xde:
						case 0xe6:
						case 0xee:
						case 0xfe:
						case 0xf6:
							v |= BV(SR(b,11)&7);
							res+=5;
							__Z80F(%XH\x2c[IY+%02x],"SET",SR(b,11) & 7,(u8)b);
						break;
						default:
							__Z80F(%x %x %x,"UNK",_opcode,SR(b,8),(u8)b);
							EnterDebugMode();
						break;
					}
					WB(REG16_(REGI_IY)+(u8)b,v);
				}
				break;
				case 0xe1:
					RW(REG_SP,REG16_(REGI_IY));
					REG_SP+=2;
					res+=2;
					_opcode=(u16)_opcode;
					__Z80F(IY,"POP");
				break;
				case 0xe3:{
					u16 v;

					RW(REG_SP,v);
					REG_IY=v;
					res+=5;
					_opcode = (u16)_opcode;
					__Z80F(NOARG,"EX [SP], IY");
				}
				break;
				case 0xe5:
					REG_SP -= 2;
					WW(REG_SP,REG16_(REGI_IY));
					res+=2;
					_opcode=(u16)_opcode;
					__Z80F(IY,"PUSH");
				break;
			}
		break;
		case 0xe3:{
			u16  v;

			_opcode=(u8)_opcode;
			RW(REG_SP,v);
			WW(REG_SP,REG16_(REGI_HL));
			REG16_(REGI_HL)=v;
			res+=4;
			__Z80F(NOARG,"EX [SP], HL");
		}
		break;
		case 0xe6:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			STATUSFLAGS("andb %0,%%cl",v,REG_A,v);
			REG_A = v;
			__Z80F(%02XH,"AND",SR(_opcode,8));
		}
		break;
		case 0xe9:
			_opcode=(u8)_opcode;
			ipc=0;
			_pc=REG16_(REGI_HL);
			__Z80F([HL],"JP");
		break;
		case 0xed:{
			ipc++;
			switch(SR(_opcode&0xFF00,8)){
				case  0x44:{
					u8 mm,v;

					_opcode=(u16)_opcode;
					mm=0;
					STATUSFLAGS("subb %0,%%cl",v,mm,REG_A);
					REG_A=v;
					REG_F |=N_BIT;
					res++;
					__Z80F(NOARG,"NEG");
				}
				break;
				case 0x4d:
				//EnterDebugMode();
					ipc=0;
					_opcode=(u16)_opcode;
					__Z80F(NOARG,"RETI");
					RW(REG_SP,_pc);
					REG_SP+=2;
					goto B;
				break;
				case 0x42:
				case 0x62:
				case 0x52:
				case 0x72:{
					u16 v,vv,mm;
					u8  i;

					_opcode=(u16)_opcode;
					v=REG16_(REGI_HL);
					i=SR(_opcode&0x3000,11);
					if(i==6)
						i=REG_SP;
					vv=REG16_(i);
					mm=REG_F & C_BIT;
					PRE_SF16(v);
					asm volatile("btw $0,%0\n"
								"sbbw %1,%%cx\n"
						: : "m"(mm),"m" (vv) : "rcx","memory");
					AFT_SF16(v);
					REG16_(REGI_HL)=v;
					REG_F |= N_BIT;
					res+=3;
					__Z80F(HL\x2c %s %x,"SBC",pr[SR(_opcode,12)&3],i);
				}
				break;
				case 0xb0:{
					u8 v;
					u16 c;

					_opcode=(u16)_opcode;
					for(c=REG16_(REGI_BC);c>0;c--){
						RB(REG16_(REGI_HL),v);
						WB(REG16_(REGI_DE),v);

						REG16_(REGI_DE)++;
						REG16_(REGI_HL)++;
						res +=2;
					}
					REG16_(REGI_BC)=0;
					REG_F &= ~(N_BIT|V_BIT|A_BIT);
					__Z80F(NOARG,"LDIR");
				}
				break;
				case 0xb8:{
					u8 v;
					u16 c;

					_opcode=(u16)_opcode;
					for(c=REG16_(REGI_BC);c>0;c--){
						RB(REG16_(REGI_HL),v);
						WB(REG16_(REGI_DE),v);

						REG16_(REGI_DE)--;
						REG16_(REGI_HL)--;
						res+=2;
					}
					REG16_(REGI_BC)=0;
					REG_F &= ~(N_BIT|V_BIT|A_BIT);
					__Z80F(NOARG,"LDDR");
				}
				break;
				case 0x43:
				case 0x53:
				case 0x63:
				case 0x73:{
					u8 i = SR(_opcode&0x3000,11);
					u16 v=(u16)SR(_opcode,16);
					__Z80F(\x28%04XH\x29\x2c%s,"LD",v,pr[i/2]);
					ipc+=2;
					if(i==6)
						i=REGI_SP;
					WW(v,REG16_(i));
					res+=4;
				}
				break;
				case 0x4b:
				case 0x5b:
				case 0x6b:
				case 0x7b:{
					u16 v;

					ipc+=2;
					v=SR(_opcode,16);
					u8 i = SR(_opcode&0x3000,11);
					if(i==6)
						i=REGI_SP;
					RW(v,REG16_(i));
					__Z80F(%s\x2c[%04XH],"LD",pr[SR(_opcode&0x3000,12)],v);
				}
				break;
				case 0x56:
					_opcode=(u16)_opcode;
					REG_I &= 0xF;
					REG_I |= 0x10;
					__Z80F(%d,"IM",1);
				break;
				default:
					__Z80F(%x,"UNK",_opcode);
					EnterDebugMode();
				break;
			}
		}
		break;
		case 0xF3:
			_opcode=(u8)_opcode;
			REG_IFF &= ~(IFF1_BIT|IFF2_BIT);
			__Z80F(NOARG,"DI");
		break;
		case 0xf6:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			v=SR(_opcode,8);
			STATUSFLAGS("orb %0,%%cl",v,REG_A,v);
			REG_A = v;
			__Z80F(%XH,"OR",SR(_opcode,8));
		}
		break;
		case 0xf9:
			_opcode=(u8)_opcode;
			REG_SP=REG16_(REGI_HL);
			__Z80F(SP\x2cHL,"LD");
		break;
		case 0xfb:
			_opcode=(u8)_opcode;
			REG_IFF |= (IFF1_BIT|IFF2_BIT);
			__Z80F(NOARG,"EI");
			goto B;
		break;
		case 0xfe:{
			_opcode=(u16)_opcode;
			ipc++;
			u8 mm=SR(_opcode,8);
			STATUSFLAGS("subb %0,%%cl",mm,REG_A,mm);
			REG_F |=N_BIT;
			__Z80F(%x,"CP",SR(_opcode,8));
			res++;
		}
		break;
		default:
			if((u8)_opcode > 0x3f && (u8)_opcode < 0x80 && (_opcode &7)!=6 && (_opcode & 0x38) != 0x30){
				_opcode = (u8)_opcode;

				REG_(SR(_opcode,3)&7) = REG_(_opcode&7);
				__Z80F(%s\x2c%s,"LD",prs[SR(_opcode,3)&7],prs[_opcode&7]);
			}
			else if((u8)_opcode > 0xc1 && (u8)_opcode < 0xfb && (_opcode & 0x7) == 2){
				_opcode &= 0xFFFFFF;
				__Z80F(%s\x2c%xH,"JP",jpc[SR(_opcode,3) & 7],SR(_opcode,8));
				JMPcc(SR(_opcode & 0x38,3));
				if(i){
					ipc = 0;
					_pc=SR(_opcode,8);
				}
				else
					ipc+=2;
			}
			else{
				__Z80F(%x,"UNK",_opcode);
				EnterDebugMode();
			}
		break;
	}
A:
	_pc+=ipc;
	return res;
B:
	_pc+=ipc;
	if(_irq_pending)
		machine->OnEvent(0,(LPVOID)(u64)(_irq_pending|SL(_idx,24)));
	return res;
}

int Z80Cpu::Load(void *){
	//_pc=pc;
	return 0;
}

int Z80Cpu::Disassemble(char *dest,u32 *padr){
	u32 op,adr,ipc;
	char c[200],cc[100];

	u8 *p;

	*((u64 *)c)=0;
	*((u64 *)cc)=0;

	adr = *padr;
	sprintf(c,"%08X ",adr);
	*((u64 *)cc)=0;
	op=_opcode;
	_opcode=*((u32 *)&_mem[adr]);
	ipc = 1;
	switch((u8)_opcode){
		case 0:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"NOP");
		break;
		case 0x1:
		case 0x11:
		case 0x21:
		case 0x31:{
			//LD BC,DE,HL,SP
			u16 v;

			_opcode &= 0xFFFFFF;
			v=(u16)SR(_opcode,8);
			ipc+=2;
			Z80LOGD(%s\x2c%04XH,"LD",pr[SR(_opcode,4)&3],v);
		}
		break;
		case 2:
			_opcode=(u8)_opcode;
			Z80LOGD([BC]\x2c A,"LD");
		break;
		case 3:
		case 0x13:
		case 0x23:
		case 0x33:{
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"INC",pr[SR(_opcode&0x30,4)]);
		}
		break;
		case 0x4:
		case 0xC:
		case 0x14:
		case 0x1C:
		case 0x24:
		case 0x2c:
		case 0x3c:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"INC",prs[SR(_opcode,3)&7]);
		break;
		case 0x12:
			_opcode=(u8)_opcode;
			Z80LOGD([DE]\x2c A,"LD");
		break;
		case 0x34:
			_opcode=(u8)_opcode;
			Z80LOGD([HL],"INC");
		break;
		case 0x35:
			_opcode=(u8)_opcode;
			Z80LOGD([HL],"DEC");
		break;
		case 0x6:
		case 0xe:
		case 0x16:
		case 0x1e:
		case 0x26:
		case 0x2e:
		case 0x3e:{
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(%s\x2c%XH,"LD",prs[SR(_opcode,3)&7],SR(_opcode,8));
		}
		break;
		case 0x7:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"RLCA");
		break;
		case 0x8:
			Z80LOGD(AF\x2c AF\x27,"EX");
		break;
		case 0xa:
			_opcode = (u8)_opcode;
			Z80LOGD(A\x2c[BC],"LD");
		break;
		case 0xF:
			_opcode = (u8)_opcode;
			Z80LOGD(NOARG,"RRCA");
		break;
		case  0x18:
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(%XH,"JR",adr+ipc+(s8)SR(_opcode,8));
		break;
		case 0x9:
		case 0x19:
		case 0x29:
		case 0x39:
			_opcode=(u8)_opcode;
			Z80LOGD(HL\x2c%s,"ADD",pr[SR(_opcode&0x30,4)]);
		break;
		case 0x10:{
			s8  d;

			ipc++;
			_opcode=(u16)_opcode;
			d=SR(_opcode,8);
			Z80LOGD(%XH,"DJNZ",adr+ipc+d);
		}
		break;
		case 0x1a:
			_opcode=(u8)_opcode;
			Z80LOGD(A\x2c [DE],"LD");
		break;
		case 0x22:{
			ipc+=2;
			_opcode = _opcode&0xFFFFFF;
			Z80LOGD([%04XH]\x2cHL,"LD",(u16)SR(_opcode,8));
		}
		break;
		case 0x20:{
			s8 d;

			ipc++;
			_opcode=(u16)_opcode;
			d=(s8)SR(_opcode,8);
			Z80LOGD(NZ\x2c%XH,"JR",adr+ipc+d);
		}
		break;
		case  0x27:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"DAA");
		break;
		case 0x28:{
			s8 d;

			_opcode=(u16)_opcode;
			ipc++;
			d=(s8)SR(_opcode,8);
			Z80LOGD(Z\x2c%XH,"JR",adr+d+ipc);
		}
		break;
		case 0x2a:{
			_opcode &= 0xFFFFFF;
			u16 v =(u16)SR(_opcode,8);
			ipc+=2;
			Z80LOGD(HL\x2c[%04XH],"LD",v);
		}
		break;
		case 0x2f:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"CPL");
		break;
		case 5:
		case 0xd:
		case 0x15:
		case 0x1d:
		case 0x25:
		case 0x2d:
		case 0x3d:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"DEC",prs[SR(_opcode,3)&7]);
		break;
		case 0xb:
		case 0x1b:
		case 0x2b:
		case 0x3b:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"DEC",pr[SR(_opcode&0x30,4)]);
		break;
		case 0x30:{
			s8 d;

			_opcode=(u16)_opcode;
			ipc++;
			d=(s8)SR(_opcode,8);
			Z80LOGD(NC\x2c%XH,"JR",adr+d + 2);
		}
		break;
		case 0x32:
			_opcode &= 0xFFFFFF;
			ipc+=2;
			Z80LOGD([%XH]\x2c A,"LD",SR(_opcode,8));
		break;
		case 0x36:
			_opcode=(u16)_opcode;
			ipc++;
			Z80LOGD([HL]\x2c %XH,"LD",SR(_opcode,8));
		break;
		case 0x37:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"SCF");
		break;
		case 0x38:{
			s8 d;

			_opcode=(u16)_opcode;
			ipc++;
			d=(s8)SR(_opcode,8);
			Z80LOGD(C\x2c%XH,"JR",adr+d + 2);
		}
		break;
		case 0x3a:
			ipc+=2;
			_opcode&=0xffffff;
			Z80LOGD(A\x2c [%xH],"LD",SR(_opcode,8));
		break;
		case 0x46:
		case 0x4e:
		case 0x56:
		case 0x5e:
		case 0x66:
		case 0x6e:
		case 0x76:
		case 0x7e:{
			_opcode=(u8)_opcode;
			Z80LOGD(%s\x2c[HL],"LD",prs[SR(_opcode,3)&7]);
		}
		break;
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x77:
			_opcode=(u8)_opcode;
			Z80LOGD([HL]\x2c%s,"LD",prs[_opcode&7]);
		break;
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x87:
			_opcode=(u8)_opcode;
			Z80LOGD(A\x2c %s,"ADD",prs[_opcode&7]);
		break;
		case 0x86:
			_opcode=(u8)_opcode;
			Z80LOGD(A\x2c [HL],"ADD");
		break;
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x97:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"SUB",prs[_opcode&7]);
		break;
		case 0xa0:
		case 0xa1:
		case 0xa2:
		case 0xa3:
		case 0xa4:
		case 0xa5:
		case 0xa7:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"AND",prs[_opcode&7]);
		break;
		case 0xa6:
			_opcode=(u8)_opcode;
			Z80LOGD([HL],"AND");
		break;
		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
		case 0xac:
		case 0xad:
		case 0xaf:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"XOR",prs[_opcode&7]);
		break;
		case 0xb0:
		case 0xb1:
		case 0xb2:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb7:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"OR",prs[_opcode&7]);
		break;
		case 0xb6:
			_opcode=(u8)_opcode;
			Z80LOGD([HL],"OR");
		break;
		case 0xbe:
			_opcode=(u8)_opcode;
			Z80LOGD([HL],"CP");
		break;
		case 0xb8:
		case 0xb9:
		case 0xba:
		case 0xbb:
		case 0xbc:
		case 0xbd:
		case 0xbf:{
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"CP",prs[_opcode&7]);
		}
		break;
		case 0xc0:
		case 0xc8:
		case 0xd0:
		case 0xd8:
		case 0xe0:
		case 0xe8:
		case 0xf0:
		case 0xf8:
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"RET",jpc[SR(_opcode,3)&7]);
		break;
		case 0xcb:
			_opcode = (u16)_opcode;
			ipc++;
			switch(SR(_opcode,8)){
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 7:
					Z80LOGD(%s,"RLC",prs[SR(_opcode,8)&7]);
				break;
				case 0x16:
					Z80LOGD([HL],"RL");
				break;
				case 0x18:
				case 0x19:
				case 0x1a:
				case 0x1b:
				case 0x1c:
				case 0x1d:
				case 0x1f:
					Z80LOGD(%s,"RR",prs[SR(_opcode,8)&7]);
				break;
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x13:
				case 0x14:
				case 0x15:
				case 0x17:
					Z80LOGD(%s,"RL",prs[SR(_opcode,8)&7]);
				break;
				case 0x20:
				case 0x21:
				case 0x22:
				case 0x23:
				case 0x24:
				case 0x25:
				case 0x27:
					Z80LOGD(%s,"SLA",prs[SR(_opcode,8)&7]);
				break;
				case 0x38:
				case 0x39:
				case 0x3a:
				case 0x3b:
				case 0x3c:
				case 0x3d:
				case 0x3f://srl
					Z80LOGD(%s,"SRL",prs[SR(_opcode,8)&7]);
				break;
				case 0x46:
					Z80LOGD(%d\x2c [HL],"BIT",SR(_opcode&0x3800,11));
				break;
				default:
					if(SR(_opcode,8) >= 0x3f && SR(_opcode,8) < 0x80){
						Z80LOGD(%d\x2c%s,"BIT",SR(_opcode&0x3800,11),prs[SR(_opcode,8)&7]);
					}
					else if(SR(_opcode,8) > 0x7f && SR(_opcode,8) < 0xc0){
						Z80LOGD(%d\x2c%s,"RES",SR(_opcode&0x3800,11),prs[SR(_opcode,8)&7]);
					}
					else if(SR(_opcode,8) > 0xbf && SR(_opcode,8) < 0x100){
						Z80LOGD(%d\x2c%s,"SET",SR(_opcode&0x3800,11),prs[SR(_opcode,8)&7]);
					}
					else
						Z80LOGD(%x,"UNK",_opcode);
				break;
			}
		break;
		case 0xce:{
			u8 v;

			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(A\x2c%02XH,"ADC",SR(_opcode,8));
		}
		break;
		case 0xcd:{
			ipc+=2;
			_opcode &= 0xFFFFFF;
			Z80LOGD(%04XH,"CALL",SR(_opcode,8));
		}
		break;
		case 0xd3:{//out
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD([%XH]\x2c A,"OUT",SR(_opcode,8));
		}
		break;
		case 0xc1:
		case 0xd1:
		case 0xe1:
		case 0xf1:{
			_opcode=(u8)_opcode;
			Z80LOGD(%s,"POP",prp[SR(_opcode&0x30,4)]);
		}
		break;
		case 0xc3:
			ipc+=2;
			_opcode&=0xffffff;
			Z80LOGD(%XH,"JP",SR(_opcode,8));
		break;
		case 0xc4:
		case 0xcc:
		case 0xd4:
		case 0xdc:
		case 0xe4:
		case 0xec:
		case 0xf4:
		case 0xfc:
			_opcode&=0xffffff;
			ipc+=2;
			Z80LOGD(%s %XH,"CALL",jpc[SR(_opcode,3)&7],SR(_opcode,8));
		break;
		case 0xc5:
		case 0xd5:
		case 0xe5:
		case 0xf5:{
			//PUSH BC DE HL AF
			_opcode = (u8)_opcode;
			Z80LOGD(%s,"PUSH",prp[SR(_opcode&0x30,4)]);
		}
		break;
		case 0xc6:
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(A\x2c%02x,"ADD",SR(_opcode,8));
		break;
		case 0xc7:
		case 0xcf:
		case 0xd7:
		case 0xdf:
		case 0xe7:
		case 0xef:
		case 0xf7:
		case 0xFF:
			_opcode=(u8)_opcode;
			Z80LOGD(%XH,"RST",SR(_opcode,3)&7);
		break;
		case 0xc9:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"RET");
		break;
		case  0xd6:
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(%02x,"SUB",SR(_opcode,8));
		break;
		case 0xd9:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"EXX");
		break;
		case  0xdb:
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(A\x2c[%XH],"IN",SR(_opcode,8));
		break;
		case 0xdd://IX
			ipc++;
			switch((u8)SR(_opcode,8)){
				default:
					Z80LOGD(%x,"UNK",_opcode);
				break;
				case 9:
				case 0x19:
				case 0x29:
				case 0x39:
					_opcode=(u16)_opcode;
					Z80LOGD(IX\x2c %s,"ADD",pr[SR(_opcode&0x3000,12)]);
				break;
				case 0x21:
					ipc+=2;
					Z80LOGD(IX\x2c%XH,"LD",SR(_opcode,16));
				break;
				case 0x22:
					ipc+=2;
					Z80LOGD([%XH]\x2c IX,"LD",SR(_opcode,16));
				break;
				case 0x2a:
					ipc+=2;
					Z80LOGD(IX\x2c[%XH],"LD",SR(_opcode,16));
				break;
				case 0x34:
					_opcode &=0xffffff;
					ipc++;
					Z80LOGD([IX+%02x],"INC",SR(_opcode,16));
				break;
				case 0x35:
					_opcode &=0xffffff;
					ipc++;
					Z80LOGD([IX+%02x],"DEC",SR(_opcode,16));
				break;
				case 0x36:
					ipc+=2;
					Z80LOGD([IX+%Xh]\x2c %Xh,"LD",SR(_opcode&0xFF0000,16),SR(_opcode,24));
				break;

				case 0x70:
				case 0x71:
				case 0x72:
				case 0x73:
				case 0x74:
				case 0x75:
				case 0x77:{
					s8 d;

					_opcode &= 0xffffff;
					ipc++;
					d=SR(_opcode,16);
					Z80LOGD([IX+%2X]\x2c%s,"LD",(u8)d,prs[SR(_opcode&0x700,8)]);
				}
				break;
				case 0x46:
				case 0x4e:
				case 0x56:
				case 0x5e:
				case 0x76:
				case 0x7e:
				case 0x66:
				case 0x6e:{
					u8 v;

					_opcode &= 0xFFFFFF;
					ipc++;
					Z80LOGD(%s\x2c[IX+%02x],"LD",prs[SR(_opcode&0x3800,11)],SR(_opcode,16));
				}
				break;
				case 0x86:
					_opcode&=0xFFFFFF;
					ipc++;
					Z80LOGD([IX+%xh],"ADD",SR(_opcode,16));
				break;
				case 0x96:
					_opcode&=0xFFFFFF;
					ipc++;
					Z80LOGD([IX+%xh],"SUB",SR(_opcode,16));
				break;
				case 0xA6:
					_opcode&=0xFFFFFF;
					ipc++;
					Z80LOGD([IX+%xh],"AND",SR(_opcode,16));
				break;
				case 0xAe:
					_opcode=(u8)_opcode;
					Z80LOGD([HL],"XOR");
				break;
				case 0xb6:
					_opcode&=0xFFFFFF;
					ipc++;
					Z80LOGD([IX+%xh],"OR",SR(_opcode,16));
				break;
				case 0xbe:{
					_opcode&=0xFFFFFF;
					ipc++;
					Z80LOGD([IX+%02x],"CP",SR(_opcode,16));
				}
				break;
				case 0xcb:{
					u8 v,d;
					u16 b;

					ipc+=2;
					b=(u16)SR(_opcode,16);
					switch(SR(b,8)){
						case 0x16:
							Z80LOGD([IX+%02x],"RL",(u8)b);
						break;
						case 0x1e:
							Z80LOGD([IX+%02x],"RR",(u8)b);
						break;
						case 0x26:
							Z80LOGD([IX+%02x],"SLA",(u8)b);
						break;
						case 0x46:
						case 0x4e:
						case 0x56:
						case 0x5e:
						case 0x66:
						case 0x6e:
						case 0x76:
						case 0x7e:
							Z80LOGD(%XH\x2c[IX+%02x],"BIT",SR(b,11) & 7,(u8)b);
						break;
						case 0xc6:
						case 0xce:
						case 0xd6:
						case 0xde:
						case 0xe6:
						case 0xee:
						case 0xf6:
						case 0xfe:
							Z80LOGD(%XH\x2c[IX+%02x],"SET",SR(b,11) & 7,(u8)b);
						break;
						default:
							Z80LOGD(%XH\x2c[IX+%02x],"RES",SR(b,11) & 7,(u8)b);
						break;
					}
				}
				break;
				case 0xe1:
					_opcode=(u16)_opcode;
					Z80LOGD(IX,"POP");
				break;
				case 0xe3:
					_opcode=(u16)_opcode;
					Z80LOGD(NOARG,"EX [SP], IX");
				break;
				case 0xe5:
					_opcode=(u16)_opcode;
					Z80LOGD(IX,"PUSH");
				break;
			}
		break;
		case 0xeb:
			_opcode=(u8)_opcode;
			Z80LOGD(DE\x2c HL,"EX");
		break;
		case 0xfd://IY
			ipc++;
			switch((u8)SR(_opcode,8)){
				default:
					Z80LOGD(%x,"UNK",_opcode);
				break;
				case 9:
				case 0x19:
				case 0x29:
				case 0x39:
					_opcode=(u16)_opcode;
					Z80LOGD(IY\x2c %s,"ADD",pr[SR(_opcode&0x3000,12)]);
				break;
				case 0x21:
					ipc+=2;
					Z80LOGD(IY\x2c%XH,"LD",SR(_opcode,16));
				break;
				case 0x86:{
					ipc++;
					_opcode&=0xFFFFFF;
					Z80LOGD(A\x2c[IY+%XH],"ADD",SR(_opcode,16));
				}
				break;
				case 0x70:
				case 0x71:
				case 0x72:
				case 0x73:
				case 0x74:
				case 0x75:
				case 0x77:{
					s8 d;

					_opcode &=0xffffff;
					ipc++;
					Z80LOGD([IY+%02x]\x2c%s,"LD",SR(_opcode,16),prs[SR(_opcode&0x700,8)]);
				}
				break;
				case 0x46:
				case 0x4e:
				case 0x56:
				case 0x5e:
				case 0x66:
				case 0x6e:
				case 0x76:
				case 0x7e:
					_opcode&=0xffffff;
					ipc++;
					Z80LOGD(%s\x2c[IY+%02x],"LD",prs[SR(_opcode&0x3800,11)],SR(_opcode,16));
				break;
				case 0x8e:{
					u8 d,v;

					ipc++;
					_opcode&=0xffffff;
					Z80LOGD(A\x2c[IY+%XH],"ADC",SR(_opcode,16));
				}
				break;
				case 0x96:{
					ipc++;
					_opcode  &= 0xffffff;
					Z80LOGD([IY+%02x],"SUB",SR(_opcode,16));
				}
				break;
				case 0x9e:{
					ipc++;
					_opcode  &= 0xffffff;
					Z80LOGD(A\x2c[IY+%02x],"SBC",SR(_opcode,16));
				}
				break;
				case 0xae:{
					u8 v;

					_opcode&=0xffffff;
					ipc++;
					Z80LOGD([IY+%02x],"XOR",SR(_opcode,16));
				}
				break;
				case 0xb6:{
					u8 v;

					_opcode&=0xffffff;
					ipc++;
					Z80LOGD([IY+%02x],"OR",SR(_opcode,16));
				}
				break;
				case 0xcb:{
					u8 v;
					u16 b;

					ipc+=2;
					b=(u16)SR(_opcode,16);
					switch(SR(b,8)){
						case 0x16:
							Z80LOGD([IY+%02x],"RL",(u8)b);
						break;
						case 0x1e:
							Z80LOGD([IY+%XH],"RR",(u8)b);
						break;
						case 0x26:
							Z80LOGD([IY+%02x],"SLA",(u8)b);
						break;
						case 0x3e:
							Z80LOGD([IY+%XH],"SRL",(u8)b);
						break;
						case 0x46:
						case 0x4e:
						case 0x56:
						case 0x5e:
						case 0x66:
						case 0x6e:
						case 0x76:
						case 0x7e:
							Z80LOGD(%XH\x2c[IY+%02x],"BIT",SR(b,11)&7,(u8)b);
						break;
						case 0x86:
						case 0x8e:
						case 0x96:
						case 0x9e:
						case 0xa6:
						case 0xae:
						case 0xb6:
						case 0xbe:
							Z80LOGD(%XH\x2c[IY+%02x],"RES",SR(b,11)&7,(u8)b);
						break;
						case 0xc6:
						case 0xce:
						case 0xd6:
						case 0xde:
						case 0xe6:
						case 0xee:
						case 0xfe:
						case 0xf6:
							Z80LOGD(%XH\x2c[IY+%02x],"SET",SR(b,11) & 7,(u8)b);
						break;
						default:
							Z80LOGD(%x %x %x,"UNK",_opcode,SR(b,8),(u8)b);
						break;
					}
				}
				break;
				case 0xe1:
					_opcode=(u16)_opcode;
					Z80LOGD(IY,"POP");
				break;
				case 0xe3:
					_opcode=(u16)_opcode;
					Z80LOGD(NOARG,"EX [SP], IY");
				break;
				case 0xe5:
					_opcode=(u16)_opcode;
					Z80LOGD(IY,"PUSH");
				break;
			}
		break;
		case 0xe3:{
			u16  v;

			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"EX [SP], HL");
		}
		break;
		case 0xe6:
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(%02XH,"AND",SR(_opcode,8));
		break;
		case 0xe9:
			_opcode=(u8)_opcode;
			Z80LOGD([HL],"JP");
		break;
		case 0xed:{
			ipc++;
			switch(SR(_opcode&0xFF00,8)){
				case 0x44:
					_opcode=(u16)_opcode;
					Z80LOGD(NOARG,"NEG");
				break;
				case 0x4d:
					_opcode=(u16)_opcode;
					Z80LOGD(NOARG,"RETI");
				break;
				case 0x42:
				case 0x62:
				case 0x52:
				case 0x72:
					_opcode=(u16)_opcode;
					Z80LOGD(HL\x2c %s,"SBC",pr[SR(_opcode,12)&3]);
				break;
				case 0xb0:
					_opcode=(u16)_opcode;
					Z80LOGD(NOARG,"LDIR");
				break;
				case 0xb8:
					_opcode=(u16)_opcode;
					Z80LOGD(NOARG,"LDDR");
				break;
				case 0x43:
				case 0x53:
				case 0x63:
				case 0x73:{
					u8 i = SR(_opcode&0x3000,11);
					Z80LOGD(\x28%04XH\x29\x2c%s,"LD",SR(_opcode,16),pr[i/2]);
					ipc+=2;
				}
				break;
				case 0x4b:
				case 0x5b:
				case 0x6b:
				case 0x7b:
					ipc+=2;
					Z80LOGD(%s\x2c[%04XH],"LD",pr[SR(_opcode&0x3000,12)],SR(_opcode,16));
				break;
				case 0x56:
					_opcode=(u16)_opcode;
					Z80LOGD(%d,"IM",1);
				break;
				default:
					Z80LOGD(%x,"UNK",_opcode);
				break;
			}
		}
		break;
		case 0xee:
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(%XH,"XOR",SR(_opcode,8));
		break;
		case 0xF3:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"DI");
		break;
		case 0xf6:
			ipc++;
			_opcode=(u16)_opcode;
			Z80LOGD(%XH,"OR",SR(_opcode,8));
		break;
		case 0xf9:
			_opcode=(u8)_opcode;
			Z80LOGD(SP\x2cHL,"LD");
		break;
		case 0xfb:
			_opcode=(u8)_opcode;
			Z80LOGD(NOARG,"EI");
		break;
		case 0xfe:{
			_opcode=(u16)_opcode;
			ipc++;
			Z80LOGD(%XH,"CP",SR(_opcode,8));
		}
		break;
		default:
			if((u8)_opcode > 0x3f && (u8)_opcode < 0x80 && (_opcode &7)!=6 && (_opcode & 0x38) != 0x30){
				_opcode = (u8)_opcode;
				Z80LOGD(%s\x2c%s,"LD",prs[SR(_opcode,3)&7],prs[_opcode&7]);
			}
			else if((u8)_opcode > 0xc1 && (u8)_opcode < 0xfb && (_opcode & 0x7) == 2){
				_opcode &= 0xFFFFFF;
				ipc+=2;
				Z80LOGD(%s\x2c%XH,"JP",jpc[SR(_opcode,3) & 7],SR(_opcode,8));
			}
			else{
				Z80LOGD(%x,"UNK",_opcode);
			}
		break;
	}
	sprintf(&c[8]," %8x ",_opcode);
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	adr +=ipc;
	*padr=adr;
	_opcode=op;
	return 0;
}

int Z80Cpu::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_REGISTER:
		{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					if(!strcasecmp(c,"A"))
						v=REG_A;
					else if(!strcasecmp(c,"B"))
						v=REG_(REGI_B);
					else if(!strcasecmp(c,"C"))
						v=REG_(REGI_C);
					else if(!strcasecmp(c,"D"))
						v=REG_(REGI_D);
					else if(!strcasecmp(c,"E"))
						v=REG_(REGI_E);
					else if(!strcasecmp(c,"H"))
						v=REG_(REGI_H);
					else if(!strcasecmp(c,"L"))
						v=REG_(REGI_L);
					else
						return -1;
				}
				else
					v=REG_(v);
				p[1]=v;
			}
			return 0;
		case ICORE_QUERY_SET_REGISTER:
		{
			u32 v,*p=(u32 *)pv;
			v=p[0];
			if(v == (u32)-1){
				char *c;

				c=*((char **)&p[2]);
				v=p[1];
				if(!strcasecmp(c,"A"))
					REG_A=v;
				else if(!strcasecmp(c,"B"))
					REG_(REGI_B)=v;
				else if(!strcasecmp(c,"C"))
					REG_(REGI_C)=v;
				else if(!strcasecmp(c,"D"))
					REG_(REGI_D)=v;
				else if(!strcasecmp(c,"E"))
					REG_(REGI_E)=v;
				else if(!strcasecmp(c,"H"))
					REG_(REGI_H)=v;
				else if(!strcasecmp(c,"L"))
					REG_(REGI_L)=v;
				else
					return -1;
			}
			else
				REG_(v)=p[1];
		}
			return 0;
		case ICORE_QUERY_IO_PORT:
			*((u64 *)pv)=(u64)_ioreg;
			return 0;
		default:
			return CCore::Query(what,pv);
	}
	return -1;
}

int Z80Cpu::_enterIRQ(int n,int v,u32 pc){
	LOGD("IRQ Enter %d %x\n",n,_pc);
	if(!(REG_IFF & IFF1_BIT)){
		_irq_pending|=n;
		return 0;
	}
	_irq_pending &= ~n;
	REG_SP -= 2;
	WW(REG_SP,_pc);
	REG_IFF &= ~(IFF1_BIT|IFF2_BIT);
	switch(SR(REG_I,4) & 3){
		case 1:
			_pc=0x38;
		break;
		case 2:
		break;
	}
	EnterDebugMode(DEBUG_BREAK_IRQ);
	//EnterDebugMode(0);
	return 0;
}

int Z80Cpu::_dumpRegisters(char *p){
	char cc[1024];

	sprintf(p,"\\bPC\\n:%04X SP:%04X IX:%04X IY:%04X \n\n",_pc,REG_SP,REG16_(REGI_IX),REG16_(REGI_IY));
	sprintf(cc,"\\bA\\n:%02X  \\bB\\n:%02X  \\bC\\n:%02X  \\bD\\n:%02X  \\bE\\n:%02X  \\bH\\n:%02X  \\bL\\n:%02X\n",REG_A,REG_(REGI_B),REG_(REGI_C),
		REG_(REGI_D),REG_(REGI_E),REG_(REGI_H),REG_(REGI_L));
	strcat(p,cc);
	sprintf(cc,"\n\\bA'\\n:%02X  \\bB'\\n:%02X  \\bC'\\n:%02X  \\bD'\\n:%02X  \\bE'\\n:%02X  \\bF'\\n:%02X  \\bH'\\n:%02X  \\bL'\\n:%02X\n",
		REG_(REGI_A+REG_I2),REG_(REGI_B+REG_I2),REG_(REGI_C+REG_I2),REG_(REGI_D+REG_I2),REG_(REGI_E+REG_I2),REG_(REGI_F+REG_I2),
		REG_(REGI_H+REG_I2),REG_(REGI_L+REG_I2));
	strcat(p,cc);
	sprintf(cc,"\nF: %02X C:%c N:%c V:%c A:%c Z:%c S:%c\tIFF: %02X II:%02X IP:%02X\n",REG_(REGI_F), REG_F&C_BIT?49:48,REG_F&N_BIT?49:48,REG_F&V_BIT?49:48,
		REG_F&A_BIT?49:48,REG_F&Z_BIT?49:48,REG_F&S_BIT?49:48,REG_IFF,REG_I,_irq_pending);
	strcat(p,cc);
	return 0;
}

//#include "z80.cpp.inc"

};