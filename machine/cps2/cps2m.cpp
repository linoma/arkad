#include "cps2m.h"
#include "gui.h"


extern GUI gui;

namespace cps2{

#undef M68IO
#define M68IO(a)  CPS2IO(a)
#define ISM68IO(a) 0

#define  CPS2_AUDIOCPU_STATE	8

CPS2M::CPS2M() : Machine(MB(90)),M68000Cpu(),CPS2GPU(),CPS2D(),CPS2Spu() {
	CCore::_freq=MHZ(16);
	_b_regs=&_a_regs[0x40];
}

CPS2M::~CPS2M(){
}

int CPS2M::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);

	m.insert({"width",to_string(_width)});
	m.insert({"height",to_string(_height)});
	return 0;
}

int CPS2M::Load(IGame *pg,char *fn){
	int  res;
	FILE *fp;
	u32 sz,d[10];

	if(!pg || pg->Open(fn,0))
		return -1;
	Reset();
	if(pg->Query(IGAME_GET_INFO,d))
		return -2;

	//_key1=d[0];
	//_key2=d[1];

	//printf("%u %x %5x\n",d[2],d[0],d[1]);

	pg->Read(_memory,MB(30),&sz);

	_decrypt((u16 *)_memory,(u16 *)M_ROM,&d[5],MB(4));
	/*for(int i=0;i<MB(4);i+=2){
		u16 v = *(u16 *)&_memory[i];
		//v=SL((u16)v,16)|(u16)SR(v,16);
		*(u16 *)&_mem[i^2] =v;
	}*/
	memcpy(&_mem[KB(1)],_memory,MB(4));
	CPS2Spu::Load(&_memory[0x1580000]);
	RLPC(0,REG_SP);
	RLPC(4,_pc);

	res=0;
A:
	printf("load %d %u\n",res,sz);
	return res;
}

int CPS2M::Destroy(){
	CPS2GPU::Destroy();
	Machine::Destroy();
	M68000Cpu::Destroy();
	CPS2Spu::Destroy();
	return 0;
}

int CPS2M::Reset(){
	_active_cpu=0;

	Machine::Reset();
	M68000Cpu::Reset();
	CPS2GPU::Reset();
	CPS2Spu::Reset();
	_eeprom._reset();
	memset(_a_regs,0,sizeof(_a_regs));

	Machine::_status &= ~CPS2_AUDIOCPU_STATE;
	//WB(0x619fff,0x77);
	return 0;
}

int CPS2M::Init(){
	if(Machine::Init())
		return -1;
	if(M68000Cpu::Init(&_memory[MB(35)]))
		return -2;
	if(CPS2GPU::Init())
		return -3;
	_cpu.Query(ICORE_QUERY_SET_MACHINE,(IObject *)(ICore *)this);
	//_cpu.Init(&_memory[MB(15)]);
	if(CPS2Spu::Init(&_memory[MB(50)],M_SOUND))
		return -4;


	for(int i=0;i<0x80;i++){
		SetIO_cb(0x804100|i,(CoreMACallback)&CPS2M::fn_port_w,(CoreMACallback)&CPS2M::fn_port_r);
		SetIO_cb(0x804000|i,(CoreMACallback)&CPS2M::fn_port_w,(CoreMACallback)&CPS2M::fn_port_r);
	}
	//_bk.push_back(0x8000000000000716);
	//_bk.push_back(0x8000000000057386);
	//_bk.push_back(0x800000000005729e);
	//_bk.push_back(0x8000000000052e92);
	//_bk.push_back(0x8000000000052eea);
	//_bk.push_back(0x800000000005307a);
	//_bk.push_back(0x80000000000530a0);
	//_bk.push_back(0x80000000000573be);
	return 0;
}

int CPS2M::Exec(u32 status){
	int ret;

	ret=M68000Cpu::Exec(status);
	__cycles=M68000Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			return -ret;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	Machine::_status^=CPS2_AUDIOCPU_STATE;

	if((Machine::_status & CPS2_AUDIOCPU_STATE)){
		switch(CPS2Spu::Exec(status)){
			case -3:
				Machine::_status &= ~CPS2_AUDIOCPU_STATE;
			//	EnterDebugMode();
				break;
			case -2:
				printf("bp\n\n\n");
				return 0x10000002;
		}
	}
	MACHINE_ONEXITEXEC(status,0);
}

int CPS2M::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case (u32)ME_ENDFRAME:
			//CPS3DAC::Update();
			if(!OnFrame())
				CPS2GPU::Update();
			return 0;
		case (u32)-2:
			return 0;
		case 0:
		/*	if(!_gic._irq_pending)
				return 0;
			ev=log2(_gic._irq_pending);
		*/
		break;
		case ME_MOVEWINDOW:{
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
			CPS2GPU::Update();
			Draw();
			return 0;
		case ME_KEYUP:{
				int key;

				va_start(arg, ev);
				key=va_arg(arg,int);
				//*((u16 *)&M_EXT[0])|=SL(1,key);
				va_end(arg);
			}
			return 0;
		case ME_KEYDOWN:{
				int key;

				va_start(arg, ev);
				key=va_arg(arg,int);
				//*((u16 *)&M_EXT[0])&=~SL(1,key);
				va_end(arg);
			}
			return 0;
		default:
			if(BVT(ev,31)){
				CALLVA(Machine::OnEvent,ev,ev);
				return ev;
			}
			{
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				if(i == 1){
				//	BS(_gic._irq_pending,BV(ev));
					return 1;
				}
			}
		break;
	}
	/*
	if(ev <= SR((u8)_sr,4)){
	//	EnterDebugMode();
		BS(_gic._irq_pending,BV(ev));
		return 1;
	}
	BC(_gic._irq_pending,BV(ev));
	if(_ppl.delay)
		return 2;
	switch(ev){
		case 5:
		{
			u8 a;

			if(_timer._ic & _timer.ICF)
				a = _gic._vectors[6];
			else if(_timer._ic & (_timer.OCFA|_timer.OCFB))
				a = _gic._vectors[5];
			else
				a = _gic._vectors[7];
			_enterIRQ(_gic._levels[5],a);
		}
		break;
		case 10:
		case 12:
			_enterIRQ(ev,0x40 + SR(ev,1));
		break;
	}*/
	return 0;
}

int CPS2M::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_NEXT_STEP:{
			switch(*((u32 *)pv)){
				case 1:
				if(_active_cpu==1)
					*((u32 *)pv)=3;
				else{
					u16 op;

					RWOP(_pc,op);
					//printf("qn %x %x\n",op,SR(op,12));
					switch(SR(op,12)){
						case 6:
						if((u8)op == 0)
							*((u32 *)pv)= 4;
						else
							*((u32 *)pv)= 2;
						break;
						case 4:
							*((u32 *)pv)= 6;
						break;
					}
				}
					return 0;
				default:
					*((u32 *)pv)=2;
					if(_active_cpu==1){
						if(Machine::_status & CPS2_AUDIOCPU_STATE)
							*((u32 *)pv)=4;
					}
					return 0;
			}
		}
			return -1;
		case ICORE_QUERY_CPU_ACTIVE_INDEX:
			*((u32 *)pv)=_active_cpu;
			return 0;
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
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"EEPROM");
				strcpy(p->name,"3105");
				p->type=1;
				//p->popup=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Call Stack");
				strcpy(p->name,"3106");
				p->type=1;
				p->popup=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
			}
			return 0;
		case ICORE_QUERY_ADDRESS_INFO:
			{
				u32 adr,*pp,*p = (u32 *)pv;
				adr =*p++;
				pp=(u32 *)*((u64 *)p);
				switch(SR(adr,24)){
					case 0:
						pp[0]=0;
						pp[1]=MB(4);
						break;
					case 2:
						pp[0]=0x02000000;
						pp[1]=KB(512);
					break;
					case 4:
						pp[0]=0x040c0000;
						pp[1]=KB(1);
					break;
					case 6:
						pp[0]=0x06000000;
						pp[1]=MB(16);
					break;
					case 0x24:
						pp[0]=0x24000000;
						pp[1]=MB(16);
						break;
					case 0x25:
						pp[0]=0x25000000;
						pp[1]=MB(64);
						break;
					case 0xc0:
						pp[0]=0xc0000000;
						pp[1]=0x400;
						break;
					default:
						return -2;
				}
			}
			return 0;
		default:
			switch(_active_cpu){
				default:
					return M68000Cpu::Query(what,pv);
				case 1:
					return _cpu.Query(what,pv);
			}
			return -1;
	}
}

int CPS2M::Dump(char **pr){
	int res,i;
	char *c,*cc,*p,*pp;
	u8 *mem;
	u32 adr;
	DEBUGGERDUMPINFO di;

	_dump(pr,&di);
	if((c = new char[600000])==NULL)
		return -1;

	*((u64 *)c)=0;
	cc = &c[590000];
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

	mem=_mem+adr;
	((CCore *)cpu)->_dumpMemory(p,mem,&di);

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
	mem=(u8 *)_eeprom.buffer();
	((CCore *)cpu)->CCore::_dumpMemory(p,mem,0,256);

	res += strlen(p)+1;
	p= &c[res];
	*((u64 *)p)=0;
	*pr = c;
	return res;
}

s32 CPS2M::fn_port_w(u32 a,pvoid,pvoid data,u32 flag){
	//printf("%s %x %x PC:%x\n",__FUNCTION__,a,*(u32 *)data,_pc);
	switch((u16)a){
		case 0x4040:
		case 0x4041:
			if(!(*(u8 *)data & 8))
			_cpu.Restart();
			_eeprom._write(SR(*(u8 *)data,4));
		break;
		default:
			printf("%s %x %x PC:%x %x\n",__FUNCTION__,a,*(u32 *)data,_pc,flag);
	}
	return -1;
}

s32 CPS2M::fn_port_r(u32 a,pvoid,pvoid data,u32){
	switch((u16)a){
		case 0x4020:{
			u8 v;

			_eeprom._read((u8 *)&v);
			*(u32 *)data = SL(v,8);
		//	printf("%s %x %x\n",__FUNCTION__,a,*(u8 *)data);
		}
			return 0;
		default:
			printf("%s %x\n",__FUNCTION__,a);
	}
	return -1;
}

//get data for source
#define LPC(a,b)
#define M(sz,src,data) data=(src)
#define MS M

#define WM(sz,dst,data) switch(sz){case 0:WB(dst,data);break;case 3:case 1:WW(dst,data);break;case 2:WL(dst,data);break;}

#define RM(sz,src,data)\
switch(sz){case 0:RB(src,data);break;case 3:case 1:RW(src,data);break;case 2:RL(src,data);break;}

#define RMPC(sz,src,data) {u32 v___;if(src){\
switch(sz){case 0:v___=*(u8 *)src;break;case 1:v___=(u32)*(u16 *)src;v___=SWAP16(v___);break;case 2:v___=*(u32 *)src;SWAP32(v___);break;}\
data=v___;}}

#define WMPC(sz,src,data) {u32 v___=data;if(src){\
switch(sz){case 0:*(u8 *)src=v___;break;case 1:*(u16 *)src=v___;break;case 2:*(u32 *)src=v___;break;}\
}}

#define RMS(sz,src,data) {void *p___;RMAP_PC((src),p___,R);RMPC(sz,p___,data);}

#define SOURCEM(a) 			(SR(a,3)&7)
#define SOURCER(a) 			(a&7)
#define SOURCEINC(sz,pa) 	(pa += BV(sz))
#define SOURCEINCS(sz,pa)

#define DESTM(a) 		SOURCER(a)
#define DESTR(a)  		SOURCEM(a)
#define DESTINC(sz,pa) 	SOURCEINC(sz,pa)

#define AEA(dst,val,mode,sz,m,iop,mm,am){\
void  *d__;u32 v__;\
switch(mm##M(mode)){\
	case 0:d__=&REG_D(mm##R(mode));v__=*(u32 *)d__;goto CONCAT(A,__LINE__);break;\
	case 1:d__=&REG_A(mm##R(mode));v__=*(u32 *)d__;goto CONCAT(A,__LINE__);break;\
	case 2:RMAP_PC(REG_A(mm##R(mode)),d__,R);m##M##am(sz,REG_A(mm##R(mode)),v__);goto CONCAT(A,__LINE__);break;\
	case 3:{u32 *c__=&REG_A(mm##R(mode));RMAP_PC(*c__,d__,R);m##M##am(sz,*c__,v__);mm##INC##am(sz,*c__);} goto CONCAT(A,__LINE__);break;\
	case 5:{RWPC(_pc+iop,v__);iop+=2;m##M##am((sz),v__+REG_A(mm##R(mode)),v__);}break;\
	case 6:{RWPC(_pc+iop,v__);iop+=2;v__=REG_A(mm##R(mode)) + (s8)v__ + REG_D(SR((u16)v__,12));RMAP_PC(v__,d__,R);m##M##am(sz,v__,v__);goto CONCAT(A,__LINE__);}break;\
	case 7:\
		switch(mm##R(mode)){\
			case 1:RLPC(_pc+iop,v__);iop += 4;m##M##am(sz,v__,v__);break;\
			case 2:RWPC(_pc+iop,v__);v__=_pc+ipc + (s16)v__;iop += 2;break;\
			case 4:switch(sz){case 0:RWPC(_pc+iop,v__);iop+=2;break;case 1:RWPC(_pc+iop,v__);iop += 2;break;case 2:RLPC(_pc+iop,v__);iop+=4;break;}break;\
			default:printf("AEA 7 %x PC:%x\n",mm##R(mode),_pc);break;\
		}\
	break;\
	default:printf("AEA %d\n",mm##R(mode));break;} RMAP_PC(v__,d__,R);\
CONCAT(A,__LINE__): (dst)=d__;(val)=v__; }

#define SMODE(dst,mode,sz,m,iop){\
u32 __d;AEA(d__,__d,mode,(sz),m,iop,SOURCE, );\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}

#define SMODES(dst,mode,sz,m,iop){\
u32 __d;AEAS(d__,__d,mode,(sz),m,iop,SOURCE);\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}
#define DMODE(dst,mode,sz,m,iop){\
u32 __d;AEA(d__,__d,mode,(sz),m,iop,DEST,);\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}

#define AEAS(dst,val,mode,sz,m,iop,mm) AEA(dst,val,mode,sz,m,iop,mm,S)

#define __68O(O,ofs,size,mode,a,b,...){\
	char size_[][10]={"\x2e\x62","\x2ew","\x2el","\x2ew",""};\
	switch(SR(mode,3)&7){\
		case 0:__##O(%s D%d a,b,size_[(size)],(mode &7),## __VA_ARGS__);break;\
		case 1:__##O(%s A%d a,b,size_[(size)],(mode &7),## __VA_ARGS__);break;\
		case 2:__##O(%s [A%d] a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 3:__##O(%s [A%d]+ a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 5:{s16 __d;RWPC(_pc+ofs,__d);__##O(%s [\x24\x23%x\x2c A%d] a,b,size_[size],__d,(mode &7),## __VA_ARGS__);} break;\
		case 6:{u16 __d;u32 __x;RWPC(_pc+ofs,__d);__##O(%s $%x[A%d\x2c %c%d] a,b,size_[size],(s8)__d,mode&7,68,SR(__d,12),## __VA_ARGS__);} break;\
		case 7:\
			switch(mode&7){\
				case 1:{u32 __d;RLPC(_pc+ofs,__d);__##O(%s [\x24%x]\x2el a,b,size_[size],__d,## __VA_ARGS__);} break;\
				case 2:{s16 __d;RWPC(_pc+ofs,__d);__##O(%s [PC\x2c\x24\x23%x] a,b,size_[size],__d,## __VA_ARGS__);}	break;\
				case 4:{\
					u32 __d;\
					switch(size){\
						case 0:RWPC(_pc+ofs,__d);__d=(u8)__d;break;\
						case 1:RWPC(_pc+ofs,__d);break;\
						case 2:RLPC(_pc+ofs,__d);break;\
					} __##O(%s \x24\x23%x a,b,size_[size],__d,## __VA_ARGS__);\
				}\
				break;\
				case 7:__##O(%s a,b,size_[size],## __VA_ARGS__);break;\
			}\
		break;\
		default:\
			__##O(M:%d %s a,b,SR(mode,3)&7,size_[size],## __VA_ARGS__);\
		break;\
	}\
}

#define __68S(ofs,size,mode,a,b,...) __68O(S,ofs,size,mode,a,b,## __VA_ARGS__)
#define _68S(a,b,...) __S(a,b,## __VA_ARGS__)

#define __68F(ofs,size,mode,a,b,...) if((gui._getStatus() & (S_PAUSE/*|S_DEBUG_NEXT*/)) == S_PAUSE){__68O(F,ofs,size,mode,a,b,## __VA_ARGS__);}
#define _68F(a,...) if((gui._getStatus() & (S_PAUSE/*|S_DEBUG_NEXT*/)) == S_PAUSE && 0) __F(a, ## __VA_ARGS__);


//#define __68F(ofs,size,mode,a,b,...)
//#define _68F(a,...)

#define MVDM(sz,mem,ofs,oper,c,sign)  EnterDebugMode();

static char _prs[][4]={};
static char _cc[][4]={"BRA","BF","BHI","BLS","BCC","BCS","BNE","BEQ","BvC","BVS","BPL","BMI","BGE","BLT","BGT","BLE"};

static u8 _szmove[]={0,0,2,1,0,0,0,2};

M68000Cpu::M68000Cpu() : CCore(){
	_regs=NULL;
}

M68000Cpu::~M68000Cpu(){
	Destroy();
}

int M68000Cpu::Destroy(){
	if(_regs)
		delete []_regs;
	_regs=NULL;
	//return 0;
	CCore::Destroy();
	return 0;
}

int M68000Cpu::Reset(){
	CCore::Reset();
	_pc=0;
	_ccr=0;
	memset(_regs,0,0x20*sizeof(u32));
	REG_USP = 0x10000;
	REG_SP = 0x20000;
	return 0;
}

int M68000Cpu::Init(void *m){
	if(!_regs && !(_regs = new u8[0x20*sizeof(u32) + (0x100 * sizeof(CoreMACallback) * 2)]))
		return -1;
	_portfnc_write = (CoreMACallback *)((u32 *)_regs + 0x20);
	_portfnc_read=&_portfnc_write[0x100];
	_mem=(u8 *)m;
	return 0;
}

int M68000Cpu::SetIO_cb(u32 adr,CoreMACallback a,CoreMACallback b){
//	printf("port %x\n",M68IO(adr));
	if(M68IO(adr) > 255 || !_portfnc_write || !_portfnc_read)
		return -1;
	_portfnc_write[M68IO(adr)]=a;
	_portfnc_read[M68IO(adr)]=b;
	return 0;
}

int M68000Cpu::_exec(u32 status){
	int ret,ipc;

	ipc=2;
	ret=1;
	RWOP(_pc,_opcode);
	switch(SR(_opcode,12)){
		case 0:
			switch(_opcode & 0x100){
				case 0:
					switch(SR(_opcode,8) & 0xf){
						case 0:{
							u32 a,b;
							u8 sz;
							void *p;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=(u8)b;
								break;
								case 1:
									RWPC(_pc+2,b);
									b=(u16)b;
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							STATUSFLAGS("orl %0,%%ecx\n",a,a,b);
							WMPC(sz,p,a);
							__68F(2+((sz+2) & 6),sz,_opcode&0x3f,\x2c\x24\x23%x,"ORI",b);
						}
						break;
						case 2:{
							u32 a,b;
							u8 sz;
							void *p;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							AEA(p,a,_opcode & 0x3f,2,R,ipc,SOURCE,);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=SL((u8)b,24);
								break;
								case 1:
									RWPC(_pc+2,b);;
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							STATUSFLAGS("andl %0,%%ecx\n",a,a,b);
							WMPC(sz,p,a);
							__68F(2+((sz+2) & 6),sz,_opcode&0x3f,\x2c #%x,"ANDI",b);
						}
						break;
						case 6:{
							u32 a,b;
							u8 sz;
							void *p;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							AEA(p,a,_opcode & 0x3f,2,R,ipc,SOURCE,);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=SL((u8)b,24);
								break;
								case 1:
									RWPC(_pc+2,b);;
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							STATUSFLAGS("add %0,%%ecx\n",a,a,b);
							WMPC(sz,p,a);
							__68F(2+((sz+2) & 6),sz,_opcode&0x3f,\x2c #%x,"ADDI",b);
						}
						break;
						case 0xc:{
							u32 a,b;
							u8 sz;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							SMODE(a,_opcode,sz,R,ipc);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=(u32)(u8)b;
								//	a=SR(a,24);
								break;
								case 1:
								//	EnterDebugMode();
									RWPC(_pc+2,b);
									b=(u32)(u16)b;
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							//printf("cmpi %x %x %x\n",a,b,sz);
							STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
							__68F(2+((sz+2) & 6),sz,_opcode,\x2c #%x,"CMPI",b);
						}
						break;
						default:
							EnterDebugMode();
							__F(%x,"0 0x000 unk",SR(_opcode,8) & 0xf);
						break;
					}
				break;
				default:
				EnterDebugMode();
					switch(SR(_opcode,6)&3){
						case 2:{
							void *p;
							u32 a;

							//fixme
							AEA(p,a,_opcode,0,R,ipc,SOURCE,);
							__68F(2,0,_opcode,\x2c D%d,"BCLR",SR(_opcode,9)&7);
						}
						break;
						case 3:{
							void *p;
							u32 a;

							//fixme
							AEA(p,a,_opcode,0,R,ipc,SOURCE,);
							__68F(2,0,_opcode,\x2c D%d,"BSET",SR(_opcode,9)&7);
						}
						break;
						default:
							_68F(%x,"0 0x100 unk",SR(_opcode,6)&3);
						break;
					}
				break;
			}
		break;
		case 1:
		case 2:
		case 3://MOVE
			_ccr &= ~(C_BIT|V_BIT);
		//	printf("\t\tmove DM:%x SM:%x\n",SR(_opcode,6)&7,SR(_opcode,3)&7);
			switch(SR(_opcode,6)&7){//dest mode
				case 0:{//Dx
					u32  a;
					void *p;
					u8 sz=_szmove[SR(_opcode,12)&3];

					AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
					REG_D(SR(_opcode,9)&7)=a;
					__68F(2,sz,_opcode,\x2c D%d,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 1:{//Ax
					u32 a,d,sz;

					//EnterDebugMode();
					sz=_szmove[SR(_opcode,12)&3];
					SMODE(a,_opcode & 0x3f,sz,R,ipc);
					REG_A(SR(_opcode,9)&7)=a;
					__68F(2,sz,_opcode,\x2c A%d,"MOVEA",SR(_opcode,9)&7);
				}
				break;
				case 2:{//(Ax)
					u32 a;

					SMODE(a,_opcode & 0x3f,_szmove[SR(_opcode,12)&3],R,ipc);
					WM(SR(_opcode,12)&3,REG_A(SR(_opcode,9)&7),a);
					__68F(2,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 3:{//(Ax)+
					u32 a,sz;
					s16 d = BV(sz=_szmove[SR(_opcode,12)&3]);

					SMODE(a,_opcode & 0x3f,sz,R,ipc);
					WM(sz,REG_A(SR(_opcode,9)&7),a);
					REG_A(SR(_opcode,9)&7)+=d;
					__68F(2,sz,_opcode & 0x3f,\x2c [A%d]+,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 4:{//-(Ax) destmode
					u32 a,sz;
					void*p;
					s16 d;

				//	EnterDebugMode();
					d=BV(sz=SR(_opcode,12)&3);
					sz=_szmove[sz];
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
				//	printf("4 %x %x %x %d %p\n",SOURCEM(_opcode),a,SOURCER(_opcode),d,p);
					switch(SR(_opcode,3)&7){//source mode
						case 0:{//dn,-(An)
							REG_A(SR(_opcode,9)&7)-=d;
							WL(REG_A(SR(_opcode,9)&7),a);
							_68F(D%d\x2c-[A%d],"MOVE",_opcode&7,SR(_opcode,9)&7);
						}
						break;
						case 1:{//An,-(An)
							REG_A(SR(_opcode,9)&7) -= d;
							WL(REG_A(SR(_opcode,9)&7),a);
							_68F(A%d\x2c-[A%d],"MOVE",_opcode&7,SR(_opcode,9)&7);
						}
						break;
						case 5:{
							u32 v,a;

							a=REG_A(SR(_opcode,9)&7);
							RL(a-d,v);
							_pc+=2;
							RWPC(_pc,d);
							MVDM(SR(_opcode,12)&3,_mem,REG_A(_opcode&7)-d,=,v,u);
							REG_A(SR(_opcode,9)&7)-=d;
							_68F(%d[A%d]\x2c-[A%d],"MOVE",d,_opcode&7,SR(_opcode,9)&7);
						}
						break;
						default:
							_68F(4 %x %x,"MOVE",SR(_opcode,9)&7,SR(_opcode,3)&7);
						break;
					}
				}
				break;
				case 5:{//(d,Ax)
					u32 a,sz;
					s16 d;
					void *p;

					//EnterDebugMode();
					RWPC(_pc+ipc,d);
					sz=SR(_opcode,12)&3;
					SMODE(a,_opcode,_szmove[sz],R,ipc);
					ipc+=2;
					WM(sz,REG_A(SR(_opcode,9) & 7)+d,a);
					__68F(2,_szmove[sz],_opcode,\x2c[\x24\x23%x\x2c A%d],"MOVE",d,SR(_opcode,9)&7);
				}
				break;
				case 7:{//dest mode
					u32 s,d;
					void *p;

					SMODE(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					//AEA(p,d,SR(_opcode,6)&63,_szmove[SR(_opcode,12)&3],R,ipc,DEST);

				//	EnterDebugMode();
					switch(SR(_opcode,9)&7){//dst reg
						case 1:
							//EnterDebugMode()
							RLPC(_pc+ipc,d);
							ipc += 4;
							WM(_szmove[SR(_opcode,12)&3],d,s);
							__68F(2,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c\x24%x.l,"MOVE",d);
						break;
						default:
							EnterDebugMode();
							__F(7 %x %x,"MOVE",DESTM(SR(_opcode,6)),DESTR(SR(_opcode,6)));
						break;
					}
				}
				break;
				default:
					EnterDebugMode();
					__F(%x,"MOVE UNK",SR(_opcode,6)&7);
				break;
			}
		break;
		case 0x4:
			switch(SR(_opcode,6)&0x3f){
				default:
					EnterDebugMode();
					__F(%x,"4 UNK",SR(_opcode,6)&0x3f);
				break;
				case 5:{
					u32 a;
					void *p;

					u8 sz = SR(_opcode,6)&3;
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
					WMPC(sz,p,0-a);
					__68F(2,sz,_opcode, ,"NEG");
				}
				break;
				case 0x1b:
					switch(_opcode & 7){
						case 4:
							RWPC(_pc+2,_ccr);
							ipc+=2;
							__68F(2,1,63,#%x\x2c SR,"MOVE",_ccr);
						break;
					}
				break;
				case 0x8:
				case 0x9:
				case 0xa:
				case 0xb:{
					u32 d,sz;

					_ccr = (_ccr & ~(C_BIT|N_BIT|V_BIT)) | Z_BIT;
					//EnterDebugMode();
					sz=SR(_opcode,6)&3;
					SMODE(d,_opcode,2, ,ipc);
					WM(sz,d,0);
					__68F(2,sz,_opcode,NOARG,"CLR");
				}
				break;
				case 0x7:
				case 0xf:
				case 0x17:
				case 0x1f:
				case 0x27:
				case 0x2f:
				case 0x37:
				case 0x3f:{
					u32 d;

					SMODE(d,_opcode,2, ,ipc);
					REG_A(SR(_opcode,9)&7)=d;
					__68F(2,2,_opcode&0x3f,\x2c A%d,"LEA",SR(_opcode,9)&7);
				}
				break;
				case 0x21:{
					u32 v;

					switch(SR(_opcode,3)&7){
						case  0:
							v=REG_D(_opcode&7);
							REG_D(_opcode&7) = SL(v,16)|SR(v,16);
							_68F(D%d,"SWAP",_opcode&7);
						break;
						case 7:
							switch(_opcode&7){
								case 0:
									_pc+=2;
									RWPC(_pc,v);
									REG_SP-=4;
									WLPC(REG_SP,v);
									_68F(#%x,"PEA",v);
								break;
							}
						break;
						default:
							EnterDebugMode();
							__F(%x,"4 21 UNK",SR(_opcode,3)&0x7);
						break;
					}
				}
				break;
				case 0x22:
				case 0x23:{
					u16 l,sz;
					u32 a;
EnterDebugMode();
					RWPC(_pc+ipc,l);
					sz=1+(SR(_opcode,6)&1);
					SMODE(a,_opcode,sz,R,ipc);
					ipc+= 2;
					for(int i=0;i<16;i++){
						if(!(l & BV(i)))
							continue;
						WM(sz,a,REG_D(i));
						a +=SL(sz,1);
					}
					__68F(2,sz,_opcode,%x,"MOVEM",l);
				}
				break;
				case 0x28:
				case 0x29:
				case 0x2a:{
					u32 a;
					u8 sz;

					SMODE(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					_ccr &= ~(N_BIT|Z_BIT|C_BIT|V_BIT);
					if(!a)
						_ccr|+ Z_BIT;
					else if((a & SL(BV(sz),3)))
						_ccr |= N_BIT;
					__68F(2,sz,_opcode,NOARG,"TST");
				}
				break;
				case 0x39:
					switch(_opcode & 0x3f){
						case 0x35:
							_68F(NOARG,"RTS");
							RLPC(REG_SP,_pc);
							REG_SP += 4;
							ipc=0;
						break;
						case 0x31:
							_68F(NOARG,"NOP");
						break;
					}
				break;
				case 0x3a:{
					u32 d;

					SMODE(d,_opcode&63,2,,ipc);
					REG_SP -= 4;
					WLPC(REG_SP,_pc+ipc);
					__68F(2,2,_opcode&63,%x %x,"JSR",_pc+ipc,ipc);
					_pc=d;
					ipc=0;
				}
				break;
				case 0xe5:
					EnterDebugMode();
					switch(_opcode&0x0408){//4e58 4e50 4808
						case 0x400:{
							s16 d;

							_pc+=2;
							RWPC(_pc,d);
							REG_SP-=4;
							WLPC(REG_SP,REG_A(_opcode&7));
							REG_A(_opcode&7)=REG_SP;
							REG_SP += d;
							_68F(A%d\x2c#%d,"LINK",_opcode&7,(s32)(s16)d);
						}
						break;
						case 0x8:
						break;
						default:
							REG_SP=REG_A(_opcode&7);
							RL(REG_SP,REG_A(_opcode&7));
							REG_SP+=4;
							_68F(A%d,"UNLK",_opcode&7);
						break;
					}
				break;
				case 0xeb:
				EnterDebugMode();printf("lino\n");
					REG_SP -= 4;
					switch(SR(_opcode,3)&7){
						case 7:{
							s32 v=0;
							switch(_opcode&7){
								case 1:
									RL(_pc+2,v);
									WL(REG_SP,_pc+4);
								break;
							}
							_68F(%x,"JSR",v);
							_pc=v-2;
						}
						break;
						default:
							_68F(%x,"JSR",SR(_opcode,3)&7);
						break;
					}
				break;
			}
		break;
		case 5:
			switch(SR(_opcode,3)&0x1f){
				case 7:
				case 8:
				case 9:
				case 0x10:{
					u32 a,sz,b;
					void *p;

					(sz=SR(_opcode,6)&3);
					AEA(p,a,_opcode,sz, ,ipc,SOURCE,);
					//RMPC(sz,p,a);
					b=SR(_opcode,9)&7;
					if(!(_opcode & 0x100)){
						STATUSFLAGS("addl %0,%%ecx\n",a,a,b);
						WMPC(sz,p,a);
						__68F(2,sz,_opcode&63,\x2c #%x,"ADDQ",b);
					}
					else{
						//printf("subq %x %x %x",sz,a,b);
						STATUSFLAGS("sub %0,%%ecx\n",a,a,b);
						WMPC(sz,p,a);
						//printf(" %x\n",a);
						__68F(2,sz,_opcode&63,\x2c #%x,"SUBQ",b);
					}
				}
				break;
				case 0x18:
				case 0x1f:{
					char i,s[10]="S\0";

					i=0;
					switch(SR(_opcode,8)&15){
						case 0:
							i=1;
						break;
					}
					if(i){
						u32 d;

						SMODE(d,_opcode,2, ,ipc);
						WM(0,d,0xff);
					}

					strcat(&s[1],_cc[SR(_opcode,8) & 15]);
					__68F(2,2,_opcode&0x3f,NOARG,s);
				}
				break;
				case 0x19:{
					s16 d;
					char i,s[10]="D\0";

					RWPC(_pc+2,d);
					//ipc+=2;
					strcat(&s[1],_cc[SR(_opcode,8) & 15]);
					//EnterDebugMode();
					i=0;
					switch(SR(_opcode,8)&15){
						case 0:
						case 1:
							i = 1;
						break;
						default:
							printf("cc  %d\n",SR(_opcode,8)&15);
						break;
					}

					_68F(D%d\x2c #%x,s,_opcode&7,_pc+d+2);

					if(i){
						u16 a=--REG_D(_opcode&7);
						if(a != (u16)-1)
							_pc += d;
						else i=0;
					}
					if(!i) ipc+=2;
				}
				break;
				default:
					__F(%x,"ADDQ",SR(_opcode,3)&0x1f);
					EnterDebugMode();
				break;
			}
		break;
		case 6:{
			s32 d = (s8)_opcode;
			u32 a,i;

			a=i=0;
			switch((u8)d){
				case 0:
					RWPC(_pc+2,d);
					d=(s32)(s16)d;
					a+=2;
					//printf("0 %d\n",d);
				break;
				case 0xff:
					RLPC(_pc+2,d);
					//printf("1 %d\n",d);
					ipc+=2;
				break;
			}

			switch(SR(_opcode,8)&0xf){
				case 0:
					_68F(%x,_cc[SR(_opcode,8)&0xf],_pc+d);
					i=1;
				break;
				case 1:
					//EnterDebugMode();
					_68F(%x %x %x %x,"BSR",_pc+d+ipc,_pc+ipc,d,ipc);
					REG_SP -= 4;
					WLPC(REG_SP,_pc+ipc+a);
					i=1;
					//ipc=0;
				break;
				case 3:
					_68F(%x,_cc[SR(_opcode,8)&0xf],_pc+d);
					if((_ccr&N_BIT))
						i=1;
				break;
				case 4:
					_68F(%x,_cc[SR(_opcode,8)&0xf],_pc+d);
					if(!(_ccr&C_BIT))
						i=1;
				break;
				case 5:
					_68F(%x,_cc[SR(_opcode,8)&0xf],_pc+d);
					if((_ccr&C_BIT))
						i=1;
				break;
				case 6://bne
					_68F(%x,_cc[SR(_opcode,8)&0xf],_pc+d);
					if(!(_ccr&Z_BIT)){
						i=1;
						//ipc=0;
					}
				break;
				case 7://beq
					_68F(\x24%x,_cc[SR(_opcode,8)&0xf],_pc+d);
					if(_ccr&Z_BIT){
						i=1;
					//	ipc=0;
					}
				break;
				case 0xc:
					EnterDebugMode();
					_68F(%x,_cc[SR(_opcode,8)&0xf],_pc+d);
					if(!(_ccr & (N_BIT|V_BIT))){
						i=1;
					//	ipc=0;
					}
				break;
				default:
					__F(6 %x,"unk",SR(_opcode,8)&0xf);
					EnterDebugMode();
				break;
			}
			if(i) _pc+=d; else ipc+=a;
		}
		break;
		case 0x7:
			REG_D(SR(_opcode,9)&7)=(u32)(s8)_opcode;
			_ccr &= ~(C_BIT|V_BIT|N_BIT|Z_BIT);
			if((u8)_opcode == 0) _ccr |= Z_BIT;
			_68F(#%x\x2c D%d,"MOVEQ",(s8)_opcode,SR(_opcode,9)&7);
		break;
		case 8:{
			u32 a,b,*r;
			void *p;

			u8 sz=SR(_opcode,6) & 3;
			AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
			//_ccr &= ~(C_BIT|V_BIT|Z_BIT|N_BIT);
			r=&REG_D(SR(_opcode,9)&7);
			switch(SR(_opcode,6)&7){
				case 0:
					b=(u8)a;
				break;
				case 1:
					b=(u16)a;
				break;
				case 2:
					b=a;
				break;
				default:
					printf("OR %x PC:%x\n",SR(_opcode,6)&7,_pc);
					EnterDebugMode();
				break;
			}
			STATUSFLAGS("orl %0,%%ecx\n",a,*r,b);
			WMPC(sz,r,a);
			__68F(2,sz,_opcode&63,\x2c D%d,"OR",(SR(_opcode,9)&7));
		}
		break;
		case 9:{
			u32 a,b;
			void *p;
			u8 sz;
			char s[20]={0};

			sz=SR(_opcode,6)&3;
			AEA(p,a,_opcode&0x3f,sz,R,ipc,SOURCE,);
			switch(SR(_opcode,6)&7){
				case 0:
				case 1:
				case 2:
				case 4:
				case 6:
				case 5:
					EnterDebugMode();
				break;
				case 3:
#ifdef _DEVELOP
					sprintf(s,"A%d",SR(_opcode,9)&7);
#endif
					b=(u32)(s32)(s16)REG_A(SR(_opcode,9)&7);
					STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
					REG_A(SR(_opcode,9)&7)=a;
				break;
				case 7:
				EnterDebugMode();
				break;
			}
			__68F(2,sz,_opcode,\x2c%s,"SUB",s);
		}
		break;
		case 0xb:{
			switch(SR(_opcode,6)&7){
				case 0:
				case 1:
				case 2:{
					u32 a,b;
					u8 sz;

					SMODE(a,_opcode&0x3f,(sz=SR(_opcode,6)&3),R,ipc);
					b=REG_D(SR(_opcode,9)&7);
					switch(sz){
						case 2:
						break;
						case 1:
							b=(u32)(u16)b;
						break;
						case 0:
							b=(u32)(u8)b;
						break;
						default:
							EnterDebugMode();
						break;
					}
					//printf("cmp %x %x %x\n",sz,a,b);
					STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
					__68F(2,sz,_opcode&63,\x2c D%d,"CMP",(SR(_opcode,9)&7));
				}
				break;
				case 7:
				case 6:{
					u32 a,b;
					u8 sz;

				//	EnterDebugMode();
					SMODE(a,_opcode,(sz=_szmove[SR(_opcode,6)&3]),R,ipc);
					b=REG_A(SR(_opcode,9)&7);
					switch(sz){
						case 1:
							b=(u16)b;
						break;
					}
					STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
					__68F(2,sz,_opcode&63,\x2c A%d,"CMPA",(SR(_opcode,9)&7));
				}
				break;
				default:
					EnterDebugMode();
					__F(%d,"CMP",SR(_opcode,6)&7);
				break;
			}
		}
		break;
		case 0xc:
			switch(SR(_opcode,6)&7){
				case 3:{
					u32 a,b;

					SMODE(a,_opcode,1,R,ipc);
					b=(u16)REG_D(SR(_opcode,9)&7)*(u16)a;
					REG_D(SR(_opcode,9)&7)=b;
					__68F(2,1,_opcode,\x2c D%d,"MULU",(SR(_opcode,9)&7));
				}
				break;
				default:
					EnterDebugMode();
					__F(%d,"unk C",SR(_opcode,6)&7);
				break;
			}
		break;
		case 0xd:
			switch(SR(_opcode,6)&7){
				case 0://Dx+=ea
				case 1:
				case 2:{
					u32 a;
					u8 sz,b;

					sz=SR(_opcode,6)&3;
					SMODE(a,_opcode,sz,R,ipc);
					switch(sz){
						case 1:
							STATUSFLAGS("add %0,%%cx\n",a,a,REG_D(SR(_opcode,9)&7));
						break;
						case 2:
							STATUSFLAGS("addl %0,%%ecx\n",a,a,REG_D(SR(_opcode,9)&7));
						break;
						default:
							EnterDebugMode();
							break;
					}
					REG_D(SR(_opcode,9)&7)=a;
					if(_ccr  & C_BIT)
						_ccr |= X_BIT;
					else
						_ccr &= ~X_BIT;
					__68F(2,sz,_opcode&63,\x2c D%d,"ADD",(SR(_opcode,9)&7));
				}
				break;
				case 3:
				case 7:{
					u32  a,sz=SR(_opcode&0x100,8)+1;
					SMODE(a,_opcode,sz,R,ipc);
					REG_A(SR(_opcode,9)&7) += a;
					__68F(2,sz,_opcode,\x2c A%d,"ADDA",(SR(_opcode,9)&7));
				}
				break;
				case 4://ea+=Dx
				case 5:
				case 6:
					switch(SR(_opcode,3)&7){
						case 5:{
							s16 d;

							_pc+=2;
							RWPC(_pc,d);
							MVDM(SR(_opcode,6)&3,_mem,REG_A(_opcode&7)+d,+=,REG_D(SR(_opcode,9)&7),u);
							_68F(D%d\x2c%d[A%d],"ADD",SR(_opcode,9)&7,d,_opcode&7);
						}
						break;
						default:
							_68F(%x,"ADD",SR(_opcode,3)&7);
						break;
					}
				break;
				default:
					EnterDebugMode();
					_68F(%x,"unk D",SR(_opcode,6)&7);
				break;
			}
		break;
		case 0xe:
			switch(SR(_opcode&0xe00,7)|SR(_opcode & 0x38,3)){
				case 0x5://lsl
				case 0x19:{
					u32 a,b,c__;
					u8 sz,v;

					a = b = REG_D(_opcode & 7);
					c__=_ccr;
					_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
					if(!(v = SR(_opcode,9)&7))
						v=8;
					switch(sz=SR(_opcode,6)&3){
						case 1:
							b = (u16)SL((u16)a,v-1);
							if(b & 0x8000)
								_ccr |= C_BIT|X_BIT;
							b=(u16)SL(b,1);
							a &= 0xffff0000;
						break;
						default:
						EnterDebugMode();
						break;
					}

					if(!(a|=b))
						_ccr |= Z_BIT;

					REG_D(_opcode&7) = a;
					__68F(2,sz,_opcode&7,\x2c #%x,"LSL",v);
				}
				break;
				case 0xd:{//lsr
					u32 a,b,c__;
					u8 sz,v;

					a= b = REG_D(_opcode&7);
					c__=_ccr;
					_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
					if(!(v = SR(_opcode,9)&7))
						v=8;
					switch(sz=SR(_opcode,6)&3){
						case 1:
							b = SR((u16)a,v-1);
							if(b&1)
								_ccr |= C_BIT|X_BIT;
							b=SR(b,1);
							a &= 0xffff0000;
						break;
						default:
						EnterDebugMode();
						break;
					}
					if(!(a|=b))
						_ccr |= Z_BIT;
					REG_D(_opcode&7)=a;
					__68F(2,sz,_opcode&7,\x2c #%x,"LSR",v);
				}
				break;
				case 0x6:{//roxr
					u32 a,b,c__;
					u8 sz,v;

					a = b = REG_D(_opcode&7);
					c__=_ccr;
					_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
					if(!(v = SR(_opcode,9)&7))
						v=8;
					switch(sz=SR(_opcode,6)&3){
						case 1:
							b = ((u16)SL((u16)a,8-v)) | SR((u16)a,v);
							if(b & 0x8000)
								_ccr |= C_BIT|X_BIT;
							b = (b & ~0x8000)|SL(c__&X_BIT,11);
							a &= 0xFFFF0000;
						break;
						default:
						EnterDebugMode();
						break;
					}
					if(!(a|=b))
						_ccr |= Z_BIT;
					REG_D(_opcode&7) = a;
					__68F(2,sz,_opcode&7,\x2c #%x,"ROXR",v);
				}
				break;
				case 8:
				case 0x10:{
					u32 a,b,c__;
					u8 sz,v;

					a = b = REG_D(_opcode & 7);
					c__=_ccr;
					_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
					if(!(v = SR(_opcode,9)&7))
						v=8;
					switch(sz=SR(_opcode,6)&3){
						case 1:
							b = (s16)SL((s16)a,v-1);
							if(b & 0x8000)
								_ccr |= C_BIT|X_BIT;
							b=(u32)(u16)SL((s16)b,1);
							a &= 0xffff0000;
						break;
						case 2:
							b = (u32)SL((s32)a,v-1);
							if(b & 0x80000000)
								_ccr |= C_BIT|X_BIT;
							b=(u32)SL((s32)b,1);
							a =0;
						break;
						default:
						EnterDebugMode();
						break;
					}
					if(!(a|=b))
						_ccr |= Z_BIT;
					REG_D(_opcode&7) = a;
					__68F(2,sz,_opcode&7,\x2c #%x,"ASL",v);
				}
				break;
				default:
					EnterDebugMode();
					__F(%x,"unk E",SR(_opcode&0xe00,7)|SR(_opcode & 0x38,3));
				break;
			}
		break;
		default:
			__F(%x,"unk",SR(_opcode,12));
			EnterDebugMode();
		break;
	}
	_pc += ipc;
	//printf("%x\n",_pc);
	return ret;
}

int M68000Cpu::Disassemble(char *dest,u32 *padr){
	u32 op,adr,ipc,pc;
	char c[400],*cc;
	u8 *p;

	*((u64 *)c)=0;
	cc=&c[200];
	*((u64 *)cc)=0;

	pc=_pc;
	op=_opcode;

	_pc = *padr;
	RWOP(_pc,_opcode);
	ipc=2;

	//printf("%x %x\n",_pc,_opcode);
	sprintf(c,"%08X ",_pc);
	switch(SR(_opcode,12)){
		case 0:
			switch(_opcode & 0x100){
				case 0:
					switch(SR(_opcode,8) & 0xf){
						case 0:{
							u32 a,b;
							u8 sz;
							void *p;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							AEAS(p,a,_opcode,2,R,ipc,SOURCE);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=(u8)b;
								break;
								case 1:
									RWPC(_pc+2,b);
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							__68S(2+((sz+2) & 6),sz,_opcode,\x2c\x23%x,"ORI",b);
						}
						break;
						case 2:{
							u32 a,b;
							u8 sz;
							void *p;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							AEAS(p,a,_opcode,2,R,ipc,SOURCE);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=SL((u8)b,24);
								break;
								case 1:
									RWPC(_pc+2,b);;
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							__68S(2+((sz+2) & 6),sz,_opcode&0x3f,\x2c #%x,"ANDI",b);
						}
						break;
						case 6:{
							u32 a,b;
							u8 sz;
							void *p;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							AEAS(p,a,_opcode,2,R,ipc,SOURCE);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=SL((u8)b,24);
								break;
								case 1:
									RWPC(_pc+2,b);;
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							__68S(2+((sz+2) & 6),sz,_opcode,\x2c #%x,"ADDI",b);
						}
						break;
						case 0xc:{
							u32 a,b;
							u8 sz;

							sz=SR(_opcode,6) & 3;
							ipc += ((sz+2) & 6);
							SMODES(a,_opcode & 0x3f,2,R,ipc);
							switch(sz){
								case 0:
									RWPC(_pc+2,b);
									b=(u32)(u8)b;
								break;
								case 1:
									RWPC(_pc+2,b);
								break;
								case 2:
									RLPC(_pc+2,b);
								break;
							}
							__68S(2+((sz+2) & 6),sz,_opcode&0x3f,\x2c #%x,"CMPI",b);
						}
						break;
						default:
							_68S(%x,"0 0x000 unk",SR(_opcode,8) & 0xf);
						break;
					}
				break;
				default:
					switch(SR(_opcode,6)&3){
						case 2:{
							void *p;
							u32 a;

							//fixme
							AEAS(p,a,_opcode,0,R,ipc,SOURCE);
							__68S(2,0,_opcode,\x2c D%d,"BCLR",SR(_opcode,9)&7);
						}
						break;
						case 3:{
							void *p;
							u32 a;

							//fixme
							AEAS(p,a,_opcode,0,R,ipc,SOURCE);
							__68S(2,0,_opcode,\x2c D%d,"BSET",SR(_opcode,9)&7);
						}
						break;
						default:
							__S(%x,"0 0x100 unk",SR(_opcode,6)&3);
							//EnterDebugMode();
						break;
					}
				break;
			}
		break;
		case 1:
		case 2:
		case 3://MOVE
			switch(SR(_opcode,6)&7){//dest mode
				case 0:{//Dx
					u32  s;
					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(2,_szmove[SR(_opcode,12)&3],_opcode,\x2c D%d,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 1:{//Ax
					u32 s;
					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(2,_szmove[SR(_opcode,12)&3],_opcode,\x2c A%d,"MOVEA",SR(_opcode,9)&7);
				}
				break;
				case 2:{//(Ax)
					__68S(2,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 3:{//(Ax)+
					__68S(2,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d]+,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 4:{//-(Ax) destmode
					__68S(2,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c -[A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 5:{
					s16 d;

					RWPC(_pc+ipc,d);
					__68S(2,_szmove[SR(_opcode,12)&3],_opcode,\x2c[\x24\x23%x\x2c A%d],"MOVE",d,SR(_opcode,9)&7);
				}
				break;
				case 7:{//dest mode
					u32 s,d;

					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					switch(SR(_opcode,9)&7){//dst reg
						case 0:{
							DMODE(d,SR(_opcode,6)&63,_szmove[SR(_opcode,12)&3],R,ipc);
							__68S(2,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c #%x,"MOVE",d);
						}
						break;
						case 1:
							RLPC(_pc+ipc,d);
							ipc += 4;
							__68S(2,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c $%x.l,"MOVE",d);
						break;
						default:
							_68S(7 %x %x,"MOVE",SR(_opcode,9)&7,SR(_opcode,3)&7);
						break;
					}
				}
				break;
				default:
					_68S(%x,"MOVE UNK",SR(_opcode,6)&7);
				break;
			}
		break;
		case 0x4:
			switch(SR(_opcode,6)&0x3f){
				default:
					_68S(%x,"4 UNK",SR(_opcode,6)&0x3f);
				break;
				case 0x8:
				case 0x9:
				case 0xa:
				case 0xb:{
					u32 d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(2,SR(_opcode,6)&3,_opcode,NOARG,"CLR");
				}
				break;
				case 5:{
					u32  d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(2,SR(_opcode,6)&3,_opcode, ,"NEG");
				}
				break;
				case 0x21:{
					u32 v;

					switch(SR(_opcode,3)&7){
						case  0:
							_68S(D%d,"SWAP",_opcode&7);
						break;
						case 7:
							switch(_opcode&7){
								case 0:
									_68S(#%x,"PEA",v);
								break;
							}
						break;
						default:
							__S(%x,"4 21 UNK",SR(_opcode,3)&0x7);
						break;
					}
				}
				break;
				case 0x1b:
					switch(_opcode & 7){
						case 4:{
							u16 d;
							RWPC(_pc+2,d);
							ipc+=2;
							__68S(2,1,63,#%x\x2c SR,"MOVE",d);
						}
						break;
					}
				break;
				case 0x7:
				case 0xf:
				case 0x17:
				case 0x1f:
				case 0x27:
				case 0x2f:
				case 0x37:
				case 0x3f:{
					u32 d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(2,2,_opcode,\x2c A%d,"LEA",SR(_opcode,9)&7);
				}
				break;
				case 0x22:
				case 0x23:{
					u16 l,sz;
					u32 a;
					char s[50];
					RWPC(_pc+ipc,l);
					sz=1+(SR(_opcode,6)&1);
					SMODES(a,_opcode,sz,R,ipc);
					ipc+= 2;

					*((u64 *)s)=0;
					for(int i=0;i<16;i++){
						char c[10];

						if(!(l&BV(i)))
							continue;
						sprintf(c,"%c%d",i<8 ? 'D':'A',i&7);
						if(*s) strcat(s,",");
						strcat(s,c);
					}
					__68S(2,sz,_opcode,%s,"MOVEM",s);
				}
				break;
				case 0x28:
				case 0x29:
				case 0x2a:{
					u32 a;
					u8 sz;

					SMODES(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					__68S(2,sz,_opcode,NOARG,"TST");
				}
				break;
				case 0x39:
					switch(_opcode & 0x3f){
						case 0x35:
							_68S(NOARG,"RTS");
						break;
						case 0x31:
							_68S(NOARG,"NOP");
						break;
					}
				break;
				case 0x3a:{
					u32 d;

					SMODES(d,_opcode&63,2,R,ipc);
					__68S(2,2,_opcode&63, ,"JSR");
				}
				break;
			}
		break;
		case 5:
			switch(SR(_opcode,3)&0x1f){
				case 7:
				case 8:
				case 9:
				case 0x10:{
					char c[][5]={"ADDQ","SUBQ"};
					__68S(2,SR(_opcode,6)&3,_opcode&63,\x2c #%x,c[SR(_opcode,8)&1],SR(_opcode,9)&7);
				}
				break;
				case 0x18:
				case 0x1f:{
					u32 d;
					char s[10]="S\0";

					strcat(s,_cc[SR(_opcode,8) & 15]);
					SMODES(d,_opcode,2, ,ipc);
					__68S(2,2,_opcode&0x3f,NOARG,s);
				}
				break;
				case 0x19:{
					s16 d;
					char s[10]="D\0";

					RWPC(_pc+2,d);
					ipc+=2;
					strcat(s,_cc[SR(_opcode,8) & 15]);
					_68S(D%d\x2c #%x,s,_opcode&7,_pc+d+2);
				}
				break;
				default:
					__S(%x,"ADDQ",SR(_opcode,3)&0x1f);
				break;
			}
		break;
		case 6:{
			s32 d = (s8)_opcode;
			switch((u8)d){
				case 0:
					RWPC(_pc+2,d);
					d=(s32)(s16)d;
					ipc+=2;
				break;
				case 0xff:
					RLPC(_pc+2,d);
					ipc+=2;
				break;
			}
			switch(SR(_opcode,8)&0xf){
				case 1:
					_68S(\x24%x,"BSR",_pc+d+ipc);
				break;
				default:
					_68S(\x24%x,_cc[SR(_opcode,8)&0xf],_pc+d+ipc);
				break;
			}
		}
		break;
		case 0x7:
			_68S(#%x\x2c D%d,"MOVEQ",(s8)_opcode,SR(_opcode,9)&7);
		break;
		case 8:{
			u32 a,sz;

			sz=SR(_opcode,6) & 3;
			SMODES(a,_opcode & 0x3f,sz,R,ipc);
			__68S(2,sz,_opcode&63,\x2c D%d,"OR",(SR(_opcode,9)&7));
		}
		break;
		case 0xb:{
			switch(SR(_opcode,6)&7){
				case 0:
				case 1:
				case 2:{
					u32 a,b;
					u8 sz;

					SMODES(a,_opcode&0x3f,(sz=SR(_opcode,6)&3),R,ipc);
					__68S(2,sz,_opcode&63,\x2c D%d,"CMP",(SR(_opcode,9)&7));
				}
				break;
				case 6:
				case 7:{
					u32 a,b;
					u8 sz;

					SMODES(a,_opcode&0x3f,(sz=SR(_opcode,6)&3),R,ipc);
					__68S(2,sz,_opcode&63,\x2c A%d %d,"CMPA",(SR(_opcode,9)&7),sz);
				}
				break;
				default:
					_68S(%d,"CMP",SR(_opcode,3)&7);
				break;
			}
		}
		break;
		case 0xd:
			switch(SR(_opcode,6)&7){
				case 0://Dx+=ea
				case 1:
				case 2:{
					u32 a;
					void *p;
					u8 sz,b;

					sz=SR(_opcode,6)&3;
					AEAS(p,a,_opcode&63,sz,R,ipc,SOURCE);
					__68S(2,sz,_opcode&63,\x2c D%d,"ADD",(SR(_opcode,9)&7));
				}
				break;
				case 3:
				case 7:{
					u32 a, sz=SR(_opcode&0x100,8)+1;
					SMODES(a,_opcode,sz,R,ipc);
					__68S(2,sz,_opcode,\x2c A%d,"ADDA",(SR(_opcode,9)&7));
				}
				break;
				case 4://ea+=Dx
				case 5:
				case 6:
					switch(SR(_opcode,3)&7){
						case 5:{
							s16 d;

							_pc+=2;
							RWPC(_pc,d);
							//MVDM(SR(_opcode,6)&3,_mem,REG_A(_opcode&7)+d,+=,REG_D(SR(_opcode,9)&7),u);
							_68S(D%d\x2c%d[A%d],"ADD",SR(_opcode,9)&7,d,_opcode&7);
						}
						break;
						default:
							_68S(%x,"ADD",SR(_opcode,3)&7);
						break;
					}
				break;
				default:
					_68S(%x,"unk D",SR(_opcode,6)&7);
				break;
			}
		break;
		case 0xe:
			switch(SR(_opcode&0xe00,7)|SR(_opcode & 0x38,3)){
				case 0x5://lsl
				case 0x19:{
					u8 sz,v;

					sz=SR(_opcode,6)&3;
					v = SR(_opcode,9)&7;
					__68S(2,sz,_opcode&7,\x2c #%x,"LSL",v);
				}
				break;
				case 0xd:{//lsr
					u8 sz,v;

					sz=SR(_opcode,6)&3;
					v = SR(_opcode,9)&7;

					__68S(2,sz,_opcode&7,\x2c #%x,"LSR",v);
				}
				break;
				case 0x6:{//rorx
					u8 sz,v;

					v = SR(_opcode,9)&7;
					sz=SR(_opcode,6)&3;
					__68S(2,sz,_opcode&7,\x2c #%x,"ROXR",v);
				}
				break;
				default:
					_68S(%x,"unk E",SR(_opcode&0xe00,7)|SR(_opcode & 0x38,3));
				break;
			}
		break;
		case 0xc:
			switch(SR(_opcode,6)&7){
				case 3:{
					u32 a;

					SMODES(a,_opcode,1,R,ipc);
					__68S(2,1,_opcode,\x2c D%d,"MULU",(SR(_opcode,9)&7));
				}
				break;
				default:
					EnterDebugMode();
					__F(%d,"unk C",SR(_opcode,6)&7);
				break;
			}
		break;
		default:
			_68S(NOARG,"NOP");
		break;
	}
	sprintf(&c[8]," %8x ",_opcode);
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	_pc += ipc;
	*padr=_pc;

	_opcode=op;
	_pc=pc;
	return 0;
}

int M68000Cpu::_dumpRegisters(char *p){
	char cc[200];

	sprintf(p,"PC: %08X CCR: %04X Z:%c C:%c V:%c N:%c X:%c\n\n",_pc,_ccr,
		_ccr&Z_BIT ? 49 : 48,_ccr&C_BIT ? 49 : 48,_ccr&V_BIT ? 49 : 48,_ccr&N_BIT ? 49 : 48,_ccr&X_BIT ? 49 : 48);
	for(int i=0;i<8;i++){
		sprintf(cc,"D%d: %08X ",i,REG_D(i));
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	strcat(p,"\n");
	for(int i=0;i<8;i++){
		sprintf(cc,"A%d: %08X ",i,REG_A(i));
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	return 0;
}

int M68000Cpu::_dumpMemory(char *p,u8 *mem,LPDEBUGGERDUMPINFO pdi,u32 sz){
	RMAP_(pdi->_dumpAddress,mem,R);
	return CCore::_dumpMemory(p,mem,pdi,sz);
}

int M68000Cpu::_enterIRQ(int n,u32 pc){
	return  0;
}
int M68000Cpu::OnException(u32 code,u32 b){
	return 0;
}
};