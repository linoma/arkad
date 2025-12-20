#include "dcm.h"
#include "gui.h"
#include "elf.h"

extern GUI gui;

namespace dc{

struct __vm{
	const int _slot=64;

	union{
		u16 _state;
		struct{
			unsigned int _invalidate:1;
			unsigned int _remap:1;
			unsigned int _step:2;
			unsigned int _play:3;
			unsigned int _loop:2;
			unsigned int _rjump:1;
			unsigned int _wsync:1;
		};
	};

	u8 *_data,*p,*pp,*_mem,*_opcode,*_rt,*_rs,_idx;
	u32 _pc,_pcu,_pcd;

	~__vm(){
	};

	void invalidate(u32 f=1){
		_invalidate=1;
		_remap |= f & 1 ? 1 : 0;
		if(f & 2){
			_pcu=0;
			_pcd=0;
		}
		p=NULL;
		if(f&4)
			_play=0;
	};

	void reset(){
		invalidate();
		_step=0;
		_idx=0;
		_pc=0;
		_pcu=_pcd=0;
	};
} _vm;

#define CHANGE_FPSCR() if(REG_FPSCR & FP_FR){_fpregs=&_frregs[16];_xfregs=&_frregs[0];}	else{_fpregs=&_frregs[0];_xfregs=&_frregs[16];}
#define RMAP_(a,b,c)\
	if(a >= 0x0c000000 && a <= 0x0fffffff) b=&_mem[MI_RAM + (a & (MS_RAM-1))];\
	else if(a >= 0x00500000 && a <= 0x005fffff) {b=&_mem[MI_IO + (a & (MS_IO-1))];__bus |= MAIOMASK;}\
	else if(a >= 0x0 && a <= 0x001fffff) b=&_mem[MI_BIOS+(a & (MS_BIOS-1))];\
	else if(a >= 0x00200000 && a <= 0x0021ffff) b=&_mem[MI_FLASH + (a & (MS_FLASH-1))];\
	else if(a >= 0x00600000 && a <= 0x006007ff) {b=&_mem[MI_IO + (a & (MS_IO-1))];__bus |= MAIOMASK;}\
	else if(a >= 0x00700000 && a <= 0x0070ffff) {b=&_mem[MI_IO + (a & (MS_IO-1))];__bus |= MAIOMASK;}\
	else if(a >= 0x00710000 && a <= 0x0071000f) {b=0;}\
	else if(a >= 0x00800000 && a <= 0x009fffff) {b=0;}\
	else if(a >= 0x04000000 && a <= 0x05ffffff) b=&_mem[MI_VRAM+(a & (MS_VRAM-1))];\
	else if(a >= 0x08000000 && a <= 0x0bffffff) {b=0;}\
	else if((a >= 0x10000000 && a <= 0x107fffff) || (a >= 0x12000000 && a <= 0x127fffff)) {b=0;__bus |= MAIOMASK;}\
	else if((a >= 0x10800000 && a <= 0x10ffffff) || (a >= 0x12800000 && a <= 0x12ffffff)) {__bus |= MAIOMASK;b=0;}\
	else if(a >= 0x11000000 && a <= 0x117fffff) b=&_mem[MI_TEXRAM + (a & (MS_TEXRAM-1))];\
	else if(a >= 0x13000000 && a <= 0x137fffff) {printf("%x\n",a);b=0;}\
	else if(a >= 0x14000000 && a <= 0x17ffffff) {b=0;}\
	else if(a >= 0x18000000 && a <= 0x1bffffff) {b=0;}\
	else if(a >= 0x1c000000 && a <= 0x1c000fff) b=&_mem[MI_DCACHE+(a & 0xfff)];\
	else if(a >= 0x1e000000 && a <= 0x1e000fff) b=&_mem[MI_DCACHE+(a & 0xfff)];\
	else if(a >= 0xe0000000 && a <= 0xe3ffffff) b=&_mem[MI_SQ+(a & (MS_SQ-1))];\
	else if(a >= 0xf0000000 && a <= 0xf7ffffff) {b=0;__bus |= MAIOMASK;}\
	else if(a >= 0xff000000 && a <= 0xffffffff) {b=&_mem[MI_IO+(MS_IO-1) - 0x10000 + SH4REGI(a)*4];__bus |= MAIOMASK;}\
	else {/*printf("def unk mem %x %x %x\n",a,_pc,_mmu._regs[(SH4_MMUCR)]);*/b=0;}
#define RMAP_PC(a,b,c)  RMAP_(a,b,c);
#define RMAP_L(a,b,c)   b=_mem;

DreamCast::DreamCast() : Machine(MB(60)),dcbios(){
	CCore::_freq=MHZ(200);
}

DreamCast::~DreamCast(){
}

int DreamCast::Load(IGame *pg,char *fn){
	int  res;
	u32 sz,d[15];

	if(!pg && !fn)
		return -1;
	if(!pg){
		pg=new DcGame();
		pg->AddFile((char*)GameManager::getFilename(fn).c_str(),1);
	}
	if(!pg || pg->Open(fn,0))
		return -1;

	Reset();
	memset(&_mem[MI_RAM],0xff,KB(64));
	if(pg->Query(IGAME_GET_INFO,d))
		return -2;
	printf("%x %x %x\n",d[IGAME_GET_INFO_SLOT+4],d[IGAME_GET_INFO_SLOT+5],d[IGAME_GET_INFO_SLOT+6]);
	pg->Seek(d[IGAME_GET_INFO_SLOT+2],SEEK_SET);
	pg->Read(&_mem[MI_RAM+(d[IGAME_GET_INFO_SLOT+5] & (MS_RAM-1))],d[IGAME_GET_INFO_SLOT+3]);
	{
		u8 *buf;

		if((buf = new u8[d[IGAME_GET_INFO_SLOT+1]+1])){
			pg->Seek(d[IGAME_GET_INFO_SLOT],SEEK_SET);//4f3ad4c-4c096e0=33166c
			pg->Read(buf,d[IGAME_GET_INFO_SLOT+1]);
			//descrambl_buffer(buf,&_mem[MI_RAM+(d[IGAME_GET_INFO_SLOT+4] & (MS_RAM-1))],d[IGAME_GET_INFO_SLOT+1]);
			memcpy(&_mem[MI_RAM+(d[IGAME_GET_INFO_SLOT+4] & (MS_RAM-1))],buf,d[IGAME_GET_INFO_SLOT+1]);
			delete []buf;
		}
		_pc=d[IGAME_GET_INFO_SLOT+6];//4c0a008 4c096e8
		//printf("%x %x %x\n",h._exe_pos,h._exe_size,h._boot_pos);
	}

	Query(ICORE_QUERY_SET_FILENAME,fn);
	_game=(DcGame *)pg;
	res=dcbios::Load(pg,fn);
	SYNCPC(_pc);
	return res;
}

int DreamCast::Destroy(){
	Machine::Destroy();
	ASIC::Destroy();
	return 0;
}

int DreamCast::Reset(){
	Machine::Reset();
	return ASIC::Reset();
}

//Area 0 mem map
//0x00000000- 0x001FFFFF	:MPX System/Boot ROM
//0x00200000- 0x0021FFFF	:Flash Memory
//0x00400000- 0x005F67FF	:Unassigned
//0x005F6800- 0x005F69FF	:System Control Reg.
//0x005F6C00- 0x005F6CFF	:Maple i/f Control Reg.
//0x005F7000- 0x005F70FF	:GD-ROM / NAOMI BD Reg.
//0x005F7400- 0x005F74FF	:G1 i/f Control Reg.
//0x005F7800- 0x005F78FF	:G2 i/f Control Reg.
//0x005F7C00- 0x005F7CFF	:PVR i/f Control Reg.
//0x005F8000- 0x005F9FFF	:TA / PVR Core Reg.
//0x00600000- 0x006007FF	:MODEM
//0x00600800- 0x006FFFFF	:G2 (Reserved)
//0x00700000- 0x00707FFF	:AICA- Sound Cntr. Reg.
//0x00710000- 0x0071000B	:AICA- RTC Cntr. Reg.
//0x00800000- 0x00FFFFFF	:AICA- Wave Memory
//0x01000000- 0x01FFFFFF	:Ext. Device
//0x02000000- 0x03FFFFFF*	:Image Area*	2MB

int DreamCast::Init(){
	if(Machine::Init())
		return -1;
	dcbios::Init(0,_memory,_memory,CCore::_freq);
	for(int i=0;i<0x4000;i++)
		SetIO_cb(0x5f6000+i,(CoreMACallback)&DreamCast::fn_asic_regs_w,0);
	//for(int i=0;i<0x2000;i++)
	//	SetIO_cb(0x5f8000|i,(CoreMACallback)&DreamCast::fn_pvr_regs_w,0);
	for(int i=0;i<0x8000;i++)
		SetIO_cb(0x700000|i,(CoreMACallback)&DreamCast::fn_aica_regs_w,0);
	for(int i=0;i<0x1000000;i+=0x100){
		SetIO_cb(0xf4000000|i,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_w,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_r);
		SetIO_cb(0xf5000000|i,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_w,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_r);
		SetIO_cb(0xf6000000|i,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_w,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_r);
		SetIO_cb(0xf7000000|i,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_w,(CoreMACallback)&DreamCast::fn_sh4_cache_regs_r);
		SetIO_cb(0xff000000|i,(CoreMACallback)&DreamCast::fn_sh4_regs_w,(CoreMACallback)&DreamCast::fn_sh4_regs_r);
	}
	//_max_frame=50;
	//_setBlankArea(0,312,0,_width,50,_freq/2);
	//AddTimerObj((ICpuTimerObj *)this,_scanline_cycles);
	AddTimerObj(this,5208);
	return 0;//ffd8000c
}

int DreamCast::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case ME_ENDFRAME:
		//	AICA::Update();
			PVR2::Update(!OnFrame());
			return 0;
		case ME_MOVEWINDOW:{
			int w,h;

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
			PVR2::Update();
			Draw();
			CALLEE(Machine::OnEventI,ev,return,arg);
			return 0;
		case 0:{//pending irq bit
			va_start(arg, ev);
			int i=va_arg(arg,int);
			va_end(arg);
			switch(i){
				case 0:
					ev=1;
					printf("irq 1\n");
				break;
				case 1:
				break;
				default:{
					u32 n;

					if(!(n=_irq_pending))
						return 0;
					for(ev=0;n;ev++){
						if(!(BV(ev) & _irq_pending))
							continue;
						break;
					}
					if(!n) return 0;
				}
				break;
			}
		}
		break;
		default:
			if(BVT(ev,31)) return -1;
			{
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				Resume();
				switch(i){
					case 1:
						BS(_irq_pending,BV(ev));
						return 1;
					case 2:
						BC(_irq_pending,BV(ev));
						return 0;
				}
			}
		break;
	}
	BVC(_irq_pending,ev);
	if(_ppl.delay)
		return 2;
	switch(ev){
		case 19:
		case 13:
		case 12:
		case 7:
		case 8:
		case 9:
		case 10:
		case 2:
		case 3://holly
		case 4://holly
			if(_enterIRQ(ev,0))
				BVS(_irq_pending,ev);
	}
	return 0;
}

int DreamCast::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_DBG_MENU:{
			char *p = new char[1000];
			((void **)pv)[0]=p;
			*p=0;

			strcpy(p,"Bank 0/1");
			p+=strlen(p)+1;
			*((u32 *)p)=1;
			*((u32 *)&p[4])=0x083;
			p+=sizeof(u32)*2;

			strcpy(p,"Bank 0");
			p+=strlen(p)+1;
			*((u32 *)p)=2;
			*((u32 *)&p[4])=0x003;
			p+=sizeof(u32)*2;

			strcpy(p,"Bank 1");
			p+=strlen(p)+1;
			*((u32 *)p)=3;
			*((u32 *)&p[4])=0x003;
			p+=sizeof(u32)*2;

			strcpy(p,"FPU Bank 0/1");
			p+=strlen(p)+1;
			*((u32 *)p)=4;
			*((u32 *)&p[4])=0x083;
			p+=sizeof(u32)*2;

			strcpy(p,"FPU Bank 0");
			p+=strlen(p)+1;
			*((u32 *)p)=5;
			*((u32 *)&p[4])=0x003;
			p+=sizeof(u32)*2;

			strcpy(p,"FPU Bank 1");
			p+=strlen(p)+1;
			*((u32 *)p)=6;
			*((u32 *)&p[4])=0x003;

			p+=sizeof(u32)*2;
			*((u64 *)p)=0;
		}
			return 0;
		case ICORE_QUERY_DBG_MENU_SELECTED:{
				u32 id =*((u32 *)pv);
				switch(id){
					case 1:{
						u32 a = S_DEBUG_USER_VAL(Machine::_status);
						a&=~3;
						S_DEBUG_USER_CLR(Machine::_status);
						S_DEBUG_USER_SET(Machine::_status,a);
					}
					break;
					case 2:{
						u32 a = S_DEBUG_USER_VAL(Machine::_status);
						a=(a & ~3)|1;
						S_DEBUG_USER_CLR(Machine::_status);
						S_DEBUG_USER_SET(Machine::_status,a);
					}
					break;
					case 3:{
						u32 a = S_DEBUG_USER_VAL(Machine::_status);
						a=(a & ~3)|2;
						S_DEBUG_USER_CLR(Machine::_status);
						S_DEBUG_USER_SET(Machine::_status,a);
					}
					break;
					case 4:{
						u32 a = S_DEBUG_USER_VAL(Machine::_status);
						a=(a & ~0xc);
						S_DEBUG_USER_CLR(Machine::_status);
						S_DEBUG_USER_SET(Machine::_status,a);
					}
					break;
					case 5:{
						u32 a = S_DEBUG_USER_VAL(Machine::_status);
						a=(a & ~0xc)|4;
						S_DEBUG_USER_CLR(Machine::_status);
						S_DEBUG_USER_SET(Machine::_status,a);
					}
					break;
					case 6:{
						u32 a = S_DEBUG_USER_VAL(Machine::_status);
						a=(a & ~0xc)|8;
						S_DEBUG_USER_CLR(Machine::_status);
						S_DEBUG_USER_SET(Machine::_status,a);
					}
					break;
				}
			}
			return 0;
		case IMACHINE_QUERY_DEBUG_NOTIFY:
			Machine::_status |= MS_NOTIFY_DEBUG;
			return 0;
		case IMACHINE_QUERY_MEMORY_ACCESS:{
			void *p;
			LPMEMORYACCESS d=(LPMEMORYACCESS)pv;

			RMAP_(MMUT(d->addr,0,AM_READ|AM_QWORD),p,R);
			d->mem=p;
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
			if(!(p = (LPDEBUGGERPAGE)malloc(4*sizeof(DEBUGGERPAGE))))
				return -2;
			*((LPDEBUGGERPAGE *)pv)=p;
			memset(p,0,4*sizeof(DEBUGGERPAGE));
			p->size=sizeof(DEBUGGERPAGE);
			strcpy(p->title,"FPU");
			strcpy(p->name,"3101");
			p->type=1;
			p->popup=1;

			p++;
			p->size=sizeof(DEBUGGERPAGE);
			strcpy(p->title,"SYS");
			strcpy(p->name,"3102");
			p->type=1;
			p->popup=1;

			p++;
			p->size=sizeof(DEBUGGERPAGE);
			strcpy(p->title,"IO");
			strcpy(p->name,"3103");
			p->type=1;
			p->popup=1;
		}
		return 0;
		case ICORE_QUERY_ADDRESS_INFO:{
			LPMEMORYACCESS d =(LPMEMORYACCESS)pv;
				u32 adr=d->addr;
				//printf("adr %x ",adr);
				switch(adr & 0x1f000000){
					case 0x0c000000:
					case 0x0d000000:
					case 0x0e000000:
					case 0x0f000000:
						d->addr=adr & ~0xffffff;
						d->size=MB(16);
						break;
					case 0x0:
						d->addr=adr & ~0xffffff;
						d->size=MB(16);
						break;
						break;
					default:
						return -2;
				}
				//printf("%x %x\n",pp[0],pp[1]);
			}
			return 0;
		case ICORE_QUERY_FILE_EXT:
			memcpy(pv,"CDI Files (*.*)\0*.cdi;\0\0\0\0\0\0\0",26);
			return 0;
		default:
			return SH4Cpu::Query(what,pv);
	}
	return -1;
}

int DreamCast::Exec(u32 status){
	int ret;

	ret=SH4Cpu::Exec(status);
	__cycles=SH4Cpu::_cycles;
	switch(ret){
		case -1:
		case -2:
			return -ret;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int DreamCast::Dump(char **pr){
	int res;
	char *c,*cc,*p;
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
	*p=0|(S_DEBUG_USER_VAL(Machine::_status) & 3);

	((CCore *)cpu)->_dumpRegisters(p);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3101");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*p=0x10|SR(S_DEBUG_USER_VAL(Machine::_status) & 0xC,2);
	((CCore *)cpu)->_dumpRegisters(p);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3104");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	//adr=di._dumpAddress;
	//pp=&cc[900];

	adr=MMUT(di._dumpAddress,0,AM_READ|AM_BYTE);
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
	strcpy(p,"3103");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	sprintf(cc,"ISTNRM:%08x MDTSEL:%08x MDEN:%08x",IOREG_(SH4Cpu::_ioreg,SB_BASE+SB_ISTNRM),
		IOREG_(SH4Cpu::_ioreg,SB_BASE+SB_MDTSEL),
		IOREG_(SH4Cpu::_ioreg,SB_BASE+SB_MDEN));
	strcat(p,cc);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3102");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	sprintf(cc,"MMUCR: %08X CCR: %08X QACR0: %08X QACR1; %08X",_mmu._regs[(SH4_MMUCR)],_mmu._regs[(SH4_CCR)],
		_mmu._regs[(SH4_QACR0)],_mmu._regs[(SH4_QACR1)]);
	strcat(p,cc);

	sprintf(cc,"\nDAR2: %08X CHCR2: %08X SAR2: %08X DMATCR2: %08X",
		IOREG_(SH4Cpu::_ioreg,(MS_IO-0x10000-1)+SH4_DAR2_ADDR*4),
		IOREG_(SH4Cpu::_ioreg,(MS_IO-0x10000-1)+SH4_CHCR2_ADDR*4),
		IOREG_(SH4Cpu::_ioreg,(MS_IO-0x10000-1)+SH4_SAR2_ADDR*4),
		IOREG_(SH4Cpu::_ioreg,(MS_IO-0x10000-1)+SH4_DMATCR2_ADDR*4));
	strcat(p,cc);
	res += strlen(p)+1;

	if(Machine::_status & MS_NOTIFY_DEBUG){
		p= &c[res];
		strcpy(p,"INVALIDATE");
		p+=11;
		*((u32 *)p)=0;
		p+=4;
		res+=15;
		Machine::_status &= ~MS_NOTIFY_DEBUG;
	}
	p= &c[res];
	*((u64 *)p)=0;
	*pr = c;
	return res;
}

int DreamCast::Run(u8 *,int cyc,void *obj){
	int res = PVR2::Run(0,cyc,obj);
	ASIC::Update();
	return res;
}

int DreamCast::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	return 0;
}

int DreamCast::OnJump(u32 pc){
	switch(pc&0x00ffffff){
		case 0x800:
			switch(REG_[4]){
				case 0:
					REG_[0]=0xc0bebc;
				default:
					printf("reios_sys_misc %x\n",REG_[4]);
				break;
			}
		break;
		case 0x1000:
			system_hle_proc();//b0
		break;
		case 0x1004:
			sys_font_hle_proc();
		break;
		case 0x1008:
			flashrom_hle_proc();//b8
		break;
		case 0x100c://bc
			gdrom_hle_proc();
		break;
		default:
			printf("%x %x\n",pc,REG_[7]);
			EnterDebugMode();
		break;
	}
	return -1;
}

int DreamCast::OnException(u32,u32){
	return -1;
}

s32 DreamCast::fn_sh4_cache_regs_w(u32 a,pvoid,pvoid pdata,u32){
	return _mmu.write(a,(u32 *)pdata);
}

s32 DreamCast::fn_sh4_cache_regs_r(u32,pvoid,pvoid,u32){
	return 0;
}

//#include "sh.cpp.inc"


#define SH2LOGP(a,...) 	if((gui._getStatus() & (S_PAUSE|S_DEBUG_NEXT)) == (S_PAUSE|S_DEBUG_NEXT)) __F(a, ## __VA_ARGS__)
#define SH2LOG(...) 	SH2LOGP(NOARG,## __VA_ARGS__)
#define SH2LOGD(a,...)	sprintf(cc,STR(%s) STR(\x20) STR(a),## __VA_ARGS__);
#define SH2LOGE()		{printf("%08X %04X UNKNOW\n",_pc,_opcode);EnterDebugMode();}

#define SH2BRANCH(a,b) 	_ppl._create_slot(a,b)


SHCORECPU::SHCORECPU() : CCore(){
	_ioreg=NULL;
}

SHCORECPU::~SHCORECPU(){
	//Destroy();
}

int SHCORECPU::_exec(u32 status){
	int res=1;

A:
	if(0 && _vm._invalidate){
		_vm._invalidate=0;
		_vm._step=0;
		if(_vm._play < 2)
			goto B;
		_vm._remap=0;
		goto C;
B:
		if(_vm._remap){
			_vm._remap=0;
			_vm._wsync=0;
			if(!(_pc < _vm._pcu || _pc >= _vm._pcd)){
				//lino++;
				if(!_vm._loop){
					_vm._loop=1;
					__bus=0;
					__address=0;
				}
				else if(_vm._loop==1){
					_vm._loop=2;
					if(!__address){
						printf("iam ready !!!\n");
					}
				}
				else if(_vm._loop==2){
				}
/*				if(BT(status,S_PAUSE|S_DEBUG_NEXT) == (S_PAUSE|S_DEBUG_NEXT)){
					printf("EL %x %x %x %x %u %u\n",_pc,_vm._pcu,_vm._pcd,_vm._idx,
							((_pc - _vm._pcu)),lino);
				}
				*/

				_vm._idx=((_pc - _vm._pcu))>>3;
				_vm._step=SR((_pc - _vm._pcu) & 7,1);
			}
			else{
				RMAP_(_pc,_vm._mem,R);
				_vm.pp=_vm._mem;
				_vm._idx=0;
				_vm._loop=0;
				_vm._pc=_vm._pcu=_vm._pcd=_pc;
				_vm._pcd-=2;
			}
		}

		if(!_vm._loop){
			u64 **p,d;
			__data=*(u64 *)_vm.pp;
			((u64 *)_vm._data)[_vm._idx] = __data;
			((u64 *)_vm._data)[_vm._idx+(_vm._slot)] = (__data & 0xf000f000f000f000)>>12;//opcode

			d=(__data & 0x0F000F000F000F00)>>8;//rs
			((u64 *)_vm._data)[_vm._idx+(_vm._slot * 2)] = d;

			d=(__data & 0x00F000F000F000F0)>>4;//rt
			((u64 *)_vm._data)[_vm._idx+(_vm._slot * 3)] = d;

			_vm._pcd += 8;
		}
		else if(_vm._loop==1){
		//	_vm._slots[_vm._idx]._pc=_pc;
			//if(__bus)
			//	EnterDebugMode();
		}

		_vm.p = _vm._data + (_vm._idx * 8) + (_vm._step*2);
		_vm._opcode=_vm.p + (_vm._slot)*8;
		_vm._rs=_vm._opcode + (_vm._slot)*8;
		_vm._rt=_vm._rs + (_vm._slot)*8;

		if(++_vm._idx >= _vm._slot){
			// _vm._idx=0;
			 //_vm._step=0;
			 _vm._pcu=_vm._pcd=0;
			 //_vm._loop=0;
			 _vm._invalidate=1;
			 _vm._remap=1;
			 goto A;
		}
		//RLPC(_pc,_opcode);
	}
C:
	ROP(_pc&0x1fffffff,_opcode);
	switch(SR(_opcode,12)){
		case 0:
			switch(_opcode & 0x3f){
				case 2:
					if(_opcode & 0x80) EnterDebugMode();
					REG(8)=REG_SR;
					SH2LOGP(SR\x2cR%d,"STC",Ri(8));
				break;
				case 3:
					switch(_opcode & 0xc0){
#if SHCORE == 4
						case 0x80:{
							u32 a,b;

							a=b=REG(8);
							if(_mmu._regs[(SH4_MMUCR)] & 1){
								EnterDebugMode();
							}
							else{
								a &= 0x03FFFFE0;
								if(a&0x20)
									a |= (SL(_mmu._regs[SH4_QACR1]&0x1c,24)) & 0x1e000000;
								else
									a |= SL(_mmu._regs[SH4_QACR0]&0x1c,24) & 0x1e000000;
							}
							for(int i=0;i<4;i++,a+=8,b+=8){
								u64 v;

								RQ_(b,v,NOARG,NOARG);
								WQ_(a,v,NOARG,NOARG);
							}
							SH2LOGP(R%d %x,"PREF",Ri(8),a);
							//EnterDebugMode();
						}
						break;
						case 0xc0:
							WL(REG(8),REG_0);
							SH2LOGP(R0\x2c\x40R%d,"MOVCA.L",Ri(8));
						break;
#endif
						case 0x00:{
							s32 a = REG(8);

							SH2LOGP(R%d,"BSRF",Ri(8));
							REG_PR=_pc+4;
							STORECALLLSTACK(REG_PR);
							SH2BRANCH(2,_pc+a+4);
							res++;
						}
						break;
						default:
							SH2LOGE();
						break;
					}
				break;
				case 4:
				case 0x14:
				case 0x24:
				case 0x34:
					WB(REG_0+REG(8),(u8)REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c\x40[R%d+R0],"MOV.BS0",Ri(4),Ri(8));
				break;
				case 0x5:
				case 0x15:
				case 0x25:
				case 0x35:
					WW(REG_0+REG(8),REG(4));
					res+=04;
					SH2LOGP(R%d\x2c\x40[R0+R%d],"MOV.WS0",Ri(4),Ri(8));
				break;
				case 0x6:
				case 0x16:
				case 0x26:
				case 0x36:
					WL(REG_0+REG(8),REG(4));
					res+=04;
					SH2LOGP(R%d\x2c\x40[R0+R%d],"MOV.LS0",Ri(4),Ri(8));
				break;
				case 7:
				case 0x17:
				case 0x27:
				case 0x37:
					REG_MACL=REG(8)*REG(4);
					res +=2;
					SH2LOGP(R%d\x2cR%d,"MULL",Ri(4),Ri(8));
				break;
				case 0x8:
					BC(REG_SR,SH_T);
					SH2LOG("CLRT");
				break;
				case 9:
					SH2LOG("NOP");
				break;
				case  0xa:
					REG(8)=REG_MACH;
					SH2LOGP(MACH\x2cR%d,"STS",Ri(8));
				break;
				case 0xb:
					SH2BRANCH(2,REG_PR);
					//_pc=REG_PR-2;
					LOADCALLSTACK(REG_PR);
					res++;
					SH2LOG("RTS");
				break;
				case 0xe:
				case 0x1e:
				case 0x2e:
				case 0x3e:
					RL(REG(4)+REG_0,REG(8));
					res+=04;
					SH2LOGP(\x40[R0+R%d]\x2cR%d,"MOV.LL0",Ri(4),Ri(8));
				break;
				case 0x12:
					REG(8)=REG_GBR;
					SH2LOGP(GBR\x2cR%d,"STC",Ri(8));
				break;
#if SHCORE == 4
				case 0x13:
					SH2LOGP(\x40R%d,"OCBI",Ri(8));
				break;
				case 0x33:
					SH2LOGP(\x40R%d,"OCBWB",Ri(8));
				break;
#endif
				case 0x18:
					REG_SR |= SH_T;
					SH2LOG("SETT");
				break;
				case 0x19:
					REG_SR &= ~(SH_M | SH_Q | SH_T);
					SH2LOG("DIV0U");
				break;
				case 0x1a:
					REG(8)=REG_MACL;
					SH2LOGP(MACL\x2cR%d,"STS",Ri(8));
				break;
				case 0x1b:
					Sleep();
					SH2LOGP(NOARG,"SLEEP");
				break;
				case 0x22:
					REG(8)=REG_VBR;
					SH2LOGP(VBR\x2cR%d,"STC",Ri(8));
				break;
				case 0x23:
					switch((u8)_opcode){
						case 0x23:
							SH2LOGP(R%d,"BRAF",Ri(8));
							SH2BRANCH(2,REG(8)+_pc+4);
							res++;
						break;
#if SHCORE == 4
						case 0xa3:
							SH2LOGP(R%d,"OCBP",Ri(8));
						break;
#endif
						default:
							SH2LOGE();
						break;
					}
				break;
				case 0x29:
					REG(8) = REG_SR&SH_T;
					SH2LOGP(R%d,"MOVT",Ri(8));
				break;
				case 0x2a:
					switch((u8)_opcode){
#if SHCORE == 4
						case 0x6a: case 0xea:
							REG(8)=REG_FPSCR;
							SH2LOGP(FPSCR\x2cR%d,"STS",Ri(8));
						break;
#endif
						default:
							REG(8)=REG_PR;
							SH2LOGP(PR\x2cR%d,"STS",Ri(8));
						break;
					}
				break;
				case 0x2b:{
						u32 a;
#if SHCORE == 4
						//REG_SR=REG_SSR;
						//REG_SP=REG_SGR;
						a=REG_SPC;
						_changeSR(REG_SSR);
#else
						RL(REG_SP,a);
						REG_SP += 4;
						RL(REG_SP,REG_SR);
						REG_SP += 4;
#endif
						SH2BRANCH(2,a);
						_ppl.irq=1;
					}
					res++;
					SH2LOG("RTE");
					EnterDebugMode(DEBUG_BREAK_IRQ);
				break;
				case 0xc:
				case 0x1c:
				case 0x2c:
				case 0x3c:{
					u8 b;
					u32 a;

					a=REG(4)+REG_0;
					RB(a,b);
					REG(8)=(u32)(s32)(s8)b;
					res+=04;
					SH2LOGP(\x40[R0+R%d]\x2cR%d,"MOV.BL0",Ri(4),Ri(8));
				}
				break;
				case 0xd:
				case 0x1d:
				case 0x2d:
				case 0x3d:{
					u32 a =REG(4)+REG_0;
					u16 u;

					RW(a,u);
					REG(8)=(u32)(s32)(s16)u;
					res += 04;
					SH2LOGP(\x40[R0+R%d]\x2cR%d,"MOV.WL0",Ri(4),Ri(8));
				}
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 1:{
			u32 a = REG(8)+ SL(_opcode&0xf,2);
			WL(a,REG(4));
			res+=04;
			SH2LOGP(R%d\x2c\x40[R%d+0x%x],"MOV.LS4",Ri(4),Ri(8),SL(_opcode&0xF,2));
		}
		break;
		case 2:
			switch(_opcode & 0xf){
				case 0:
					WB(REG(8),(u8)REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c\x40R%d,"MOV.BS",Ri(4),Ri(8));
				break;
				case 1:
					WW(REG(8),(u16)REG(4));//R4->(R8
					res+=04;
					SH2LOGP(R%d\x2c\x40R%d,"MOV.WS",Ri(4),Ri(8));
				break;
				case 2:
					WL(REG(8),REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c\x40R%d,"MOV.LS",Ri(4),Ri(8));
				break;
				case 4:{
					u32 a = REG(8)-1;
					REG(8)=a;
					WB(a,REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c\x40-R%d,"MOV.BM",Ri(4),Ri(8));
				}
				break;
				case 6:{
					u32 a = REG(8)-4;
					REG(8)-=4;
					WL(a,REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c\x40-R%d,"MOV.LM",Ri(4),Ri(8));
				}
				break;
				case 0x7:
					if(REG(8)&0x80000000)
						BS(REG_SR,SH_Q);
					else
						BC(REG_SR,SH_Q);
					if(REG(4)&0x80000000)
						BS(REG_SR,SH_M);
					else
						BC(REG_SR,SH_M);
					if(
						((REG_SR&SH_Q) && (REG_SR&SH_M)) ||
						(!(REG_SR&SH_Q) && !(REG_SR&SH_M)))
						BC(REG_SR,SH_T);
					else
						BS(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"DIV0S",Ri(4),Ri(8));
				break;
				case 8:
					if((REG(8) & REG(4))==0)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"TST",Ri(4),Ri(8));
				break;
				case 9:
					REG(8)&=REG(4);
					SH2LOGP(R%d\x2cR%d,"AND",Ri(4),Ri(8));
				break;
				case 0xa:
					REG(8)^=REG(4);
					SH2LOGP(R%d\x2cR%d,"XOR",Ri(4),Ri(8));
				break;
				case 0xb:
					REG(8)|=REG(4);
					SH2LOGP(R%d\x2cR%d,"OR",Ri(4),Ri(8));
				break;
				case 0xc:{
					u32 a,b;
					a=REG(8);
					b=REG(4);
					BC(REG_SR,SH_T);
					for(int i=0;i<4;i++,a>>=8,b>>=8){
						if((u8)a == (u8)b){
							BS(REG_SR,SH_T);
							break;
						}
					}
					SH2LOGP(R%d\x2cR%d,"CMPSTR",Ri(4),Ri(8));
				}
				break;
				case 0xd:
					REG(8) = SL((u16)REG(4),16)|SR(REG(8),16);
					SH2LOGP(R%d\x2cR%d,"XTRCT",Ri(4),Ri(8));
				break;
				case 0xe:
					REG_MACL=(u32)((u16)REG(8)*(u16)REG(4));
					SH2LOGP(R%d\x2cR%d,"MULU.W",Ri(4),Ri(8));
					res++;
				break;
				case 0xf:
					REG_MACL=(u32)((s16)REG(8)*(s16)REG(4));
					SH2LOGP(R%d\x2cR%d,"MULS.W",Ri(4),Ri(8));
					res++;
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 3:
			switch(_opcode & 0xf){
				case 0:
					if(REG(8) == REG(4))
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPEQ",Ri(4),Ri(8));
				break;
				case 2:
					if(REG(8) >= REG(4))
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPHS",Ri(4),Ri(8));
				break;
				case 3:
					if((s32)REG(8) >= (s32)REG(4))
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPGE",Ri(4),Ri(8));
				break;
				case 0x4:{
					u32 a,oq,b;

					oq=REG_SR&SH_Q;
					if(REG(8)&0x80000000)
						BS(REG_SR,SH_Q);
					else
						BC(REG_SR,SH_Q);
					REG(8)=SL(REG(8),1)|(REG_SR&SH_T);
					a=REG(8);
					switch(oq){
						case 0:
							switch(REG_SR&SH_M){
								case 0:
									REG(8)-= REG(4);
									b=REG(8)>a;
									switch(REG_SR&SH_Q){
										case 0:
											if(b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
										default:
											if(!b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
									}
								break;
								default:
									REG(8)+=REG(4);
									b=REG(8)<a;
									switch(REG_SR&SH_Q){
										case 0:
											if(!b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
										default:
											if(b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
									}
								break;
							}
						break;
						default:
							switch(REG_SR&SH_M){
								case 0:
									REG(8)+=REG(4);
									b=REG(8)<a;
									switch(REG_SR&SH_Q){
										case 0:
											if(b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
										default:
											if(!b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
									}
								break;
								default:
									REG(8)-=REG(4);
									b=REG(8)>a;
									switch(REG_SR&SH_Q){
										case 0:
											if(!b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
										default:
											if(b)
												BS(REG_SR,SH_Q);
											else
												BC(REG_SR,SH_Q);
										break;
									}
								break;
							}
						break;
					}

					if(	((REG_SR&SH_Q) && (REG_SR&SH_M)) || (!(REG_SR&SH_Q) && !(REG_SR&SH_M)) )
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"DIV1",Ri(4),Ri(8));
				}
				break;
				case 5:{
					u64 v = REG(8);
					v *= REG(4);
					REG_MACL=(u32)v;
					REG_MACH=(u32)SR(v,32);
					SH2LOGP(R%d\x2cR%d,"DMULU",Ri(4),Ri(8));
				}
				break;
				case 6:
					if(REG(8) > REG(4))
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPHI",Ri(4),Ri(8));
				break;
				case 7:
					if((s32)REG(8) > (s32)REG(4))
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPGT",Ri(4),Ri(8));
				break;
				case 8:
					REG(8)=REG(8)-REG(4);
					SH2LOGP(R%d\x2cR%d,"SUB",Ri(4),Ri(8));
				break;
				case 0xa:{
					u32 a,b;

					a=REG(8)-REG(4);
					b=REG(8);
					REG(8)=a-(REG_SR&SH_T);

					if(b<a)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					if(a<REG(8))
						BS(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"SUBC",Ri(4),Ri(8));
				}
				break;
				case 0xc:
					REG(8)+=REG(4);
					SH2LOGP(R%d\x2cR%d,"ADD",Ri(4),Ri(8));
				break;
				case 0xd:{
					s64 v = (s32)REG(8);
					v *= (s32)REG(4);
					REG_MACL=(u32)v;
					REG_MACH=(u32)SR(v,32);
					SH2LOGP(R%d\x2cR%d,"DMULS",Ri(4),Ri(8));
				}
				break;
				case 0xe:{
					u32 a,b;

					a=REG(8)+REG(4);
					b=REG(8);
					REG(8)=(REG_SR&SH_T)+a;

					if(b>a)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					if(a>REG(8))
						BS(REG_SR,SH_T);
					SH2LOGP(R%d\x2cR%d,"ADDC",Ri(4),Ri(8));
				}
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 4:
			switch((u8)_opcode){
				case 0:
					if(REG(8) & 0x80000000)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					REG(8)=SL(REG(8),1);
					SH2LOGP(R%d,"SHLL",Ri(8));
				break;
				case 1:
					REG_SR=(REG_SR & ~SH_T) | (REG(8) & 1);
					REG(8)=SR(REG(8),1);
					SH2LOGP(R%d,"SHLR",Ri(8));
				break;
				case 2: case 0x82: case 0x42: case 0xc2:
					REG(8)-=4;
					WL(REG(8),REG_MACH);
					res++;
					SH2LOGP(MACH\x2c\x40-R%d,"STS.L",Ri(8));
				break;
#if SHCORE == 4
				case 3:
					REG(8)-=4;
					WL(REG(8),REG_SR);
					SH2LOGP(SR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x0c: case 0x1c: case 0x2c: case 0x3c: case 0x8c: case 0x9c: case 0xac: case 0xbc:
				case 0x4c: case 0x5c: case 0x6c: case 0x7c: case 0xcc: case 0xdc: case 0xec: case 0xfc:
					if((REG(4) & 0x80000000) == 0)
						REG(8) <<= REG(4) & 0x1f;
					else if((REG(4) & 0x1f) == 0){
						if((REG(8) & 0x80000000) == 0)
							REG(8) =0;
						else
							REG(8)=0xffffffff;
					}
					else
						REG(8) = SR((s32)REG(8), (~REG(4) & 0x1f) + 1);
					SH2LOGP(R%d\x2c R%d,"SHAD",Ri(4),Ri(8));
				break;
				case 0x13: case 0x53:
					REG(8)-=4;
					WL(REG(8),REG_GBR);
					SH2LOGP(GBR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x1b: case 0x5b:{
					u8 a;

					RB(REG(8),a);
					if(!a)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					a |= 0x80;
					WB(REG(8),a);
					SH2LOGP(\x40R%d,"TAS.B",Ri(8));
				}
				break;
				case 0x23: case 0x63:
					REG(8)-=4;
					WL(REG(8),REG_VBR);
					SH2LOGP(VBR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x47:{
					u32 a;

					RL(REG(8),a);
					REG(8) += 4;
					REG_SPC=a;
					res++;
					SH2LOGP(\x40R%d+\x2cSPC,"LDC.L",Ri(8));
				}
				break;
				case 0x5a:
					REG_FPUL=REG(8);
					SH2LOGP(R%d\x2c FPUL,"LDS",Ri(8));
				break;
				case 0x83: case 0x93: case 0xa3: case 0xb3:
				case 0xc3: case 0xd3: case 0xe3: case 0xf3:{
					int i = (REG_SR & SH_RB) ? 0 : 1;
					REG(8) -= 4;
					WL(REG(8),_regs_bank[i][(Ri(4)&7)]);
					res++;
					SH2LOGP(R%d_%d\x2c\x40-R%d,"STC.L",Ri(4) & 7,i,Ri(8));
				}
				break;
				case 0x87: case 0x97: case 0xa7: case 0xb7:
				case 0xc7: case 0xd7: case 0xe7: case 0xf7:{
					int i = (REG_SR & SH_RB) ? 0 : 1;
					RL(REG(8),_regs_bank[i][(Ri(4)&7)]);
					REG(8)+=4;
					res++;
					SH2LOGP(\x40R%d+\x2cR%d_%d,"LDC.L",Ri(8),Ri(4)&7,i);
				}
				break;
				case 0x27: case 0x67:
					RL(REG(8),REG_VBR);
					REG(8)+=4;
					SH2LOGP(\x40R%d+\x2c VBR,"LDC.L",Ri(8));
				break;
				case 0x32: case 0x72:
					REG(8)-=4;
					WL(REG(8),REG_SGR);
					SH2LOGP(SGR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x33: case 0x73:
					REG(8)-=4;
					WL(REG(8),REG_SSR);
					SH2LOGP(SRR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x37: case 0x77:
					RL(REG(8),REG_SSR);
					REG(8)+=4;
					SH2LOGP(\x40R%d+\x2c SSR,"LDC.L",Ri(8));
				break;
				case 0x43:
					REG(8)-=4;
					WL(REG(8),REG_SPC);
					SH2LOGP(SPC\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0xb2: case 0xf2:
					REG(8)-=4;
					WL(REG(8),REG_DBR);
					SH2LOGP(DBR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0xb6: case 0xf6:
					RL(REG(8),REG_DBR);
					REG(8)+=4;
					SH2LOGP(\x40R%d+\x2c DBR,"LDC.L",Ri(8));
				break;
#endif
				case 4:{
					if(SR(REG(8),31))
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					REG(8)=SL(REG(8),1)|SR(REG(8),31);
					SH2LOGP(R%d,"ROTL",Ri(8));
				}
				break;
				case 5:
					if(REG(8)&1)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					REG(8)=SR(REG(8),1)|SL(REG(8),31);
					SH2LOGP(R%d,"ROTR",Ri(8));
				break;
				case 6: case 0x86: case 0x46: case 0xc6:
					RL(REG(8),REG_MACH);
					REG(8)+=4;
					res++;
					SH2LOGP(\x40R%d+\x2cMACH,"LDS.L",Ri(8));
				break;
				case 7:{
					u32 a;

					RL(REG(8),a);
					REG(8) += 4;
					_changeSR(a);
					_ppl.irq=1;
					res++;
					SH2LOGP(\x40R%d+\x2cSR,"LDC.L",Ri(8));
				}
				break;
				case 0x8:
					REG(8)=SL(REG(8),2);
					SH2LOGP(R%d,"SHLL2",Ri(8));
				break;
				case 9:
					REG(8)=SR(REG(8),2);
					SH2LOGP(R%d,"SHRL2",Ri(8));
				break;
				case 0xa: case 0x8a: case 0x4a: case 0xca:
					SH2LOGP(R%d\x2cMACH,"LDS",Ri(8));
					REG_MACH=REG(8);
				break;
				case 0xb: case 0x8b: case 0x4b: case 0xcb:
					SH2LOGP(R%d,"JSR",Ri(8));
					REG_PR=_pc+4;
					STORECALLLSTACK(REG_PR);
					SH2BRANCH(2,REG(8));
					res++;
				break;
				case 0xd: case 0x8d:case 0x1d: case 0x9d:
				case 0x2d: case 0xad:case 0x3d: case 0xbd:
				case 0x4d: case 0xcd:case 0x5d: case 0xdd:
				case 0x6d: case 0xed:case 0x7d: case 0xfd:
					if(!(REG(4) & 0x80000000))
						REG(8) <<= REG(4)&0x1f;
					else if((REG(4) & 0x1f)== 0)
						REG(8) = 0;
					else
						REG(8) >>= ((~REG(4) & 0x1f) + 1);
					SH2LOGP(R%d\x2cR%d,"SHLD",Ri(4),Ri(8));
				break;
				case 0xe:{
					_changeSR(REG(8));
					_ppl.irq=1;
					SH2LOGP(R%d\x2cSR,"LDC",Ri(8));
				}
				break;
				case 0x10: case 0x90: case 0x50: case 0xd0:
					REG(8)--;
					if(REG(8) == 0)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d,"DT",Ri(8));
				break;
				case 0x11: case 0x91: case 0x51: case 0xd1:
					if((s32)REG(8) >= 0)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d\x2cPZ,"CMPPZ",Ri(8));
				break;
				case 0x12: case 0x92: case 0x52: case 0xd2:
					REG(8)-=4;
					WL(REG(8),REG_MACL);
					res++;
					SH2LOGP(MACL\x2c\x40-R%d,"STS.L",Ri(8));
				break;
				case 0x15: case 0x95:case 0x55: case 0xd5:
					if((s32)REG(8) > 0)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d,"CMPPL",Ri(8));
				break;
				case 0x16: case 0x96:case 0x56:case 0xd6:
					RL(REG(8),REG_MACL);
					REG(8)+=4;
					res++;
					SH2LOGP(\x40R%d+\x2cMACL,"LDS.L",Ri(8));
				break;
				case 0x17:case 0x57:
					RL(REG(8),REG_GBR);
					REG(8)+=4;
					res++;
					SH2LOGP(\x40R%d+\x2cGBR,"LDS.L",Ri(8));
				break;
				case 0x18:
					REG(8)=SL(REG(8),8);
					SH2LOGP(R%d,"SHLL8",Ri(8));
				break;
				case 0x19:
					REG(8)=SR(REG(8),8);
					SH2LOGP(R%d,"SHRL8",Ri(8));
				break;
				case 0x1a: case 0x9a:case 0xda:
					SH2LOGP(R%d\x2cMACL,"LDS",Ri(8));
					REG_MACL=REG(8);
				break;
				case 0x1e: case 0x9e:case 0x5e:case 0xde:
					REG_GBR=REG(8);
					SH2LOGP(R%d\x2cGBR,"LDC",Ri(8));
				break;
				case 0x21: case 0xa1: case 0x61:case 0xe1:{
					BC(REG_SR,SH_T);
					REG_SR |= (REG(8) & 1);
					REG(8) = (u32)SR((s32)REG(8),1);
					SH2LOGP(R%d,"SHAR",Ri(8));
				}
				break;
				case 0x22: {
					REG(8) -= 4;
					WL(REG(8),REG_PR);
					res++;
					SH2LOGP(PR\x2c\x40-R%d,"STS",Ri(8));
				}
				break;
				case 0x24:{
					u32 a = SR(REG(8),31);
					REG(8)=SL(REG(8),1)|(REG_SR&SH_T);
					if(a)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d,"ROTCL",Ri(8));
				}
				break;
				case 0x25:{
					u32 a = REG(8)&1;
					REG(8)=SR(REG(8),1)|SL(REG_SR&SH_T,31);
					if(a)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(R%d,"ROTCR",Ri(8));
				}
				break;
				case 0x26:
					RL(REG(8),REG_PR);
					REG(8)+=4;
					res++;
					SH2LOGP(\x40R%d+\x2cPR,"LDS.L",Ri(8));
				break;
				case 0x28:
					REG(8)=SL(REG(8),16);
					SH2LOGP(R%d,"SHLL16",Ri(8));
				break;
				case 0x29:
					REG(8)=SR(REG(8),16);
					SH2LOGP(R%d,"SHLR16",Ri(8));
				break;
				case 0x2a:
					REG_PR=REG(8);
					SH2LOGP(R%d\x2cPR,"LDS",Ri(8));
				break;
				case 0x2b: case 0xab:case 0x6b:case 0xeb:
					SH2LOGP(\x40R%d,"JMP",Ri(8));
					SH2BRANCH(2,REG(8));
					res++;
				break;
				case 0x2e: case 0xae:case 0x6e: case 0xfe:
					REG_VBR=REG(8);
					SH2LOGP(R%d\x2cVBR,"LDC",Ri(8));
				break;
				case 0x62:
					REG(8)-=4;
					WL(REG(8),REG_FPSCR);
					SH2LOGP(FPCSR\x2c\x40-R%d,"STS",Ri(8));
				break;
				case 0x66:
					RL(REG(8),REG_FPSCR);
					REG(8)+=4;
					CHANGE_FPSCR();
					Query(IMACHINE_QUERY_DEBUG_NOTIFY,0);
					SH2LOGP(\x40R%d+\x2c FPCSR,"LDS",Ri(8));
				break;
				case 0x6a:
					REG_FPSCR=REG(8);
					CHANGE_FPSCR();
					Query(IMACHINE_QUERY_DEBUG_NOTIFY,0);
					SH2LOGP(R%d\x2c fFPCSR,"LDS",Ri(8));
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 5:{
			u32 a = REG(4) + SL(_opcode&0xf,2);//(R4+d))->R8
			RL(a,REG(8));
			res+=04;
			SH2LOGP(\x40[R%d+0x%x]\x2cR%d,"MOV.LL4",Ri(4),(int)SL(_opcode&0xf,2),Ri(8));
		}
		break;
		case 6:
			switch(_opcode & 0xf){
				case 0:{
					u8 a;

					RB(REG(4),a);
					REG(8)=(u32)(s32)(s8)a;
					res+=04;
					SH2LOGP(\x40R%d\x2cR%d,"MOV.BL",Ri(4),Ri(8));
				}
				break;
				case 1:{
						u16 a;
						u32 b=REG(4);

						RW(b,a);
						REG(8)=(u32)(s32)(s16)a;
						res+=04;
						SH2LOGP(\x40R%d\x2cR%d,"MOV.WL",Ri(4),Ri(8));
						//printf("reg %d %x %x %x %x PC::%x\n",Ri(8),REG(8),a,b,__data,_pc);
					}
				break;
				case 2://(Rm)->Rn
					RL(REG(4),REG(8));
					res+=04;
					SH2LOGP(\x40R%d\x2cR%d,"MOV.LL",Ri(4),Ri(8));
				break;
				case 3:
					REG(8)=REG(4);
					SH2LOGP(R%d\x2cR%d,"MOV",Ri(4),Ri(8));
				break;
				case 4:{
						u8 a;

						RB(REG(4),a);
						REG(8)=(u32)(s32)(s8)a;
						if(Ri(4) != Ri(8))
							REG(4)++;
						res+=04;
						SH2LOGP(\x40R%d+\x2cR%d,"MOV.BP",Ri(4),Ri(8));
					}
					break;
				case 5:{
						u16 a;

						RW(REG(4),a);
						REG(8)=(u32)(s32)(s16)a;
						if(Ri(4) != Ri(8))
							REG(4)+=2;
						res+=04;
						SH2LOGP(\x40R%d+\x2cR%d,"MOV.WP",Ri(4),Ri(8));
					}
					break;
				case 6:
					RL(REG(4),REG(8));
					if(Ri(4)!=Ri(8))
						REG(4)+=4;
					res+=04;
					SH2LOGP(\x40R%d+\x2cR%d,"MOV.LP",Ri(4),Ri(8));
				break;
				case 7:
					REG(8)=~REG(4);
					SH2LOGP(R%d\x2cR%d,"NOT",Ri(4),Ri(8));
				break;
				case 8:
					REG(8)=MAKEHWORD((u8)SR(REG(4),8),(u8)REG(4));
					SH2LOGP(R%d\x2cR%d,"SWAPB",Ri(4),Ri(8));
				break;
				case 9:
					REG(8)=MAKELONG(SR(REG(4),16),(u16)REG(4));
					SH2LOGP(R%d\x2cR%d,"SWAPW",Ri(4),Ri(8));
				break;
				case 0xa:{
					RSZU a=REG(4);
					REG(8)=0-a-(REG_SR & SH_T);
					if(a || (REG_SR & SH_T))
						REG_SR |= SH_T;
					else
						REG_SR &= ~SH_T;
					SH2LOGP(R%d\x2cR%d,"NEGC",Ri(4),Ri(8));
				}
				break;
				case 0xb:
					REG(8)=0-REG(4);
					SH2LOGP(R%d\x2cR%d,"NEG",Ri(4),Ri(8));
				break;
				case 0xC:
					REG(8) = (u32)(u8)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTU.B",Ri(4),Ri(8));
				break;
				case 0xd:
					REG(8) = (u32)(u16)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTU.W",Ri(4),Ri(8));
				break;
				case 0xe:
					REG(8) = (u32)(s32)(s8)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTS.B",Ri(4),Ri(8));
				break;
				case 0xf:
					REG(8) = (u32)(s32)(s16)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTS.W",Ri(4),Ri(8));
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 7:
			REG(8)+=(s32)(s16)(s8)_opcode;
			SH2LOGP(#%d\x2cR%d,"ADDI",(s32)(s8)_opcode,Ri(8));
		break;
		case 8:
			switch(SR(_opcode,8)&0xf){
				case 0:{
					u32 a = REG(4) + (_opcode&0xf);
					WB(a,(u8)REG_0);
					res+=04;
					SH2LOGP(\x40[R%d+%d]\x2cR0,"MOV.BS4",Ri(4),(_opcode&0xF));
				}
				break;
				case 1:{
					u32 a = REG(4) + SL(_opcode&0xf,1);
					WW(a,REG_0);
					res+=04;
					SH2LOGP(R0\x2c\x40[R%d+%d],"MOV.WS4",Ri(4),SL(_opcode&0xF,1));
				}
				break;
				case 4:{
					u32 a = REG(4) + (_opcode&0xf);
					u8 u;

					RB(a,u);
					REG_0=(u32)(s32)(s8)u;
					res+=04;
					SH2LOGP(\x40[R%d+%d]\x2cR0,"MOV.BL4",Ri(4),_opcode&0xF);
				}
				break;
				case 5:{
					u32 a = (REG(4) + SL(_opcode&0xf,1));
					u16 u;

					RW(a,u);
					REG_0=(u32)(s32)(s16)u;
					res+=04;
					SH2LOGP(\x40[R%d+%d]\x2cR0,"MOV.WL4",Ri(4),SL(_opcode&0xF,1));
				}
				break;
				case 8:
					if((s32)REG_0==(s32)(s8)_opcode)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(#%d\x2cR0,"CMPIM",(s32)(s8)_opcode);
				break;
				case 9:{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BT",a);
					if(BT(REG_SR,SH_T)){
						SH2BRANCH(1,a);
						res += 2;
						goto Z1;
					}
				}
				break;
				case 0xb:{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BF",a);
					if(!BT(REG_SR,SH_T)){
						res+=2;
						SH2BRANCH(1,a);
						goto Z1;
					}
				}
				break;
				case 0xd:{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BTS",a);
					if(BT(REG_SR,SH_T)){
						SH2BRANCH(2,a);
						res+=2;
					}
				}
				break;
				case 0xf:{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BFS",a);
					if(!BT(REG_SR,SH_T)){
						SH2BRANCH(2,a);
						res+=2;
					}
				}
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 9:{
			u32 a = ((_pc+4) + SL((u8)_opcode,1));
			u16 b;

			RW(a,b);
			REG(8)=(u32)(s32)(s16)b;
			res+=04;
			SH2LOGP(\x40[PC+%d]\x2cR%d,"MOV.WI",SL((u8)_opcode,1),Ri(8));
		}
		break;
		case 0xa:{
			s32 a = _opcode&0xFFF;
			if(a&0x800)
				a-=0x1000;
			SH2LOGP(%X,"BRA",_pc+SL(a,1)+4);
			SH2BRANCH(2,_pc+SL(a,1)+4);
			res++;
		}
		break;
		case 0xb:{
				s32 a = _opcode&0xFFF;
				if(a&0x800)
					a-=0x1000;
				SH2LOGP(%X,"BSR",_pc+SL(a,1)+4);
				REG_PR=_pc+4;
				STORECALLLSTACK(REG_PR);
				SH2BRANCH(2,_pc+SL(a,1)+4);
				res++;
			}
		break;
		case 0xC:
			switch(SR(_opcode,8)&0xf){
				case 5:{
					u32 a =(REG_GBR+SL((u8)_opcode,1));
					u16 u;

					RW(a,u);
					REG_0=(u32)(s32)(s16)u;
					res+=04;
					SH2LOGP(\x40[%d+gbr]\x2cR%d,"MOV.WLG",SL((u8)_opcode,1),0);
				}
				break;
				case 7:
					REG_0=((_pc+4) & ~3) + SL((u8)_opcode,2);
					res++;
					SH2LOGP(\x40[PC+%d]\x2cR0,"MOVA",SL((u8)_opcode,2));
				break;
				case 8:
					if((REG_0 & (u8)_opcode)==0)
						BS(REG_SR,SH_T);
					else
						BC(REG_SR,SH_T);
					SH2LOGP(#%d\x2cR0,"TSTI",(u8)_opcode);
				break;
				case 9:
					REG_0&=(u32)(u8)_opcode;
					SH2LOGP(#%d\x2cR0,"ANDI",(u32)(u8)_opcode);
				break;
				case 0xa:
					REG_0^=(u32)(u8)_opcode;
					SH2LOGP(#%d\x2cR0,"XORI",(u32)(u8)_opcode);
				break;
				case 0xb:
					REG_0|=(u8)_opcode;
					SH2LOGP(#%d\x2cR0,"ORI",(u32)(u8)_opcode);
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 0xd:{
			u32 a = ((_pc+4) & ~3) + SL((u8)_opcode,2);

			RL(a,REG(8));
			res+=04;
			SH2LOGP(\x40[PC+0x%x]\x2cR%d,"MOV.LI",SL((u8)_opcode,2),Ri(8));
		}
		break;
		case 0xe:
			REG(8)=(u32)(s32)(s8)_opcode;
			SH2LOGP(#%d\x2cR%d,"MOV",(int)(s8)_opcode,Ri(8));
		break;
#if SHCORE == 4
		case 0xf:
			switch(_opcode & 0xf){
				default:
					SH2LOGE();
				break;
				case 0x000:
					if(REG_FPSCR & FP_PR){
						DREG(8) += DREG(4);
						SH2LOGP(DR%d\x2c DR%d,"FADD",Di(4),Di(8));
					}
					else{
						FREG(8) += FREG(4);
						SH2LOGP(FR%d\x2c FR%d,"FADD",Fi(4),Fi(8));
					}
				break;
				case 0x001:
					if(REG_FPSCR & FP_PR){
						DREG(8) -= DREG(4);
						SH2LOGP(DR%d\x2c DR%d,"FSUB",Di(4),Di(8));
					}
					else{
						FREG(8) -= FREG(4);
						SH2LOGP(FR%d\x2c FR%d,"FSUB",Fi(4),Fi(8));
					}
				break;
				case 0x002:
					if(REG_FPSCR & FP_PR){
						DREG(8) *= DREG(4);
						SH2LOGP(DR%d\x2c DR%d,"FMUL",Di(4),Di(8));
					}
					else{
						FREG(8) *= FREG(4);
						SH2LOGP(FR%d\x2c FR%d,"FMUL",Fi(4),Fi(8));
					}
				break;
				case 0x003:{
					if(REG_FPSCR & FP_PR){
						if(DREG(4) != 0)
							DREG(8) /= DREG(4);
						SH2LOGP(DR%d\x2c DR%d,"FDIV",Di(4),Di(8));
					}
					else{
						if(FREG(4) != 0)
							FREG(8) /= FREG(4);
						SH2LOGP(FR%d\x2c FR%d,"FDIV",Fi(4),Fi(8));
					}
				}
				break;
				case 0x004:{
					if(REG_FPSCR & FP_PR){
						if(DREG(8) == DREG(4))
							REG_SR |= SH_T;
						else
							REG_SR &= ~SH_T;
						SH2LOGP(DR%d\x2c DR%d,"FCMPEQ",Di(4),Di(8));
					}
					else{
						if(FREG(8) == FREG(4))
							REG_SR |= SH_T;
						else
							REG_SR &= ~SH_T;
						SH2LOGP(FR%d\x2c FR%d,"FCMPEQ",Fi(4),Fi(8));
					}
				}
				break;
				case 0x005:{
					if(REG_FPSCR & FP_PR){
						if(DREG(8) > DREG(4))
							REG_SR |= SH_T;
						else
							REG_SR &= ~SH_T;
						SH2LOGP(DR%d\x2c DR%d,"FCMPGT",Di(4),Di(8));
					}
					else{
						if(FREG(8) > FREG(4))
							REG_SR |= SH_T;
						else
							REG_SR &= ~SH_T;
						SH2LOGP(FR%d\x2c FR%d,"FCMPGT",Fi(4),Fi(8));
					}
				}
				break;
				case 0x006:{
					u32 b,a=REG_0+REG(4);
					if(REG_FPSCR & FP_SZ) EnterDebugMode();
					RL(a,b);
					PFREG(8)=b;
					SH2LOGP(\x40[R0+R%d]\x2c FR%d,"FMOV",Ri(4),Fi(8));
				}
				break;
				case 0x007:
					if(REG_FPSCR & FP_SZ) EnterDebugMode();
					WL(REG_0+REG(8),PFREG(4));
					SH2LOGP(FR%d\x2c\x40[R0+R%d],"FMOV",Fi(4),Ri(8));
				break;
				case 0x008:{
					u32 v;

					if(REG_FPSCR & FP_SZ) EnterDebugMode();
					RL(REG(4),v);
					PFREG(8) = v;
					SH2LOGP(\x40R%d\x2c FR%d,"FMOV",Ri(4),Fi(8));
				}
				break;
				case 0x009:{
					if((REG_FPSCR & FP_SZ)){
						u64 v;

						RQ(REG(4),v);
						REG(4)+=8;
						if(!(_opcode & 0x100)){
							PDREG(8) = v;
							SH2LOGP(\x40R%d+\x2c DR%d,"FMOV",Ri(4),Di(8));
						}
						else{
							PXDREG(8) = v;
							SH2LOGP(\x40R%d+\x2c XD%d,"FMOV",Ri(4),Di(8));
						}
					}
					else{
						u32 v;

						RL(REG(4),v);
						PFREG(8) = v;
						REG(4)+=4;
						SH2LOGP(\x40R%d+\x2c FR%d %x,"FMOV",Ri(4),Fi(8),v);
					}
				}
				break;
				case 0x00a:{
					u32 *p,v;

					if(REG_FPSCR & FP_SZ){
						WQ(REG(8),PDREG(4));
						SH2LOGP(DR%d\x2c\x40R%d,"FMOV",Di(4),Ri(8));
					}
					else{
						//v = *(u32 *)&FREG_[Fi(4)];
						WL(REG(8),PFREG(4));
						SH2LOGP(FR%d\x2c\x40R%d,"FMOV",Fi(4),Ri(8));
					}
				}
				break;
				case 0x00b:
					if(REG_FPSCR & FP_SZ){
						REG(8)-=8;
						if(_opcode & 0x10){
							WQ(REG(8),PXDREG(4));
							SH2LOGP(XD%d\x2c\x40-R%d,"FMOV",Di(4),Ri(8));
						}
						else{
							WQ(REG(8),PDREG(4));
							SH2LOGP(DR%d\x2c\x40-R%d,"FMOV",Di(4),Ri(8));
						}
					}
					else{
						REG(8)-=4;
						WL(REG(8),PFREG(4));
						SH2LOGP(FR%d\x2c\x40-R%d,"FMOV",Fi(4),Ri(8));
					}
				break;
				case 0x00c:
					if(!(REG_FPSCR & FP_SZ)){
						FREG(8)=FREG(4);
						SH2LOGP(FR%d\x2c FR%d,"FMOV",Fi(4),Fi(8));
					}
					else if(_opcode & 0x10){
						if(_opcode & 0x100){
							//xf(n)=xf(m)
							XDREG(8)=XDREG(4);
							SH2LOGP(XD%d\x2c XD%d,"FMOV",Di(4),Di(8));
						}
						else{
							//dr(n)=xd(m)
							DREG(8)=XDREG(4);
							SH2LOGP(XD%d\x2c DR%d,"FMOV",Di(4),Di(8));
						}
					}
					else{
						if(_opcode & 0x100){
							//xd(n)=dr(m)
							XDREG(8)=DREG(4);
							SH2LOGP(DR%d\x2c XD%d,"FMOV",Di(4),Di(8));
						}
						else{
							DREG(8)=DREG(4);
							SH2LOGP(DR%d\x2c DR%d,"FMOV",Di(4),Di(8));
						}
					}
				break;
				case 0x00d:
					switch(SR(_opcode,4)&0xf){
						case 0:
							FREG(8) = REG_FPUL;
							SH2LOGP(FPUL\x2c FR%d,"FSTS",Fi(8));
						break;
						case 1:
							REG_FPUL=FREG(8);
							SH2LOGP(FR%d\x2c FPUL,"FLDS",Fi(8));
						break;
						case 0x2:
							if(!(REG_FPSCR & FP_PR)){
								FREG(8)=(float)(s32)REG_FPUL;
								SH2LOGP(FPUL\x2c FR%d,"FLOAT",Fi(8));
							}
							else{
								DREG(8)=(double)(s32)REG_FPUL;
								SH2LOGP(FPUL\x2c DR%d,"FLOAT",Fi(8));
							}
						break;
						case 0x3:
							if(!(REG_FPSCR & FP_PR)){
								REG_FPUL=(s32)FREG(8);
								SH2LOGP(FR%d\x2c FPUL,"FTRC",Fi(8));
							}
							else{
								REG_FPUL=(s32)DREG(8);
								SH2LOGP(DR%d\x2c FPUL,"FTRC",Di(8));
							}
						break;
						case 0x4:
							if(!(REG_FPSCR & FP_PR)){
								PFREG(8)^=0x80000000;
								SH2LOGP(FR%d,"FNEG",Fi(8));
							}
							else{
								PDREG(8)^=0x8000000000000000;
								SH2LOGP(DR%d,"FNEG",Di(8));
							}
						break;
						case 0x5:
							if(!(REG_FPSCR & FP_PR)){
								PFREG(8)&=0x7fffffff;
								SH2LOGP(FR%d,"FABS",Fi(8));
							}
							else{
								*(u32 *)&FREG_[Fi(8)&0xe] &= 0x7fffffff;
								SH2LOGP(DR%d,"FABS",Di(8));
							}
						break;
						case 0x6:
							if(!(REG_FPSCR & FP_PR)){
								FREG(8)=sqrt(FREG(8));
								SH2LOGP(FR%d,"FSQRT",Fi(8));
							}
							else{
								DREG(8)=sqrt(DREG(8));
								SH2LOGP(DR%d,"FSQRT",Di(8));
							}
						break;
						case 0x7:
							FREG(8)=1.0f/sqrt(FREG(8));
							SH2LOGP(FR%d,"FSRRA",Fi(8));
						break;
						case 0x8:
							PFREG(8) = 0;
							SH2LOGP(FR%d,"FLD0",Fi(8));
						break;
						case 0x9:
							PFREG(8) = 0x3f800000;
							SH2LOGP(FR%d,"FLD1",Fi(8));
						break;
						case 0xa:
							DREG(8)=(double)(float)REG_FPUL;
							SH2LOGP(FPUL\x2c DR%d,"FCNVSD",Di(8));
						break;
						case 0xb:{
							REG_FPUL=(u32)(float)DREG(8);
							SH2LOGP(DR%d\x2c FPUL,"FCNVDS",Di(8));
						}
						break;
						case 0xe:{
							double f;
							u32 m,n= Fi(8);
							m=SL(n & 3,2);
							n &= 0xc;
							f=0;
							for(u32 i=0;i<4;i++)
								f+=FREG_[m+i]*FREG_[n+i];
							FREG_[n+3]=(float)f;
							SH2LOGP(FV%d\x2c FV%d,"FIPR",R_IDX(_opcode,8,3),R_IDX(_opcode,10,3));
						}
						break;
						case 0xf:
							switch(_opcode & 0x300){
								case 0x000:
								case 0x200:{
									float f = ((u16)REG_FPUL / 65536.0f) * 2.0f * 3.141592f;
									FREG(8)=sin(f);
									FREG_[Fi(8) + 1]=cos(f);
									SH2LOGP(FPUL\x2c FR%d,"FSCA",Fi(8));
								}
								break;
								case 0x100:{//ftrv
									u32 n;
									float sum[4];

									n = SR(_opcode & 0xC00,8);
									for (int i = 0;i < 4;i++) {
										sum[i] = 0;
										for (int j = 0;j < 4;j++)
											sum[i] += _xfregs[(j << 2) + i]*FREG_[n + j];
									}
									for (int i = 0;i < 4;i++)
										FREG_[n + i] = sum[i];
									SH2LOGP(XMTRX\x2c FV%d,"FTRV",SR(n,2));
								}
								break;
								default:
									switch(_opcode & 0xc00){
										case 0:
											REG_FPSCR ^= FP_SZ;
											Query(IMACHINE_QUERY_DEBUG_NOTIFY,0);
											SH2LOGP(NOARG,"FSCHG");
										break;
										case 0x800:
											REG_FPSCR ^= FP_FR;
											CHANGE_FPSCR();
											SH2LOGP(NOARG,"FRCHG");
										break;
										default:
											SH2LOGE();
										break;
									}
								break;
							}
						break;
						default:
							SH2LOGE();
						break;
					}
				break;
				case 0x00e:
					if(!(REG_FPSCR & FP_PR)){
						FREG(8) += FREG(0)*FREG(4);
						SH2LOGP(FR0\x2c FR%d\x2c FR%d,"FMAC",Fi(4),Fi(8));
					}
					else EnterDebugMode();
				break;
			}
		break;
#endif
		default:
			SH2LOGE();
		break;
	}
	_pc+=2;
Z1:
	if(_ppl.delay){
		if(--_ppl.delay==1){
			_pc=_ppl.pc;
			if((_ppl.pc & 0x00FFFFFF) < 0x1fff && !OnJump(_ppl.pc))
				_pc=REG_PR;
			SYNCPC(_pc);
		}
	}
	else {
		if(_vm._play < 2 && ++_vm._step == 0){
			_vm.pp += 8;
			_vm.invalidate(0);
		}
		else{
			_vm.p += 2;
			_vm._opcode += 2;
			_vm._rs += 2;
			_vm._rt += 2;
			//_vm._rd += 4;
			//_vm._regrt++;
			//_vm._regrs++;
		}
		if(_ppl.irq){
				machine->OnEvent(0,(LPVOID)-1);
				_ppl.irq=0;
		}
	}
	return res;
}

int SHCORECPU::Destroy(){
	//printf("Destroy %s\n",__FILE__);
	CCore::Destroy();
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int SHCORECPU::Reset(){
	int i;

	CCore::Reset();
	_cycles=0;
	_pc=0;
	_irq_pending=0;
	REG_SR=SH_I;
	REG_VBR=REG_GBR=0;
	REG_MACH=REG_MACL=0;
	_ppl._value=0;
	REG_PR=0;
	i=0x20;
#if SHCORE == 4
	_mmu.reset();
	i += 0x8 * 2 + 0x10*2;
#endif
	memset(_regs,0,i*sizeof(u32));
	return 0;
}

int SHCORECPU::Init(void *m,u32 ss,u32 f){
	u32 i,n;

	i=0x20;
	n=0;
#if SHCORE == 4
	i += 0x8 * 2 + 0x10*3;
#endif
	n += ((0x200)*sizeof(u32)) + (ss*sizeof(CoreMACallback)*2) + i*sizeof(u32);
	if(!(_regs = new u8[n]))
		return -1;
	_ioreg=(u8 *)&((RSZU *)_regs)[i];
	_portfnc_write = (CoreMACallback *)&_ioreg[0x200*sizeof(u32)];
	_portfnc_read = &_portfnc_write[ss];
#if SHCORE == 4
	_regs_bank[0]=&REG_[0x20];
	_regs_bank[1]=&_regs_bank[0][8];
	_frregs=_fpregs=(float *)&_regs_bank[1][8];
	_xfregs=&_fpregs[16];

	_mmu.init(_ioreg+ (MS_IO-1) - 0x10000);
#endif
	memset(_regs,0,n);
	_mem=(u8 *)m;
	return 0;
}

int SHCORECPU::SetIO_cb(u32 a,CoreMACallback w,CoreMACallback r){
	if(!_portfnc_write || !_portfnc_read) return -1;
	_portfnc_write[RMAP_IO(a)]=w;
	_portfnc_read[RMAP_IO(a)]=r;
	return 0;
}

int SHCORECPU::Disassemble(char *dest,u32 *padr){
	u32 op,adr;
	char c[200],cc[100];

	*((u64 *)c)=0;
	*((u64 *)cc)=0;

	adr = *padr;
//	RMAP_PC((adr&~3),p,R);

	sprintf(c,"%08X ",adr);
	op=_opcode;
	RWPC(adr & 0x1fffffff,_opcode);
	adr += 2;
	switch(SR(_opcode,12)){
		case 0:
			switch(_opcode & 0x3f){
				case 2:
					SH2LOGD(SR\x2cR%d,"STC",Ri(8));
				break;
				case 3:
					switch(_opcode & 0xc0){
#if SHCORE == 4
						case 0x80:
							SH2LOGD(R%d,"PREF",Ri(8));
						break;
						case 0xc0:
							SH2LOGD(R0\x2c\x40R%d,"MOVCA.L",Ri(8));
						break;
#endif
						case 0:
						SH2LOGD(R%d,"BSRF",Ri(8));
						break;

					}
				break;
				case 4:
				case 0x14:
				case 0x24:
				case 0x34:
					SH2LOGD(R%d\x2c\x40[R%d+R0],"MOV.BS",Ri(4),Ri(8));
				break;
				case 0x5:
				case 0x15:
				case 0x25:
				case 0x35:
					SH2LOGD(R%d\x2c\x40[R0+R%d],"MOV.WS0",Ri(4),Ri(8));
				break;
				case 0x6:
				case 0x16:
				case 0x26:
				case 0x36:
					SH2LOGD(R%d\x2c\x40[R0+R%d],"MOV.LS0",Ri(4),Ri(8));
				break;
				case 7:
				case 0x17:
				case 0x27:
				case 0x37:
					SH2LOGD(R%d\x2cR%d,"MULL",Ri(4),Ri(8));
				break;
				case 0x8:
					SH2LOGD(NOARG,"CLRT");
				break;
				case 0x9:
					SH2LOGD(NOARG,"NOP");
				break;
				case 0xb:
					SH2LOGD(NOARG,"RTS");
				break;
				case 0x12:
					SH2LOGD(GBR\x2cR%d,"STC",Ri(8));
				break;
#if SHCORE==4
				case 0x13:
					SH2LOGD(\x40R%d,"OCBI",Ri(8));
				break;
				case 0x33:
					SH2LOGD(\x40R%d,"OCBWb",Ri(8));
				break;
#endif
				case 0x18:
					SH2LOGD(NOARG,"SETT");
				break;
				case 0x19:
					SH2LOGD(NOARG,"DIV0U");
				break;
				case 0x1a:
					SH2LOGD(MACL\x2cR%d,"STS",Ri(8));
				break;
				case 0x22:
					SH2LOGD(VBR\x2cR%d,"STC",Ri(8));
				break;
				case 0xe:
				case 0x1e:
				case 0x2e:
				case 0x3e:
					SH2LOGD(\x40[R0+R%d]\x2cR%d,"MOV.LL0",Ri(4),Ri(8));
				break;
				case 0x23:
					switch(_opcode & 0xff){
						case 0x23:
							SH2LOGD(R%d,"BRAF",Ri(8));
						break;
#if SHCORE == 4
						case 0xa3:
							SH2LOGD(R%d,"OCBP",Ri(8));
						break;
#endif
						default:
							SH2LOGD(NOARG,"unkk 0x23");
						break;
					}

				break;
				case 0x29:
					SH2LOGD(R%d,"MOVT",Ri(8));
				break;
				case 0x2a:
					SH2LOGD(PR\x2cR%d,"STS",Ri(8));
				break;
				case 0x2b:
					SH2LOGD(NOARG,"RTE");
				break;
				case 0xc:
				case 0x1c:
				case 0x2c:
				case 0x3c:
					SH2LOGD(\x40[R0+R%d]\x2cR%d,"MOV.BL0",Ri(4),Ri(8));
				break;
				case 0xd:
				case 0x1d:
				case 0x2d:
				case 0x3d:
					SH2LOGD(\x40[R0+R%d]\x2cR%d,"MOV.WL0",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 1:
			SH2LOGD(R%d\x2c\x40[R%d+0x%x],"MOV.LS4",Ri(4),Ri(8),SL(_opcode&0xF,2));
		break;
		case 2:
			switch(_opcode & 0xf){
				case 0:
					SH2LOGD(R%d\x2c\x40R%d,"MOV.BS",Ri(4),Ri(8));
				break;
				case 1:
					SH2LOGD(R%d\x2c\x40R%d,"MOV.WS",Ri(4),Ri(8));
				break;
				case 2:
					SH2LOGD(R%d\x2c\x40R%d,"MOV.LS",Ri(4),Ri(8));
				break;
				case 4:
					SH2LOGD(R%d\x2c\x40-R%d,"MOV.BM",Ri(4),Ri(8));
				break;
				case 6:
					SH2LOGD(R%d\x2c\x40-R%d,"MOV.LM",Ri(4),Ri(8));
				break;
				case 7:
					SH2LOGD(R%d\x2cR%d,"DIV0S",Ri(4),Ri(8));
				break;
				case 8:
					SH2LOGD(R%d\x2cR%d,"TST",Ri(4),Ri(8));
				break;
				case 9:
					SH2LOGD(R%d\x2cR%d,"AND",Ri(4),Ri(8));
				break;
				case 0xa:
					SH2LOGD(R%d\x2cR%d,"XOR",Ri(4),Ri(8));
				break;
				case 0xb:
					SH2LOGD(R%d\x2cR%d,"OR",Ri(4),Ri(8));
				break;
				case 0xc:
					SH2LOGD(R%d\x2cR%d,"CMPSTR",Ri(4),Ri(8));
				break;
				case 0xd:
					SH2LOGD(R%d\x2cR%d,"XTRCT",Ri(4),Ri(8));
				break;
				case 0xe:
					SH2LOGD(R%d\x2cR%d,"MULU.W",Ri(4),Ri(8));
				break;
				case 0xf:
					SH2LOGD(R%d\x2cR%d,"MULS.W",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"");
				break;
			}
		break;
		case 3:
			switch(_opcode & 0xf){
				case 0:
					SH2LOGD(R%d\x2cR%d,"CMPEQ",Ri(4),Ri(8));
				break;
				case 0x2:
					SH2LOGD(R%d\x2cR%d,"CMPHS",Ri(4),Ri(8));
				break;
				case 0x3:
					SH2LOGD(R%d\x2cR%d,"CMPGE",Ri(4),Ri(8));
				break;
				case 4:
					SH2LOGD(R%d\x2cR%d,"DIV1",Ri(4),Ri(8));
				break;
				case 5:
					SH2LOGD(R%d\x2cR%d,"DMULU",Ri(4),Ri(8));
				break;
				case 6:
					SH2LOGD(R%d\x2cR%d,"CMPHI",Ri(4),Ri(8));
				break;
				case 7:
					SH2LOGD(R%d\x2cR%d,"CMPGT",Ri(4),Ri(8));
				break;
				case 8:
					SH2LOGD(R%d\x2cR%d,"SUB",Ri(4),Ri(8));
				break;
				case 0xa:
					SH2LOGD(R%d\x2cR%d,"SUBC",Ri(4),Ri(8));
				break;
				case 0xc:
					SH2LOGD(R%d\x2cR%d,"ADD",Ri(4),Ri(8));
				break;
				case 0xe:
					SH2LOGD(R%d\x2cR%d,"ADDC",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 4:
			switch((u8)_opcode){
				case 0:
					SH2LOGD(R%d,"SHLL",Ri(8));
				break;
				case 1:
					SH2LOGD(R%d,"SHLR",Ri(8));
				break;
				case 2: case 0x82: case 0x42: case 0xc2:
					SH2LOGD(MACH\x2c\x40-R%d,"STS.L",Ri(8));
				break;
#if SHCORE == 4
				case 3:
					SH2LOGD(SR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x0c: case 0x1c: case 0x2c: case 0x3c: case 0x8c: case 0x9c: case 0xac: case 0xbc:
				case 0x4c: case 0x5c: case 0x6c: case 0x7c: case 0xcc: case 0xdc: case 0xec: case 0xfc:
					SH2LOGD(R%d\x2c R%d,"SHAD",Ri(4),Ri(8));
				break;
				case 0x13: case 0x53:
					SH2LOGD(GBR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x1b: case 0x5b:
					SH2LOGD(\x40R%d,"TAS.B",Ri(8));
				break;
				case 0x23: case 0x63:
					SH2LOGD(VBR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x83: case 0x93: case 0xa3: case 0xb3:
				case 0xc3: case 0xd3: case 0xe3: case 0xf3:
					SH2LOGD(R%d_%d\x2c\x40-R%d,"STC.L",Ri(4)&7,REG_SR&SH_RB ? 0 : 1,Ri(8));
				break;
				case 0x87: case 0x97: case 0xa7: case 0xb7:
				case 0xc7: case 0xd7: case 0xe7: case 0xf7:
					SH2LOGD(\x40R%d+\x2cR%d_%d,"LDC.L",Ri(8),Ri(4)&7,REG_SR & SH_RB ? 0 : 1);
				break;
				case 0x27: case 0x67:
					SH2LOGD(\x40R%d+\x2c VBR,"LDC.L",Ri(8));
				break;
				case 0x37: case 0x77:
					SH2LOGD(\x40R%d+\x2c SSR,"LDC.L",Ri(8));
				break;
				case 0x32: case 0x72:
					SH2LOGD(SGR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x33: case 0x73:
					SH2LOGD(SSR\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x43:
					SH2LOGD(SPC\x2c\x40-R%d,"STC.L",Ri(8));
				break;
				case 0x47:
					SH2LOGD(\x40R%d+\x2cSPC,"LDC.L",Ri(8));
				break;
				case 0x5a:
					SH2LOGD(R%d\x2c FPUL,"LDS",Ri(8));
				break;
				case 0xb2:case 0xf2:
					SH2LOGD(DBR\x2c \x40-R%d,"STC.L",Ri(8));
				break;
				case 0xb6:case 0xf6:
					SH2LOGD(\x40R%d+\x2c DBR,"LDC.L",Ri(8));
				break;
#endif
				case 4:case 0x84:case 0x44:case 0xc4:
					SH2LOGD(R%d,"ROTL",Ri(8));
				break;
				case 6:case 0x86: case 0x46: case 0xc6:
					SH2LOGD(\x40R%d+\x2cMACH,"LDS.L",Ri(8));
				break;
				case 7:
					SH2LOGD(\x40R%d+\x2cSR,"LDC.L",Ri(8));
				break;
				case 8:
					SH2LOGD(R%d,"SHLL2",Ri(8));
				break;
				case 9:
					SH2LOGD(R%d,"SHRL2",Ri(8));
				break;
				case 0xa:case 0x8a: case 0x4a: case 0xca:
					SH2LOGD(R%d\x2cMACH,"LDS",Ri(8));
				break;
				case 0xb:case 0x8b: case 0x4b: case 0xcb:
					SH2LOGD(R%d,"JSR",Ri(8));
				break;
				case 0xd:case 0x8d:case 0x1d:case 0x9d:
				case 0x4d:case 0xcd:case 0x5d:case 0xdd:
				case 0x2d:case 0xad:case 0x3d:case 0xbd:
				case 0x6d:case 0xed:case 0x7d:case 0xfd:
					SH2LOGD(R%d\x2cR%d,"SHLD",Ri(4),Ri(8));
				break;
				case 0xe: case 0x8e: case 0x4e: case 0xce:
					SH2LOGD(R%d\x2cSR,"LDC",Ri(8));
				break;
				case 0x10:case 0x90: case 0x50: case 0xd0:
					SH2LOGD(R%d,"DT",Ri(8));
				break;
				case 0x11:case 0x91: case 0x51: case 0xd1:
					SH2LOGD(R%d\x2cPZ,"CMPPZ",Ri(8));
				break;
				case 0x12:case 0x92: case 0x52: case 0xd2:
					SH2LOGD(MACL\x2c\x40-R%d,"STS.L",Ri(8));
				break;
				case 0x15:case 0x95: case 0x45: case 0xd5:
					SH2LOGD(R%d,"CMPPL",Ri(8));
				break;
				case 0x16:case 0x96: case 0x56: case 0xd6:
					SH2LOGD(\x40R%d+\x2cMACL,"LDS.L",Ri(8));
				break;
				case 0x17: case 0x57:
					SH2LOGD(\x40R%d+\x2cGBR,"LDS.L",Ri(8));
				break;
				case 0x18:
					SH2LOGD(R%d,"SHLL8",Ri(8));
				break;
				case 0x19:
					SH2LOGD(R%d,"SHRL8",Ri(8));
				break;
				case 0x1a:case 0x9a: case 0xda:
					SH2LOGD(R%d\x2cMACL,"LDS",Ri(8));
				break;
				case 0x1e:case 0x9e: case 0x5e: case 0xde:
					SH2LOGD(R%d\x2cGBR,"LDC",Ri(8));
				break;
				case 0x21:case 0xa1: case 0x61: case 0xe1:
					SH2LOGD(R%d,"SHAR",Ri(8));
				break;
				case 0x22:
					SH2LOGD(PR\x2c-R%d,"STS.L",Ri(8));
				break;
				case 0x24:case 0xa4: case 0x64: case 0xe4:
					SH2LOGD(R%d,"ROTCL",Ri(8));
				break;
				case 0x25:case 0xa5: case 0x65: case 0xe5:
					SH2LOGD(R%d,"ROTCR",Ri(8));
				break;
				case 0x26:
					SH2LOGD(\x40R%d+\x2cPR,"LDS.L",Ri(8));
				break;
				case 0x28:
					SH2LOGD(R%d,"SHLL16",Ri(8));
				break;
				case 0x29:
					SH2LOGD(R%d,"SHRL16",Ri(8));
				break;
				case 0x2a:
					SH2LOGD(R%d\x2cPR,"LDS",Ri(8));
				break;
				case 0x2b:case 0xab: case 0x6b: case 0xeb:
					SH2LOGD(\x40R%d,"JMP",Ri(8));
				break;
				case 0x2e:case 0xae: case 0x6e: case 0xee:
					SH2LOGD(R%d\x2cVBR,"LDC",Ri(8));
				break;
				case 0x6a:
					SH2LOGD(R%d\x2c FPCSR,"LDS",Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 5:
			SH2LOGD(\x40[R%d+0x%x]\x2cR%d,"MOV.LL4",Ri(4),SL(_opcode&0xf,2),Ri(8));
		break;
		case 6:
			switch(_opcode & 0xf){
				case 0:
					SH2LOGD(\x40R%d\x2cR%d,"MOV.BL",Ri(4),Ri(8));
				break;
				case 1:
					SH2LOGD(\x40R%d\x2cR%d,"MOV.WL",Ri(4),Ri(8));
				break;
				case 2://(Rm)->Rn
					SH2LOGD(\x40R%d\x2cR%d,"MOV.LL",Ri(4),Ri(8));
				break;
				case 3:
					SH2LOGD(R%d\x2cR%d,"MOV",Ri(4),Ri(8));
				break;
				case 4:
					SH2LOGD(\x40R%d\x2cR%d,"MOV.BP",Ri(4),Ri(8));
					break;
				case 5:
					SH2LOGD(\x40R%d\x2cR%d,"MOV.WP",Ri(4),Ri(8));
					break;
				case 6:
					SH2LOGD(\x40R%d+\x2cR%d,"MOV.LP",Ri(4),Ri(8));
				break;
				case 7:
					SH2LOGD(R%d\x2cR%d,"NOT",Ri(4),Ri(8));
				break;
				case 8:
					SH2LOGD(R%d\x2cR%d,"SWAPW",Ri(4),Ri(8));
				break;
				case 9:
					SH2LOGD(R%d\x2cR%d,"SWAPW",Ri(4),Ri(8));
				break;
				case 0xa:
					SH2LOGD(R%d\x2cR%d,"NEGC",Ri(4),Ri(8));
				break;
				case 0xb:
					SH2LOGD(R%d\x2cR%d,"NEG",Ri(4),Ri(8));
				break;
				case 0xc:
					SH2LOGD(R%d\x2cR%d,"EXTU.B",Ri(4),Ri(8));
				break;
				case 0xd:
					SH2LOGD(R%d\x2cR%d,"EXTU.W",Ri(4),Ri(8));
				break;
				case 0xe:
					SH2LOGD(R%d\x2cR%d,"EXTS.B",Ri(4),Ri(8));
				break;
				case 0xf:
					SH2LOGD(R%d\x2cR%d,"EXTS.W",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 7:
			SH2LOGD(#%d\x2cR%d,"ADDI",(int)(s8)_opcode,Ri(8));
		break;
		case 8:
			switch(SR(_opcode,8)&0xf){
				case 0:
					SH2LOGD(\x40[R%d+%d]\x2cR0,"MOV.BS4",Ri(4),(_opcode&0xF));
				break;
				case 1:
					SH2LOGD(R0\x2c\x40[R%d+%d],"MOV.WS4",Ri(4),SL(_opcode&0xF,1));
				break;
				case 4:
					SH2LOGD(\x40[R%d+%d]\x2cR0,"MOV.BL4",Ri(4),(_opcode&0xF));
				break;
				case 5:
					SH2LOGD(\x40[R%d+%d]\x2cR0,"MOV.WL4",Ri(4),SL(_opcode&0xF,1));
				break;
				case 8:
					SH2LOGD(#%d\x2cR0,"CMPIM",(s32)(s8)_opcode);
				break;
				case 9:{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BT",a);
				}
				break;
				case 0xb:{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BF",a);
				}
				break;
				case 0xd:{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BTS",a);
				}
				break;
				case 0xf:{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BFS",a);
				}
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 9:
			SH2LOGD(\x40[PC+%d]\x2cR%d,"MOV.WI",SL((u8)_opcode,1),Ri(8));
		break;
		case 0xa:{
			s32 a = _opcode&0xFFF;
			if(a&0x800)
				a-=0x1000;
			SH2LOGD(%X,"BRA",adr+SL(a,1)+2);
		}
		break;
		case 0xb:{
				s32 a = _opcode&0xFFF;
				if(a&0x800)
					a-=0x1000;
				SH2LOGD(%X,"BSR",adr+2+SL(a,1));
			}
		break;
		case 0xC:
			switch(SR(_opcode,8)&0xf){
				case 2:
					SH2LOGE();
				break;
				case 4:
					//SH2LOG("MOVBLG");
				break;
				case 5:
					SH2LOGD(\x40[%d+gbr]\x2cR%d,"MOV.WLG",SL((u8)_opcode,1),0);
				break;
				case 7:
					SH2LOGD(\x40[PC+%d]\x2cR0,"MOVA",SL((u8)_opcode,2));
				break;
				case 8:
					SH2LOGD(#%d\x2cR0,"TSTI",(u8)_opcode);
				break;
				case 9:
					SH2LOGD(#%d\x2cR0,"ANDI",(u32)(u8)_opcode);
				break;
				case 0xa:
					SH2LOGD(#%d\x2cR0,"XORI",(u32)(u8)_opcode);
				break;
				case 0xb:
					SH2LOGD(#%d\x2cR0,"ORI",(u32)(u8)_opcode);
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 0xd:
			SH2LOGD(\x40[PC+0x%x]\x2cR%d,"MOV.LI",SL((u8)_opcode,2),Ri(8));
		break;
		case 0xe:
			SH2LOGD(#%d\x2cR%d,"MOV",(int)(s8)_opcode,Ri(8));
		break;
#if SHCORE == 4
		case 0xf:
			switch(_opcode & 0xf){
				default:
					SH2LOGD(NOARG,"unkk");
				break;
				case 0x000:
					SH2LOGD(FR%d\x2c FR%d,"FADD",Fi(4),Fi(8));
				break;
				case 0x001:
					SH2LOGD(FR%d\x2c FR%d,"FSUB",Fi(4),Fi(8));
				break;
				case 0x002:
					SH2LOGD(FR%d\x2c FR%d,"FMUL",Fi(4),Fi(8));
				break;
				case 0x003:
					SH2LOGD(FR%d\x2c FR%d,"FDIV",Fi(4),Fi(8));
				break;
				case 0x004:
					SH2LOGD(FR%d\x2c FR%d,"FCMPEQ",Fi(4),Fi(8));
				break;
				case 0x005:
					SH2LOGD(FR%d\x2c FR%d,"FCMPGT",Fi(4),Fi(8));
				break;
				case 0x006:
					SH2LOGD(\x40[R0+R%d]\x2c FR%d,"FMOV",Ri(4),Fi(8));
				break;
				case 0x007:
					SH2LOGD(FR%d\x2c\x40[R0+R%d],"FMOV",Fi(4),Ri(8));
				break;
				case 0x008:
					SH2LOGD(\x40R%d\x2c FR%d,"FMOV",Ri(4),Fi(8));
				break;
				case 0x009:
					if(REG_FPSCR & FP_SZ){
						SH2LOGD(\x40R%d+\x2c DR%d_%d,"FMOV",Ri(4),Di(8),0);
					}
					else{
						SH2LOGD(\x40R%d+\x2c FR%d_%d,"FMOV",Ri(4),Fi(8),0);
					}
				break;
				case 0x00a:
					if(REG_FPSCR & FP_SZ){
						SH2LOGD(FR%d\x2c\x40R%d,"FMOV",Fi(4),Ri(8));
					}
					else{
						SH2LOGD(FR%d\x2c\x40R%d,"FMOV",Fi(4),Ri(8));
					}
				break;
				case 0x00b:
					if(REG_FPSCR & FP_SZ){
						SH2LOGD(DR%d\x2c\x40-R%d,"FMOV",Di(4),Ri(8));
					}
					else{
						SH2LOGD(FR%d\x2c\x40-R%d,"FMOV",Fi(4),Ri(8));
					}
				break;
				case 0x00c:
					if(!(REG_FPSCR & FP_SZ)){
						SH2LOGD(FR%d\x2c FR%d,"FMOV",Fi(4),Fi(8));
					}
					else{
						SH2LOGD(DR%d\x2c DR%d,"FMOV",Di(4),Di(8));
					}
				break;
				case 0x00d:
					switch(SR(_opcode,4)&0xf){
						case 0:
							SH2LOGD(FPUL\x2c FR%d,"FSTS",Fi(8));
						break;
						case 0x2:
							SH2LOGD(FPUL\x2c FR%d,"FLOAT",Fi(8));
						break;
						case 0x3:
							if(!(REG_FPSCR & FP_PR)){
								SH2LOGD(FR%d\x2c FPUL,"FTRC",Fi(8));
							}
							else{
								SH2LOGD(DR%d\x2c FPUL,"FTRC",Di(8));
							}
						break;
						case 0x4:
							if(!(REG_FPSCR & FP_PR)){
								SH2LOGD(FR%d,"FNEG",Fi(8));
							}
							else{
								SH2LOGD(DR%d,"FNEG",Di(8));
							}
						break;
						case 0x5:
							if(!(REG_FPSCR & FP_PR)){
								SH2LOGD(FR%d,"FABS",Fi(8));
							}
							else{
								SH2LOGD(DR%d,"FABS",Di(8));
							}
						break;
						case 0x6:
							if(!(REG_FPSCR & FP_PR)){
								SH2LOGD(FR%d,"FSQRT",Fi(8));
							}
							else{
								SH2LOGD(DR%d,"FSQRT",Di(8));
							}
						break;
						case 0x7:
							SH2LOGD(FR%d,"FSRRA",Fi(8));
						break;
						case 0x8:
							SH2LOGD(FR%d,"FLD0",Fi(8));
						break;
						case 0x9:
							SH2LOGD(FR%d,"FLD1",Fi(8));
						break;
						case 0xe:
							SH2LOGD(FV%d\x2c FV%d,"FIPR",FVi(8),FVi(10));
						break;
						case 0xf:
							switch(_opcode & 0x300){
								case 0x000:
								case 0x200:{
									SH2LOGD(FPUL\x2c FR%d,"FSCA",Fi(8));
								}
								break;
								case 0x100://ftrv
									SH2LOGD(XMTRX\x2c FV%d,"FTRV",R_IDX(_opcode,10,3));
								break;
								default:
									switch(_opcode & 0xc00){
										case 0:
											SH2LOGD(NOARG,"FSCHG");
										break;
										case 0x800:
											SH2LOGD(NOARG,"FRCHG");
										break;
										default:
											SH2LOGD(NOARG,"unkk");
										break;
									}
								break;
							}
						break;
						default:
							SH2LOGD(NOARG,"unkk");
						break;
					}
				break;
				case 0x00e:
						SH2LOGD(FR0\x2c FR%d\x2c FR%d,"FMAC",Fi(4),Fi(8));
				break;
			}
		break;
#endif
		default:
			SH2LOGD(NOARG,"unkk");
		break;
	}
	sprintf(&c[8]," %6x ",_opcode);
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	*padr=adr;
	_opcode=op;
	return 0;
}

int SHCORECPU::_enterIRQ(int n,int v,u32 pc){
#if SHCORE == 4
	//printf("irq %u %x\n",n,REG_SR);
	if(REG_SR & SH_BL)
		return -1;
	if( ((REG_SR & 0xf0) >> 4) >= (15-n) )
		return -2;

	SH4REG(SH4_INTEVT)=0x200+SL((15-n),5);

	REG_SSR=REG_SR;
	REG_SPC=_pc;
	REG_SGR=REG_SP;
	_changeSR(REG_SR | SH_MD|SH_BL|SH_RB);
	_pc=REG_VBR + v;
#else
	REG_SP -= 4;
	WLPC(REG_SP, REG_SR);
	REG_SP -= 4;
	WLPC(REG_SP, _pc);
	RLPC(REG_VBR + v * 4,_pc);
	if (n > 15)
		BS(REG_SR,SH_I);
	else
		REG_SR = BS(BC(REG_SR,SH_I),SL(n,4));
#endif
	SYNCPC(_pc);
	LOGD("IRQ Enter %d %x\n",n,_pc);
	EnterDebugMode(DEBUG_BREAK_IRQ);
	return 0;
}

int SHCORECPU::_changeSR(u32 v){
#if SHCORE == 4
	if((v & SH_RB) != (REG_SR&SH_RB))
		_swap_register_bank(v&SH_RB ? 1 : 0);
#endif
	REG_SR=v;
	return 0;
}

int SHCORECPU::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_SET_LOCATION:{
			u32 *p=(u32 *)pv;
			void *__tmp;

			RMAP_(p[0],__tmp,W);
			if(!__tmp) return -2;
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
		case ICORE_QUERY_NEXT_STEP:{
				u32 *p=(u32 *)pv;
				switch(*p){
					default:
						*p=2;
						return 0;
					case 1:
						*p=4;
						return 0;
				}
			}
			return 0;
		case ICORE_QUERY_SET_REGISTER:
		{
			u32 v,*p=(u32 *)pv;
			v=p[0];
			if(v == (u32)-1){
				char *c;

				c=*((char **)&p[2]);
				if(!strcasecmp(c,"SR"))
					REG_SR=p[1];
				else
					return -1;
			}
			else
				REG_[v]=p[1];
		}
			return 0;
		case ICORE_QUERY_REGISTER:
			{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					if(!strcasecmp(c,"SR"))
						v=REG_SR;
					else
						return -1;
				}
				else
					v=REG_[v];
				p[1]=v;
			}
			return 0;
		default:
			return CCore::Query(what,pv);
	}
	return -1;
}

int SHCORECPU::_dumpRegisters(char *p){
	char cc[1024];
	int n;

	if(!p) return -1;

#if SHCORE == 4
	if(!(*p & 0x10)) goto A;
	float *fp;

	switch((*p&15)){
		default:
			fp=_frregs;
		break;
		case 2:
			fp=&_frregs[16];
		break;
	}
	sprintf(p,"FPCSR:%08X FR:%c SZ:%c PR:%c FPUL: %08X\n\n",REG_FPSCR,REG_FPSCR & FP_FR ? 49 : 48,
		REG_FPSCR&FP_SZ ? 49:48, REG_FPSCR&FP_PR ? 49:48,REG_FPUL);
	for(int i=0;i<16;i++){
		sprintf(cc,"f%02d %07e ",i,fp[i]);
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	strcat(p,"\n");
	for(int i=0;i<8;i++){
		sprintf(cc,"d%02d %09e ",i,((double *)fp)[i]);
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	return 0;
#endif
A:
	n=0;
	if((n=*p&15)) n=(n-1)*16;
	sprintf(p,"PC:%08X GBR:%08X PR:%08X MACL:%08X MACH:%08X\n\n",_pc,REG_GBR,REG_PR,REG_MACL,REG_MACH);
	for(int ii=0;ii<2;ii++){
		for(int i=0;i<8;i++){
			sprintf(cc,"r%02d:%08x ",i+(ii*8),REG_[i+(ii*8)+n]);
			strcat(p,cc);
			if((i&3)==3)
				strcat(p,"\n");
		}
		n=0;
	}
#if SHCORE == 4
	sprintf(cc,"\nVBR:%08X SPC:%08X SSR:%08X DBR:%08X\nSR:%08X T:%c IL:%d Q:%c M:%c FD:%c BL:%c RB:%c MD:%c\n",REG_VBR,REG_SPC,REG_SSR,REG_DBR,
	REG_SR,REG_SR&SH_T ? 49:48,SR(REG_SR&0xf0,4),REG_SR & SH_Q ? 49:48,REG_SR&SH_M ? 49:48,REG_SR&SH_FD ? 49:48,REG_SR&SH_BL ? 49:48,REG_SR&SH_RB ? 49:48,REG_SR&SH_MD ? 49:48);
#else
	sprintf(cc,"\nVBR: %08X SR: %08X T:%c IL:%d Q:%c M:%c IP:%08X\n",
		REG_VBR,REG_SR,REG_SR&SH_T ? 49:48,SR(REG_SR&0xf0,4),REG_SR&SH_Q ? 49:48,REG_SR&SH_M ? 49:48,_irq_pending);
#endif
	strcat(p,cc);
	return 0;
}

void SHCORECPU::_swap_register_bank(int bank){
	int i=(bank ^ 1) & 1;
	_sync_register_bank(i);
	memcpy(_regs,_regs_bank[bank],8*sizeof(RSZU));
}

void SHCORECPU::_sync_register_bank(int bank){
	memcpy(_regs_bank[bank],_regs,8*sizeof(RSZU));
}

void SHCORECPU::_swap_fpregister_bank(int){
}

int SHCORECPU::SaveState(IStreamer *p){
#ifdef _DEVELOP
	if(CCore::SaveState(p))
		return -1;
	p->Write(_ioreg,0x210*sizeof(u32),0);
	p->Write(&REG_SR,sizeof(u32),0);
	p->Write(&REG_PR,sizeof(u32),0);
	p->Write(&REG_GBR,sizeof(u32),0);
	p->Write(&REG_VBR,sizeof(u32),0);
	p->Write(&REG_MACL,sizeof(u32),0);
	p->Write(&REG_MACH,sizeof(u32),0);
	p->Write(&_ppl,sizeof(_ppl),0);
#endif
	return 0;
}

int SHCORECPU::LoadState(IStreamer *p){
#ifdef _DEVELOP
	if(CCore::LoadState(p))
		return -1;
	p->Read(_ioreg,0x210*sizeof(u32),0);
	p->Read(&REG_SR,sizeof(u32),0);
	p->Read(&REG_PR,sizeof(u32),0);
	p->Read(&REG_GBR,sizeof(u32),0);
	p->Read(&REG_VBR,sizeof(u32),0);
	p->Read(&REG_MACL,sizeof(u32),0);
	p->Read(&REG_MACH,sizeof(u32),0);
	p->Read(&_ppl,sizeof(_ppl),0);
#endif
	return 0;
}

SHCORECPU::__mmu::__mmu(){
	_regs=NULL;
	_itlb=NULL;
	_utlb=NULL;
}

int SHCORECPU::__mmu::reset(){
	return 0;
}

int SHCORECPU::__mmu::init(void *m){
	_regs=(u32 *)m;
	_itlb=&_regs[0x4000];
	_utlb=&_itlb[0x40];
	return 0;
}

int I_INLINE SHCORECPU::__mmu::translate(RSZU a,u32){
	//printf("mmu %x\n",a);

	if(a >= 0xe0000000)
		return a;
	a &= ~0xe0000000;
//	printf("mmu %x\n",a);
	return a;
}

int SHCORECPU::__mmu::write(u32 a,u32 *pv){
	return 0;
}

/*int I_INLINE SHCORECPU::__mmu::translate(RSZU a,u32){
	//printf("mmu %x\n",a);

	if(a >= 0xe4000000)
		return a;
	else if(a >= 0xe0000000){
		if(_regs[(SH4_MMUCR)] & 1){
			EnterDebugMode();
		}
		else{
			a &= 0x03FFFFE0;
			if(a&0x20)
				a = (a&~0x20)|(SL(_regs[SH4_QACR1]&0x1c,24) & 0x1e000000);
			else
				a |= SL(_regs[SH4_QACR0]&0x1c,24) & 0x1e000000;
		//	if(SR(a,16) < 0x80 || SR(a,16) > 0x140){printf("%x\n",a);
		//		EnterDebugMode();
		//	}
		}
		return a;
	}
	a &= ~0xe0000000;
//	printf("mmu %x\n",a);
	return a;

}
*/

};
