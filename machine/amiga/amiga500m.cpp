#include "amiga500m.h"
#include "gui.h"

extern GUI gui;

namespace amiga{

#define MI_RAM 		0
#define MI_BIOS 	(MI_RAM+MB(1))

#define M_RAM 		(&CCore::_mem[MI_RAM])
#define M_RAM2 		(&CCore::_mem[MI_RAM+KB(512)])
#define M_BIOS 		(&CCore::_mem[MI_BIOS])

#define RAM_(a) 	MA_(a,0,0x80000)
#define RAM_R(a,b) 	RAM_(a){b=&M_RAM[(a) & 0x7ffff];}
#define RAM_W(a,b) 	RAM_R(a,b)

#define RAM2_(a) 	MA_(a,0xc00000,0xc7ffff)
#define RAM2_R(a,b) RAM2_(a){b=&M_RAM2[(a) & 0x7ffff];}
#define RAM2_W(a,b) RAM2_R(a,b)

#define BIOS_(a) 	MA_(a,0xf80000,0xFFFFFF)
#define BIOS_R(a,b) BIOS_(a){b=&M_BIOS[(a) & 0x7ffff];}
#define BIOS_W(a,b) BIOS_(a){b=0;}

#define RMAP_(a,b,c)\
BIOS_##c(a,b)\
MAE_ MA_(a,0xd00000,0xdFFFFF){b=(u8 *)M68000Cpu::_io_regs + (u8)a;}\
MAE_ RAM_##c(a,b)\
MAE_ RAM2_##c(a,b)\
MAE_ b=NULL;

#define RMAP_PC(a,b,c)\
BIOS_##c(a,b)\
MAE_ RAM_##c(a,b)\
MAE_ RAM2_##c(a,b)\
MAE_ b=NULL;

#define ISM68IO(a) (a&0x100000)
#define M68IO(a) (SR(a&0x400000,13)|(a&0x1ff))

amiga500m::amiga500m() : Machine(MB(50)),amiga500dev(){
	_freq=MHZ(28.37516)/4;
}

amiga500m::~amiga500m(){
}

int amiga500m::Load(IGame *pg,char *fn){
	int  res;
	u32 sz,d[10];

	if(!pg && !fn)
		return -1;
	if(!pg){
		pg=new FileGame();
		pg->AddFile((char*)GameManager::getFilename(fn).c_str(),1);
	}

	if(!pg || pg->Open(fn,0))
		return -1;

	Reset();
	pg->Read(_memory,MB(20),&sz);
	memcpy(M_BIOS,_memory,KB(512));
	for(int i=0;i<sz&& 0;i+=2){
		M_BIOS[i]=_memory[i+1];
		M_BIOS[i+1]=_memory[i];
	}
	_pc=0xfc00d2;
printf("load %u\n",sz);
	delete (FileGame *)pg;
	Query(ICORE_QUERY_SET_FILENAME,fn);
	res=0;
	return res;
}

int amiga500m::Destroy(){
	Machine::Destroy();
	M68000Cpu::Destroy();
	return 0;
}

int amiga500m::Reset(){
	Machine::Reset();
	amiga500dev::Reset();
	return 0;
}

int amiga500m::Init(){
	if(Machine::Init())
		return -1;
	if(amiga500dev::Init(&_memory[MB(20)],&_memory[MB(36)]))
		return -2;
	_gpu_mem=&M68000Cpu::_io_regs[0x400];
	for(int i=0;i<0x200;i++)
		SetIO_cb(0xdff000|i,(CoreMACallback)&amiga500m::fn_write_io,0);
	for(int i=0;i<0x10;i++)
		SetIO_cb(0xbfe000+i,(CoreMACallback)&amiga500m::fn_write_cia,(CoreMACallback)&amiga500m::fn_read_cia);
	return 0;
}

int amiga500m::OnEvent(u32,...){return 0;}

int amiga500m::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_NEXT_STEP:{
			switch(*((u32 *)pv)){
				case 1:{
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

					return 0;
			}
		}
			return -1;
		case ICORE_QUERY_REGISTER:{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					sscanf(&c[1],"%d",&v);
					if(*c=='A') v+=8;
					v=REG_(v);
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
				int i;

				c=*((char **)&p[2]);
				sscanf(&c[1],"%d",&i);
				if(*c=='A') i+=8;
				REG_(i)=p[1];
				return 0;
			}
			else
				REG_(v)=p[1];
		}
			return 0;
		case ICORE_QUERY_CPUS:{
			char *p = new char[500];
			if(!p) return -1;
			((void **)pv)[0]=p;
			memset(p,0,500);
			strcpy(p,"CPU");
		}
			return 0;
		case ICORE_QUERY_DBG_PAGE:{
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
				strcpy(p->title,"Call Stack");
				strcpy(p->name,"3106");
				p->type=1;
				p->popup=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
			}
			return 0;
		case ICORE_QUERY_ADDRESS_INFO:{
				u32 adr,*pp,*p = (u32 *)pv;
				adr =*p++;
				pp=(u32 *)*((u64 *)p);
				///printf("adr %x\n",adr);
				switch(SR(adr,20)){
					case 0:
						pp[0]=adr&~0x7ffff;
						pp[1]=MB(.5);
						break;
					case 0xf:
						pp[0]=adr&~0xfffff;
						pp[1]=MB(1);
						break;
					case 0xc:
						pp[0]=adr&~0x7ffff;
						pp[1]=KB(512);
						break;
					case 0xd:
						pp[0]=adr&~0x1ff;
						pp[1]=0x200;
						break;
					default:
						return -2;
				}
			}
			return 0;
		default:
			return M68000Cpu::Query(what,pv);
	}
	return -1;
}

int amiga500m::Exec(u32 status){
	int ret;

	ret=M68000Cpu::Exec(status);
	__cycles=M68000Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			return -ret;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int amiga500m::Dump(char **pr){
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

	RMAP_(adr,mem,R);
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

	//mem=(u8 *)_eeprom.buffer();
	//((CCore *)cpu)->CCore::_dumpMemory(p,mem,0,256);

	res += strlen(p)+1;
	p= &c[res];
	*((u64 *)p)=0;
	*pr = c;
	return res;
}

s32 amiga500m::fn_read_io(u32 a,void *pmem,void *pdata,u32){
	return 0;
}

s32 amiga500m::fn_write_io(u32 a,void *pmem,void *pdata,u32){
	switch((u8)SR(a,1)){
		case REG_BLTDDAT:   case REG_DMACONR:   case REG_VPOSR:     case REG_VHPOSR:
		case REG_DSKDATR:   case REG_JOY0DAT:   case REG_JOY1DAT:   case REG_CLXDAT:
		case REG_ADKCONR:   case REG_POT0DAT:   case REG_POT1DAT:   case REG_POTGOR:
		case REG_SERDATR:   case REG_DSKBYTR:   case REG_INTENAR:   case REG_INTREQR:
			return 0;
		case REG_INTENA:
			return Paula::write((u8)SR(a,1),*(u16 *)pdata);
		case REG_JOYTEST:
			return Denise::write((u8)SR(a,1),*(u16 *)pdata);
		case REG_SERDAT:
		if(M68000Cpu::_io_regs[REG_SERDATR] & __cia::SERDATR_TSRE){
			*((u16 *)pdata)=0;
			M68000Cpu::_io_regs[REG_SERDATR] &= ~__cia::SERDATR_TSRE;
			M68000Cpu::_io_regs[REG_SERDATR]|=__cia::SERDATR_TBE;
		}
		else{
			M68000Cpu::_io_regs[REG_SERDATR] &=~__cia::SERDATR_TBE;
		}
			return 1;
		default:
			printf("reg %x\n",a&0x1ff);
			break;
	}
	return 1;
}

s32 amiga500m::fn_read_cia(u32 a,void *pmem,void *pdata,u32){
	switch(SR(a,12)&3){
		case 2:
			return _cia[0].read(a,(u16 *)pdata);
		case 1:
			return _cia[1].read(a,(u16 *)pdata);
		case 0:
		break;
	}
	return 0;
}

s32 amiga500m::fn_write_cia(u32 a,void *pmem,void *pdata,u32){
	if((a&0x2000)==0)
		_cia[1].write(a,*(u16 *)pdata);
	if((a&0x1000)==0)
		_cia[0].write(a,*(u16 *)pdata);
	return 0;
}

//get data for source
#define LPC(a,b)
#define M(sz,src,data) data=(src)
#define MS M

#define RLOP(a,b) RLPC(a,b) //warning  do not touch me!!!!!!

#define WM(sz,dst,data) switch(sz){case 0:WB(dst,data);break;case 3:case 1:WW(dst,data);break;case 2:WL(dst,data);break;}

#define RM(sz,src,data)\
switch(sz){case 0:RB(src,data);break;case 3:case 1:RW(src,data);break;case 2:RL(src,data);break;}

#define RMPC(sz,src,data) {u32 v___;if(src){\
switch(sz){case 0:v___=*(u8 *)src;break;case 1:v___=(u32)*(u16 *)src;break;case 2:v___=*(u32 *)src;break;}data=v___;}}

#define WMPC(sz,src,data) {__data=(data);if(src){\
switch(sz){case 0:*(u8 *)src=__data;break;case 1:*(u16 *)src=__data;break;case 2:*(u32 *)src=__data;break;}}}

#define WMP(sz,a,src,data,op){\
	void *__tmp=(src);\
	__data=(data);\
	if(ISM68IO((a))){\
		register u32 __a=M68IO(a);\
		if(_portfnc_write[__a]){\
			if(!(((CCore *)this)->*_portfnc_write[__a])(a,__tmp,&__data,AM_WRITE)){\
				__tmp=0;\
			}\
		}\
	}\
	WMPC(sz,__tmp,__data);\
}

#define RMS(sz,src,data) {void *p___;RMAP_PC((src),p___,R);RMPC(sz,p___,data);}

#define SOURCEM(a) 			(SR(a,3)&7)
#define SOURCER(a) 			(a&7)
#define SOURCEINC(sz,pa) 	(pa += BV(sz))
#define SOURCEDEC(sz,pa) 	(pa -= BV(sz))
#define SOURCEINCS(sz,pa)
#define SOURCEDECS(sz,pa)

#define DESTM(a) 		SOURCER(a)
#define DESTR(a)  		SOURCEM(a)
#define DESTINC(sz,pa) 	SOURCEINC(sz,pa)

#define AEA(dst,val,mode,sz,m,iop,mm,am)\
void  *d__,*c__=0;u32 v__;__address=0;\
switch(mm##M(mode)){\
	case 0:c__=d__=&REG_D(mm##R(mode));v__=*(u32 *)d__;goto CONCAT(A,__LINE__);break;\
	case 1:c__=d__=&REG_A(mm##R(mode));v__=*(u32 *)d__;goto CONCAT(A,__LINE__);break;\
	case 2:__address=REG_A(mm##R(mode));RMAP_PC(__address,d__,R);m##M##am(sz,__address,v__);goto CONCAT(A,__LINE__);break;\
	case 3:{c__=&REG_A(mm##R(mode));RMAP_PC(*(u32 *)c__,d__,R);__address=*(u32 *)c__;m##M##am(sz,__address,v__);mm##INC##am(sz,*(u32 *)c__);} goto CONCAT(A,__LINE__);break;\
	case 4:{c__=&REG_A(mm##R(mode));mm##DEC##am(sz,*(u32 *)c__);__address=*(u32 *)c__;RMAP_PC(__address=*(u32 *)c__,d__,R);m##M##am(sz,__address,v__);} goto CONCAT(A,__LINE__);break;\
	case 5:{RWPC(_pc+iop,v__);iop+=2;v__=REG_A(mm##R(mode))+(s16)v__;RMAP_PC(v__,d__,R);__address=v__;m##M##am((sz),__address,v__);} goto CONCAT(A,__LINE__);break;\
	case 6:{RWPC(_pc+iop,v__);iop+=2;v__=REG_A(mm##R(mode)) + (s8)v__ + (s16)REG_D(SR((u16)v__,12));RMAP_PC(v__,d__,R);__address=v__;m##M##am(sz,__address,v__);goto CONCAT(A,__LINE__);}break;\
	case 7:\
		switch(mm##R(mode)){\
			case 0:RWPC(_pc+iop,v__);iop += 2;__address=v__;m##M##am(sz,v__,v__);break;\
			case 1:RLOP(_pc+iop,v__);iop += 4;__address=v__;m##M##am(sz,v__,v__);break;\
			case 2:RWPC(_pc+iop,v__);__address=v__;v__=_pc+ipc + (s16)v__;iop += 2;break;\
			case 4:__address=_pc+iop;switch(sz){case 0:RWPC(_pc+iop,v__);iop+=2;break;case 1:RWPC(_pc+iop,v__);iop += 2;break;case 2:RLOP((_pc+iop),v__);iop+=4;break;}break;\
			default:printf("AEA 7 %x PC:%x\n",mm##R(mode),_pc);break;\
		}\
	break;\
	default:printf("AEA %d\n",mm##M(mode));break;} RMAP_PC(v__,d__,R);\
CONCAT(A,__LINE__): (dst)=d__;(val)=v__;

#define AEA_(dst,src,val,mode,sz,m,iop,mm,am){AEA(dst,val,mode,sz,m,iop,mm,am) (src)=c__;}

#define SMODE(dst,mode,sz,m,iop){\
u32 __d;AEA(d__,__d,mode,(sz),m,iop,SOURCE, );\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}

#define AEAS(dst,val,mode,sz,m,iop,mm){ AEA(dst,val,mode,sz,m,iop,mm,S)}

#define SMODES(dst,mode,sz,m,iop){\
u32 __d;AEAS(d__,__d,mode,(sz),m,iop,SOURCE);\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}
#define DMODE(dst,mode,sz,m,iop){\
u32 __d;AEA(d__,__d,mode,(sz),m,iop,DEST,);\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}

#define __68O(O,ofs,size,mode,a,b,...){\
	char size_[][10]={"\x2e\x62","\x2ew","\x2el","\x2ew",""};\
	switch(SR(mode,3)&7){\
		case 0:__##O(%s D%d a,b,size_[(size)],(mode &7),## __VA_ARGS__);break;\
		case 1:__##O(%s A%d a,b,size_[(size)],(mode &7),## __VA_ARGS__);break;\
		case 2:__##O(%s [A%d] a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 3:__##O(%s [A%d]+ a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 4:__##O(%s -[A%d] a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 5:{s16 __d;RWPC(_pc+ofs,__d);ofs+=2;__##O(%s [\x24\x23%x\x2c A%d] a,b,size_[size],__d,(mode &7),## __VA_ARGS__);} break;\
		case 6:{u16 __d;u32 __x;RWPC(_pc+ofs,__d);__##O(%s $%x[A%d\x2c %c%d] a,b,size_[size],(s8)__d,mode&7,68,SR(__d,12),## __VA_ARGS__);} break;\
		case 7:\
			switch(mode&7){\
				case 0:{u16 __d;RWPC(_pc+ofs,__d);__##O(%s [\x24%x]\x2ew a,b,size_[size],__d,## __VA_ARGS__);} break;\
				case 1:{u32 __d;RLOP(_pc+ofs,__d);__##O(%s [\x24%x]\x2el a,b,size_[size],__d,## __VA_ARGS__);} break;\
				case 2:{s16 __d;RWPC(_pc+ofs,__d);__##O(%s [PC\x2c\x24\x23%x] a,b,size_[size],__d,## __VA_ARGS__);}	break;\
				case 4:{\
					u32 __d;\
					switch(size){\
						case 0:RWPC(_pc+ofs,__d);__d=(u8)__d;break;\
						case 1:RWPC(_pc+ofs,__d);ofs+=2;break;\
						case 2:RLOP(_pc+ofs,__d);break;\
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

#define __68F(ofs,size,mode,a,b,...) if((gui._getStatus() & (S_PAUSE/*|S_DEBUG_NEXT*/)) == S_PAUSE){u32 o__=ofs;__68O(F,o__,size,mode,a,b,## __VA_ARGS__);}
#define _68F(a,...) if((gui._getStatus() & (S_PAUSE/*|S_DEBUG_NEXT*/)) == S_PAUSE) __F(a, ## __VA_ARGS__);

//#define __68F(ofs,size,mode,a,b,...)
//#define _68F(a,...)

#define MVDM(sz,mem,ofs,oper,c,sign)  EnterDebugMode();

static char _prs[][4]={};
static char _cc[][4]={"BT","BF","BHI","BLS","BCC","BCS","BNE","BEQ","BVC","BVS","BPL","BMI","BGE","BLT","BGT","BLE"};

static u8 _szmove[]={0,0,2,1,0,0,2,2};

M68000Cpu::M68000Cpu() : CCore(){
	_regs=NULL;
	_io_regs=NULL;
	_opcodescb=NULL;
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
	_ccr=0x2700;
	memset(_regs,0,0x20*sizeof(u32));
	REG_USP = 0;
	REG_ISP = 0;
	REG_SP = REG_ISP;
	return 0;
}

int M68000Cpu::Init(void *m){
	u32 sz;

	sz=0x20*sizeof(u32) + (0x401 * sizeof(CoreMACallback) * 2) +0x2000*sizeof(CoreDecode) +0x2000*sizeof(CoreDisassebler);
	if(!_regs && !(_regs = new u8[sz]))
		return -1;
	memset(_regs,0,sz);
	_portfnc_write = (CoreMACallback *)((u32 *)_regs + 0x20);
	_portfnc_read=&_portfnc_write[0x401];
	_opcodescb=(CoreDecode *)&_portfnc_read[0x401];
	_disopcodecb=(CoreDisassebler *)&_opcodescb[0x2000];

	for(int i=0;i<0x2000;i++){
		_opcodescb[i]=(CoreDecode)&M68000Cpu::_empty_op;
		_disopcodecb[i]=(CoreDisassebler)&M68000Cpu::_empty_op_dis;
	}
	for(int i=0;i<8;i++){
		_opcodescb[0x800|0x22*8+i]=_opcodescb[0x800|0x23*8+i]=(CoreDecode)&M68000Cpu::_movemw;
		_disopcodecb[0x800|0x33*8+i]=_disopcodecb[0x800|0x22*8+i]=_disopcodecb[0x800|0x23*8+i]=(CoreDisassebler)&M68000Cpu::_movemr_dis;
	}
	_opcodescb[0x800|0x23*8]=(CoreDecode)&M68000Cpu::_ext;
	_disopcodecb[0x800|0x23*8]=(CoreDisassebler)&M68000Cpu::_ext_dis;

	for(int i=0;i<0x20;i++){
		_opcodescb[i]=(CoreDecode)&M68000Cpu::_ori;
		_disopcodecb[i]=(CoreDisassebler)&M68000Cpu::_ori_dis;

		_opcodescb[0x40|i]=(CoreDecode)&M68000Cpu::_andi;
		_disopcodecb[0x40|i]=(CoreDisassebler)&M68000Cpu::_andi_dis;

		_opcodescb[0x80|i]=(CoreDecode)&M68000Cpu::_subi;
		_disopcodecb[0x80|i]=(CoreDisassebler)&M68000Cpu::_subi_dis;

		_opcodescb[0xc0|i]=(CoreDecode)&M68000Cpu::_addi;
		_disopcodecb[0xc0|i]=(CoreDisassebler)&M68000Cpu::_addi_dis;

		_opcodescb[0x180|i]=(CoreDecode)&M68000Cpu::_cmpi;
		_disopcodecb[0x180|i]=(CoreDisassebler)&M68000Cpu::_cmpi_dis;
	}
	for(int n=0;n<0x8;n++){
		for(int i=0;i<0x8;i++){
			_opcodescb[SL(n,6)|0x20|i]=(CoreDecode)&M68000Cpu::_btst;
			_disopcodecb[SL(n,6)|0x20|i]=(CoreDisassebler)&M68000Cpu::_btst_dis;

			_opcodescb[SL(n,6)|0x38|i]=(CoreDecode)&M68000Cpu::_bset;
			_disopcodecb[SL(n,6)|0x38|i]=(CoreDisassebler)&M68000Cpu::_bset_dis;

			_opcodescb[SL(n,6)|0x30|i]=(CoreDecode)&M68000Cpu::_bclr;
			_disopcodecb[SL(n,6)|0x30|i]=(CoreDisassebler)&M68000Cpu::_bclr_dis;

			_opcodescb[0x1800|SL(n,6)|0x0|i]=(CoreDecode)&M68000Cpu::_and;
			_opcodescb[0x1800|SL(n,6)|0x8|i]=(CoreDecode)&M68000Cpu::_and;
			_opcodescb[0x1800|SL(n,6)|0x10|i]=(CoreDecode)&M68000Cpu::_and;
			_opcodescb[0x1800|SL(n,6)|0x20|i]=(CoreDecode)&M68000Cpu::_and;
			_opcodescb[0x1800|SL(n,6)|0x28|i]=(CoreDecode)&M68000Cpu::_and;
			_opcodescb[0x1800|SL(n,6)|0x30|i]=(CoreDecode)&M68000Cpu::_and;
			_opcodescb[0x1800|SL(n,6)|0x38|i]=(CoreDecode)&M68000Cpu::_muls;
			_opcodescb[0x1800|SL(n,6)|0x18|i]=(CoreDecode)&M68000Cpu::_mulu;


			_disopcodecb[0x1800|SL(n,6)|0x0|i]=(CoreDisassebler)&M68000Cpu::_and_dis;
			_disopcodecb[0x1800|SL(n,6)|0x8|i]=(CoreDisassebler)&M68000Cpu::_and_dis;
			_disopcodecb[0x1800|SL(n,6)|0x10|i]=(CoreDisassebler)&M68000Cpu::_and_dis;
			_disopcodecb[0x1800|SL(n,6)|0x20|i]=(CoreDisassebler)&M68000Cpu::_and_dis;
			_disopcodecb[0x1800|SL(n,6)|0x28|i]=(CoreDisassebler)&M68000Cpu::_and_dis;
			_disopcodecb[0x1800|SL(n,6)|0x30|i]=(CoreDisassebler)&M68000Cpu::_and_dis;
			_disopcodecb[0x1800|SL(n,6)|0x38|i]=(CoreDisassebler)&M68000Cpu::_muls_dis;
			_disopcodecb[0x1800|SL(n,6)|0x18|i]=(CoreDisassebler)&M68000Cpu::_mulu_dis;
		}
		_opcodescb[0x1800|SL(n,6)|0x20|0x8]=(CoreDecode)&M68000Cpu::_exg;
		_opcodescb[0x1800|SL(n,6)|0x20|0x9]=(CoreDecode)&M68000Cpu::_exg;
		_opcodescb[0x1800|SL(n,6)|0x20|0x11]=(CoreDecode)&M68000Cpu::_exg;

		_disopcodecb[0x1800|SL(n,6)|0x20|0x8]=(CoreDisassebler)&M68000Cpu::_exg_dis;
		_disopcodecb[0x1800|SL(n,6)|0x20|0x9]=(CoreDisassebler)&M68000Cpu::_exg_dis;
		_disopcodecb[0x1800|SL(n,6)|0x20|0x11]=(CoreDisassebler)&M68000Cpu::_exg_dis;
	}

	for(int i=0;i<0x8;i++){
		_opcodescb[0x100|i]=(CoreDecode)&M68000Cpu::_btsti;
		_disopcodecb[0x100|i]=(CoreDisassebler)&M68000Cpu::_btsti_dis;
		_opcodescb[0x110|i]=(CoreDecode)&M68000Cpu::_bclri;
		_disopcodecb[0x110|i]=(CoreDisassebler)&M68000Cpu::_bclri_dis;
		_opcodescb[0x118|i]=(CoreDecode)&M68000Cpu::_bseti;
		_disopcodecb[0x118|i]=(CoreDisassebler)&M68000Cpu::_bseti_dis;

		_opcodescb[0x1C00|SL(i,6)|0x0]=(CoreDecode)&M68000Cpu::_asri;
		_opcodescb[0x1C00|SL(i,6)|0x4]=(CoreDecode)&M68000Cpu::_asri;
		_opcodescb[0x1C00|SL(i,6)|0x8|0]=(CoreDecode)&M68000Cpu::_asri;
		_opcodescb[0x1C00|SL(i,6)|0x8|0|4]=(CoreDecode)&M68000Cpu::_asri;
		_opcodescb[0x1C00|SL(i,6)|0x10|4]=(CoreDecode)&M68000Cpu::_asri;
		_opcodescb[0x1C00|SL(i,6)|0x10|4|0]=(CoreDecode)&M68000Cpu::_asri;

		_opcodescb[0x1C00|SL(i,6)|0x0|0x20]=(CoreDecode)&M68000Cpu::_asli;
		_opcodescb[0x1C00|SL(i,6)|0x4|0x20]=(CoreDecode)&M68000Cpu::_asli;
		_opcodescb[0x1C00|SL(i,6)|0x8|0|0x20]=(CoreDecode)&M68000Cpu::_asli;
		_opcodescb[0x1C00|SL(i,6)|0x8|0|4|0x20]=(CoreDecode)&M68000Cpu::_asli;
		_opcodescb[0x1C00|SL(i,6)|0x10|4|0x20]=(CoreDecode)&M68000Cpu::_asli;
		_opcodescb[0x1C00|SL(i,6)|0x10|4|0|0x20]=(CoreDecode)&M68000Cpu::_asli;

		_opcodescb[0x1C00|SL(i,6)|0x1]=(CoreDecode)&M68000Cpu::_lsri;
		_opcodescb[0x1C00|SL(i,6)|0x5]=(CoreDecode)&M68000Cpu::_lsri;
		_opcodescb[0x1C00|SL(i,6)|0x8|1]=(CoreDecode)&M68000Cpu::_lsri;
		_opcodescb[0x1C00|SL(i,6)|0x8|1|4]=(CoreDecode)&M68000Cpu::_lsri;
		_opcodescb[0x1C00|SL(i,6)|0x10|1]=(CoreDecode)&M68000Cpu::_lsri;
		_opcodescb[0x1C00|SL(i,6)|0x10|4|1]=(CoreDecode)&M68000Cpu::_lsri;

		_opcodescb[0x1C00|SL(i,6)|0x1|0x20]=(CoreDecode)&M68000Cpu::_lsli;
		_opcodescb[0x1C00|SL(i,6)|0x5|0x20]=(CoreDecode)&M68000Cpu::_lsli;
		_opcodescb[0x1C00|SL(i,6)|0x8|1|0x20]=(CoreDecode)&M68000Cpu::_lsli;
		_opcodescb[0x1C00|SL(i,6)|0x8|1|4|0x20]=(CoreDecode)&M68000Cpu::_lsli;
		_opcodescb[0x1C00|SL(i,6)|0x10|1|0x20]=(CoreDecode)&M68000Cpu::_lsli;
		_opcodescb[0x1C00|SL(i,6)|0x10|4|1|0x20]=(CoreDecode)&M68000Cpu::_lsli;

		_opcodescb[0x1C00|SL(i,6)|0x3]=(CoreDecode)&M68000Cpu::_rori;
		_opcodescb[0x1C00|SL(i,6)|0x4|3]=(CoreDecode)&M68000Cpu::_rori;
		_opcodescb[0x1C00|SL(i,6)|0x8|3]=(CoreDecode)&M68000Cpu::_rori;
		_opcodescb[0x1C00|SL(i,6)|0x8|3|4]=(CoreDecode)&M68000Cpu::_rori;
		_opcodescb[0x1C00|SL(i,6)|0x10|3]=(CoreDecode)&M68000Cpu::_rori;
		_opcodescb[0x1C00|SL(i,6)|0x10|4|3]=(CoreDecode)&M68000Cpu::_rori;

		_opcodescb[0x1C00|SL(i,6)|0x3|0x20]=(CoreDecode)&M68000Cpu::_roli;
		_opcodescb[0x1C00|SL(i,6)|0x4|3|0x20]=(CoreDecode)&M68000Cpu::_roli;
		_opcodescb[0x1C00|SL(i,6)|0x8|3|0x20]=(CoreDecode)&M68000Cpu::_roli;
		_opcodescb[0x1C00|SL(i,6)|0x8|3|4|0x20]=(CoreDecode)&M68000Cpu::_roli;
		_opcodescb[0x1C00|SL(i,6)|0x10|3|0x20]=(CoreDecode)&M68000Cpu::_roli;
		_opcodescb[0x1C00|SL(i,6)|0x10|4|3|0x20]=(CoreDecode)&M68000Cpu::_roli;
	}
	_opcodescb[0x4f]=(CoreDecode)&M68000Cpu::_andisr;
	_disopcodecb[0x4f]=(CoreDisassebler)&M68000Cpu::_andisr_dis;
	_opcodescb[0xf]=(CoreDecode)&M68000Cpu::_orisr;
	_disopcodecb[0xf]=(CoreDisassebler)&M68000Cpu::_orisr_dis;
	_mem=(u8 *)m;
	return 0;
}

int M68000Cpu::SetIO_cb(u32 adr,CoreMACallback a,CoreMACallback b){
	//printf("port %x %x\n",adr,M68IO(adr));
	if(M68IO(adr) >= 0x400 || !_portfnc_write || !_portfnc_read)
		return -1;
	_portfnc_write[M68IO(adr)]=a;
	_portfnc_read[M68IO(adr)]=b;
	return 0;
}

int M68000Cpu::_exec(u32 status){
	int ret;

	ipc=2;
	ret=1;
	RWOP(_pc,_opcode);
	switch(SR(_opcode,12)){
		case 0:
			(((CCore *)this)->*_opcodescb[SR(_opcode&0xffff,3)])();
		break;
		case 1:
		case 2:
		case 3:{//MOVE
			u32 res__;

			_ccr &= ~(C_BIT|V_BIT|Z_BIT|V_BIT);
		//	printf("\t\tmove DM:%x SM:%x\n",SR(_opcode,6)&7,SR(_opcode,3)&7);
			switch(SR(_opcode,6)&7){//dest mode
				case 0:{//Dx
					void *p;
					u8 sz=_szmove[SR(_opcode,12)&3];

					AEA(p,res__,_opcode,sz,R,ipc,SOURCE, );
					p=&REG_D(SR(_opcode,9)&7);
					WMPC(sz,p,res__);
					__68F(2,sz,_opcode,\x2c D%d %x %x %x,"MOVE",SR(_opcode,9)&7,sz,__address,res__);
				}
				break;
				case 1:{//Ax
					u32 d,sz;
					void *p;
					//EnterDebugMode();
					sz=_szmove[SR(_opcode,12)&3];
					SMODE(res__,_opcode & 0x3f,sz,R,ipc);
					p=&REG_A(SR(_opcode,9)&7);
					WMPC(sz,p,res__);
					__68F(2,sz,_opcode,\x2c A%d,"MOVEA",SR(_opcode,9)&7);
				}
				break;
				case 2:{//(Ax)
					SMODE(res__,_opcode & 0x3f,_szmove[SR(_opcode,12)&3],R,ipc);
					WM(SR(_opcode,12)&3,REG_A(SR(_opcode,9)&7),res__);
					__68F(2,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 3:{//(Ax)+
					u32 sz;
					s16 d = BV(sz=_szmove[SR(_opcode,12)&3]);

					SMODE(res__,_opcode & 0x3f,sz,R,ipc);
					WM(sz,REG_A(SR(_opcode,9)&7),res__);
					REG_A(SR(_opcode,9)&7)+=d;
					__68F(2,sz,_opcode & 0x3f,\x2c [A%d]+,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 4:{//-(Ax) destmode
					u32 sz;
					void*p;
					s16 d;

				//	EnterDebugMode();
					sz=_szmove[SR(_opcode,12)&3];
					d=BV(sz);

					AEA(p,res__,_opcode,sz,R,ipc,SOURCE,);
				//	printf("4 %x %x %x %d %p\n",SOURCEM(_opcode),a,SOURCER(_opcode),d,p);
					switch(SR(_opcode,3)&7){//source mode
						case 0:{//dn,-(An)
							REG_A(SR(_opcode,9)&7)-=d;
							WM(sz,REG_A(SR(_opcode,9)&7),res__);
							__68F(2,sz,_opcode,\x2c-[A%d],"MOVE",SR(_opcode,9)&7);
						}
						break;
						case 1:{//An,-(An)
							REG_A(SR(_opcode,9)&7) -= d;
							WM(sz,REG_A(SR(_opcode,9)&7),res__);
							__68F(2,sz,_opcode,\x2c-[A%d] %d,"MOVE",SR(_opcode,9)&7,d);
						}
						break;
						case 5:{
							u32 v,a;

							a=REG_A(SR(_opcode,9)&7);
							RL(a-d,v);
							_pc+=2;
							RWPC(_pc,d);
							//MVDM(SR(_opcode,12)&3,_mem,REG_A(_opcode&7)-d,=,v,u);
							EnterDebugMode();
							REG_A(SR(_opcode,9)&7) -= d;
							_68F(%d[A%d]\x2c-[A%d],"MOVE",d,_opcode&7,SR(_opcode,9)&7);
						}
						break;
						case 7:{
							REG_A(SR(_opcode,9)&7) -= d;
							switch(_opcode&7){
								case 4:
									WM(sz,REG_A(SR(_opcode,9)&7),res__);
									__68F(2,sz,_opcode,\x2c -[A%d] %d,"MOVE",SR(_opcode,9)&7,d);
								break;
								default:
									printf("%x\n",_opcode&7);
								break;
							}
						}
						break;
						default:
							_68F(4 %x %x,"MOVE",SR(_opcode,9)&7,SR(_opcode,3)&7);
						break;
					}
				}
				break;
				case 5:{//(d16,Ax)
					u32 sz;
					s16 d;

					sz=_szmove[SR(_opcode,12)&3];
					SMODE(res__,_opcode,sz,R,ipc);
					RWPC(_pc+ipc,d);
					ipc +=2;
					WM(sz,REG_A(SR(_opcode,9) & 7)+d,res__);
					__68F(2,sz,_opcode,\x2c[\x24\x23%x\x2c A%d],"MOVE",d,SR(_opcode,9)&7);
				}
				break;
				case 7:{//dest mode
					u32 d;
					void *p;

					SMODE(res__,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					switch(SR(_opcode,9)&7){//dst reg
						case 0:
							//EnterDebugMode()
							RWPC(_pc+ipc,d);
							ipc += 2;
							WM(_szmove[SR(_opcode,12)&3],d,res__);
							__68F(2,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c\x24%x.l %x %x,"MOVE",d,res__,_szmove[SR(_opcode,12)&3]);
						break;
						case 1:
							//EnterDebugMode()
							RLOP(_pc+ipc,d);
							ipc += 4;
							WM(_szmove[SR(_opcode,12)&3],d,res__);
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
			if(!res__) _ccr |= Z_BIT;
		}
		break;
		case 0x4:{
			switch(SR(_opcode,6)&0x3f){
				default:
					EnterDebugMode();
					__F(%x,"4 UNK",SR(_opcode,6)&0x3f);
				break;
				case 3:{
					switch(SOURCEM(_opcode)){
						case 4:
							REG_A(_opcode&7)-=2;
							WM(1,REG_A(_opcode&7),_ccr);
						break;
					}
					__68F(2,1,_opcode,%d ,"MVSR",SOURCEM(_opcode));
				}
				break;
				case 5:
				case 0x12:{
					u32 a;
					void *p;

					u8 sz = SR(_opcode,6)&3;
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
					WMP(sz,__address,p,0-a,_opcode);
					__68F(2,sz,_opcode, ,"NEG");
				}
				break;
				case 0x19:
				case 0x1a:{
					u32 a;
					void *p;

					u8 sz = SR(_opcode,6)&3;
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
					_ccr &= ~(C_BIT|N_BIT|V_BIT|Z_BIT);
					if((a=~a)==0)
						_ccr |= Z_BIT;
					else if(a&0x80000000)
						_ccr |= N_BIT;
					WMP(sz,__address,p,a,_opcode);
					__68F(2,sz,_opcode, ,"NOT");
				}
				break;
				case 0x1b:
					switch(_opcode & 7){
						case 4:{
							u16 d;

							RWPC(_pc+2,d);
							ipc+=2;
							if(_ccr&S_BIT)
								_ccr=d;
							else
								_ccr= (_ccr & 0xff00) |(u8)d;
							_68F(#%x\x2c SR,"MOVE",_ccr);
						}
						break;
					}
				break;
				case 0x8:
				case 0x9:
				case 0xa:{
					u32 d,sz;

					_ccr = (_ccr & ~(C_BIT|N_BIT|V_BIT)) | Z_BIT;
					//EnterDebugMode();
					sz=SR(_opcode,6)&3;
					SMODE(d,_opcode,2, ,ipc);
					WM(sz,d,0);
					__68F(2,sz,_opcode,NOARG,"CLR");
				}
				break;
				case 0x27:
				case 0x7:
				case 0xf:
				case 0x17:
				case 0x1f:
				case 0x2f:
				case 0x37:
				case 0x3f:
					_lea();
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
								case 1:
									ipc+=4;
									RLPC(_pc+2,v);
									REG_SP -=4;
									WLPC(REG_SP,v);
									_68F(#%x,"PEA",v);
								break;
								case 0:
									ipc+=2;
									RWPC(_pc+2,v);
									REG_SP -=4;
									WLPC(REG_SP,v);
									_68F(#%x,"PEA",v);
								break;
								default:
									printf("4 21 7 %x\n",_opcode&7);
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
				case 0x23:
					(((CCore *)this)->*_opcodescb[0x800|SR(_opcode&0xff8,3)])();
				break;
				case 0x28:
				case 0x29:
				case 0x2a:{
					u32 a;
					u8 sz;

					SMODE(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					///printf("tst %x\n",a);
					_ccr &= ~(N_BIT|Z_BIT|C_BIT|V_BIT);
					if(!a)
						_ccr |= Z_BIT;
					else if((a & SL(BV(sz),3)))
						_ccr |= N_BIT;
					__68F(2,sz,_opcode,NOARG,"TST");
				}
				break;
				case 0x32:
				case 0x33:{
					u16 l,sz;
					u32 a,v;
					void *p,*ps;
					int i,n;

					RWPC(_pc+ipc,l);
					ipc+= 2;
					sz=1+(SR(_opcode,6)&1);
					AEA_(p,ps,a,_opcode,sz,R,ipc,SOURCE, );
					if(SOURCEM(_opcode)==4){
						for(n=0,i=15;i>=0;i--){
							if(!(l & BV(15-i)))
								continue;
							RMPC(sz,p,v);
							REG_D(i)=v;
							p = (u8 *)p  - SL(sz,1);
							n+=SL(sz,1);
						}
						*(u32 *)ps -= (n-SL(sz,1));
					}
					else{
						for(n=i=0;i<16;i++){
							if(!(l & BV(i)))
								continue;
							RMPC(sz,p,v);
							REG_D(i)=v;
							p = (u8 *)p  + SL(sz,1);
							n+=SL(sz,1);
						}
						if(ps)
							*(u32 *)ps += n-SL(sz,1);
					}
					__68F(4,sz,_opcode,%x,"MOVEM",l);
				}
				break;
				case 0x39:
					switch(_opcode & 0x3f){
						case 0x15:{
							s16 d;

							RWPC(_pc+2,d);
							ipc+=2;
							REG_SP-=4;
							WLPC(REG_SP,REG_A(_opcode&7));
							REG_A(_opcode&7)=REG_SP;
							REG_SP += d;
							_68F(A%d\x2c#%x,"LINK",_opcode&7,(s32)(s16)d);
						}
						break;
						case 0x31:
							_68F(NOARG,"NOP");
						break;
						case 0x30:
							_68F(NOARG,"RESET");
						break;
						case 0x33:{
							u16 v;

							_68F(NOARG,"RTE");
							RWPC(REG_SP,v);
							REG_SP += 2;
							RLPC(REG_SP,_pc);
							REG_SP += 4;
							LOADCALLSTACK(_pc);
							_change_sr(v);
							ipc=0;
						}
						break;
						case 0x35:
							_68F(NOARG,"RTS");
							RLPC(REG_SP,_pc);
							REG_SP += 4;
							LOADCALLSTACK(_pc);
							ipc=0;
						break;
						case 0x3b:
							_68F(NOARG,"ILLEGAL");
							OnException(4,0);
						break;
						default:
							switch(SR(_opcode,4)&3){
								case 2:
									if(_opcode &8){
										REG_A(_opcode&7)=REG_USP;
										_68F(USP\x2c A%d,"MOVE",_opcode&7);
									}
									else{
										REG_USP=REG_A(_opcode&7);
										_68F(A%d\x2cUSP,"MOVE",_opcode&7);
									}
								break;
								default:
									printf("4 39 %x\n",SR(_opcode,4)&3);
									EnterDebugMode();
								break;
							}
						break;
					}
				break;
				case 0x3a:{
					u32 d;

					SMODE(d,_opcode&63,2,,ipc);
					REG_SP -= 4;
					WLPC(REG_SP,_pc+ipc);
					STORECALLLSTACK(_pc+ipc);
					__68F(2,2,_opcode&63,%x %x,"JSR",_pc+ipc,ipc);
					_pc=d;
					ipc=0;
				}
				break;
				case 0x3b:{
					u32 d;

					SMODE(d,_opcode&63,2,,ipc);
					__68F(2,2,_opcode&63,%x %x,"JMP",_pc+ipc,ipc);
					_pc=d;
					ipc=0;
				}
				break;
			}
		}
		break;
		case 5:
			switch(SR(_opcode,3)&0x1f){
				case 5:
				case 7:
				case 8:
				case 9:
				case 0x10:
				case 0x11:
				case 0x12:{
					u32 a,sz,b;
					void *p;

					sz=SR(_opcode,6)&3;
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
					if(!(b=SR(_opcode,9)&7))b=8;
				//	printf("%x %x %x %x\n",a,b,__address,_pc);
					if(!(_opcode & 0x100)){
						STATUSFLAGS("addl %0,%%ecx\n",a,a,b);
						if(sz==2 && (SR(_opcode,3)&7) > 1) a=BELE32(a);
						WMP(sz,__address,p,a,_opcode);
						__68F(2,sz,_opcode,\x2c #%x,"ADDQ",b);
					}
					else{
						//printf("subq %x %x %x",sz,a,b);
						STATUSFLAGS("sub %0,%%ecx\n",a,a,b);
						if(sz==2 && (SR(_opcode,3)&7) > 1) a=BELE32(a);
						WMP(sz,__address,p,a,_opcode);
					//	printf(" %x\n",a);
						__68F(2,sz,_opcode,\x2c #%x,"SUBQ",b);
					}
				}
				break;
				case 0x18:
				case 0x1f:{
					char i,s[10]="S\0";

					i=0;
					switch(SR(_opcode,8)&15){
						case 0:
							i=0xff;
						break;
						case 6:
							if(!(_ccr&Z_BIT)){
								i=0xff;
						//ipc=0;
							}
						break;
						default:
							EnterDebugMode();
							printf("Scc %x\n",SR(_opcode,8)&15);
						break;
					}
					{
						u32 d;
						void *p;
						AEA(p,d,_opcode,2, ,ipc,SOURCE,);
						WMPC(0,p,i);
					}
					__68F(2,0,_opcode,%s,"S",_cc[SR(_opcode,8) & 15]);
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
							i=1;
						break;
						case 1:
							i=1;
						break;
						case 7:
							if(!(_ccr & Z_BIT)) i=1;
						break;
						default:
							printf("Dcc  %d\n",SR(_opcode,8)&15);
						break;
					}

					_68F(D%d\x2c #%x,s,_opcode&7,_pc+d+2);

					if(i){
						u16 a=(u16)REG_D(_opcode&7);
						REG_D(_opcode&7)=(REG_D(_opcode&7)&0xffff0000)|--a;
						if(a != (u16)-1)
							_pc += d;
						else
							i=0;
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
			char s[10];

			a=i=0;
			switch((u8)d){
				case 0:
					RWPC(_pc+2,d);
					d=(s32)(s16)d;
					a+=2;
					//printf("0 %d\n",d);
				break;
				case 0xff:
					RLOP(_pc+2,d);
					//printf("1 %d\n",d);
					ipc+=2;
				break;
			}
			strcpy(s,_cc[SR(_opcode,8)&0xf]);
			switch(SR(_opcode,8)&0xf){
				case 0:
					i=1;
				break;
				case 1:
					//EnterDebugMode();
					strcpy(s,"BSR");
					REG_SP -= 4;
					WLPC(REG_SP,_pc+ipc+a);
					i=1;
					STORECALLLSTACK(_pc+ipc+a);
					//ipc=0;
				break;
				case 2://bhi
					if(!(_ccr & N_BIT))
						i=1;
				break;
				case 3:
					if((_ccr&N_BIT))
						i=1;
				break;
				case 4:
					if(!(_ccr&C_BIT))
						i=1;
				break;
				case 5:
					if((_ccr&C_BIT))
						i=1;
				break;
				case 6://bne
					if(!(_ccr&Z_BIT)){
						i=1;
						//ipc=0;
					}
				break;
				case 7://beq
					if(_ccr & Z_BIT){
						i=1;
					//	ipc=0;
					}
				break;
				case 0xb://bmi
					if(_ccr & N_BIT){
						i=1;
					//	ipc=0;
					}
				break;
				case 0xc://bge
					if(!(_ccr & (N_BIT|V_BIT))){
						i=1;
					//	ipc=0;
					}
				break;
				case 0xd://blt
					if(((_ccr & V_BIT)^(_ccr&N_BIT)))
						i=1;
				break;
				case 0xe://bgt
					if(!((_ccr & Z_BIT) || ((_ccr & V_BIT)^(_ccr&N_BIT))))
						i=1;
				break;
				case 0xf://ble
					if(((_ccr & Z_BIT) || ((_ccr & V_BIT)^(_ccr&N_BIT))))
						i=1;
				break;
				default:
					__F(6 %x,"unk",SR(_opcode,8)&0xf);
					EnterDebugMode();
				break;
			}
			_68F($%x,s,_pc+d+ipc);
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
			switch(SR(_opcode,6)&7){
				case 0:
					b=(u8)a;
					r=&REG_D(SR(_opcode,9)&7);
				break;
				case 1:
					b=(u16)a;
					r=&REG_D(SR(_opcode,9)&7);
				break;
				case 2:
					b=a;
					r=&REG_D(SR(_opcode,9)&7);
				break;
				case 5:
					b=(u32)(u16)REG_D(SR(_opcode,9)&7);
					r=(u32 *)p;
				break;
				default:
					printf("OR %x PC:%x\n",SR(_opcode,6)&7,_pc);
					EnterDebugMode();
				break;
			}
			STATUSFLAGS("orl %0,%%ecx\n",a,*r,b);
			WMP(sz,__address,r,a,_opcode);
			__68F(2,sz,_opcode&63,\x2c D%d,"OR",(SR(_opcode,9)&7));
		}
		break;
		case 9:{
			u32 a,b;
			void *p;
			u8 sz;
			char s[20]={0};

			sz=_szmove[SR(_opcode,6)&7];
			AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
			switch(SR(_opcode,6)&7){
				case 2:
					sprintf(s,"D%d",SR(_opcode,9)&7);
					b=(u32)(s32)REG_D(SR(_opcode,9)&7);
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					REG_D(SR(_opcode,9)&7)=a;
				break;
				case 1:
					sprintf(s,"D%d",SR(_opcode,9)&7);
					b=(u32)(s16)REG_D(SR(_opcode,9)&7);
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					REG_D(SR(_opcode,9)&7)=a;
				break;
				case 0:
				case 4:
				case 5:
					printf("%x\n",SR(_opcode,6)&7);
					EnterDebugMode();
				break;
				case 6:
					sprintf(s,"D%d",SR(_opcode,9)&7);
					b=a;
					a=(u32)(s32)REG_D(SR(_opcode,9)&7);
					//printf("sub %x %x",a,b);
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					//printf(" %x\n",a);
					WMPC(sz,p,BELE32(a));
				break;
				case 3:
					sprintf(s,"A%d",SR(_opcode,9)&7);
					b=(u32)(s32)REG_A(SR(_opcode,9)&7);
					STATUSFLAGS("subw %0,%%cx\n",a,b,a);
					REG_A(SR(_opcode,9)&7)=a;
				break;
				case 7:
					sprintf(s,"A%d",SR(_opcode,9)&7);
					b=(u32)(s32)REG_A(SR(_opcode,9)&7);
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					REG_A(SR(_opcode,9)&7)=a;
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

					SMODE(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					b=REG_D(SR(_opcode,9)&7);
					switch(sz){
						case 2:
							//if ((SR(_opcode,3)&7) > 1)  a=SWAP32(a);
						break;
						case 1:
							b=(u32)(s16)b;
							a=(u32)(s16)a;
						break;
						case 0:
							b=(u32)(s8)b;
							a=(u32)(s8)a;
						break;
						default:
							EnterDebugMode();
						break;
					}
					//printf("cmp  %x %x %x\n",a,b,__address);
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					__68F(2,sz,_opcode&63,\x2c D%d %x,"CMP",(SR(_opcode,9)&7),a);
				}
				break;
				case 4:{
					u32 a,b;

					switch(SR(_opcode,6)&3){
						case 0:
							a=REG_A(_opcode&7)++;
							b=REG_A(SR(_opcode,9)&7)++;
							RB(a,a);
							RB(b,b);
						break;
					}
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					_68F([A%d]+\x2c [A%d]+,"CMPM",(SR(_opcode,9)&7),_opcode&7);
				}
				break;
				case 7:
				case 6:{
					u32 a,b;
					u8 sz;

				//	EnterDebugMode();
					SMODE(a,_opcode,(sz=1+(SR(_opcode,8) & 1)),R,ipc);
					b=REG_A(SR(_opcode,9)&7);
					switch(sz){
						case 1:
							b=(u16)b;
						break;
					}
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
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
			(((CCore *)this)->*_opcodescb[0x1800|SR(_opcode&0xfff,3)])();
		break;
		case 0xd:
			switch(SR(_opcode,6)&7){
				default:{
					u32 a,b,*r;
					void *p;

					u8 sz=SR(_opcode,6) & 3;
					AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
					//_ccr &= ~(C_BIT|V_BIT|Z_BIT|N_BIT);
					switch(SR(_opcode,6)&7){
						case 0:
							b=(u8)a;
							r=&REG_D(SR(_opcode,9)&7);
						break;
						case 1:
							b=(u16)a;
							r=&REG_D(SR(_opcode,9)&7);
						break;
						case 2:
							b=a;
							r=&REG_D(SR(_opcode,9)&7);
						break;
						case 5:
							b=(u32)(u16)REG_D(SR(_opcode,9)&7);
							r=(u32 *)p;
						break;
						case 6:
							b=(u32)REG_D(SR(_opcode,9)&7);
							r=(u32 *)p;*r=a;
						break;
						default:
							printf("aDD %x PC:%x\n",SR(_opcode,6)&7,_pc);
							EnterDebugMode();
						break;
					}
					STATUSFLAGS("addl %0,%%ecx\n",a,*r,b);
					WMP(sz,__address,r,a,opcode);
					__68F(2,sz,_opcode,\x2c D%d,"ADD",(SR(_opcode,9)&7));
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

			}
		break;
		case 0xe:
			(((CCore *)this)->*_opcodescb[0x1C00|SR(_opcode&0xfff,3)])();
		break;
		default:
			__F(%x,"unk",SR(_opcode,12));
			EnterDebugMode();
		break;
	}
W:
	_pc += ipc;
	//printf("%x\n",_pc);
	return ret;
}

int M68000Cpu::Disassemble(char *dest,u32 *padr){
	u32 op,adr,pc;
	char c[400],*cc;
	u8 *p;

	*((u64 *)c)=0;
	cc=&c[200];
	*((u64 *)cc)=0;

	pc=_pc;
	op=_opcode;

	_pc = *padr;
	RWOP(_pc,_opcode);
	dipc=ipc=2;
	//printf("%x %x\n",_pc,_opcode);
	sprintf(c,"%08X ",_pc);
	switch(SR(_opcode,12)){
		case 0:
			(((CCore *)this)->*_disopcodecb[SR(_opcode&0xff8,3)])(cc);
		break;
		case 1:
		case 2:
		case 3://MOVE
			switch(SR(_opcode,6)&7){//dest mode
				case 0:{//Dx
					u32  s;
					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode,\x2c D%d,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 1:{//Ax
					u32 s;
					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode,\x2c A%d,"MOVEA",SR(_opcode,9)&7);
				}
				break;
				case 2:{//(Ax)
					__68S(ipc,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 3:{//(Ax)+
					__68S(ipc,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d]+,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 4:{//-(Ax) destmode
					__68S(ipc,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c -[A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 5:{
					s16 d;
					u32 s,sz;

					sz=_szmove[SR(_opcode,12)&3];
					SMODES(s,_opcode&63,sz,R,ipc);
					RWPC(_pc+ipc,d);
					ipc+=2;
					__68S(dipc,sz,_opcode,\x2c[\x24\x23%x\x2c A%d],"MOVE",d,SR(_opcode,9)&7);

				}
				break;
				case 7:{//dest mode
					u32 s,d;

					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					switch(SR(_opcode,9)&7){//dst reg
						case 0:{
							RWPC(_pc+ipc,d);
							ipc += 2;
							__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c #%x,"MOVE",d);
						}
						break;
						case 1:
							RLOP(_pc+ipc,d);
							ipc += 4;
							__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c $%x.l,"MOVE",d);
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
					__68S(dipc,SR(_opcode,6)&3,_opcode,NOARG,"CLR");
				}
				break;
				case 5:
				case 0x12:{
					u32  d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(dipc,SR(_opcode,6)&3,_opcode, ,"NEG");
				}
				break;
				case 0x19:
				case 0x1a:{
					u32  d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(dipc,SR(_opcode,6)&3,_opcode, ,"NOT");
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
								case 1:
									ipc+=4;
									RLPC(_pc+2,v);
									_68S(#%x,"PEA",v);
								break;
								case 0:
									ipc+=2;
									RWPC(_pc+2,v);
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
							__68S(ipc,1,63,#%x\x2c SR,"MOVE",d);
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
					__68S(dipc,2,_opcode,\x2c A%d,"LEA",SR(_opcode,9)&7);
				}
				break;
				case 0x22:
				case 0x23:
				case 0x33:
					(((CCore *)this)->*_disopcodecb[0x800|SR(_opcode&0xff8,3)])(cc);
				break;
				case 0x28:
				case 0x29:
				case 0x2a:{
					u32 a;
					u8 sz;

					SMODES(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					__68S(dipc,sz,_opcode,NOARG,"TST");
				}
				break;
				case 0x39:
					switch(_opcode & 0x3f){
						case 0x15:{
							s16 d;

							RWPC(_pc+2,d);
							ipc+=2;
							_68S(A%d\x2c#%d,"LINK",_opcode&7,(s32)(s16)d);
						}
						break;
						case 0x31:
							_68S(NOARG,"NOP");
						break;
						case 0x35:
							_68S(NOARG,"RTS");
						break;
						case 0x30:
							_68S(NOARG,"RESET");
						break;
						case 0x3b:
							_68S(NOARG,"ILLEGAL");
						break;
						default:
							if(_opcode &8){
								_68S(USP\x2c A%d,"MOVE",_opcode&7);
							}
							else{
								_68S(A%d\x2cUSP,"MOVE",_opcode&7);
							}

						break;
					}
				break;
				case 0x3a:{
					u32 d;

					SMODES(d,_opcode&63,2,R,ipc);
					__68S(dipc,2,_opcode&63, ,"JSR");
				}
				break;
				case 0x3b:{
					u32 d;

					SMODES(d,_opcode&63,2,R,ipc);
					__68S(dipc,2,_opcode&63, ,"JMP");
				}
				break;
			}
		break;
		case 5:
			switch(SR(_opcode,3)&0x1f){
				case 5:
				case 7:
				case 8:
				case 9:
				case 0x10:
				case 0x11:
				case 0x12:{
					char c[][5]={"ADDQ","SUBQ"};
					u8 v;

					if(!(v=SR(_opcode,9)&7)) v=8;
					__68S(ipc,SR(_opcode,6)&3,_opcode&63,\x2c #%x,c[SR(_opcode,8)&1],v);
				}
				break;
				case 0x18:
				case 0x1f:{
					u32 d;
					char s[10]="S\0";

					strcat(s,_cc[SR(_opcode,8) & 15]);
					SMODES(d,_opcode,2, ,ipc);
					__68S(ipc,2,_opcode&0x3f,NOARG,s);
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
					RLOP(_pc+2,d);
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
			__68S(dipc,sz,_opcode&63,\x2c D%d,"OR",(SR(_opcode,9)&7));
		}
		break;
		case 9:{
			u32 a,b;
			void *p;
			u8 sz;
			char s[20]={0};

			sz=_szmove[SR(_opcode,6)&7];
			SMODES(a,_opcode&0x3f,sz,R,ipc);
			switch(SR(_opcode,6)&7){
				case 0:
				case 1:
				case 2:
					sprintf(s,"D%d",SR(_opcode,9)&7);
				break;
				case 4:
				case 6:
				case 5:
					sprintf(s,"D%d",SR(_opcode,9)&7);
				break;
				case 3:
				case 7:
					sprintf(s,"A%d",SR(_opcode,9)&7);
				break;

			}
			__68S(dipc,sz,_opcode,\x2c%s,"SUB",s);
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
					__68S(dipc,sz,_opcode&63,\x2c D%d,"CMP",(SR(_opcode,9)&7));
				}
				break;
				case 6:
				case 7:{
					u32 a,b;
					u8 sz;

					SMODES(a,_opcode&0x3f,(sz=1+(SR(_opcode,8) & 1)),R,ipc);
					__68S(dipc,sz,_opcode&63,\x2c A%d,"CMPA",(SR(_opcode,9)&7));
				}
				break;
				default:
					_68S(%d,"CMP",SR(_opcode,3)&7);
				break;
			}
		}
		break;
		case 0xc:
			(((CCore *)this)->*_disopcodecb[0x1800|SR(_opcode&0xff8,3)])(cc);
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
					__68S(dipc,sz,_opcode&63,\x2c D%d,"ADD",(SR(_opcode,9)&7));
				}
				break;
				case 3:
				case 7:{
					u32 a, sz=SR(_opcode&0x100,8)+1;
					SMODES(a,_opcode,sz,R,ipc);
					__68S(dipc,sz,_opcode,\x2c A%d,"ADDA",(SR(_opcode,9)&7));
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
			switch(SR(_opcode&0x100,5)|SR(_opcode&0x38,3)){
				case 0x5://lsl
				case 0x1:{
					u8 sz,v;

					sz=SR(_opcode,6)&3;
					v = SR(_opcode,9)&7;
					__68S(ipc,sz,_opcode&7,\x2c #%x,"LSL",v);
				}
				break;
				case 9:
				case 0xd:{//lsr
					u8 sz,v;

					sz=SR(_opcode,6)&3;
					v = SR(_opcode,9)&7;
					__68S(ipc,sz,_opcode&7,\x2c #%x,"LSR",v);
				}
				break;
				case 0xa:
				case 2:{//rorx
					u8 sz,v;

					v = SR(_opcode,9)&7;
					sz=SR(_opcode,6)&3;
					__68S(ipc,sz,_opcode&7,\x2c #%x,"ROXR",v);
				}
				break;
				default:
					_68S(%x,"unk E",SR(_opcode&0xe00,7)|SR(_opcode & 0x38,3));
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

	sprintf(p,"PC: %08X CCR: %04X Z:%c C:%c V:%c N:%c X:%c IM:%x M:%x S:%x\n\n",_pc,_ccr,
		_ccr&Z_BIT ? 49 : 48,_ccr&C_BIT ? 49 : 48,_ccr&V_BIT ? 49 : 48,_ccr&N_BIT ? 49 : 48,_ccr&X_BIT ? 49 : 48,SR(_ccr,8)&7
		,SR(_ccr,12)&1,SR(_ccr,13)&1);
	for(int i=0;i<8;i++){
		sprintf(cc,"D%d: %08X ",i,REG_D(i));
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	//strcat(p,"\n");
	for(int i=0;i<8;i++){
		sprintf(cc,"A%d: %08X ",i,REG_A(i));
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	sprintf(cc,"\nUSP: %08X ISP: %08X VBB: %08X",REG_USP,REG_ISP,0);
	strcat(p,cc);
	return 0;
}

int M68000Cpu::_dumpMemory(char *p,u8 *mem,LPDEBUGGERDUMPINFO pdi,u32 sz){
	RMAP_(pdi->_dumpAddress,mem,R);
	pdi->_dumpMode|=0x80;
	return CCore::_dumpMemory(p,mem,pdi,sz);
}

int M68000Cpu::_change_sr(u16 v){
	u16 vv;

	vv=_ccr;
	if(_ccr & S_BIT)
		_ccr=v;
	else
		_ccr= (_ccr&0xff00)|(u8)v;
	if((vv&S_BIT) != (v&S_BIT)){
		if(vv&S_BIT){
			REG_ISP=REG_SP;
			REG_SP=REG_USP;
		}
		else{
			REG_USP=REG_SP;
			REG_SP=REG_ISP;
		}
	}
	return 0;
}

int M68000Cpu::_empty_op(){
	EnterDebugMode();
	printf("unk %x %x %x\n",_opcode,SR(_opcode,3),SR(_opcode&0xfff,3));
	return 0;
}

int M68000Cpu::_empty_op_dis(char *cc){
	return 0;
}

int M68000Cpu::_and(){
	u32 a,b,*r;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	//_ccr &= ~(C_BIT|V_BIT|Z_BIT|N_BIT);
	switch(SR(_opcode,6)&7){
		case 0:
			b=(u8)a;
			r=&REG_D(SR(_opcode,9)&7);
		break;
		case 1:
			b=(u16)a;
			r=&REG_D(SR(_opcode,9)&7);
		break;
		case 2:
			b=a;
			r=&REG_D(SR(_opcode,9)&7);
		break;
		case 5:
			b=(u32)(u16)REG_D(SR(_opcode,9)&7);
			r=(u32 *)p;
		break;
		case 6:
		break;
		default:
			printf("AND %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	STATUSFLAGS("andl %0,%%ecx\n",a,*r,b);
	WMPC(sz,r,a);
	__68F(2,sz,_opcode&63,\x2c D%d,"AND",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_and_dis(char *cc){
	u32 a,sz;

	sz=SR(_opcode,6) & 3;
	SMODES(a,_opcode & 0x3f,sz,R,ipc);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"AND",(SR(_opcode,9)&7));

	return 1;
}

int M68000Cpu::_exg(){
	u32 a;

	switch(_opcode&0x88){
		case 8:a=REG_A(_opcode&7);
			REG_A(_opcode&7)=REG_A(SR(_opcode,9)&7);
			REG_A(SR(_opcode,9)&7)=a;
			_68F(A%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x0:
			a=REG_D(_opcode&7);
			REG_D(_opcode&7)=REG_D(SR(_opcode,9)&7);
			REG_D(SR(_opcode,9)&7)=a;
			_68F(D%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x88:
			a = REG_A(_opcode&7);
			REG_A(_opcode&7)=REG_D(SR(_opcode,9)&7);
			REG_D(SR(_opcode,9)&7)=a;
			_68F(D%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
	}
	return 1;
}

int M68000Cpu::_exg_dis(char *cc){
	u32 a;

	switch(_opcode&0x88){
		case 8:
			_68S(A%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x0:
			_68S(D%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x88:
			_68S(D%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
	}
	return 1;
}

int M68000Cpu::_mulu(){
	u32 a,b;

	SMODE(a,_opcode,1,R,ipc);
	b=(u16)REG_D(SR(_opcode,9)&7)*(u16)a;
	REG_D(SR(_opcode,9)&7)=b;
	__68F(2,1,_opcode,\x2c D%d,"MULU",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_mulu_dis(char *cc){
	u32 a;

	SMODES(a,_opcode,1,R,ipc);
	__68S(dipc,1,_opcode,\x2c D%d,"MULU",(SR(_opcode,9)&7));

	return 1;
}

int M68000Cpu::_muls(){
	s32 a,b;

	SMODE(a,_opcode,1,R,ipc);
	b=(u16)REG_D(SR(_opcode,9)&7)*(s16)a;
	REG_D(SR(_opcode,9)&7)=b;
	__68F(2,1,_opcode,\x2c D%d,"MULS",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_muls_dis(char *cc){
	u32 a;

	SMODES(a,_opcode,1,R,ipc);
	__68S(ipc,1,_opcode,\x2c D%d,"MULS",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_andisr(){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68F(#%x\x2c SR,"AND",d);
	_change_sr(_ccr&d);
	return 1;
}

int M68000Cpu::_andisr_dis(char *cc){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68S(#%x\x2c SR,"AND",d);
	return 0;
}

int M68000Cpu::_andi(){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEA(p,a,_opcode & 0x3f,2,R,ipc,SOURCE,);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
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
	return 1;
}

int M68000Cpu::_andi_dis(char *cc){
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
			RLOP(_pc+2,b);
		break;
	}
	__68S(ipc,sz,_opcode&0x3f,\x2c #%x,"ANDI",b);
	return 0;
}

int M68000Cpu::_ori(){
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
	return 1;
}

int M68000Cpu::_ori_dis(char *cc){
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
			RLOP(_pc+2,b);
		break;
	}
	__68S(ipc,sz,_opcode,\x2c\x23%x,"ORI",b);
	return 0;
}

int M68000Cpu::_orisr(){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68F(#%x\x2c SR,"OR",d);
	d|=_ccr;
	if(!(_ccr&S_BIT)){
		d=(u8)_ccr | S_BIT;
		_change_sr(d);
		OnException(8,0);
		_ccr |= S_BIT;
	}
	else
	_change_sr(d);
	return 1;
}

int M68000Cpu::_orisr_dis(char *cc){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68S(#%x\x2c SR,"OR",d);
	return 0;
}

int M68000Cpu::_cmpi(){
	u32 a,b;
	u8 sz;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	SMODE(a,_opcode,sz,R,ipc);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u32)(s8)b;
			a=(u32)(s8)a;
		break;
		case 1:
		//	EnterDebugMode();
			RWPC(_pc+2,b);
			b=(u32)(s16)b;
			a=(u32)(s16)a;
		break;
		case 2:
			RLPC(_pc+2,b);
		break;
	}
	//printf("cmpi %x %x %x",a,b,sz);
	STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
	//printf(" %x\n",a);
	__68F(2+((sz+2) & 6),sz,_opcode,\x2c #%x,"CMPI",b);
	return 1;
}

int M68000Cpu::_cmpi_dis(char *cc){
	u32 a,b;
	u8 sz;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	SMODES(a,_opcode & 0x3f,2,R,ipc);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u32)(u8)b;
			dipc+=2;
		break;
		case 1:
			RWPC(_pc+2,b);
			dipc+=2;
		break;
		case 2:
			RLOP(_pc+2,b);
			dipc+=4;
		break;
	}
	__68S(dipc,sz,_opcode&0x3f,\x2c #%x,"CMPI",b);
	return 0;
}

int M68000Cpu::_subi(){
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
	STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
	WMPC(sz,p,a);
	__68F(2+((sz+2) & 6),sz,_opcode&0x3f,\x2c\x24\x23%x,"SUBI",b);
	return 1;
}

int M68000Cpu::_subi_dis(char *cc){
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
			RLOP(_pc+2,b);
		break;
	}
	__68S(ipc,sz,_opcode,\x2c\x23%x,"SUBI",b);
	return 0;
}

int M68000Cpu::_addi(){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEA(p,a,_opcode & 0x3f,2,R,ipc,SOURCE,);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
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
	__68F(ipc,sz,_opcode&0x3f,\x2c #%x,"ADDI",b);
	return 1;
}

int M68000Cpu::_addi_dis(char *cc){
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
			RWPC(_pc+2,b);;
		break;
		case 2:
			RLOP(_pc+2,b);
		break;
	}
	__68S(ipc,sz,_opcode,\x2c #%x,"ADDI",b);
	return 0;
}

int M68000Cpu::_bset(){
	void *p;
	u32 a,b;

	AEA(p,a,_opcode,2,R,ipc,SOURCE,);
	b=BV(REG_D(SR(_opcode,9)&7)&7);
	if(a & b) _ccr &= ~Z_BIT; else _ccr |= Z_BIT;
	a|=b;
	WMP(2,__address,p,a,_opcode);
	__68F(2,2,_opcode,\x2c D%d,"BSET",SR(_opcode,9)&7);
	return 1;
}

int M68000Cpu::_bset_dis(char *cc){
	void *p;
	u32 a;

	//fixme
	AEAS(p,a,_opcode,0,R,ipc,SOURCE);
	__68S(ipc,0,_opcode,\x2c D%d,"BSET",SR(_opcode,9)&7);
	return 0;
}

int M68000Cpu::_bseti(){
	u16 b;
	void *p;
	u32 a;
	u8 sz;

	RWPC(_pc+ipc,b);
	ipc+=2;
	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	if(a & BV(b)) _ccr &= ~Z_BIT; else _ccr |= Z_BIT;
	a|=BV(b);
	WMP(sz,__address,p,a,_opcode);
	__68F(4,2,_opcode&0x3f,\x2c #%x,"BSET",b);
	return 1;
}

int M68000Cpu::_bseti_dis(char *cc){
	void *p;
	u32 a;
	u16 b;

	//fixme
	RWPC(_pc+ipc,b);
	ipc+=2;
	dipc+=2;
	AEAS(p,a,_opcode,0,R,ipc,SOURCE);
	__68S(dipc,2,_opcode,\x2c#%x,"BSET",b);
	return 0;
}

int M68000Cpu::_btst(){
	u16 b;
	void *p;
	u32 a;

	AEA(p,a,_opcode & 0x3f,2,R,ipc,SOURCE,);
	if(a & BV(REG_D(SR(_opcode,9)&7))) _ccr &= ~Z_BIT; else _ccr |= Z_BIT;
	__68F(2,2,_opcode,\x2c D%d,"BTST",SR(_opcode,9)&7);
	return 1;
}

int M68000Cpu::_btst_dis(char *cc){
	void *p;
	u32 a;
	u16 b;

	RWPC(_pc+ipc,b);
	ipc+=2;
	dipc+=2;
	//AEAS(p,a,_opcode,0,R,ipc,SOURCE);
	__68S(ipc,2,_opcode,\x2c#%x,"BTST",b);
	return 0;
}

int M68000Cpu::_btsti(){
	u16 b;
	void *p;
	u32 a;
	u8 sz;

	RWPC(_pc+ipc,b);
	ipc+=2;
	sz=SOURCEM(_opcode) >1 ? 0 : 2;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	if(a & BV(b)) _ccr &= ~Z_BIT; else _ccr |= Z_BIT;
	__68F(4,sz,_opcode,\x2c #%x,"BTST",b);
	return 1;
}

int M68000Cpu::_btsti_dis(char *cc){
	void *p;
	u32 a;
	u16 d;

	RWPC(_pc+ipc,d);
	ipc += 2;
	__68S(ipc,0,_opcode,\x2c #%x,"BTST",d);
	return 0;
}

int M68000Cpu::_bclr(){
	void *p;
	u32 a,b;

	AEA(p,a,_opcode,2,R,ipc,SOURCE,);
	b=BV(REG_D(SR(_opcode,9)&7) & 7);
	if(a & b) _ccr &= ~Z_BIT; else _ccr |= Z_BIT;
	a&=~b;
	WMP(2,__address,p,a,_opcode);
	__68F(2,2,_opcode,\x2c D%d,"BCLR",SR(_opcode,9)&7);
	return 1;
}

int M68000Cpu::_bclr_dis(char *cc){
	void *p;
	u32 a;

	//fixme
	AEAS(p,a,_opcode,0,R,ipc,SOURCE);
	__68S(ipc,0,_opcode,\x2c D%d,"BCLR",SR(_opcode,9)&7);
	return 0;
}

int M68000Cpu::_bclri(){
	u16 b;
	void *p;
	u32 a;
	u8 sz;

	RWPC(_pc+ipc,b);
	ipc+=2;
	sz=SOURCEM(_opcode) >1 ? 0 : 2;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	if(a & BV(b)) {
		_ccr &= ~Z_BIT;
		a &= ~BV(b);
		WMP(sz,__address,p,a,_opcode);
	}
	else _ccr |= Z_BIT;

	__68F(4,sz,_opcode,\x2c #%x,"BCLR",b);
	return 1;
}

int M68000Cpu::_bclri_dis(char *cc){
	void *p;
	u32 a;
	u16 b;

	RWPC(_pc+ipc,b);
	ipc+=2;
	dipc +=2;
	AEAS(p,a,_opcode,2,R,ipc,SOURCE);
	__68S(dipc,2,_opcode,\x2c #%x,"BCLR",b);
	return 0;
}

int M68000Cpu::_lea(){
	u32 d;

	SMODE(d,_opcode,2, ,ipc);
	REG_A(SR(_opcode,9)&7)=d;
	__68F(2,2,_opcode&0x3f,\x2c A%d,"LEA",SR(_opcode,9)&7);
	return 1;
}

int M68000Cpu::_movemr(){
	u16 l,sz;
	u32 a,v;
	void *p,*ps;
	int i,n;

	RWPC(_pc+ipc,l);
	ipc+= 2;
	sz=1+(SR(_opcode,6)&1);
	AEA_(p,ps,a,_opcode,sz,R,ipc,SOURCE, );
	if(SOURCEM(_opcode)==4){
		for(n=0,i=15;i>=0;i--){
			if(!(l & BV(15-i)))
				continue;
			RMPC(sz,p,v);
			REG_D(i)=v;
			p = (u8 *)p  - SL(sz,1);
			n+=SL(sz,1);
		}
		*(u32 *)ps -= (n-SL(sz,1));
	}
	else{
		printf("%x %x %p %p  %x\n",a,sz,p,ps,_opcode);
		for(n=i=0;i<16;i++){
			if(!(l & BV(i)))
				continue;
			RMPC(sz,p,v);
			REG_D(i)=v;
			p = (u8 *)p  + SL(sz,1);
			n+=SL(sz,1);
		}
		if(ps)
			*(u32 *)ps += n-SL(sz,1);
	}
	__68F(4,sz,_opcode,%x,"MOVEM",l);
	return 1;
}

int M68000Cpu::_movemw(){
	u16 l,sz;
	u32 a,v;
	void *p,*ps;
	int i,n;

	RWPC(_pc+ipc,l);
	ipc+= 2;
	sz=1+(SR(_opcode,6)&1);
	AEA_(p,ps,a,_opcode,sz,R,ipc,SOURCE,);
	if(SOURCEM(_opcode)==4){
		for(i=15,n=0;i>=0;i--){
			if(!(l & BV(15-i)))
				continue;
			v=REG_D(i);
			WMPC(sz,p,v);
			p = (u8 *)p - SL(sz,1);
			n +=SL(sz,1);
		}
		*(u32 *)ps -= (n-SL(sz,1));
	}
	else{
		for(i=n=0;i<16;i++){
			if(!(l & BV(i)))
				continue;
			v=REG_D(i);
			WMPC(sz,p,v);
			p = (u8 *)p + SL(sz,1);
			n+=SL(sz,1);
		}
		if(ps)
			*(u32 *)ps +=  n;
	}
	__68F(4,sz,_opcode,%x,"MOVEM",l);
	return 1;
}

int M68000Cpu::_ext(){
	u32 sz;

	sz=SR(_opcode,6)&7;
	__68F(2,sz,_opcode,NOARG,"EXT");
	return 1;
}

int M68000Cpu::_ext_dis(char *cc){
	_68S(D%d,"EXT",_opcode&7);
	return 0;
}

int M68000Cpu::_movemr_dis(char *cc){
	u16 l,sz;
	u32 a;
	char s[50];

	RWPC(_pc+ipc,l);
	sz=1+(SR(_opcode,6)&1);
	SMODES(a,_opcode,sz,R,ipc);
	ipc+= 2;
	dipc+=2;
	*((u64 *)s)=0;
	if(SOURCEM(_opcode)==4){
		for(int i=15;i>=0;i--){
			char c[10];

			if(!(l&BV(15-i)))
				continue;
			sprintf(c,"%c%d",i<8 ? 'D':'A',i&7);
			if(*s) strcat(s,",");
			strcat(s,c);
		}
	}
	else{
		for(int i=0;i<16;i++){
			char c[10];

			if(!(l&BV(i)))
				continue;
			sprintf(c,"%c%d",i<8 ? 'D':'A',i&7);
			if(*s) strcat(s,",");
			strcat(s,c);
		}
	}
	__68S(dipc,sz,_opcode,%s,"MOVEM",s);
	return 0;
}

int M68000Cpu::_lsl(){

	return 1;
}

int M68000Cpu::_lsl_dis(char *cc){
	return 1;
}

int M68000Cpu::_lsr(){
	EnterDebugMode();
	return 1;
}

int M68000Cpu::_lsr_dis(char *cc){
	return 1;
}

int M68000Cpu::_asl(){
	EnterDebugMode();
	return 1;
}

int M68000Cpu::_asl_dis(char *cc){
	return 1;
}

int M68000Cpu::_asr(){
	return 1;
}

int M68000Cpu::_asr_dis(char *cc){
	return 1;
}

int M68000Cpu::_rol(){
	return 1;
}

int M68000Cpu::_rol_dis(char *cc){
	return 1;
}

int M68000Cpu::_ror(){
	return 1;
}

int M68000Cpu::_ror_dis(char *cc){
	return 1;
}

int M68000Cpu::_roxl(){
	return 1;
}

int M68000Cpu::_roxl_dis(char *cc){
	return 1;
}

int M68000Cpu::_roxr(){
	return 1;
}

int M68000Cpu::_roxr_dis(char *cc){
	return 1;
}

int M68000Cpu::_lsli(){
	u32 a,b,c__;
	u8 sz,v;

	a = b = REG_D(_opcode & 7);
	c__=_ccr;
	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	v=SR(_opcode,9)&7;
	if(_opcode&0x20) v=REG_D(v);
	if(!v) v=8;
	switch(sz=SR(_opcode,6)&3){
		case 2:
			b = (u32)SL((u32)a,v-1);
			if(b & 0x80000000)
				_ccr |= C_BIT|X_BIT;
			b=(u32)SL(b,1);
			a=0;
		break;
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

	return 1;
}

int M68000Cpu::_lsli_dis(char *cc){
	return 1;
}

u32 M68000Cpu::lsl_(u32,u32,u32){return 0;}
u32 M68000Cpu::asr_(u32,u32,u32){return 0;}
u32 M68000Cpu::asl_(u32,u32,u32){return 0;}
u32 M68000Cpu::rol_(u32,u32,u32){return 0;}
u32 M68000Cpu::ror_(u32,u32,u32){return 0;}
u32 M68000Cpu::roxr_(u32,u32,u32){return 0;}
u32 M68000Cpu::roxl_(u32,u32,u32){return 0;}

u32 M68000Cpu::lsr_(u32 a,u32 v,u32 sz){
	u32 b,c__;

	b = a;
	c__=_ccr;
	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	if(!v)
		v=8;
	switch(sz&3){
		case 2:
			b = SR(a,v-1);
			if(b&1)
				_ccr |= C_BIT|X_BIT;
			b=SR(b,1);
			a=0;
		break;
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
	return a;
}

int M68000Cpu::_lsri(){
	u8 v,sz;

	v=SR(_opcode,9)&7;
	if(_opcode&0x20) v=REG_D(v);
	REG_D(_opcode&7)=lsr_(REG_D(_opcode&7),v,(sz=SR(_opcode,6)&3));
	__68F(2,sz,_opcode&7,\x2c #%x,"LSR",v);
	return 1;
}

int M68000Cpu::_lsri_dis(char *cc){
	return 1;
}

int M68000Cpu::_asli(){
	return 1;
}

int M68000Cpu::_asli_dis(char *cc){
	return 1;
}

int M68000Cpu::_asri(){
	u32 a,b,c__;
	u8 sz,v;

	a = b = REG_D(_opcode & 7);
	c__=_ccr;
	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	v=SR(_opcode,9)&7;
	if(_opcode&0x20) v=REG_D(v);
	if(!v) v=8;
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
	return 1;
}

int M68000Cpu::_asri_dis(char *cc){
	return 1;
}

int M68000Cpu::_roli(){
	return 1;
}

int M68000Cpu::_roli_dis(char *cc){
	return 1;
}

int M68000Cpu::_rori(){
	return 1;
}

int M68000Cpu::_rori_dis(char *cc){
	return 1;
}

int M68000Cpu::_roxli(){
	return 1;
}

int M68000Cpu::_roxli_dis(char *cc){
	return 1;
}

int M68000Cpu::_roxri(){
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
	return 1;
}

int M68000Cpu::_roxri_dis(char *cc){
	return 1;
}
int M68000Cpu::_enterIRQ(int n,u32 pc){
	return  0;
}

int M68000Cpu::OnException(u32 code,u32 b){
	REG_SP -= 4;
	WLPC(REG_SP,_pc);
	REG_SP -= 2;
	WwPC(REG_SP,_ccr);
	RLPC(code*4,_pc);
	ipc=0;
	return 0;
}

};
