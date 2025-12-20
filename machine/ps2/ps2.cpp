#include "ps2.h"
#include "gui.h"
#include "elf.h"

extern GUI gui;

namespace ps2{

//#define __R5900F(a,...)
#define __R5900F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == (S_PAUSE|S_DEBUG_NEXT)) __F(a,## __VA_ARGS__);
#define __R5900D(a,...)	sprintf(cc,STR(%s) STR(\x20) STR(a),## __VA_ARGS__);
static u32 lino=0;

struct __vm {
	const int _slot=64;

	union{
		u16 _state;
		struct{
			unsigned int _invalidate:1;
			unsigned int _remap:1;
			unsigned int _step:1;
			unsigned int _play:3;
			unsigned int _loop:2;
			unsigned int _rjump:1;
			unsigned int _wsync:1;
		};
	};
	u8 *_data,*p,*pp,*_mem,*_opcode,*_rt,*_rs,_idx,*_rd,**_regrt,**_regrs;
	u32 _pc,_pcu,_pcd,_label,_branch;

	~__vm(){
		for(auto i=_routines.begin();i!=_routines.end();i++)
			delete (*i);
		_routines.clear();
		_jal.clear();
	};

	void invalidate(u32 f=1){
		_invalidate=1;
		_remap |= f & 1 ? 1 : 0;
		if(f & 2){
			_pcu=0;
			_pcd=0;
		}
		p=NULL;
		if(f&4)	_play=0;
	};

	void reset(){
		invalidate();
		_step=0;
		_idx=0;
		_pc=0;
		_pcu=_pcd=0;
		for(auto i=_routines.begin();i!=_routines.end();i++)
			delete (*i);
		_routines.clear();
		_jal.clear();
	};

	struct{
		u32 _adr,_pc,_type;
	} _slots[64];

	struct __routine{
		public:
		u8 *_buf,*_mem;
		u32 _start,_end,_count,_size;

		union{
			u32 _state;
			struct{
				u32 _empty:1;
				u32 _mode:3;
			};
		};

		__routine(u32 s=0,u32 e=0){
			_buf=NULL;
			_count=0;
			reset();
			_start=s;
			_end=e;
		};

		virtual ~__routine(){
			reset();
		};

		void reset(){
			if(_buf)
				delete []_buf;
			_buf=NULL;
			_size=0;
			_state=0;
			_empty=1;
		};
	} *_routine;

	vector<__routine *> _routines,_jal;

	int jr(u32 e){
		if(!_routine)
			return 1;
		if(_play==2){
			if(e>_routine->_start && e<=_routine->_end)
				_play=3;
			goto Z;
		}
		if(e < _routine->_start)
			return -1;

		if(e > _routine->_end){
			_routine->_mode=1;
			if(_routine->_end){
				printf("routine resize %08X %08x %08x\n",_routine->_start,_routine->_end,e);
				_routine->reset();
			}
			_routine->_end=e;
		}
		_routine=NULL;
		_label=_branch=0;
		_rjump=1;
	Z:
		if(_jal.size()){
			_routine=_jal.back();
			_jal.pop_back();
		}
		return 0;
	};

	int jal(u32 s,u32 e){
		int res;

		res=0;
		u32 n=0;
		for(auto i=_routines.begin();i!=_routines.end();i++,n++){
			if(s < (*i)->_start)
				break;
			if(s==(*i)->_start){
				__routine *r=(*i);
				res=1;
				if(r->_empty){
					if(r->_mode==3){
						u8 *mem=r->_mem;
						u32 nn,n,pc=r->_start;

						r->_size=((r->_end - r->_start)+15)& ~7;
						printf("ra %08x %08x %u %u",r->_start,r->_end,r->_size,r->_count);
						r->_buf=new u8[r->_size * 6];

						for(n=0;pc<=r->_end;pc+=8,n+=8){
							u64 d;

							d=*(u64 *)mem;
							mem+=8;
							*(u64 *)&r->_buf[n]=d;
							*(u64 *)&r->_buf[n+r->_size]= (d & 0xfc000000fc000000)>>26;
							*(u64 *)&r->_buf[n+r->_size*2]=(d & 0x03e0000003e00000)>>21;
							*(u64 *)&r->_buf[n+r->_size*3]=(d & 0x001f0000001f0000)>>16;
							*(u64 *)&r->_buf[n+r->_size*4]=(d & 0x0000f8000000f800)>>11;
						}
						printf("%u\n",n);
						r->_empty=0;
						r->_mode=4;
					}
					else if(r->_mode==1)
						r->_mode=2;
				}

				if(!r->_empty){
						//printf("enter %08x %08x Pc;%08X %u %u\n",s,r->_end,e,_play,r->_mode);
					//	if(_play)
					//		invalidate(1|2|4);
					_play=1;
					lino++;
					//if(r->_start==0x10c7f0) EnterDebugMode();
					//for(u32 n=0;n<p->_size;n+=4)
					//	printf("%08x %08x\n",n,*(u32 *)&p->_buf[n]);
				}
				else{
				//	printf("skip %08x %08x Pc;%08X %u %u\n",s,r->_end,e,_play,r->_mode);
					if(_play==2)
					invalidate(1|2|4);
				}
				goto A;
			}
		}

		_routines.push_back(new __routine(s));

		for (auto it = _routines.begin();it != _routines.end(); ++it){
			for (auto i = it+1;i != _routines.end(); ++i){
				if((*it)->_start > (*i)->_start) {
					__routine *b = *it;
					*it=*i;
					*i=b;
				}
			}
		}

		//for (auto it = _routines.begin();it != _routines.end(); ++it)
		//	printf("%x %u\n",(*it)->_start,(*it)->_count);
		A:
		if(_routine) _jal.push_back(_routine);

		_routine=_routines.at(n);
		++_routine->_count;
		_label=_branch=0;
		_rjump=1;
		return res;
	};
} _vm;

PS2M::PS2M() : Machine(MB(50)),PS2BIOS(){
	_freq=MHZ(294.912);
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

	REG_(REGI_A0)=_pc=d[IGAME_GET_INFO_SLOT+2];
	_vm.invalidate();
	//REG_SP=d[IGAME_GET_INFO_SLOT+4];
	///REG_GP=d[IGAME_GET_INFO_SLOT+3];
	Query(ICORE_QUERY_SET_FILENAME,fn);
	PS2BIOS::Load(0);
	res=0;
A:
	return res;
}

int PS2M::Destroy(){
	Machine::Destroy();
	GRE::Destroy();
	R5900Cpu::Destroy();
	PCMDAC::Destroy();
	return 0;
}

int PS2M::Reset(){
	lino=0;
	Machine::Reset();
	return PS2BIOS::Reset();
}

int PS2M::Init(){
	if(Machine::Init())
		return -1;
	if(PS2BIOS::Init(*this))
		return -2;
	//tim
	for(int i=0;i<4;i++){
		for(int ii=0;ii<4;ii++)
			SetIO_cb(0x10000000+SL(i,11)+SL(ii,4),(CoreMACallback)&PS2M::fn_timer_regs_w,0);
	}
	//gif
	for(int i=0;i<11;i++){
		SetIO_cb(0x10003000|SL(i,4),(CoreMACallback)&PS2M::fn_write_io,0);
	}
	//dmac
	for(int i=0;i<10;i++){
		for(int ii=0;ii<8;ii++)
			SetIO_cb(0x10008000+SL(ii,4)+SL(i,12),(CoreMACallback)&PS2M::fn_dma_regs_w,0);
	}
	for(int ii=0;ii<8;ii++){
		SetIO_cb(0x1000b400+SL(ii,4),(CoreMACallback)&PS2M::fn_dma_regs_w,0);
		SetIO_cb(0x1000c400+SL(ii,4),(CoreMACallback)&PS2M::fn_dma_regs_w,0);
		SetIO_cb(0x1000c800+SL(ii,4),(CoreMACallback)&PS2M::fn_dma_regs_w,0);
		SetIO_cb(0x1000d400+SL(ii,4),(CoreMACallback)&PS2M::fn_dma_regs_w,0);
	}
	for(int i=0;i<7;i++)
		SetIO_cb(0x1000E000|SL(i,4),(CoreMACallback)&PS2M::fn_dma_regs_w,0);

	SetIO_cb(0x1000F520,(CoreMACallback)&PS2M::fn_dma_regs_w,0);
	SetIO_cb(0x1000F590,(CoreMACallback)&PS2M::fn_dma_regs_w,0);

	SetIO_cb(0x12001000,(CoreMACallback)&PS2M::fn_write_io,0);
	SetIO_cb(0x120000A0,(CoreMACallback)&PS2M::fn_write_io,0);
	SetIO_cb(0x12000080,(CoreMACallback)&PS2M::fn_write_io,0);

	for(int i=0;i<16;i++)
		SetIO_cb(0x10006000|i,(CoreMACallback)&PS2M::fn_write_io,0);
	SetIO_cb(0x1000F000,(CoreMACallback)&PS2M::fn_write_io,0);
	SetIO_cb(0x1000F010,(CoreMACallback)&PS2M::fn_write_io,0);
	_max_frame=50;
	_setBlankArea(0,312,0,_width,50,_freq/2);
	AddTimerObj((ICpuTimerObj *)this,_scanline_cycles);
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
		default:
			if(_vm._wsync)
				Sleep();
		break;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int PS2M::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case ME_ENDFRAME:
			ps2gpu::Update(!OnFrame());
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
			ps2gpu::Update();
			Draw();
			CALLEE(Machine::OnEventI,ev,return,arg);
			return 0;
		case (u32)-2:
			return 0;
		case 0:
			if(!_irq_pending)
				return 0;
			for(ev=0;ev<32;ev++){
				if(BV(ev) & _irq_pending){
					goto W;
				}
			}
			return 1;
		break;
		default:
			if(BVT(ev,31))
				return -1;
			{
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				if(i == 1){
					BS(_irq_pending,BV(ev));
					return 1;
				}
			}
		break;
	}
W:
	switch(ev){
		default:
			BVC(_irq_pending,ev);
			OnException(0,ev);
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
		case ICORE_QUERY_ADDRESS_INFO:{
				LPMEMORYACCESS d =(LPMEMORYACCESS)pv;
				u32 adr=d->addr;
				switch(SR(adr,24)){
					case 0x0:
					case 0x2:
					case 0x3:
						d->addr=adr&0xff000000;
						d->size=MB(32);
						break;
					case 0x10: case 0x11: case 0x12:
						d->addr=0x10000000;
						d->size=KB(64);
					break;
					case 0x20:
						d->addr=0x20000000;
						d->size=MB(4);
						break;
					case 0x1f:	case 0x80:
						d->addr=0x80000000;
						d->size=MB(4);
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
				strcpy(p->title,"FPU");
				strcpy(p->name,"3101");
				p->type=1;
				//p->editable=1;
				//p->popup=1;
				p->clickable=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"VU0");
				strcpy(p->name,"3102");
				p->type=1;
				//p->editable=1;
				//p->popup=1;
				p->clickable=1;
				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"VU1");
				strcpy(p->name,"3103");
				p->type=1;
				//p->editable=1;
				//p->popup=1;
				p->clickable=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"IO");
				strcpy(p->name,"3105");
				p->type=1;
				p->popup=1;

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
	*p=0;
	((CCore *)cpu)->_dumpRegisters(p);
#ifdef _DEVELOP
	sprintf(cc,"C: %u L:%u SR:%8X EPC:%08X %u %u",__cycles,__line,CP0._regs[CP0.SR],CP0._regs[CP0.EPC],
		lino,_vm._play);
	strcat(p,cc);
#endif
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3101");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	*p=1;
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
	*p=2;
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
	adr=di._dumpAddress;
	pp=&cc[900];
	RMAP_(adr,mem,R);
	_dumpMemory(p,mem,&di);
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
	//strcat(p,cc);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;

	*pr = c;
	return res;
}

int PS2M::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	return 0;
}

int PS2M::OnJump(u32 pc){
	switch(pc&0x1fffffff){
		case 0x1000:
			return ReturnFromCall();
	}
	return -1;
}

int PS2M::OnException(u32 code,u32 b){
	_vm.invalidate(1|2|4);
	switch(CP0._regs[CP0.CAUSE]=code){
		case 0:
			if(_enterIRQ(b))
				_irq_pending |= b;
		break;
		case 0x20:
			//CP0._regs[CP0.EPC]=_pc;
			//CP0._regs[CP0.SR]=  (CP0._regs[CP0.SR] & ~0x3f)|SL(CP0._regs[CP0.SR]&0xf,2);
			_biosCall();
			//_pc=0x80000180-4;
			//EnterDebugMode();
		break;
	}
	return 0;
}

s32 PS2M::fn_write_io(u32 a,void *pmem,void *pdata,u32 f){
	//printf("io %x %x %x\n",a,*(u32 *)pdata,f);
	//EnterDebugMode();
	switch(SR(a,24)&0xf){
		case 2:
			switch((u16)a){
				case 0x80:
				case 0xA0:
					printf("GS %08X %016llx %08X %08X\n",a,*(u64 *)pdata,(u32)SR(*(u64 *)pdata,44)&0x7ff,_pc);
				break;
				case 0x1000:
					PS2IOREG(0x12001000) &= (~(*(u32 *)pdata));
					return 0;
			}
		break;
		case 0:
			switch(SR(a,12)&0xf){
				case 0:
				case 1:

				break;
				case 2:
				break;
				case 3:
				case 6:
					printf("GS io %x %x\n",a,*(u32 *)pdata);
				break;
				case 0xF:
					switch(SR(a,4)&15){
						case 0:
						case 1:
							printf("InTc io %x %x\n",a,*(u32 *)pdata);
						break;
					}
				break;
			}
		break;
	}
	return 1;
}

struct v128{
	union{
		struct{
			s64 lo,hi;
		};
		s64 v64[2];
		s32 v32[4];
		float f32[4];
		struct{
			float x,y,z,w;
		};
	};
	v128(){lo=hi=0;}
	v128(int a){v32[0]=a;v32[1]=v32[2]=v32[3]=0;};
	v128(float a){x=a;y=z=w=0;};
	v128(u128 a){v64[0]=a.lo;v64[1]=a.hi;};
	v128 &operator =(u128 &a){v64[0]=a.lo;v64[1]=a.hi;return *this;};
	v128 &operator =(int a){lo=0;hi=0;v32[0]=a;return *this;};
	v128 &operator =(float a){lo=0;hi=0;f32[0]=a;return *this;};
	operator u128() const{u128 a((u64)lo);a.hi=hi;return a;};
};

#define REGRS_ 		REG_(*_vm._rs)
//#define REGRS_ 		*(u64 *)*_vm._regrs
#define REGRT_ 		REG_(*_vm._rt)
//#define REGRT_ 		*(u64 *)*_vm._regrt
//#define REGRD_ 		REG_(RD(_opcode))
#define REGRD_ 		REG_(*_vm._rd)

#define REGI_VU0	144
#define REGV0_(a)	((v128 *)&REG_(REGI_VU0))[a]
#define REGV0A_		((v128 *)&REG_(REGI_VU0))[33]//acc
#define REGV0Q_		(((v128 *)&REG_(REGI_VU0))[34]).f32[0]//fdiv
#define REGV0P_		(((v128 *)&REG_(REGI_VU0))[34]).v32[1]//eu
#define REGV0I_		(((v128 *)&REG_(REGI_VU0))[34]).f32[2]//imm reg

#define REGI_VU0I	(REGI_VU0 + 35 * 2)
#define REGVU0I_(a)	((u32 *)&REG_(REGI_VU0I))[(a)]
#define REGVU0C_(a)	((u32 *)&REG_(REGI_VU0I))[(a)+32]

#define VS(a)		RD(a)
#define VT(a)		RT(a)
#define VD(a)		SA(a)

#define REGFCCR_(a)	((u32 *)&REG_(REGI_FPU_CR+(a)))[1]

#define ROL4(a) ((SL(a,3)&15)|SR(a,1))

static char prs[][3]={"zr","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7",
	"s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"};
static char pvrs[][5]={"","x","y","xy","z","xz","yz","xyz","w","xw","yw","xyw","zw","xzw","yzw","xyzw"};
static char pvrs0[][3]={"x","y","z","w"};

R5900Cpu::R5900Cpu() : CCore(){
	_regs=NULL;
	_ioreg=NULL;
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
	if(_regs){
		u32 n=sizeof(RSZU)*40;
#if MIPSCORE==R5900
		n=(n*2) + (32 * sizeof(u64))+(32*sizeof(u64));
		n += sizeof(v128)*36 + sizeof(u64)*32;
#endif
		memset(_regs,0,n);
		CP0._reset();
		REGV0_(0).w=1;
	}
	_jump=0;
	_irq_pending=0;
	_vm.reset();
	return 0;
}

int R5900Cpu::Init(void *m,u32 ss,u32 f){
	u32 n,nn;

	n=40*sizeof(RSZU);
#if MIPSCORE==R5900
	n=(n*2) + (32 * sizeof(u64))+(32*sizeof(u64));
	n += sizeof(v128)*36 + sizeof(u64)*32;
#endif
	nn = n+(ss*sizeof(CoreMACallback)*2)+KB(5);

	if(!(_regs = new u8[nn]))
		return -1;
	memset(_regs,0,nn);
	_vm._data=(u8 *)&((u8 *)_regs)[n];
	_portfnc_write = (CoreMACallback *)&_vm._data[KB(4)];
	//_portfnc_write = (CoreMACallback *)&((u8 *)_regs)[n];//&_vm._data[KB(1)];
	_portfnc_read = &_portfnc_write[ss];
	_mem=(u8 *)m;
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
	char cc[1024],s;
	int nr,mode;

	mode=*p;
	*p=0;
	nr = sizeof(RSZU)==8 ? 3 : 5;
	switch(mode & 0xf){
		case 1:
			sprintf(p,"FCR31;%08X FR:%d %d\n\n",REG_FCR31,IS_FR1 ? 1:0,REGFCCR_(0));
			for(int i=0;i<32;i++){
				sprintf(cc,"f%02d %07e ",i,REGDF0_(i));
				strcat(p,cc);
				if((i&3) == 3)
					strcat(p,"\n");
			}
			strcat(p,"\n");
		break;
		case 2:{
			sprintf(p,"vacc %07e %07e %07e %07e\nvq %07e vi %07e vp %08X\n\n",
				REGV0A_.x,REGV0A_.y,REGV0A_.z,REGV0A_.w
				,REGV0Q_,REGV0I_,REGV0P_);
			mode=mode&15;
			switch(SR(mode,4)&15){
				case 0:
					for(int i=0;i<32;i++){
						sprintf(cc,"v%02d %08X %08X %08X %08X\n",i,
							REGV0_(i).v32[0],REGV0_(i).v32[1],REGV0_(i).v32[2],REGV0_(i).v32[3]);
						strcat(p,cc);
					}
				break;
				default:
					for(int i=0;i<32;i++){
						sprintf(cc,"v%02d %07e %07e %07e %07e\n",i,REGV0_(i).x,REGV0_(i).y,REGV0_(i).z,REGV0_(i).w);
						strcat(p,cc);
					}
				break;
			}
			strcat(p,"\n\n");
			for(int i=0;i<32;i++){
				sprintf(cc,"vi%02d %04X ",i,REGVU0I_(i));
				strcat(p,cc);
				if((i&3) == 3)
					strcat(p,"\n");
			}
			strcat(p,"\n");
		}
		break;
		default:
			sprintf(p,"PC: %08X HI: %08X",_pc,(u32)REG_(REGI_HI));
			if(sizeof(RSZU)==8){
				sprintf(cc,":%08X",(u32)SR(REG_(REGI_HI),32));
				strcat(p,cc);
			}
			sprintf(cc," LO: %08X",(u32)REG_(REGI_LO));
			strcat(p,cc);
			if(sizeof(RSZU)==8){
				sprintf(cc,":%08X",(u32)SR(REG_(REGI_LO),32));
				strcat(p,cc);
			}
			strcat(p,"\n\n");
			for(int i=1,n=0;i<sizeof(prs)/sizeof(prs[0]);i++){
				sprintf(cc,"%s:%08x",prs[i],(u32)REG_(i));
				strcat(p,cc);
				if(sizeof(RSZU)==8){
					sprintf(cc,":%08X",(u32)SR(REG_(i),32));
					strcat(p,cc);
				}
				if(++n == nr){
					strcat(p,"\n");
					n=0;
				}
				else strcat(p," ");
				cc[0]=0;
			}
			//sprintf(cc,"\nVBR: %08X SR: %08X T:%c IL:%d Q:%c M:%c",_vbr,_sr,_sr&SH_T ? 49:48,SR(_sr&0xf0,4),_sr&SH_Q ? 49:48,_sr&SH_M ? 49:48);
			strcat(p,cc);
		break;
	}
	return 0;
}

int R5900Cpu::_exec(u32 status){
	int ret;

	ret=2;
A:
	if(_vm._invalidate){
		_vm._invalidate=0;
		_vm._step=0;
		if(_vm._play < 2)
			goto B;

		_vm._remap=0;

		_vm._opcode=_vm.p + _vm._routine->_size;
		_vm._rs=_vm._opcode + _vm._routine->_size;
		_vm._rt=_vm._rs + _vm._routine->_size;
		_vm._rd=_vm._rt + _vm._routine->_size;
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
					if(__address==0x12001000){
						_vm._wsync=1;
				//	EnterDebugMode();
					}
				}
/*				if(BT(status,S_PAUSE|S_DEBUG_NEXT) == (S_PAUSE|S_DEBUG_NEXT)){
					printf("EL %x %x %x %x %u %u\n",_pc,_vm._pcu,_vm._pcd,_vm._idx,
							((_pc - _vm._pcu)),lino);
				}
				*/

				_vm._idx=((_pc - _vm._pcu))>>3;
				_vm._step=SR((_pc - _vm._pcu) & 7,2);
			}
			else{
				RMAP_(_pc,_vm._mem,R);
				_vm.pp=_vm._mem;
				_vm._idx=0;
				_vm._loop=0;
				_vm._pc=_vm._pcu=_vm._pcd=_pc;
				_vm._pcd-=4;

				if(_vm._routine){
					switch(_vm._routine->_mode){
						case 2://nter record
							_vm._routine->_mode=3;
							_vm._label=0;
							_vm._routine->_mem=_vm._mem;
						break;
						case 3:
							if(_vm._label && !_vm._rjump){
								if(_vm._branch < _vm._routine->_start ||
									_vm._branch > _vm._routine->_end){
										_vm._routine->_mode=7;
								}
							}
						break;
					}
				}
			}
			_vm._branch=_vm._label=0;
			_vm._rjump=0;
		}

		if(!_vm._loop){
			u64 **p,d;
			__data=*(u64 *)_vm.pp;
			((u64 *)_vm._data)[_vm._idx] = __data;
			((u64 *)_vm._data)[_vm._idx+(_vm._slot)] = (__data & 0xfc000000fc000000)>>26;//opcode
			d=(__data & 0x03e0000003e00000)>>21;//rs
			((u64 *)_vm._data)[_vm._idx+(_vm._slot * 2)] = d;

			p=((u64 **)&(((u64 *)_vm._data)[(_vm._slot * 5)]));
			p[_vm._idx*2]=&REG_((u8)d);
			p[_vm._idx*2+1]=&REG_((u8)(d>>32));

			d=(__data & 0x001f0000001f0000)>>16;//rt
			((u64 *)_vm._data)[_vm._idx+(_vm._slot * 3)] = d;
			p+=_vm._slot*2;

			p[_vm._idx*2]=&REG_((u8)d);
			p[_vm._idx*2+1]=&REG_((u8)(d>>32));

			d=(__data & 0x0000f8000000f800)>>11;//rd
			((u64 *)_vm._data)[_vm._idx+(_vm._slot * 4)] = d;

			_vm._pcd += 8;
		}
		else if(_vm._loop==1){
			_vm._slots[_vm._idx]._pc=_pc;
			//if(__bus)
			//	EnterDebugMode();
		}

		_vm.p = _vm._data + (_vm._idx * 8) + (_vm._step*4);
		_vm._opcode=_vm.p + (_vm._slot)*8;
		_vm._rs=_vm._opcode + (_vm._slot)*8;
		_vm._rt=_vm._rs + (_vm._slot)*8;
		_vm._rd=_vm._rt + (_vm._slot)*8;
		_vm._regrs=&((u8 **)(_vm._data + (_vm._slot)*8*5))[_vm._idx*2+_vm._step];
		_vm._regrt=_vm._regrs+_vm._slot*2;

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
	_opcode=*(u32 *)_vm.p;
	/*{
		u32 __cazzo;
		RLPC(_pc,__cazzo);
		if(_opcode != __cazzo){
			printf("cazzo %08x %08x %08x %u %u %u\n",_pc,__cazzo,_opcode,_vm._idx,_vm._step,lino);
			for(u32 i=0,p=_vm._pcu;p<_vm._pcd;p+=4,i+=4)
				printf("%08X ",*(u32 *)&_vm._data[i]);
			printf("\n");
			EnterDebugMode();
		}
	/*	else{
			u32 __cazzo;
			RLPC(_vm._pcu,__cazzo);
			if(*(u32 *)_vm._data != __cazzo){
				printf("cazzo2 %08x %08x %08x %08x %08x %u %u %u\n",_pc,__cazzo,_opcode,_vm._pcu,_vm._pcd,
				_vm._idx,_vm._step,lino);

				for(u32 i=0,p=_vm._pcu;p<_vm._pcd;p+=4,i+=4)
					printf("%08X ",*(u32 *)&_vm._data[i]);
				printf("\n");
				EnterDebugMode();
			}
		}
	}*/
	//printf("%08X\n",_pc);

	switch(*_vm._opcode){
		default:
			printf("PC %08x %08x %x\n",_pc,(_opcode&0xfc000000),SR(_opcode,26));
			EnterDebugMode();
		break;
		case 0:
			switch(_opcode & 0x3f){
				case 0:
					if(_opcode){
						REGRD_ = (s32)SL((u32)REGRT_,POS(_opcode));
						__R5900F(%s\x2c%s\x2c$%x,"SLL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
					}
					else{
						__R5900F(NOARG,"NOP");
					}
				break;
				case 2:
					REGRD_ = (s32)SR((u32)REGRT_,POS(_opcode));
					__R5900F(%s\x2c%s\x2c$%x,"SRL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 3:
					REGRD_ = (s32)SR((s32)REGRT_,POS(_opcode));//fiixme
					__R5900F(%s\x2c%s\x2c$%x,"SRA",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 4:
					REGRD_ = (s32)SL((u32)REGRT_,REGRS_&0x1f);
					__R5900F(%s\x2c%s\x2c%s,"SLLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[*_vm._rs]);
				break;
				case 6:
					REGRD_ = (s32)SR((u32)REGRT_,REGRS_&0x1f);
					__R5900F(%s\x2c%s\x2c%s,"SRLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[*_vm._rs]);
				break;
				case 7:
					REGRD_ = SR((s32)REGRT_,REGRS_&0x1f);
					__R5900F(%s\x2c%s\x2c%s,"SRAV",prs[RD(_opcode)],prs[RT(_opcode)],prs[*_vm._rs]);
				break;
				case 0x8:
					ret+=3;
					_next_pc=REGRS_;
					_jump=2;
					LOADCALLSTACK(_next_pc);
					_vm.jr(_pc+4);
					__R5900F(%s,"JR",prs[RS(_opcode)]);
				break;
				case 0x9:
					REG_RA=_pc+8;
					_next_pc=REGRS_;
					_jump=2;
					ret += 3;
					STORECALLLSTACK(REG_RA);
					//_vm.jal(_next_pc,_pc);
					if(_vm._play==2)
						_vm.invalidate(1|2|4);
					__R5900F(%s,"JALR",prs[RS(_opcode)]);
				break;
#if MIPS >= 3
				case 0xa:
					if(!REGRT_)
						REGRD_ = REGRS_;
					__R5900F(%s\x2c%s\x2c%s,"MOVZ",prs[RD(_opcode)],prs[RT(_opcode)],prs[*_vm._rs]);
				break;
				case 0xb:
					if(REGRT_)
						REGRD_ = REGRS_;
					__R5900F(%s\x2c%s\x2c%s,"MOVN",prs[RD(_opcode)],prs[RT(_opcode)],prs[*_vm._rs]);
				break;
				case 0xf:
					__R5900F(,"SYNC");
				break;
#endif
				case 0xc:
					__R5900F(%x,"SYSCALL",SR(_opcode,6)&0xFFFFF);
					OnException(0x20,SR(_opcode,6)&0xFFFFF);
				break;
				case 0xd:
					EnterDebugMode();
					__R5900F(NOARG,"BREAK");
				break;
				case 0x10:
					REGRD_ = REG_HI;
					__R5900F(%s,"MFHI",prs[RD(_opcode)]);
				break;
				case 0x11:
					REG_LO=REGRD_;
					__R5900F(%s,"MTLO",prs[RD(_opcode)]);
				break;
				case 0x12:
					REGRD_ = REG_LO;
					__R5900F(%s,"MFLO",prs[RD(_opcode)]);
				break;
				case 0x13:
					REG_HI=REGRD_;
					__R5900F(%s,"MTHI",prs[RD(_opcode)]);
				break;
				case 0x18:{
					u64 value;

					value = (s64)(s32)REGRS_*(s64)(s32)REGRT_;
					REG_LO=(s32)value;
					REG_HI=(s32)(value >> 32);
					ret += 2;
#if MIPSCORE==R5900
					if(RD(_opcode)/* &&  RT(_opcode) != RD(_opcode)*/)
						REGRD_=REG_LO;
					__R5900F(%s\x2c %s\x2c%s,"MULT",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
#else
					__R5900F(%s\x2c%s,"MULT",prs[RS(_opcode)],prs[RT(_opcode)]);
#endif
				}
				break;
				case 0x19:{
					u64 value;

					value = (u64)(u32)REGRS_*(u64)(u32)REGRT_;
					REG_LO=(u32)value;
					REG_HI=(u32)(value >> 32);
					ret += 2;
#if MIPSCORE==R5900
					if(RD(_opcode))
						REGRD_=REG_LO;
#endif
					__R5900F(%s\x2c%s,"MULTU",prs[*_vm._rs],prs[RT(_opcode)]);
				}
				break;
				case 0x1a:{
					RSZS v0,v1,v2;

					v1=REGRT_;
					if(v1){
						v2 = (v0=(RSZS)REGRS_) / v1;
						REG_LO=(u32)v2;
						REG_HI=(u32)(v0-(v2*v1));
					}
					__R5900F(%s\x2c%s,"DIV",prs[*_vm._rs],prs[RT(_opcode)]);
				}
				break;
				case 0x1b:{
					u32 v0,v1,v2;

					v1=REGRT_;
					if(v1){
						v2 = (v0=REGRS_) / v1;
						REG_LO=(u32)v2;
						REG_HI=(u32)(v0-(v2*v1));
					}
					__R5900F(%s\x2c%s,"DIVU",prs[*_vm._rs],prs[RT(_opcode)]);
				}
				break;
				case 0x20://'fixme add oveflow xcepion
					REGRD_ = (s32)((u32)REGRS_+(u32)REGRT_);
					__R5900F(%s\x2c%s\x2c%s,"ADD",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x21:
					REGRD_ = (s32)((u32)REGRS_+ (u32)REGRT_);
					__R5900F(%s\x2c%s\x2c%s,"ADDU",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x22:
					REGRD_ = (s32)((u32)REGRS_ - (u32)REGRT_);
					__R5900F(%s\x2c%s\x2c%s,"SUB",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x23:
					REGRD_ = (s32)((u32)REGRS_ - (u32)REGRT_);
					__R5900F(%s\x2c%s\x2c%s,"SUBU",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x24:
					REGRD_ = REGRS_ & REGRT_;
					__R5900F(%s\x2c%s\x2c%s,"AND",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x25:
					REGRD_ = REGRS_|REGRT_;
					__R5900F(%s\x2c%s\x2c%s,"OR",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x26:
					REGRD_ = REGRS_^REGRT_;
					__R5900F(%s\x2c%s\x2c%s,"XOR",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x27:
					REGRD_ = ~(REGRS_|REGRT_);
					__R5900F(%s\x2c%s\x2c%s,"NOR",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x2a:
					REGRD_ = (RSZS)REGRS_ < (RSZS)REGRT_;
					__R5900F(%s\x2c%s\x2c%s,"SLT",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x2b:
					REGRD_ = REGRS_ < REGRT_;
					__R5900F(%s\x2c%s\x2c%s,"SLTU",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
#if MIPS >= 3
				case 0x16:
					REGRD_ = REGRT_ >> (REGRS_ & 0x3f);
					__R5900F(%s\x2c%s\x2c#%s,"DSRLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[*_vm._rs]);
				break;
				case 0x2d:
					REGRD_ = REGRS_ + REGRT_;
					__R5900F(%s\x2c%s\x2c%s,"DADDU",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x2f:
					REGRD_ = REGRS_ - REGRT_;
					__R5900F(%s\x2c%s\x2c%s,"DSUBU",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				break;
				case 0x34:
					if(REGRS_== REGRT_)
						OnException(0x34,SA(_opcode));
					__R5900F(%s\x2c%s\x2c%x,"TEQ",prs[*_vm._rs],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x38:
					REGRD_ = REGRT_ << (SA(_opcode));
					__R5900F(%s\x2c%s\x2c#%x,"DSLL",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3a:
					REGRD_ = REGRT_ >> (SA(_opcode));
					__R5900F(%s\x2c%s\x2c#%x,"DSRL",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3c:
					REGRD_ = REGRT_ << (32+SA(_opcode));
					__R5900F(%s\x2c%s\x2c#%x,"DSLL32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3e:
					REGRD_ = REGRT_ >> (32+SA(_opcode));
					__R5900F(%s\x2c%s\x2c#%x,"DSRL32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3f:
					REGRD_ = (s64)REGRT_ >> (32+SA(_opcode));
					__R5900F(%s\x2c%s\x2c#%x,"DSRA32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
#endif
				default:
					printf("0 %x PC:%08X\n",_opcode & 0x3f,_pc);
					EnterDebugMode();
				break;
			}
		break;
		case 0x1:
			switch(FT(_opcode)){
				case 0:
					if((RSZS)REGRS_ < 0){
						_next_pc=_pc+SL(IMM(_opcode),2)+4;
						_jump=2;
						ret += 3;
					}
					else if(_vm._loop)
						_vm.invalidate(2|1);
					__R5900F(%s\x2c$%x,"BLTZ",prs[*_vm._rs],_pc+(IMM(_opcode)<<2));
				break;
				case 1:
					if((RSZS)REGRS_ >=0){
						_next_pc=_pc+SL(IMM(_opcode),2)+4;
						_jump=2;
						ret += 3;
					}
					else if(_vm._loop)
						_vm.invalidate(2|1);
					__R5900F(%s\x2c$%x,"BGEZ",prs[*_vm._rs],_pc+(IMM(_opcode)<<2));
				break;
				case 0x11:
					if((RSZS)REGRS_ >=0){
						REG_RA=_pc+8;
						_next_pc=_pc+SL(IMM(_opcode),2)+4;
						_jump=2;
						ret += 3;
						STORECALLLSTACK(REG_RA);
					}
					else if(_vm._loop)
						_vm.invalidate(2|1);
					__R5900F(%s\x2c$%x,"BGEZAL",prs[*_vm._rs],_pc+(IMM(_opcode)<<2));
				break;
				default:
					printf("1 %x %08x\n",FT(_opcode),_pc);
					EnterDebugMode();
				break;
			}
		break;
		case 0x2:
			_next_pc=JUMP(_pc,_opcode);
			_jump=2;
			ret += 3;
			__R5900F(%X,"J",_next_pc);
		break;
		case 0x3:
			REG_RA=_pc+8;
			_next_pc=JUMP(_pc,_opcode);
			_jump=2;
			STORECALLLSTACK(REG_RA);
			ret+=3;
			//_vm.jal(_next_pc,REG_RA);
			__R5900F(%X,"JAL",_next_pc);
		break;
		case 0x4:
			if(REGRT_== REGRS_){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			else if(_vm._loop)
				_vm.invalidate(2|1);
			__R5900F(%s\x2c%s\x2c$%x,"BEQ",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x5:
			if(REGRT_ != REGRS_){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			else if(_vm._loop)
				_vm.invalidate(2|1);
			__R5900F(%s\x2c%s\x2c$%x,"BNE",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x6:
			if((RSZS)REGRS_<=0){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret += 3;
			}
			else if(_vm._loop)
				_vm.invalidate(2|1);
			__R5900F(%s\x2c$%x,"BLEZ",prs[*_vm._rs],_pc+(IMM(_opcode)<<2));
		break;
		case 0x7:
			if((RSZS)REGRS_>0){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret += 3;
			}
			else if(_vm._loop)
				_vm.invalidate(2|1);
			__R5900F(%s\x2c$%x,"BGTZ",prs[*_vm._rs],_pc+(IMM(_opcode)<<2));
		break;
		case 0x8:
			REGRT_ = (s32)((u32)REGRS_+IMM(_opcode));
			__R5900F(%s\x2c%s\x2c$%x,"ADDI",prs[RT(_opcode)],prs[*_vm._rs],IMM(_opcode));
		break;
		case 0x9:
			REGRT_ = (s32)((u32)REGRS_+IMM(_opcode));
			__R5900F(%s\x2c%s\x2c$%x,"ADDIU",prs[RT(_opcode)],prs[*_vm._rs],(u16)IMM(_opcode));
		break;
		case 0xa:
			REGRT_ = (RSZS)REGRS_ < IMM(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"SLTI",prs[RT(_opcode)],prs[*_vm._rs],IMM(_opcode));
		break;
		case 0xb:
			REGRT_ = REGRS_ < (RSZU)IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"SLTIU",prs[RT(_opcode)],prs[*_vm._rs],(u32)IMMU(_opcode));
		break;
		case 0xc:
			REGRT_ = REGRS_ & IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"ANDI",prs[RT(_opcode)],prs[*_vm._rs],(u32)IMMU(_opcode));
		break;
		case 0xd:
			REGRT_ = REGRS_|IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"ORI",prs[RT(_opcode)],prs[*_vm._rs],(u32)IMMU(_opcode));
		break;
		case 0xe:
			REGRT_ = REGRS_^IMMU(_opcode);
			__R5900F(%s\x2c%s\x2c$%x,"XORI",prs[RT(_opcode)],prs[*_vm._rs],(u32)IMMU(_opcode));
		break;
		case 0xf:
			REGRT_ = IMM(_opcode) << 16;
			__R5900F(%s\x2c$%X,"LUI",prs[RT(_opcode)],(u16)IMM(_opcode));
		break;
#if MIPS>=3
		case 0x10:
			switch(*_vm._rs){
				case 0:
					OnCop(SR(_opcode,26)&3,0,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"MFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 2:
					OnCop(SR(_opcode,26)&3,2,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"CFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 4:
					OnCop(SR(_opcode,26)&3,4,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"MTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 6:
					OnCop(SR(_opcode,26)&3,6,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"CTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 1:
				case 3:
				case 5:
					printf("10 %x PC:%08X\n",*_vm._rs,_pc);
					EnterDebugMode();
				break;
				case 0x8:
					switch(RT(_opcode)){
						case 0:
							__R5900F($%x,"BC0F",_pc+(IMM(_opcode)<<2));
						break;
						case 1:
							__R5900F($%x,"BC0T",_pc+(IMM(_opcode)<<2));
						break;
						default:
							EnterDebugMode();
							printf("cop0 bc %x %08x\n",RT(_opcode),_pc);
						break;
					}
				break;
				default:
					switch(_opcode & 0x3f){
						default:
							OnCop(SR(_opcode,26)&3,1,_opcode & 0xFFFFFF,0);
							__R5900F(%d %x,"COP",SR(_opcode,26)&3,_opcode & 0xFFFFFF);
						break;
						case 0x18:
							EnterDebugMode();
				/*		if(!ReturnFromCall()){
							_pc=REG_RA;

						}
						goto Z1;*/
						break;
					}
				break;
			}
		break;
		case 0x11:
			switch(*_vm._rs){
				case 0:
					if(IS_FR1)
						REGRT_=REG_((FS(_opcode) + REGI_FPU));
					else
						REGRT_=(u32)REGDL0_(FS(_opcode));
					__R5900F(%s\x2c $f%d,"MFC1",prs[RT(_opcode)],FS(_opcode));
				break;
				case 4://1016e4
					if(IS_FR1)
						REG_((FS(_opcode) + REGI_FPU)) = REGRT_;
					else
						REGDL0_(FS(_opcode)) = (u32)REGRT_;
					__R5900F(%s\x2c $f%d,"MTC1",prs[RT(_opcode)],FS(_opcode));
				break;
				case 0x8:
					switch(RT(_opcode)){
						case 0:
							if(REGFCCR_(0)==0){
								_next_pc=_pc+SL(IMM(_opcode),2)+4;
								_jump=2;
								ret+=2;
							}
							else if(_vm._loop)
								_vm.invalidate(2|1);
							__R5900F($%x,"BC1F",_pc+(IMM(_opcode)<<2));
						break;
						case 1:
							if(REGFCCR_(0)){
								_next_pc=_pc+SL(IMM(_opcode),2)+4;
								_jump=2;
								ret+=2;
							}
							else if(_vm._loop)
								_vm.invalidate(2|1);
							__R5900F($%x,"BC1T",_pc+(IMM(_opcode)<<2));
						break;
						default:
							EnterDebugMode();
							printf("cop1 bc %x %08x\n",RT(_opcode),_pc);
						break;
					}
				break;
				default:
					switch(_opcode & 0x3f){
						case 0x0://add
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									REGDF0_(FD(_opcode)) = REGDF0_(FS(_opcode)) + REGDF0_(FT(_opcode));
								}
								else{

								}
							}
							__R5900F($f%d\x2c $f%d\x2c $f%d,"ADD",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x1://sub
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									REGDF0_(FD(_opcode))=REGDF0_(FS(_opcode))-REGDF0_(FT(_opcode));
								}
								else{

								}
							}
							__R5900F($f%d\x2c $f%d\x2c $f%d,"SUB",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x2://mul
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									REGDF0_(FD(_opcode))=REGDF0_(FS(_opcode))*REGDF0_(FT(_opcode));
								}
								else{

								}
							}
							__R5900F($f%d\x2c $f%d\x2c $f%d,"MUL",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x3://div
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									REGDF0_(FD(_opcode))=REGDF0_(FS(_opcode))/ REGDF0_(FT(_opcode));
								}
								else{

								}
							}
							__R5900F($f%d\x2c $f%d\x2c $f%d,"DIV",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x6:
							if(IS_SINGLE(_opcode))
								REGDF0_(FD(_opcode))=REGDF0_(FS(_opcode));
							else
								REGDD0_(FD(_opcode)) = REGDD0_(FS(_opcode));
							__R5900F(%c $f%d\x2c $f%d,"MOV",IS_SINGLE(_opcode)?'S':'D',FD(_opcode),FS(_opcode));
						break;
						case 0x7:
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									REGDF0_(FD(_opcode)) = -REGDF0_(FS(_opcode));
								}
								else{
									REGDD0_(FD(_opcode)) = -REGDD0_(FS(_opcode));
								}
							}
							__R5900F($f%d\x2c $f%d,"NEG",FD(_opcode),FS(_opcode));
						break;
						case 0x20://cvt.s
							if(IS_FR1){

							}
							else{
								if(BVT(_opcode,23)){
									if(IS_SINGLE(_opcode)){//single
										REGDF0_(FD(_opcode)) = REGDL0_(FS(_opcode));
									}
									else
										REGDF0_(FD(_opcode)) = REGDQ0_(FS(_opcode));
								}
								else
									REGDF0_(FD(_opcode))=REGDD0_(FS(_opcode));
							}
							__R5900F($f%d\x2c $f%d,"CVT.S",FD(_opcode),FS(_opcode));
						break;
						case 0x24://cvt.w.s
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode))
									REGDL0_(FD(_opcode))=(s32)REGDF0_(FS(_opcode));
							}
							__R5900F($f%d\x2c $f%d,"CVT.W",FD(_opcode),FS(_opcode));
						break;
						case 0x32:
						case 0x3a:
							REGFCCR_(0)=0;
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									if(REGDF0_(FS(_opcode)) == REGDF0_(FT(_opcode))){
										REGFCCR_(0)=1;
									}
									else{
										if(REGDD0_(FS(_opcode)) == REGDD0_(FT(_opcode))){
											REGFCCR_(0)=1;
										}
									}
								}
							}
							__R5900F($f%d\x2c $f%d,"C.EQ",FS(_opcode),FT(_opcode));
						break;
						case 0x34:
						case 0x3c:
							REGFCCR_(0)=0;
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									if(REGDF0_(FS(_opcode)) < REGDF0_(FT(_opcode))){
										REGFCCR_(0)=1;
									}
								}
								else{
									if(REGDD0_(FS(_opcode)) < REGDD0_(FT(_opcode))){
										REGFCCR_(0)=1;
									}
								}
							}
							__R5900F($f%d\x2c $f%d,"C.LT",FS(_opcode),FT(_opcode));
						break;
						case 0x36:
						case 0x3e:
							REGFCCR_(0)=0;
							if(IS_FR1){

							}
							else{
								if(IS_SINGLE(_opcode)){
									if(REGDF0_(FS(_opcode))<=REGDF0_(FT(_opcode))){
										REGFCCR_(0)=1;
									}
								}
								else{
									if(REGDD0_(FS(_opcode))<=REGDD0_(FT(_opcode))){
										REGFCCR_(0)=1;
									}
								}
							}
							__R5900F($f%d\x2c $f%d,"C.LE",FS(_opcode),FT(_opcode));
						break;
						default:
							printf("cop1 %x %x %08x %08X\n",SR(_opcode,21) &0x1f,_opcode & 0x3f,_opcode,_pc);
							EnterDebugMode();
						break;
					}
				break;
			}
		break;
		case 0x12:
			switch(*_vm._rs){
				case 0x02:  /* CFCz */
					REGRT_ = (u32)REGVU0C_(FS(_opcode));
					__R5900F(%s\x2c $vi%d,"CFC2",prs[RT(_opcode)],FS(_opcode));
				break;
				case 0x00:  /* MFCz */
				case 0x01:  /* DMFCz */
				case 0x04:  /* MTCz */
				case 0x05:  /* DMTCz */
				case 0x06:  /* CTCz */
					EnterDebugMode();
					printf("cop2 %x %x\n",*_vm._rs,_pc);
				break;
				case 0x08:  /* BC */
					switch (*_vm._rt){
						case 0x00:  /* BCzF */
						case 0x01:  /* BCzT */
						case 0x02:  /* BCzFL */
						case 0x03:  /* BCzTL */
							EnterDebugMode();
							printf("cop2 8 %x %x\n",RT(_opcode),_pc);
						break;
					}
					break;
				default:
					switch (_opcode & 0x3f){
						case 0x08: case 0x09: case 0x0a: case 0x0b:{
								const u32 bc = _opcode & 3;
								float *fs = REGV0_(VS(_opcode)).f32;
								float *ft = REGV0_(VT(_opcode)).f32;
								float *fd = REGV0_(VD(_opcode)).f32;
								for (int field = 0; field < 4; field++){
									if (BVT(_opcode, 24-field))
									{
										fd[field] = REGV0A_.f32[field] + fs[field] * ft[bc];
									}
								}
							}
							__R5900F(%s $vf%d\x2c $vf%d\x2c $vf%d.%s,"VMADD",pvrs[ROL4(SR(_opcode,21) & 15)],
								FD(_opcode),FS(_opcode),FT(_opcode),pvrs0[_opcode&3]);
						break;
						case 0x1c:{
								float *fs = REGV0_(VS(_opcode)).f32;
								float *fd = REGV0_(VD(_opcode)).f32;
								for (int field = 0; field < 4; field++){
									if (BVT(_opcode, 24-field))
									{
										fd[field] =  REGV0Q_ * fs[field];
									}
								}
							}
							__R5900F(%s $vf%d\x2c $vf%d,"VMULQ",pvrs[ROL4(SR(_opcode,21) & 15)],
									FD(_opcode),FS(_opcode));
						break;
						case 0x3c: case 0x3d: case 0x3e: case 0x3f:
							switch(((_opcode >> 4) & 0x7c) | (_opcode & 3)){
								case 0x08: case 0x09: case 0x0a: case 0x0b:{
									const u32 bc = _opcode & 3;
									float *fs = REGV0_(VS(_opcode)).f32;
									float *ft = REGV0_(VT(_opcode)).f32;
									for (int field = 0; field < 4; field++){
										if (BVT(_opcode, 24-field))
										{
											REGV0A_.f32[field] += fs[field] * ft[bc];
										}
									}
								}
								__R5900F(%s $vf%d\x2c $vf%d.%s,"VMADDA",pvrs[ROL4(SR(_opcode,21) & 15)],
										FS(_opcode),FT(_opcode),pvrs0[_opcode&3]);
								break;
								case 0x18: case 0x19: case 0x1a: case 0x1b:{
									const u32 bc = _opcode & 3;
									float *fs = REGV0_(VS(_opcode)).f32;
									float *ft = REGV0_(VT(_opcode)).f32;
									for (int field = 0; field < 4; field++){
										if (BVT(_opcode, 24-field))
										{
											REGV0A_.f32[field] = fs[field] * ft[bc];
										}
									}
									__R5900F(%s $vf%d\x2c $vf%d.%s,"VMULA",pvrs[ROL4(SR(_opcode,21) & 15)],
										VS(_opcode),VT(_opcode),pvrs0[_opcode&3]);
								}
								break;
								case 0x1f:{
									float w = fabs(REGV0_(VT(_opcode)).f32[3]);
									float *fs = REGV0_(VS(_opcode)).f32;

									REGVU0C_(18) = (REGVU0C_(18) << 6) & 0xffffff;
									if(fs[0] > w) REGVU0C_(18) |= 1;
									if(fs[0] < -w) REGVU0C_(18) |= 2;
									if(fs[1] > w) REGVU0C_(18) |= 4;
									if(fs[1] < -w) REGVU0C_(18) |= 8;
									if(fs[2] > w) REGVU0C_(18) |= 16;
									if(fs[2] < -w) REGVU0C_(18) |= 32;

									__R5900F(xyz $vf%d\x2c $vf%d.w,"VCLIP",
										FS(_opcode),FT(_opcode));
								}
								break;
								case 0x2f:
									__R5900F(NOARG,"VNOP");
								break;
								case 0x38:{
									const u32 fsf = (_opcode >> 21) & 3;
									const u32 ftf = (_opcode >> 23) & 3;
									float *fs = REGV0_(VS(_opcode)).f32;
									float *ft = REGV0_(VT(_opcode)).f32;
									const float ftval = ft[ftf];
									if (ftval)
										REGV0Q_ = fs[fsf] / ftval;
									__R5900F($Q $vf%d.%s\x2c $vf%d.%s,"VDIV",
										FS(_opcode),pvrs0[fsf],FT(_opcode),pvrs0[ftf]);
								}
								break;
								case 0x3b: /* VWAITQ */
									__R5900F(NOARG,"VWAITQ");
								break;
								default:
									EnterDebugMode();
									printf("cop2 %x %x %x %08x\n",*_vm._rs,_opcode & 63,((_opcode >> 4) & 0x7c) | (_opcode & 3),_pc);
								break;
							}
						break;
						default:
							EnterDebugMode();
							printf("cop2 %x %x %08x\n",*_vm._rs,_opcode & 63,_pc);
						break;
					}
				break;
			}
		break;
#else
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			switch(*_vm._rs){
				case 0:
					OnCop(SR(_opcode,26)&3,0,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"MFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 2:
					OnCop(SR(_opcode,26)&3,2,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"CFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 4:
					OnCop(SR(_opcode,26)&3,4,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"MTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 6:
					OnCop(SR(_opcode,26)&3,6,RD(_opcode),(RSZU *)&REGRT_);
					__R5900F(%d %s\x2c%s,"CTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 1:
				case 3:
				case 5:
					printf("40 %x PC:%08X\n",*_vm._rs,_pc);
					EnterDebugMode();
				break;
				default:
				//	EnterDebugMode();
					OnCop(SR(_opcode,26)&3,1,_opcode & 0xFFFFFF,0);
					__R5900F(%d %x,"COP",SR(_opcode,26)&3,_opcode & 0xFFFFFF);
				break;
			}
		break;
#endif
		case 0x14:
			__R5900F(%s\x2c%s\x2c$%x,"BEQL",prs[RT(_opcode)],prs[*_vm._rs],_pc+(IMM(_opcode)<<2));
			if(REGRT_==REGRS_){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			else {
				_next_pc =_pc + 8;
				_jump=1;
				//_vm._idx++;
				//_vm._mem+=4;
				//_vm.invalidate(2|1);
			}
		break;
		case 0x15:
			__R5900F(%s\x2c%s\x2c$%x,"BNEZL",prs[RT(_opcode)],prs[*_vm._rs],_pc+(IMM(_opcode)<<2));
			if(REGRT_!= REGRS_){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			else {
				_next_pc =_pc + 8;
				_jump=1;
				//_vm.invalidate(2|1);
			}
		break;
		case 0x19:
			REGRT_ = (REGRS_+IMM(_opcode));
			__R5900F(%s\x2c%s\x2c$%x,"DADDUI",prs[RT(_opcode)],prs[*_vm._rs],(u16)IMM(_opcode));
		break;
		case 0x1c:
			switch(_opcode & 0x3f){
#if MIPSCORE==R5900
				case 0:{
					u64 a = (s64)REGS32_(*_vm._rs)* (s64)REGS32_(RT(_opcode));
					REG_LO+=(s32)a;
					REG_HI+=(s32)(a >> 32);
					if(RD(_opcode))
						REGRD_= REG_LO;
					ret += 2;
					__R5900F(%s\x2c%s,"MADD",prs[*_vm._rs],prs[RT(_opcode)]);
				}
				break;
				case 4:{
					u64 r,v = REGRS_;
					r=0;
					for(u32 i=0;i<2;i++){
						u32 v0=(u32)v;
						v>>=32;
						for(int ii=31;ii>=0;ii--){
							if(BVT(v0,ii)){
								 r |= SL(30-ii,SL(i,5));
								 break;
							}
						}
					}
					REGRD_=r;
					__R5900F(%s\x2c%s,"PLZCW",prs[RD(_opcode)],prs[RS(_opcode)]);
				}
				break;
				case 8:
					switch(SR(_opcode,6)&0x1f){
						case 9:{
							u128 v[3];

							v[0].lo=REGRT_;
							v[0].hi=REG_(RT(_opcode)+REGI_HIGH);
							v[1].lo=REGRS_;
							v[1].hi=REG_(*_vm._rs+REGI_HIGH);
							for(int i0=0;i0<4;i0++){
								u32 c,b,a=v[0].v32[i0];
								b=v[1].v32[i0];
								c=0;
								for(int i1=0;i1<4;i1++,a>>=8,b >>= 8,c<<=8){
									u8 r=(u8)a - (u8)b;
									c|=r;
								}
								v[2].v32[i0]=c;
							}
							REGRD_=v[2].lo;
							REG_(RD(_opcode)+REGI_HIGH)=v[2].hi;
							__R5900F(%s\x2c%s\x2c%s,"PSUBB",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
						}
						break;
						default:
							printf("0x70:8 %x Pc:%08X\n",SR(_opcode,6)&0x1f,_pc);
							EnterDebugMode();
						break;
					}
				break;
				case 9:
					switch(SR(_opcode,6)&0x1f){
						case 0xe:
							REGRD_=REGRT_;
							REG_(RD(_opcode)+REGI_HIGH)=REGRS_;
							__R5900F(%s\x2c%s\x2c%s,"PCPYLD",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
						break;
						case 0x12:
							REGRD_=REGRT_&REGRS_;
							REG_(RD(_opcode)+REGI_HIGH)=REG_(RT(_opcode)+REGI_HIGH)&REG_(*_vm._rs+REGI_HIGH);
							__R5900F(%s\x2c%s\x2c%s,"PAND",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
						break;
						default:
							printf("0x70:9 %x Pc:%08X\n",SR(_opcode,6)&0x1f,_pc);
							EnterDebugMode();
						break;
					}
				break;
#endif
				case 0x18:{
					s64 value;

					value = (s64)(s32)REGRS_*(s64)(s32)REGRT_;
					REG_LO=(u32)value;
					REG_HI=(u32)(value >> 32);
#if MIPSCORE==R5900
					if(RD(_opcode))
						REGRD_=REG_LO;
#endif
					ret += 2;
					__R5900F(%s\x2c%s\x2c%s,"MULT1",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
				}
				break;
				case 0x29:
					switch(SR(_opcode,6)&0x1f){
						case 0xe:
							REGRD_=REGRT_;
							REG_(RD(_opcode)+REGI_HIGH)=REGRS_;
							__R5900F(%s\x2c%s\x2c%s,"PCPYUD",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
						break;
						case 0x12:
							REGRD_=REGRT_|REGRS_;
							REG_(RD(_opcode)+REGI_HIGH)=REG_(RT(_opcode)+REGI_HIGH)|REG_(*_vm._rs+REGI_HIGH);
							__R5900F(%s\x2c%s\x2c%s,"POR",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
						break;
						case 0x13:
							REGRD_=~(REGRT_|REGRS_);
							REG_(RD(_opcode)+REGI_HIGH)=~(REG_(RT(_opcode)+REGI_HIGH)|REG_(*_vm._rs+REGI_HIGH));
							__R5900F(%s\x2c%s\x2c%s,"PNOR",prs[RD(_opcode)],prs[*_vm._rs],prs[RT(_opcode)]);
						break;
						default:
							printf("0x70:29 %x PC:%08X\n",SR(_opcode,6)&0x1f,_pc);
							EnterDebugMode();
						break;
					}
				break;
				case 0x30:
					switch(SA(_opcode)){
						case 0:{
							u32 v[4];
							v[0]=(u32)REG_LO;
							v[1]=(u32)REG_HI;
							v[2]=(u32)REGH_(REGI_LO);
							v[3]=(u32)REGH_(REGI_HI);
							REGRD_=*(u64 *)v;
							REGH_(RD(_opcode))=*(u64 *)&v[2];
							__R5900F(%s,"PMFHL.LW",prs[RD(_opcode)]);
						}
						break;
						default:
							printf("0x70 0x30 %x Pc:%08X\n",SA(_opcode),_pc);
							EnterDebugMode();
						break;
					}
				break;
				default:
					printf("0x70 %x Pc:%08X\n",_opcode&0x3f,_pc);
					EnterDebugMode();
				break;
			}
		break;
		case 0x1e:{
			u32 a=REGRS_+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			//RQ(a,REGRT_);
			//EnterDebugMode();
			RP(a,REGRT_);
			RP(a+8,REGH_(RT(_opcode)));
			ret+=2+2;
			__R5900F(%s\x2c$%x[%s],"LQ",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x1f:{
			u32 a=REGRS_+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			//EnterDebugMode();
			WP(a,REGRT_);
			WP(a+8,REGH_(RT(_opcode)));
			//WQ(a,REGRT_);
			ret+=2+2;
			__R5900F(%s\x2c$%x[%s],"SQ",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x20:{
			u32 a=REGRS_+IMM(_opcode);
			u8 v;

			RB(a,v);
			REGRT_=(RSZS)(s8)v;
			__R5900F(%s\x2c$%x[%s],"LB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x21:{
			u16 v;
			u32 a=REGRS_+IMM(_opcode);

			RW(a,v);
			REGRT_=(RSZS)(s16)v;
			__R5900F(%s\x2c$%x[%s],"LH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x22:{
			u32 c,b,a=REGRS_+IMM(_opcode);
			RL(a&~3,b);
			c=REGRT_;
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
			REGRT_=c;
			__R5900F(%s\x2c$%x[%s],"LWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x23:{
			u32 b,a=REGRS_+IMM(_opcode);
			RL(a,b);
			REGRT_=(RSZU)(s32)b;
			ret++;
			__R5900F(%s\x2c$%x[%s],"LW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x24:{
			u32 a=REGRS_+IMM(_opcode);
			RB(a,REGRT_ );
			__R5900F(%s\x2c$%x[%s],"LBU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x25:{
			u32 a=REGRS_+IMM(_opcode);
			RW(a,REGRT_);
			ret++;
			__R5900F(%s\x2c$%x[%s],"LHU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x26:{
			u32 c,b,a=REGRS_+IMM(_opcode);
			RL(a & ~3,b);
			c=REGRT_;
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
			REGRT_=c;
			__R5900F(%s\x2c$%x[%s],"LWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x28:{
			u32 a=REGRS_+IMM(_opcode);
			WB(a,REGRT_);
			__R5900F(%s\x2c$%x[%s],"SB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x29:{
			u32 a=REGRS_+IMM(_opcode);
			WW(a,REGRT_);
			__R5900F(%s\x2c$%x[%s],"SH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x2a:{
			 u32 adr,value,reg;

			adr = REGRS_ + IMM(_opcode);
			reg = REGRT_;
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
			__R5900F(%s\x2c$%x[%s],"SWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x2b:{
			u32 a=REGRS_+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			WL(a,REGRT_);
			ret+=2;
			__R5900F(%s\x2c$%x[%s],"SW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x2e:{
			 u32 adr,value,reg;

			adr = REGRS_ + IMM(_opcode);
			reg = REGRT_;
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
			__R5900F(%s\x2c$%x[%s],"SWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
#if MIPS >= 3
		case 0x27:{
			u32 b,a=REGRS_+IMM(_opcode);
			RL(a,b);
			REGRT_=(RSZU)b;
			ret++;
			__R5900F(%s\x2c$%x[%s],"LWU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x2f:
			__R5900F(%x\x2c$%x[%s],"CACHE",SR(_opcode,1)&0x1f,(u16)IMM(_opcode),prs[*_vm._rs]);
		break;
		case 0x31:{
				u32 a;

				RL(REGRS_+IMM(_opcode),a);
				if(IS_FR1)
					*(u64 *)&REGDD0_(FT(_opcode))=a;
				else
					*(u32 *)&REGDF0_(FT(_opcode))=a;
			}
			__R5900F($f%d\x2c$%x[%s],"LWC1",FT(_opcode),(u16)IMM(_opcode),prs[*_vm._rs]);
		break;
		case 0x36:{
			u128 a;
			u32 b=REGRS_+IMM(_opcode);

			RP(b,a.lo);
			RP(b+8,a.hi);
			REGV0_(FT(_opcode))=a;
			ret += 2+2;
			__R5900F($vf%d\x2c$%x[%s],"LQC2",FT(_opcode),(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x37:{
			u32 a=REGRS_+IMM(_opcode);
			RP(a,REGRT_);
			ret+=2;
			__R5900F(%s\x2c$%x[%s],"LD",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x39:{
			u32 a;

			if(IS_FR1)
				a=*(u64 *)&REGDD0_(FT(_opcode));
			else
				a=*(u32 *)&REGDF0_(FT(_opcode));
			WL(REGRS_+IMM(_opcode),a);
		}
			__R5900F($f%d\x2c$%x[%s],"SWC1",RT(_opcode),(u16)IMM(_opcode),prs[*_vm._rs]);
		break;
#else
		case 0x31:
		case 0x32:
			OnCop(SR(_opcode,26)&3,8,REGRS_+IMM(_opcode),(RSZU *)(u64)RT(_opcode));
			__R5900F(%d %d\x2c$%x[%s],"LWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[*_vm._rs]);
		break;
		case 0x39:
			OnCop(1,9,REGRS_+IMM(_opcode),(RSZU *)(u64)RT(_opcode));
			__R5900F($f%d\x2c$%x[%s],"SWC1",RT(_opcode),(u16)IMM(_opcode),prs[*_vm._rs]);
		break;
#endif
		case 0x3a:
			OnCop(SR(_opcode,26)&3,9,REGRS_+IMM(_opcode),(RSZU *)(u64)RT(_opcode));
			__R5900F(%d %d\x2c$%x[%s],"SWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[*_vm._rs]);
		break;
#if MIPS >= 3
		case 0x3e:{
			u32 a=REGRS_+IMM(_opcode);
			u128 b=REGV0_(RT(_opcode));
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			WP(a,b.lo);
			WP(a+8,b.hi);
			ret+=2+2;
			__R5900F($vf%d\x2c$%x[%s],"SQC2",RT(_opcode),(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
		case 0x3f:{
			u32 a=REGRS_+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			WP(a,REGRT_);
			ret+=2;
			__R5900F(%s\x2c$%x[%s],"SD",prs[RT(_opcode)],(u16)IMM(_opcode),prs[*_vm._rs]);
		}
		break;
#endif
	}
Z:
	_pc+=4;
	if(_jump && --_jump==0){
		_vm._label=_next_pc;
		_vm._branch=_pc;
		_pc=_next_pc;
		_vm.invalidate();
		if(_vm._play == 1){
			_vm._play = 2;
			_vm.p=_vm._routine->_buf;
		}
		else if(_vm._play == 2){
			if(_pc >= _vm._routine->_end){
				printf("dl outine %08x %x %x\n",_pc,_vm._routine->_start,_vm._routine->_end);
				_vm._routine->reset();
				_vm.invalidate(1|2|4);
			//	_vm._routine=0;
				//EnterDebugMode();
			}
			else{
				u32 a = _pc-_vm._routine->_start;
				_vm.p=&_vm._routine->_buf[a];
				_vm._wsync=0;
				if(__address==0x12001000){
					_vm._wsync=1;
				//	EnterDebugMode();
				}
			}
		}
		else if(_vm._play == 3)
			_vm._play=0;

		if(!(_pc & 0x0fff0000) && !OnJump(_pc))
			_pc = REG_RA;

		if(_irq_pending){//fixme
			//EnterDebugMode();
			machine->OnEvent(0,(LPVOID)-1);
			//_irq=0;
		}
	}
	else if(_vm._play < 2 && ++_vm._step == 0){
		_vm.pp += 8;
		_vm.invalidate(0);
	}
	else{
		_vm.p += 4;
		_vm._opcode += 4;
		_vm._rs += 4;
		_vm._rt += 4;
		_vm._rd += 4;
		_vm._regrt++;
		_vm._regrs++;
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
					if(_opcode==0){
						__R5900D(NOARG,"NOP");
					}
					else
					__R5900D(%s\x2c%s\x2c$%x,"SLL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
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
#if MIPS >= 3
				case 0xa:
					__R5900D(%s %s\x2c%s,"MOVz",prs[RT(_opcode)],prs[RD(_opcode)],prs[RS(_opcode)]);
				break;
				case 0xb:
					__R5900D(%s %s\x2c%s,"MOVN",prs[RT(_opcode)],prs[RD(_opcode)],prs[RS(_opcode)]);
				break;
				case 0xf:
					__R5900D(,"SYNC");
				break;
#endif
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
#if MIPSCORE==R5900
					__R5900D(%s\x2c%s\x2c%s,"MULT",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
#else
					__R5900D(%s\x2c%s,"MULT",prs[RS(_opcode)],prs[RT(_opcode)]);
#endif
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
				case 0x16:
					__R5900D(%s\x2c%s\x2c#%s,"DSRLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 0x2d:
					__R5900D(%s\x2c%s\x2c%s,"DADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2f:
					__R5900D(%s\x2c%s\x2c%s,"DSUBU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x34:
					__R5900D(%s\x2c%s\x2c%x,"TEQ",prs[RS(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x38:
					__R5900D(%s\x2c%s\x2c#%x,"DSLL",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3a:
					__R5900D(%s\x2c%s\x2c#%x,"DSRL",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3c:
					__R5900D(%s\x2c%s\x2c#%x,"DSLL32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3e:
					__R5900D(%s\x2c%s\x2c#%x,"DSRL32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
				break;
				case 0x3f:
					__R5900D(%s\x2c%s\x2c#%x,"DSRA32",prs[RD(_opcode)],prs[RT(_opcode)],SA(_opcode));
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
				case 0x11:
					__R5900D(%s\x2c$%x,"BGEZAL",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
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
			__R5900D(%s\x2c%s\x2c$%x,"ADDIU",prs[RT(_opcode)],prs[RS(_opcode)],(u16)IMM(_opcode));
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
			switch(RS(_opcode)){
				case 0x10:
					switch(_opcode & 0x3f){
						case 0x18:
							__R5900D(NOARG,"RFE");
						break;
					}
					goto A;
				break;
#if MIPS>=3
				case 0x8:
					switch(RT(_opcode)){
						case 0:
							__R5900D($%x,"BC0F",adr+(IMM(_opcode)<<2));
						break;
						case 1:
							__R5900D($%x,"BC0T",adr+(IMM(_opcode)<<2));
						break;
					}
					goto A;
				break;
#endif
			}
		case 0x44:
#if MIPS>=3
			switch(RS(_opcode)){
				case 0:
					__R5900D(%s\x2c $f%d,"MFC1",prs[RT(_opcode)],FS(_opcode));
				break;
				case 4:
					__R5900D(%s\x2c $f%d,"MTC1",prs[RT(_opcode)],FS(_opcode));
				break;
				case 0x8:
					switch(RT(_opcode)){
						case 0:
							__R5900D($%x,"BC1F",adr+(IMM(_opcode)<<2));
						break;
						case 1:
							__R5900D($%x,"BC1T",adr+(IMM(_opcode)<<2));
						break;
					}
				break;
				default:
					switch(_opcode & 0x3f){
						case 0x0://add
							__R5900D($f%d\x2c $f%d\x2c $f%d,"ADD",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x1://sub
							__R5900D($f%d\x2c $f%d\x2c $f%d,"SUB",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x2://mul
							__R5900D($f%d\x2c $f%d\x2c $f%d,"MUL",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x3://div
							__R5900D($f%d\x2c $f%d\x2c $f%d,"DIV",FD(_opcode),FS(_opcode),FT(_opcode));
						break;
						case 0x7://neg
							__R5900D($f%d\x2c $f%d,"NEG",FD(_opcode),FS(_opcode));
						break;
						case 0x6:
							__R5900D(%c $f%d\x2c $f%d,"MOV",IS_SINGLE(_opcode)?'S':'D',FD(_opcode),FS(_opcode));
						break;
						case 0x20://cvt
							__R5900D($f%d\x2c $f%d,"CVT.S",FD(_opcode),FS(_opcode));
						break;
						case 0x24://cvt.w.s
							__R5900D($f%d\x2c $f%d,"CVT.W",FD(_opcode),FS(_opcode));
						break;
						case 0x32:
						case 0x3a:
							__R5900D($f%d\x2c $f%d,"C.EQ",FS(_opcode),FT(_opcode));
						break;
						case 0x34:
						case 0x3c:
							__R5900D($f%d\x2c $f%d,"C.LT",FS(_opcode),FT(_opcode));
						break;
						case 0x36:
						case 0x3e:
							__R5900D($f%d\x2c $f%d,"C.LE",FS(_opcode),FT(_opcode));
						break;
					}
					break;
			}
		break;
#endif
		case 0x48:
#if MIPS>=3
			switch(RS(_opcode)){
				case 0x02:  /* CFCz */
					__R5900D(%s\x2c $vi%d,"CFC2",prs[RT(_opcode)],FS(_opcode));
				break;
				case 0x00:  /* MFCz */
				case 0x01:  /* DMFCz */
				case 0x04:  /* MTCz */
				case 0x05:  /* DMTCz */
				case 0x06:  /* CTCz */
				break;
				case 0x08:  /* BC */
					switch (RT(_opcode)){
						case 0x00:  /* BCzF */
						case 0x01:  /* BCzT */
						case 0x02:  /* BCzFL */
						case 0x03:  /* BCzTL */
						break;
					}
					break;
				default:
					switch (_opcode & 0x3f){
						case 0x08: case 0x09: case 0x0a: case 0x0b:
							__R5900D(%s $vf%d\x2c $vf%d\x2c $vf%d.%s,"VMADD",pvrs[ROL4(SR(_opcode,21) & 15)],
										FD(_opcode),FS(_opcode),FT(_opcode),pvrs0[_opcode&3]);
						break;
						case 0x1c:
							__R5900D(%s $vf%d\x2c $vf%d,"VMULQ",pvrs[ROL4(SR(_opcode,21) & 15)],
								FD(_opcode),FS(_opcode));
						break;
						case 0x3c:
						case 0x3d:
						case 0x3e:
						case 0x3f:
							switch(((_opcode >> 4) & 0x7c) | (_opcode & 3)){
								case 0x08: case 0x09: case 0x0a: case 0x0b:
									__R5900D(%s $vf%d\x2c $vf%d.%s,"VMADDA",pvrs[ROL4(SR(_opcode,21) & 15)],
										FS(_opcode),FT(_opcode),pvrs0[_opcode&3]
									);
								break;
								case 0x18: case 0x19: case 0x1a: case 0x1b:{
									__R5900D(%s $vf%d\x2c $vf%d.%s,"VMULA",pvrs[ROL4(SR(_opcode,21) & 15)],
										FS(_opcode),FT(_opcode),pvrs0[_opcode&3]
									);
								}
								break;
								case 0x1f:{
									__R5900D(%s $vf%d\x2c $vf%d.%s,"VCLIP",pvrs[ROL4(SR(_opcode,21) & 15)],
										FS(_opcode),FT(_opcode),pvrs0[_opcode&3]
									);
								}
								break;
								case 0x2f:
									__R5900D(NOARG,"VNOP");
								break;
								case 0x38:{
									__R5900D($Q $vf%d.%s\x2c $vf%d.%s,"VDIV",
										FS(_opcode),pvrs0[(_opcode >> 21) & 3],FT(_opcode),pvrs0[(_opcode >> 23) & 3]
									);
								}
								break;
								case 0x3b: /* VWAITQ */
									__R5900D(NOARG,"VWAITQ");
								break;
							}
						break;
					}
				break;
			}
		break;
#endif
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
		case 0x50:
			__R5900D(%s\x2c%s\x2c$%x,"BEQL",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x54:
			__R5900D(%s\x2c%s\x2c$%x,"BNEZL",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x64:
			__R5900D(%s\x2c%s\x2c$%x,"DADDUI",prs[RT(_opcode)],prs[RS(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x70:
			switch(_opcode&0x3f){
#if MIPSCORE==R5900
				case 0:
					__R5900D(%s\x2c%s,"MADD",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 4:
					__R5900D(%s\x2c%s,"PLZCW",prs[RD(_opcode)],prs[RS(_opcode)]);
				break;
				case 8:
					switch(SR(_opcode,6)&0x1f){
						case 9:
							__R5900D(%s\x2c%s\x2c%s,"PSUBB",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
						break;
					}
				break;
				case 9:
					switch(SR(_opcode,6)&0x1f){
						case 0xe:
							__R5900D(%s\x2c%s\x2c%s,"PCPYLD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
						break;
						case 0x12:
							__R5900D(%s\x2c%s\x2c%s,"PAND",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
						break;
					}
				break;
#endif
				case 0x18:
					__R5900D(%s\x2c%s\x2c%s,"MULT1",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x29:
					switch(SR(_opcode,6)&0x1f){
						case 0xe:
							__R5900D(%s\x2c%s\x2c%s,"PCPYUD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
						break;
						case 0x12:
							__R5900D(%s\x2c%s\x2c%s,"POR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
						break;
						case 0x13:
							__R5900D(%s\x2c%s\x2c%s,"PNOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
						break;
					}
				break;
				case 0x30:
					switch(SA(_opcode)){
						case 0:
							__R5900D(%s,"PMFHL.LW",prs[RD(_opcode)]);
						break;
					}
				break;
			}
		break;
		case 0x78:
			__R5900D(%s\x2c$%x[%s],"LQ",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
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
#if MIPS >= 3
		case 0x9c:
			__R5900D(%s\x2c$%x[%s],"LWU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xbc:
			__R5900D(%x\x2c$%x[%s],"CACHE",SR(_opcode,16) & 0x1f,(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#endif
		case 0xc4:
#if MIPS >= 3
			__R5900D($f%d\x2c$%x[%s],"LWC1",FT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
#else
			__R5900D(%d\x2c$%x[%s],"LWC1",RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
#endif
		break;
		case 0xc8:
			__R5900D(%d %d\x2c$%x[%s],"LWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#if MIPS >= 3
		case 0xd8:
			__R5900D($vf%d\x2c$%x[%s],"LQC2",FT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xdc:
			__R5900D(%s\x2c$%x[%s],"LD",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#endif
		case 0xe4:
			__R5900D($f%d\x2c$%x[%s],"SWC1",RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xe8:
			__R5900D(%d %d\x2c$%x[%s],"SWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
#if MIPS >= 3
		case 0xf8:
			__R5900D($vf%d\x2c$%x[%s],"SQC2",RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
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
		case ICORE_QUERY_REGISTER:{
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