#include "ps2.h"
#include "gui.h"
#include "elf.h"

extern GUI gui;

namespace ps2{

#define __R5900F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE) __F(a,## __VA_ARGS__);
#define __R5900D(a,...)	sprintf(cc,STR(%s) STR(\x20) STR(a),## __VA_ARGS__);

PS2M::PS2M() : Machine(MB(50)),PS2BIOS(){
//	_freq=MHZ(294.912);
}

PS2M::~PS2M(){
}

int PS2M::Load(IGame *pg,char *fn){
	int  res;
	u32 sz,d[10];


	if(!pg && !fn)
		return -1;
	if(!pg)
		pg=new PS2ROM();
	if(!pg || pg->Open(fn,0))
		return -1;
	Reset();
	d[0]=sizeof(d);
	if(pg->Query(IGAME_GET_INFO,d))
		return -2;
printf("%x %x %x %x\n",d[IGAME_GET_INFO_SLOT+1],d[IGAME_GET_INFO_SLOT],d[IGAME_GET_INFO_SLOT+2],d[IGAME_GET_INFO_SLOT+4]);
	pg->Seek(d[IGAME_GET_INFO_SLOT+4],SEEK_SET);
	pg->Read(&_mem[d[IGAME_GET_INFO_SLOT] & 0x1ffffff],d[IGAME_GET_INFO_SLOT+1],NULL);


	_pc=d[IGAME_GET_INFO_SLOT+2];
	//REG_SP=d[IGAME_GET_INFO_SLOT+4];
	///REG_GP=d[IGAME_GET_INFO_SLOT+3];
	Query(ICORE_QUERY_SET_FILENAME,fn);
	res=0;
A:
	return res;
}

int PS2M::Destroy(){
	Machine::Destroy();
	PS2BIOS::Destroy();
	if(_portfnc_write)
		delete []_portfnc_write;
	_portfnc_write = _portfnc_read=NULL;
	return 0;
}

int PS2M::Reset(){
	Machine::Reset();
	return PS2BIOS::Reset();
}

int PS2M::Init(){
	if(Machine::Init())
		return -1;
	if(PS2BIOS::Init(*this))
		return -2;
	//if(PS1GPU::Init())
	//	return -3;
	_portfnc_write = (CoreMACallback *)new CoreMACallback[0x40000];
	if(_portfnc_write==NULL)
		return -5;
	_portfnc_read = &_portfnc_write[0x20000];
	memset(_portfnc_write,0,sizeof(CoreMACallback)*0x40000);

	//AddTimerObj((ICpuTimerObj *)this,_freq/60/314);
	//_bk.push_back(0x80000000800ab014);

	return 0;
}

int PS2M::Exec(u32 status){
	int ret;

	ret=R5900Cpu::Exec(status);
	__cycles=R5900Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			return ret*-1;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int PS2M::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case ME_ENDFRAME:
			//CPS3DAC::Update();
			//if(!OnFrame())
				//PS1GPU::Update();
			return 0;
		case (u32)-2:
			return 0;
		case 0:
			if(!_irq_pending)
				return 0;
			ev=1;
		break;
		default:
			if(BVT(ev,31))
				return -1;
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
	switch(ev){
		default:
		//	OnException(8,ev);
			break;
	}
	return 0;
}

int PS2M::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_MEMORY_WRITE:{
			u32 *p=(u32 *)pv;
			printf("ICORE_QUERY_MEMORY_WRITE %x\n",p[2]);
		}
			return 0;
		case ICORE_QUERY_ADDRESS_INFO:
			{
				u32 adr,*pp,*p = (u32 *)pv;
				adr =*p++;
				pp=(u32 *)*((u64 *)p);
				switch(SR(adr,24)){
					case 0x0:
					case 2:
					case  3:
						pp[0]=adr&0xff000000;
						pp[1]=MB(32);
						break;
					case 0x1f:
						pp[0]=0x1F000000;
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

			}
			return 0;
		default:
			return R5900Cpu::Query(what,pv);
	}
}

int PS2M::Dump(char **pr){
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
#ifdef _DEVELOP
	//sprintf(cc,"C: %u L:%u SR:%8X",__cycles,__line,CP0._regs[CP0.SR]);
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

	RMAP_(adr,mem,R);
	_dumpMemory(p,mem,&di);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;

	*pr = c;
	return res;
}

int PS2M::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	//m["width"]=to_string(_width);
	//m["height"]=to_string(_height);
	return 0;
}

int PS2M::OnCop(RSZU cop,RSZU op,RSZU s,RSZU *d){
	switch(op){
		case 0://mfc
			switch(cop){
				case 0:
					return 0;
			}
		break;
		case 1://cop
			switch(cop){
				case 0:
					switch(_opcode& 0x3f){
						case 0x38:
							//ei
						break;
						case 0x39:
						//di
						break;
					}
					return 0;
			}
		break;
		case 2://cfc
			switch(cop){
			case 0:
				return 0;
			}
		break;
		case 4://mtc
			switch(cop){
				case 0:
					return 0;
			}
		break;
		case 6://ctc
			switch(cop){
				case 0:
					return 0;
			}
	}
	return -1;
}

int PS2M::OnJump(u32 pc){
	return -1;
}

int PS2M::OnException(u32 code,u32 b){
	switch(CP0._regs[CP0.CAUSE]=code){
		case 0:
			DLOG("SYSCALL %x",REG_(REGI_V0+1));
			//	EnterDebugMode();
			switch(REG_(REGI_V0+1)){
				default:
				printf("%08x\n",REG_(REGI_V0+1));
				break;
			}
		break;
	}
	return 0;
}

s32 PS2M::fn_write_io(u32 a,void *pmem,void *pdata,u32){
//	printf("io %x %x\n",a,*(u32 *)pdata);
	//EnterDebugMode();
	switch(a){
		case 0x1f801070:
		//	_irq_pending &= ~*((u32 *)pdata);
	//		PS1IOREG(a) = _irq;
		break;
	}
	return 0;
}


static char prs[][3]={"zr","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7",
	"s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"};

R5900Cpu::R5900Cpu() : CCore(){
	_regs=NULL;
	_io_regs=NULL;
	_freq=MHZ(33);
}

R5900Cpu::~R5900Cpu(){
	Destroy();
}

int R5900Cpu::Destroy(){
	CCore::Destroy();
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int R5900Cpu::Reset(){
	CCore::Reset();
	if(_regs)
		memset(_regs,0,sizeof(RSZU)*40*2);
	_jump=0;
	_irq_pending=0;
	return 0;
}

int R5900Cpu::Init(void *m){
	u32 n;

	if(!(_regs = new u8[n=40*sizeof(RSZU)*2]))
		return -1;
	memset(_regs,0,n);
	_mem=(u8 *)m;
	//_bk.push_back(0x80000000800b13c0);
	return 0;
}

int R5900Cpu::SetIO_cb(u32 a,CoreMACallback w,CoreMACallback r){
	if(!_portfnc_write && !_portfnc_read)
		return -1;
	if(w  && _portfnc_write)
		_portfnc_write[RMAP_IO(a)]=w;
	if(r && _portfnc_read)
		_portfnc_read[RMAP_IO(a)]=r;
	return 0;
}

int R5900Cpu::_dumpRegisters(char *p){
	char cc[1024];

	sprintf(p,"PC: %08X HI: %08X LO: %08X\n\n",_pc,REG_(REGI_HI),REG_(REGI_LO));
	for(int i=1,n=0;i<sizeof(prs)/sizeof(prs[0]);i++){
		sprintf(cc,"%s:%08x ",prs[i],REG_(i));
		strcat(p,cc);
		if(++n == 6){
			strcat(p,"\n");
			n=0;
		}
		cc[0]=0;
	}
	//sprintf(cc,"\nVBR: %08X SR: %08X T:%c IL:%d Q:%c M:%c",_vbr,_sr,_sr&SH_T ? 49:48,SR(_sr&0xf0,4),_sr&SH_Q ? 49:48,_sr&SH_M ? 49:48);
	strcat(p,cc);
	return 0;
}

int R5900Cpu::_exec(u32 status){
	int ret;

	ret=1;
	RLPC(_pc,_opcode);
	//if(_pc==0x80058fdc) EnterDebugMode();
	switch((_opcode >> 24) & 0xFC){
		default:
			printf("PC %08x %08x\n",_pc,(_opcode&0xfc000000));
			EnterDebugMode();
		break;
		case 0:
			switch(_opcode & 0x3f){
				case 0:
					switch(_opcode & 0x7c0){
						case 0:
							__R5900F(NOARG,"NOP");
						break;
						default:
							REG_(RD(_opcode)) = SL(REG_(RT(_opcode)),POS(_opcode));
							__R5900F(%s\x2c%s\x2c$%x,"SLL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
						break;
					}
				break;
				case 2:
					REG_(RD(_opcode)) = SR(REG_(RT(_opcode)),POS(_opcode));
					__R5900F(%s\x2c%s\x2c$%x,"SRL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 3:
					REG_(RD(_opcode)) = SR((s32)REG_(RT(_opcode)),POS(_opcode));//fiixme
					__R5900F(%s\x2c%s\x2c$%x,"SRA",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 4:
					REG_(RD(_opcode)) = SL(REG_(RT(_opcode)),REG_(RS(_opcode))&0x1f);
					__R5900F(%s\x2c%s\x2c%s,"SLLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 6:
					REG_(RD(_opcode)) = SR(REG_(RT(_opcode)),REG_(RS(_opcode))&0x1f);
					__R5900F(%s\x2c%s\x2c%s,"SRLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 7:
					REG_(RD(_opcode)) = SR((s32)REG_(RT(_opcode)),REG_(RS(_opcode))&0x1f);
					__R5900F(%s\x2c%s\x2c%s,"SRAV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 0x8:
					ret+=3;
					_next_pc=REG_(RS(_opcode));
					_jump=2;
					LOADCALLSTACK(_next_pc);
					__R5900F(%s,"JR",prs[RS(_opcode)]);
				break;
				case 0x9:
					REG_RA=_pc+8;
					_next_pc=REG_(RS(_opcode));
					_jump=2;
					STORECALLLSTACK(REG_RA);
					__R5900F(%s,"JALR",prs[RS(_opcode)]);
				break;
				case 0xc:
					OnException(0,SR(_opcode,6)&0xFFFFF);
					__R5900F(%x,"SYSCALL",SR(_opcode,6)&0xFFFFF);
				break;
				case 0xd:
					EnterDebugMode();
					__R5900F(NOARG,"BREAK");
				break;
				case 0x10:
					REG_(RD(_opcode)) = REG_HI;
					__R5900F(%s,"MFHI",prs[RD(_opcode)]);
				break;
				case 0x11:
					REG_LO=REG_(RD(_opcode));
					__R5900F(%s,"MTLO",prs[RD(_opcode)]);
				break;
				case 0x12:
					REG_(RD(_opcode)) = REG_LO;
					__R5900F(%s,"MFLO",prs[RD(_opcode)]);
				break;
				case 0x13:
					REG_HI=REG_(RD(_opcode));
					__R5900F(%s,"MTHI",prs[RD(_opcode)]);
				break;
				case 0x18:{
					s64 value;

					value = (s64)(s32)REG_(RS(_opcode))*(s64)(s32)REG_(RT(_opcode));
					REG_LO=(u32)value;
					REG_HI=(u32)(value >> 32);
					ret += 2;
					__R5900F(%s\x2c%s,"MULT",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x19:{
					u64 value;

					value = (u64)REG_(RS(_opcode))*(u64)REG_(RT(_opcode));
					REG_LO=(u32)value;
					REG_HI=(u32)(value >> 32);
					__R5900F(%s\x2c%s,"MULTU",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x1a:{
					s32 v0,v1,v2;

					v1=REG_(RT(_opcode));
					if(v1){
						v2 = (v0=(s32)REG_(RS(_opcode))) / v1;
						REG_LO=(u32)v2;
						REG_HI=(u32)(v0-(v2*v1));
					}
					__R5900F(%s\x2c%s,"DIV",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x1b:{
					u32 v0,v1,v2;

					v1=REG_(RT(_opcode));
					if(v1){
						v2 = (v0=REG_(RS(_opcode))) / v1;
						REG_LO=(u32)v2;
						REG_HI=(u32)(v0-(v2*v1));
					}
					__R5900F(%s\x2c%s,"DIVU",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x20://'fixme add oveflow xcepion
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"ADD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x21:
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"ADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x22:
					REG_(RD(_opcode)) = REG_(RS(_opcode)) - REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"SUB",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x23:
					REG_(RD(_opcode)) = REG_(RS(_opcode)) - REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"SUBU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x24:
					REG_(RD(_opcode)) = REG_(RS(_opcode))&REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"AND",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x25:
					REG_(RD(_opcode)) = REG_(RS(_opcode))|REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"OR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x26:
					REG_(RD(_opcode)) = REG_(RS(_opcode))^REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"XOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x27:
					REG_(RD(_opcode)) = ~(REG_(RS(_opcode))|REG_(RT(_opcode)));
					__R5900F(%s\x2c%s\x2c%s,"NOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2a:
					REG_(RD(_opcode)) = (s32)REG_(RS(_opcode)) < (s32)REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"SLT",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2b:
					REG_(RD(_opcode)) = REG_(RS(_opcode)) < REG_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"SLTU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
#if MIPS >= 3
				case 0x2d:
					REG64_(RD(_opcode)) = REG64_(RS(_opcode)) + REG64_(RT(_opcode));
					__R5900F(%s\x2c%s\x2c%s,"DADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x3c:
					REG64_(RD(_opcode)) = REG64_(RT(_opcode)) << (32+SA(_opcode));
					__R5900F(%s\x2c%s\x2c#%x,"DSLL32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3f:
					REG64_(RD(_opcode)) = (s64)REG64_(RT(_opcode)) >> (32+SA(_opcode));
					__R5900F(%s\x2c%s\x2c#%x,"DSRA32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
#endif
				default:
					printf("0 %x\n",_opcode & 0x3f);
					EnterDebugMode();
				break;
			}
		break;
		case 0x4:
			switch(FT(_opcode)){
				case 0:
					if((s32)REG_(RS(_opcode))<0){
						_next_pc=_pc+SL(IMM(_opcode),2)+4;
						_jump=2;
					}
					__R5900F(%s\x2c$%x,"BLTZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
				break;
				case 1:
					if((s32)REG_(RS(_opcode)) >=0){
						_next_pc=_pc+SL(IMM(_opcode),2)+4;
						_jump=2;
					}
					__R5900F(%s\x2c$%x,"BGEZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
				break;
				default:
					printf("4 %x\n",_opcode & 0x3f);
					EnterDebugMode();
				break;
			}
		break;
		case 0x8:
			_next_pc=JUMP(_pc,_opcode);
			_jump=2;
			__R5900F(%X,"J",_next_pc);
		break;
		case 0xc:
			REG_RA=_pc+8;
			_next_pc=JUMP(_pc,_opcode);
			_jump=2;
			STORECALLLSTACK(REG_RA);
			ret+=3;
			__R5900F(%X,"JAL",_next_pc);
		break;
		case 0x10:
			if(REG_(RT(_opcode))==REG_(RS(_opcode))){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			__R5900F(%s\x2c%s\x2c$%x,"BEQ",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x14:
			if(REG_(RT(_opcode)) !=REG_(RS(_opcode))){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			__R5900F(%s\x2c%s\x2c$%x,"BNE",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x18:
			if((s32)REG_(RS(_opcode))<=0){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
			}
			__R5900F(%s\x2c$%x,"BLEZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x1c:
			if((s32)REG_(RS(_opcode))>0){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
			}
			__R5900F(%s\x2c$%x,"BGTZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x20:
			REG_(RT(_opcode)) = REG_(RS(_opcode))+IMM(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"ADDI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x24:
			REG_(RT(_opcode)) = (REG_(RS(_opcode))+IMM(_opcode));
			__R5900F(%s\x2c%s\x2c$%x,"ADDUI",prs[RT(_opcode)],prs[RS(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x28:
			REG_(RT(_opcode)) = (s32)REG_(RS(_opcode)) < IMM(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"SLTI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x2C:
			REG_(RT(_opcode)) = REG_(RS(_opcode)) < (u32)IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"SLTIU",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x30:
			REG_(RT(_opcode)) = REG_(RS(_opcode)) & IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"ANDI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x34:
			REG_(RT(_opcode)) = REG_(RS(_opcode))|IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"ORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x38:
			REG_(RT(_opcode)) = REG_(RS(_opcode))^IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"XORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x3c:
			REG_(RT(_opcode)) = IMM(_opcode) << 16;
			__R5900F(%s\x2c$%X,"LUI",prs[RT(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x40:
		case 0x44:
		case 0x48:
		case 0x4c:
			switch(RS(_opcode)){
				case 0:
					OnCop(SR(_opcode,26)&3,0,RD(_opcode),(RSZU *)&REG_(RT(_opcode)));
					__R5900F(%d %s\x2c%s,"MFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 2:
					OnCop(SR(_opcode,26)&3,2,RD(_opcode),(RSZU *)&REG_(RT(_opcode)));
					__R5900F(%d %s\x2c%s,"CFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 4:
					OnCop(SR(_opcode,26)&3,4,RD(_opcode),(RSZU *)&REG_(RT(_opcode)));
					__R5900F(%d %s\x2c%s,"MTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 6:
					OnCop(SR(_opcode,26)&3,6,RD(_opcode),(RSZU *)&REG_(RT(_opcode)));
					__R5900F(%d %s\x2c%s,"CTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 1:
				case 3:
				case 5:
					printf("40 %x\n",RS(_opcode));
					EnterDebugMode();
				break;
				default:
				//	EnterDebugMode();
					OnCop(SR(_opcode,26)&3,1,_opcode & 0xFFFFFF,0);
					__R5900F(%d %x,"COP",SR(_opcode,26)&3,_opcode & 0xFFFFFF);
				break;
			}
		break;
#if MIPS>=3
		case 0x70:
			switch(_opcode&0x3f){
				case 0x18:{
					s64 value;

					value = (s64)(s32)REG_(RS(_opcode))*(s64)(s32)REG_(RT(_opcode));
					REG_LO=(u32)value;
					REG_HI=(u32)(value >> 32);
					if(RD(_opcode))
						REG_(RD(_opcode))=REG_LO;
					ret += 2;
					__R5900F(%s\x2c%s\x2c%s,"MULT1",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				default:
					printf("0x70 %x\n",_opcode&0x3f);
				break;
			}
		break;
		case 0x7c:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			WQ(a,REG_(RT(_opcode)));
			ret+=2;
			__R5900F(%s\x2c$%x[%s],"SQ",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
#endif
		case 0x80:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			u8 v;

			RB(a,v);
			REG_(RT(_opcode))=(s32)(s8)v;
			__R5900F(%s\x2c$%x[%s],"LB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x84:{
			u16 v;
			u32 a=REG_(RS(_opcode))+IMM(_opcode);

			RW(a,v);
			REG_(RT(_opcode))=(s32)(s16)v;
			__R5900F(%s\x2c$%x[%s],"LH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x88:{
			u32 c,b,a=REG_(RS(_opcode))+IMM(_opcode);
			RL(a&~3,b);
			c=REG_(RT(_opcode));
			switch(a & 3){
				case 0:
					c = (c & 0xFFFFFF) | (b << 24);
				break;
				case 1:
					c = (c & 0xFFFF) | (b << 16);
				break;
				case 2:
					c = (c & 0xFF) | (b << 8);
				break;
				case 3:
					c = b;
				break;
			}
			REG_(RT(_opcode))=c;
			__R5900F(%s\x2c$%x[%s],"LWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x8c:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			RL(a,REG_(RT(_opcode)));
			ret++;
			__R5900F(%s\x2c$%x[%s],"LW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x90:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			RB(a,REG_(RT(_opcode)) );
			__R5900F(%s\x2c$%x[%s],"LBU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x94:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			RW(a,REG_(RT(_opcode)));
			ret++;
			__R5900F(%s\x2c$%x[%s],"LHU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x98:{
			u32 c,b,a=REG_(RS(_opcode))+IMM(_opcode);
			RL(a & ~3,b);
			c=REG_(RT(_opcode));
			switch(a & 3){
				case 3:
					c = (c & 0xFFFFFF00) | (b >> 24);
				break;
				case 2:
					c = (c & 0xFFFF0000) | (b >> 16);
				break;
				case 1:
					c = (c & 0xFF000000) | (b >> 8);
				break;
				case 0:
					c = b;
				break;
			}
			REG_(RT(_opcode))=c;
			__R5900F(%s\x2c$%x[%s],"LWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xa0:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			WB(a,REG_(RT(_opcode)));
			__R5900F(%s\x2c$%x[%s],"SB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xa4:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			WW(a,REG_(RT(_opcode)));
			__R5900F(%s\x2c$%x[%s],"SH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xa8:{
			 u32 adr,value,reg;

			adr = REG_(RS(_opcode)) + IMM(_opcode);
			reg = REG_(RT(_opcode));
			RL(adr & ~3,value);
			switch(adr & 3){
				case 2:
					reg = (reg >> 8) | (value & 0xFF000000);
				break;
				case 1:
					reg = (reg >> 16) | (value & 0xFFFF0000);
				break;
				case 0:
					reg = (reg >> 24) | (value & 0xFFFFFF00);
				break;
			}
			WL(adr & ~3,reg);
			__R5900F(%s\x2c$%x[%s],"SWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xac:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			WL(a,REG_(RT(_opcode)));
			ret+=2;
			__R5900F(%s\x2c$%x[%s],"SW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xb8:{
			 u32 adr,value,reg;

			adr = REG_(RS(_opcode)) + IMM(_opcode);
			reg = REG_(RT(_opcode));
			RL(adr & ~3,value);
			switch(adr & 3){
				case 1:
					reg = (reg << 8) | (value & 0xFF);
				break;
				case 2:
					reg = (reg << 16) | (value & 0xFFFF);
				break;
				case 3:
					reg = (reg << 24) | (value & 0xFFFFFF);
				break;
			}
			WL(adr & ~3,reg);
			__R5900F(%s\x2c$%x[%s],"SWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xc8:
			OnCop(SR(_opcode,26)&3,8,REG_(RS(_opcode))+IMM(_opcode),(RSZU *)(u64)RT(_opcode));
			__R5900F(%d %d\x2c$%x[%s],"LWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#if MIPS >= 3
		case 0xdc:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			RP(a,REG64_(RT(_opcode)));
			ret++;
			__R5900F(%s\x2c$%x[%s],"LD",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
#endif
		case 0xe8:
			OnCop(SR(_opcode,26)&3,9,REG_(RS(_opcode))+IMM(_opcode),(RSZU *)(u64)RT(_opcode));
			__R5900F(%d %d\x2c$%x[%s],"SWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#if MIPS >= 3
		case 0xfc:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			WP(a,REG64_(RT(_opcode)));
			ret+=2;
			__R5900F(%s\x2c$%x[%s],"SD",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
#endif
	}
Z:
	_pc+=4;
	if(_jump && --_jump==0){
		_pc=_next_pc;
		if(!(_pc & 0x0fff0000) && !OnJump(_pc))
			_pc = REG_RA;
		if(_irq_pending){//fixme
			//EnterDebugMode();
			machine->OnEvent(0,(LPVOID)-1);
			//_irq=0;
		}
	}
Z1:
	return ret;
}

int R5900Cpu::Disassemble(char *dest,u32 *padr){
	u32 op,adr;
	char c[200],cc[100];
	u8 *p;

	*((u64 *)c)=0;
	*((u64 *)cc)=0;

	adr = *padr;
	RMAP_(adr,p,R);

	sprintf(c,"%08X ",adr);

	*((u64 *)cc)=0;
	op=_opcode;
	RLPC(adr,_opcode);
	sprintf(&c[8]," %8x ",_opcode);
	adr +=4;
	switch((_opcode >> 24) & 0xFC){
		default:
		break;
		case 0:
			switch(_opcode & 0x3f){
				case 0:
					switch(_opcode & 0x7c0){
						case 0:
							__R5900D(NOARG,"NOP");
						break;
						default:
							__R5900D(%s\x2c%s\x2c$%x,"SLL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
						break;
					}
				break;
				case 2:
					__R5900D(%s\x2c%s\x2c$%x,"SRL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 3:
					__R5900D(%s\x2c%s\x2c$%x,"SRA",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 4:
					__R5900D(%s\x2c%s\x2c%s,"SLLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 6:
					__R5900D(%s\x2c%s\x2c%s,"SRLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 7:
					__R5900D(%s\x2c%s\x2c%s,"SRAV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 0x8:
					__R5900D(%s,"JR",prs[RS(_opcode)]);
				break;
				case 0x9:
					__R5900D(%s,"JALR",prs[RS(_opcode)]);
				break;
				case 0xc:
					__R5900D(%x,"SYSCALL",SR(_opcode,6)&0xFFFFF);
				break;
				case 0xd:
					__R5900D(NOARG,"BREAK");
				break;
				case 0x10:
					__R5900D(%s,"MFHI",prs[RD(_opcode)]);
				break;
				case 0x11:
					__R5900D(%s,"MTLO",prs[RD(_opcode)]);
				break;
				case 0x12:
					__R5900D(%s,"MFLO",prs[RD(_opcode)]);
				break;
				case 0x13:
					__R5900D(%s,"MTHI",prs[RD(_opcode)]);
				break;
				case 0x18:
					__R5900D(%s\x2c%s,"MULT",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x19:
					__R5900D(%s\x2c%s,"MULTU",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x1a:
					__R5900D(%s\x2c%s,"DIV",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x1b:
					__R5900D(%s\x2c%s,"DIVU",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x20:
					__R5900D(%s\x2c%s\x2c%s,"ADD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x21:
					__R5900D(%s\x2c%s\x2c%s,"ADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x22:
					__R5900D(%s\x2c%s\x2c%s,"SUB",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x23:
					__R5900D(%s\x2c%s\x2c%s,"SUBU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x24:
					__R5900D(%s\x2c%s\x2c%s,"AND",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x25:
					__R5900D(%s\x2c%s\x2c%s,"OR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x26:
					__R5900D(%s\x2c%s\x2c%s,"XOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x27:
					__R5900D(%s\x2c%s\x2c%s,"NOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2a:
					__R5900D(%s\x2c%s\x2c%s,"SLT",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2b:
					__R5900D(%s\x2c%s\x2c%s,"SLTU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
#if MIPS >=3
				case 0x2d:
					__R5900D(%s\x2c%s\x2c%s,"DADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x3c:
					__R5900D(%s\x2c%s\x2c#%x,"DSLL32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3f:
					__R5900D(%s\x2c%s\x2c#%x,"DSA32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
#endif
			}
		break;
		case 0x4:
			switch(FT(_opcode)){
				case 0:
					__R5900D(%s\x2c$%x,"BLTZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
				break;
				case 1:
					__R5900D(%s\x2c$%x,"BGEZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
				break;
			}
		break;
		case 0x8:
			__R5900D(%X,"J",JUMP(adr,_opcode));
		break;
		case 0xc:
			__R5900D(%X,"JAL",JUMP(adr,_opcode));
		break;
		case 0x10:
			__R5900D(%s\x2c%s\x2c$%x,"BEQ",prs[RT(_opcode)],prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x14:
			__R5900D(%s\x2c%s\x2c$%x,"BNE",prs[RT(_opcode)],prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x18:
			__R5900D(%s\x2c$%x,"BLEZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x1C:
			__R5900D(%s\x2c$%x,"BGTZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x20:
			__R5900D(%s\x2c%s\x2c$%x,"ADDI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x24:
			__R5900D(%s\x2c%s\x2c$%x,"ADDUI",prs[RT(_opcode)],prs[RS(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x28:
			__R5900D(%s\x2c%s\x2c$%x,"SLTI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x2C:
			__R5900D(%s\x2c%s\x2c$%x,"SLTIU",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x30:
			__R5900D(%s\x2c%s\x2c$%x,"ANDI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x34:
			__R5900D(%s\x2c%s\x2c$%x,"ORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x38:
			__R5900D(%s\x2c%s\x2c$%x,"XORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x3c:
			__R5900D(%s\x2c$%X,"LUI",prs[RT(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x40:
			if(RS(_opcode) == 0 && (_opcode &0x04000000)){
				__R5900D(NOARG,"RFE");
				goto A;
			}
		case 0x44:
		case 0x48:
		case 0x4c:
			switch(RS(_opcode)){
				case 0:
					__R5900D(%d %s\x2c%s,"MFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 2:
					__R5900D(%d %s\x2c%s,"CFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 4:
					__R5900D(%d %s\x2c%s,"MTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 6:
					__R5900D(%d %s\x2c%s,"CTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 1:
				case 3:
				case 5:
				break;
				default:
					__R5900D(%d %x,"COP",SR(_opcode,26)&3,_opcode & 0xFFFFFF);
				break;
			}
		break;
#if MIPS>=3
		case 0x70:
			switch(_opcode&0x3f){
				case 0x18:
					__R5900D(%s\x2c%s\x2c%s,"MULT1",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
			}
		break;
		case 0x7c:
			__R5900D(%s\x2c$%x[%s],"SQ",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#endif
		case 0x80:
			__R5900D(%s\x2c$%x[%s],"LB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x84:
			__R5900D(%s\x2c$%x[%s],"LH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x88:
			__R5900D(%s\x2c$%x[%s],"LWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x8c:
			__R5900D(%s\x2c$%x[%s],"LW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x90:
			__R5900D(%s\x2c$%x[%s],"LBU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x94:
			__R5900D(%s\x2c$%x[%s],"LHU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x98:
			__R5900D(%s\x2c$%x[%s],"LWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xa0:
			__R5900D(%s\x2c$%x[%s],"SB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xa4:
			__R5900D(%s\x2c$%x[%s],"SH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xa8:
			__R5900D(%s\x2c$%x[%s],"SWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xac:
			__R5900D(%s\x2c$%x[%s],"SW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xb8:
			__R5900D(%s\x2c$%x[%s],"SWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xc8:
			__R5900D(%d %d\x2c$%x[%s],"LWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#if MIPS >= 3
		case 0xdc:
			__R5900D(%s\x2c$%x[%s],"LD",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#endif
		case 0xe8:
			__R5900D(%d %d\x2c$%x[%s],"SWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#if MIPS >= 3
		case 0xfc:
			__R5900D(%s\x2c$%x[%s],"SD",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#endif
	}
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	*padr=adr;
	_opcode=op;
	return 0;
}

int R5900Cpu::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_REGISTER:
		{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					for(int i =0;i<sizeof(prs)/sizeof(prs[0]);i++){
						if(!strcasecmp(c,prs[i])){
							v=REG_(i);
							goto A;
						}
					}
						return -1;
				}
				else
					v=REG_(v);
A:
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
				for(int i =0;i<sizeof(prs)/sizeof(prs[0]);i++){
					if(!strcasecmp(c,prs[i])){
						REG_(i)=p[1];
						return 0;
					}
				}
				return -1;
			}
			else
				REG_(v)=p[1];
		}
			return 0;
		case ICORE_QUERY_NEXT_STEP:{
			switch(*((u32 *)pv)){
				case 1:{
				//	u32 op;

					//RLPC(_pc,op);
					*((u32 *)pv)=8;
				}
					return 0;
				case 2://f4
					*((u32 *)pv)=2;
					return 0;
				default:
					*((u32 *)pv)=4;
					return 0;
			}
			return -1;
		}
		case ICORE_QUERY_SET_LOCATION:{
			u32 *p=(u32 *)pv;
			void *__tmp;

			RMAP_(p[0],__tmp,W);
			if(!__tmp)
				return -2;
			switch(p[2]){
				case 0:
					memset(__tmp,(u8)p[1],p[4]);
					return 0;
				case 1:{
					u8 *c=(u8 *)__tmp;
					for(u32 i=p[4];i>0;i--)
						*c++=*c-1;
					return 0;
				}
				case 2:{
					u8 *c=(u8 *)__tmp;
					for(u32 i=p[4];i>0;i--)
						*c++=*c+1;
					return 0;
				}
			}
		}
			return -1;
		default:
			return CCore::Query(what,pv);
	}
}

int R5900Cpu:: _enterIRQ(int n,u32 pc){
	return _jump==0 ? 0 : 1;
}



};