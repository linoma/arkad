#include "ps2.h"

namespace ps2{

struct __sif{
	u32 _regs[7],_cycles;
	struct{
		union{
			u32 _control;
			struct{
				u32 _state:2;
				u32 _cmd:4;
			};
		};
		vector<u32> _fifo;
	} _ch[2];

	int reset(){
		memset(_regs,0,sizeof(_regs));
		_regs[2]=0x20000;
		for(int i=0;i<2;i++){
			_ch[i]._control=0;
			_ch[i]._fifo.clear();
		}
		return 0;
	};

	int update(){
		_cycles=__cycles;
		return 0;
	};
} _sif;
u32 *_sif_regs=_sif._regs;

struct __gif_tag{
	u8 _regs[16],_n_regs,_cr_regs;
	union{
		u64 _v64[2];
		struct{
			u64 _lo;
			u64 _hi;
		};
		u64 _control;
		struct{
			unsigned long _nloop:15;
			unsigned long _end:1;
			unsigned long _a0:30;
			unsigned long _enable_field:1;
			unsigned long _data:11;
			unsigned long _format:2;
			unsigned long _nregs:4;
		};
	};

	__gif_tag(){
		_control=0;
		memset(_regs,0,sizeof(_regs));
	};
	__gif_tag(u128 v){
		int i,n;

		memset(_regs,0,sizeof(_regs));
		_lo=v.lo;
		_hi=v.hi;
		if(!(_n_regs=_nregs))
			_n_regs=16;
		if(_n_regs==2 && _format)
			i=0;
		for(i = 0;i<_n_regs;i++)
			_regs[i]=(v.hi>>(i*4))&15;
		_cr_regs=0;
	};
};

struct __gif : vector<u128>{
	u32 _cycles;

	int reset(){
		clear();
		return 0;
	};
	int update(){
		_cycles=__cycles;
		return 0;
	};
} _gif;


struct __timer {
	union{
		u16 _status;
		struct{
			unsigned int _clock:2;
			unsigned int _genable:1;
			unsigned int _gtype:1;
			unsigned int _gmode:2;
			unsigned int _clear:1;
			unsigned int _enabled:1;
			unsigned int _iec:1;
			unsigned int _ieo:1;
			unsigned int _ifc:1;
			unsigned int _ifo:1;
			unsigned int _a0:4;
			unsigned int _idx:2;
		};
	};
	u32 *_ioreg,_step,_cycles,_freq;
	u16 _counter,_hold,_compare;

	u8 *_mem;

	__timer();
	int reset();
	int init(int,void *,void *,u32);
	int write(u32,u32);
	int read(u32,u32 *);
	int update(){
		if(!_enabled) return 0;
		u32 d=SR(D_CYCLES(_cycles,_freq,__cycles),_step);
		if(!d)
			return 0;
		_cycles=__cycles;
		u16 counter=_counter;
		_counter += d;
		PS2IOREG(0x10000000|SL(_idx,11))=_counter;
		return 0;
	};

	protected:
		int enable(){
			//freq / 2
			_cycles=__cycles;
			switch(_clock){
				case 2:
					_step=8;
				break;
			}
			return 0;
		};
} _timers[4];

struct __dma{
	union{
		u16 _control;
		struct{
			unsigned int _dir:1;
			unsigned int _d0:1;
			unsigned int _mode:2;
			unsigned int _asp:2;
			unsigned int _tte:1;
			unsigned int _tie:1;
			unsigned int _enabled:1;
			unsigned int _d1:7;
			unsigned int _et:1;
		};
	};
	u32 *_ioreg,_src,_dst,_count,_io_adr,_tsrc;
	u8 *_mem,_idx;

	__dma();
	int reset();
	int init(int,void *,void *,u32);
	int write(u32,u32,PS2DEV &);
	virtual int _do_transfer(u32,u32 *);
	virtual int _do_dma(PS2DEV &);

	friend class PS2DEV;
};

struct __dvdrom{
	u32 *_ioreg;
	u8 *_mem;

	__dvdrom();
	int reset();
	int init(int,void *,void *);
} _dvdrom;

struct __dmac{
	enum : u8{
		VIF0 = 0,VIF1,GIF,IPU_OUT,IPU_IN,SIF0,SIF1,
		SIF2,SPR_OUT,SPR_IN
	};
	u32 *_ioreg;
	u8 *_mem;

	struct __dma _ch[10];

	int reset(){
		for(int i=0;i<sizeof(_ch) / sizeof(struct __dma);i++)
			_ch[i].reset();
		return 0;
	};

	int init(int,void *b,void *c){
		int a[]={0x80,0x90,0xa0,0xb0,0xb4,0xc0,0xc4,0xc8,0xd0,0xd4};

		for(int i=0;i<sizeof(_ch) / sizeof(struct __dma);i++)
			_ch[i].init(i,b,c,0x10000000|SL(a[i],8));
		return 0;
	};

	int write(u32 a,u32 v,PS2DEV &m){
		switch((u8)SR(a,10)){
			case SR(0x8000,10):
			break;
			case SR(0x9000,10):
			break;
			case SR(0xa000,10):
				switch(_ch[2].write(a,v,m)){
					case 1:
						//EnterDebugMode();
						m.do_dma(&_ch[2]);
						return 0;
					break;
				}
			break;
			case SR(0xc400,10):
				switch(_ch[6].write(a,v,m)){
					case 1:
						//EnterDebugMode();
						m.do_dma(&_ch[6]);
						return 0;
					break;
				}
			break;
			case 0x38:
				switch((u8)(a,4)){
					case 1:
					break;
				}
			break;
		}
		return 1;
	};
} _dmac;

PS2DEV::PS2DEV() : R5900Cpu(),ps2gpu(),PCMDAC(){
}

PS2DEV::~PS2DEV(){
}

int PS2DEV::Init(PS2M &g){
	if(R5900Cpu::Init(&g._memory[MB(2)],0x20000))
		return -1;
	_ioreg=(u32 *)&_mem[MB(32)];
	_gpu_mem=&_mem[MI_VRAM];
	_gpu_regs=_ioreg;
	_outBuffer=(u8 *)_gpu_mem + MS_VRAM;
	_texture=(u8 *)_outBuffer + MB(2);
	_palette=(u8 *)_texture + MB(2);
	if(ps2gpu::Init(0,_ioreg,_mem,R5900Cpu::_freq))
		return -3;
	for(int i =0;i<sizeof(_timers)/sizeof(__timer);i++)
		_timers[i].init(i,_ioreg,_mem,_freq);
	_dmac.init(0,_ioreg,_mem);
	_dvdrom.init(0,_ioreg,_mem);
	return 0;
}

int PS2DEV::Reset(){
	R5900Cpu::Reset();
	ps2gpu::Reset();
	PCMDAC::Reset();
	for(int i =0;i<sizeof(_timers)/sizeof(__timer);i++)
		_timers[i].reset();
	_dmac.reset();
	_dvdrom.reset();
	_sif.reset();
	return 0;
}

int PS2DEV::Run(u8 *a,int b,void *c){
	int res=ps2gpu::Run(a,b,c);
	if(__line==26){
		PS2IOREG(0x12001000) |= 8;
	//	PS2IOREG(0x12001000) &= ~2;
		res=3;
	}
	else if(__line==0){
		PS2IOREG(0x12001000) &= ~8;
	//	PS2IOREG(0x12001000) |= 2;
		res=2;
	}
	for(int i =0;i<sizeof(_timers)/sizeof(__timer);i++)
		_timers[i].update();
	return res;
}

int PS2DEV::Query(u32 what,void *pv){
	switch(what){
		default:
			return R5900Cpu::Query(what,pv);
	}
}

s32 PS2DEV::fn_timer_regs_r(u32 a,pvoid,pvoid pdata,u32 f){
}

s32 PS2DEV::fn_timer_regs_w(u32 a,pvoid,pvoid pdata,u32 f){
	//printf("timer %d %x %x %x D:%llx\n",_idx,a,*(u32 *)pdata,f,__data);
	return _timers[SR(a,11)&3].write(a,*(u32 *)pdata);
}

s32 PS2DEV::fn_dma_regs_w(u32 a,pvoid,pvoid pdata,u32){
	return _dmac.write(a,*(u32 *)pdata,*this);
}

s32 PS2DEV::fn_gif_regs_w(u32 a,pvoid,pvoid pdata,u32){
	return _dmac.write(a,*(u32 *)pdata,*this);
}

s32 PS2DEV::fn_ipu_regs_w(u32 a,pvoid,pvoid pdata,u32){
	return _dmac.write(a,*(u32 *)pdata,*this);
}

s32 PS2DEV::fn_gs_regs_w(u32 a,pvoid,pvoid pdata,u32){
	return _dmac.write(a,*(u32 *)pdata,*this);
}

s32 PS2DEV::fn_sif_regs_w(u32 a,pvoid,pvoid pdata,u32){
	return _dmac.write(a,*(u32 *)pdata,*this);
}

int PS2DEV::do_dma(struct __dma *dma){
	u32 dst,n;

	dma->_et=dma->_count != 0;
A:
	dst=dma->_src;
	for(n=0;dma->_count;dma->_count--,n++){
		u128 v=0;
		for (int i = 0; i < 4;i++){
			v.v32[i]=*(u32 *)(_mem + (dst & (MB(32)-1)));
			dst += 4;
		}
		switch(dma->_idx){
			case 2:
				_gif.push_back({v});
			break;
			case 6:
				for (int i = 0; i < 4;i++)
					_sif._ch[1]._fifo.push_back(v.v32[i]);
			break;
		}
	}
B:
		if(!dma->_et){
			u128 a;
			a=*(u128 *)&_mem[dma->_tsrc & (MS_RAM-1)];
			u32 tag=(u32)a.v32[0];
			u32 adr=a.v32[1] & ~15;
			dma->_count=(u16)tag;
			//dma->_control=SR(tag,16);
			switch(SR(tag,28) & 7){
				case 1:
					dma->_src=dma->_tsrc+16;
					dma->_tsrc=dma->_src+SL(dma->_count,4);
					goto A;
				break;
				case 3:
					dma->_src=adr;
					dma->_tsrc += 16;
					goto A;
				break;
				case 7:
					dma->_et=1;
					dma->_src=dma->_tsrc+16;
					goto A;
				break;
				default:
					goto A;
				break;
			}
		}

	PS2IOREG(dma->_io_adr) &= ~0x100;
	if(dma->_tie) {
		if(PS2IOREG(0x1000e010) & BV(16+dma->_idx)){
			PS2IOREG(0x1000e010) |= BV(dma->_idx);
			machine->OnEvent(16,1);
		}
	}

	if(dma->_idx == 6) {//emulate sif0 request
#ifdef _DEVELOP
		for(auto it=_sif._ch[1]._fifo.begin();it!=_sif._ch[1]._fifo.end();it++){
			printf("%x ",*(it));
		}
		printf("\n");
		for(u32 i=0;i<sizeof(_sif._regs)/sizeof(u32);i++)
			printf("%x ",_sif._regs[i]);
		printf("\n");
#endif
		if(_sif._ch[1]._state==0){
			_sif._ch[1]._state=1;
			_sif._ch[1]._cmd=_sif._ch[1]._fifo[2];
		}
		else if(_sif._ch[1]._state==2){
			_sif._ch[1]._state=0;
		}
		switch(_sif._ch[1]._cmd){
			case 0x2:{//init
				u32 *p=(u32 *)&_mem[_sif._ch[1]._fifo[4]&(MS_RAM-1)];

				p[0]=24;
				p[2]=0x80000001;
				p[4]=0;
				p[5]=_sif._regs[0]=_sif._ch[1]._fifo[4];
				_sif._regs[6]=1;
				_sif._regs[2]=0x20000;
			}
			break;
			case 0x8:{//end
				u32 *p=(u32 *)&_mem[_sif._regs[0]  & (MS_RAM-1)];

			}
			break;
			case 0x9:{//bind
				u32 *p=(u32 *)&_mem[_sif._regs[0]  & (MS_RAM-1)];
				p[0]=64;
				p[2]=0x80000008;//nd
				p[3]=0;
				p[8]=_sif._ch[1]._fifo[8];
				p=(u32 *)&_mem[_sif._ch[1]._fifo[7]  & (MS_RAM-1)];
				p[9]=0x3333;//server (SifRpcClientData)
				//_sif._fifo[1][8] Service ID
				_sif._ch[1]._state++;
			}
			break;
			case 0xa:
				switch(_sif._ch[1]._fifo[8]){
					case 0:{//open
						char c[100];

						for(int i=0;i<sizeof(c);i++){
							*(u32 *)&c[i*4] = _sif._ch[1]._fifo[1+i];
							if(c[i+3]==0) break;
						}
						printf("open %s\n",c);
						u32 *p=(u32 *)&_mem[_sif._regs[0]  & (MS_RAM-1)];
						p[0]=64;
						p[2]=0x8000000c;
					}
					break;
					case 1:
					break;
					case 2:
					break;
				}
			break;
		}
		if(_sif._ch[1]._state==1)_sif._ch[1]._state=0;
		_sif._ch[1]._fifo.clear();

		if(PS2IOREG(0x1000e010) & BV(16+5)){
			PS2IOREG(0x1000e010) |= BV(5);
			machine->OnEvent(16,1);
		}

		return 0;
	}

	__gif_tag cr;
	n=0;
	for(auto it=_gif.begin();it!=_gif.end();it++,n++){
		u128 v = *it;
		if(cr._nloop==0){
			cr=v;
			continue;
		}
		switch(cr._format){
			case 0://packet
				if(cr._nloop){
					u8 r=cr._regs[cr._cr_regs++];
					if(r != 14)
						printf("%u PAK:%x %16llx %16llx\n",n,r,v.lo,v.hi);
					ps2gpu::write(v.hi,v.lo);
					if(cr._cr_regs >= cr._n_regs){
						cr._cr_regs=0;
						cr._nloop--;
					}
				}
			break;
			case 1://reglist
				if(cr._nloop){
				//	printf("%u list %16llx %16llx\n",n,v.lo,v.hi);
					for(int i=0;i<2;i++){
						ps2gpu::write(cr._regs[cr._cr_regs++],v.v64[i]);
						if(cr._cr_regs >= cr._n_regs){
							cr._cr_regs-=cr._n_regs;
							//cr._cr_regs=0;
							if(--cr._nloop == 0) break;
							//--cr._nloop;
						}
					}
				}
			break;
			case 2:
			case 3:
				if(cr._nloop){
					//printf("%u list %16llx %16llx\n",n,v.lo,v.hi);
					ps2gpu::write(0x54,v.v64[0]);
					ps2gpu::write(0x54,v.v64[1]);
					cr._nloop--;
				}
			break;
			default:
				printf("%u DIR;%u %16llx %16llx\n",n,cr._format,v.lo,v.hi);
				//EnterDebugMode();
				//return 0;
			break;
		}
	}

	_gif.clear();
	return 0;
}

int PS2DEV::OnCop(RSZU cop,RSZU op,RSZU s,RSZU *d){
	switch(op){
		case 0://mfc
			switch(cop){
				case 0:
					if(s != 12 && s != 28)
						printf("read cop0 %x %08x \n",(u32)s,_pc);
					*d=CP0._regs[s];
					return 0;
			}
		break;
		case 1://cop
			switch(cop){
				case 0:
					return CP0._op(_opcode);
				case 1:
				break;
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
		break;
		case 8://lwc
			switch(cop){
				case 21:{
					u32 v_;

					RL(s,v_);
					//return CP1._mv_to((u32)(u64)d,&v_);
					return 0;
				}
			}
		break;
		case 9://swc
			switch(cop){
				case 1:{
					u32 v_;

					//CP1._mv_from((u32)(u64)d,&v_);
					WL(s,v_);
					//EnterDebugMode();
				}
			}
		break;
	}
	return -1;
}

int PS2DEV::_enterIRQ(int n,u32 pc){
	Resume();
	if(	(CP0._regs[CP0.SR] & (SR_IE|SR_EIE|SR_ERL|SR_EXL)) !=  (SR_IE|SR_EIE) )
		return -1;
	if(R5900Cpu::_enterIRQ(n,pc))
		return -2;
	switch(n){
		case 16:
			if((CP0._regs[CP0.SR] & SR_INT1) == 0)
				return -1;
			break;
		default:
			if((CP0._regs[CP0.SR] & SR_INT0) == 0)
				return -1;
			if(!(PS2IOREG(0x1000f010) & BV(n)) )
				return -2;
			break;
	}
	CP0._regs[CP0.EPC]=_pc;
	CP0._regs[CP0.SR] |= SR_EXL;
	return 0;
}

__timer::__timer(){
	_status=0;
}

int __timer::init(int n,void *m,void *mm,u32 f){
	_idx=n;
	_ioreg=(u32 *)m;
	_mem=(u8 *)mm;
	_freq=f;
	return 0;
}

int __timer::reset(){
	_status=0;
	_counter=0;
	_step=0;
	return 0;
}

int __timer::write(u32 a,u32 b){
	switch(SR(a,4)&3){
		case 0:
			_counter=(u16)b;
			return 0;
		break;
		case 1:{
			u16 status=_status;
			_status=b&0x3ff;
			if(!(status&0x80) && (b&0x80))
				enable();
		}
		break;
		case 2:
			_compare=(u32)(u16)b;
		break;
	}
	return 1;
}

int __timer::read(u32 a,u32 *b){
	return 0;
}

__dma::__dma(){
	_control=0;
}

int __dma::write(u32 r,u32 v,PS2DEV &g){
	//printf("dma write %d %x %x\n",_idx,r,v);
	switch(SR((u8)r,4)){
		case 0:
			_et=0;
			_control=v;
			return _enabled;
		break;
		case 1:
			_src=v;
		break;
		case 2:
			_count=v;
		break;
		case 3:
			_tsrc=v;
		break;
	}
	return 0;
}

int __dma::reset(){
	_mode=0;
	_count=_src=_dst=0;
	return 0;
}

int __dma::init(int n,void *m,void *mm,u32 a){
	_ioreg=(u32 *)m;
	_mem=(u8 *)mm;
	_idx=n;
	_io_adr=a;
	return 0;
}

int __dma::_do_transfer(u32 a,u32 *r){
	//printf("dma data %d %x\n",_idx,a);
	return 0;
}

int __dma::_do_dma(PS2DEV &g){
	u32 v=_ioreg[RMAP_IO(0x1F801088+SL(_idx,4)) / 4];
	_mode=SR(v,9);
	_src=_ioreg[RMAP_IO(0x1F801080+SL(_idx,4)) / 4];
	_count=_ioreg[RMAP_IO(0x1F801084+SL(_idx,4)) / 4];
	//printf("do dma %d %x %x %x\n",_idx,_mode,_src,_count);
	if(!g.do_dma(this))
		_ioreg[RMAP_IO(0x1F801088+SL(_idx,4)) / 4] &= ~BV(24);
	return 0;
}

__dvdrom::__dvdrom(){
}

int __dvdrom::reset(){
	return 0;
}

int __dvdrom::init(int,void *m,void *mm){
	_ioreg=(u32 *)m;
	_mem=(u8 *)mm;
	return 0;
}

int PS2DEV::__CP0::_rfe(u32 *pc){
	_regs[SR] &= ~SR_EXL;
	*pc=_regs[EPC];
	return 0;
}

int PS2DEV::__CP0::_reset(){
	memset(_regs,0,sizeof(_regs));
	_regs[SR] = 0x70030c11; // COP0 enabled | BEV = 1 | TS = 1
	_regs[PRID] = 0x00000002; // PRevID = Revision ID, same as R3000A
	return 0;
}

int PS2DEV::__CP0::_op(u32 s){
	switch(s & 0x3f){
		case 0x38:
			_regs[SR] |= SR_EIE;
			//ei
		break;
		case 0x39:
			//di
			_regs[SR] &= ~SR_EIE;
		break;
		default:
			EnterDebugMode();
			printf("cp cop %x \n",s);
			break;
	}
	return 0;
}

PS2ROM::PS2ROM() : Game(){
	_type=-1;
}

PS2ROM::~PS2ROM(){
}

int PS2ROM::Open(char *path,u32){
	LElfFile *p;
	int res=-1;

	_files.clear();
	if(!(_data = new RomStream(path)))
		goto Z;
	_type=1;
	if(!((RomStream *)_data)->Parse(this,&p))
		goto Y;
	delete (RomStream *)_data;
	_data=NULL;
	res=-10;
	_type=-1;
	goto Z;
Y:
	res=0;
	_machine="ps2";
Z:
	if(res)
		Close();
	return res;
}

int PS2ROM::Read(void *buf,u32 sz,u32 *po){
	if(!_data)
		return -1;
	//fread(PSXM(SWAP32(tmpHead.t_addr)), SWAP32(tmpHead.t_size), 1, (FILE *)_data);
	return ((RomStream *)_data)->Read(buf,sz,po);
}

int PS2ROM::Write(void *buf,u32 sz,u32 *po){
	if(!_data)
		return -1;
	return -1;
}

int PS2ROM::Seek(s64 a,u32 b){
	if(!_data)
		return -1;
	//fread(PSXM(SWAP32(tmpHead.t_addr)), SWAP32(tmpHead.t_size), 1, (FILE *)_data);
	return ((RomStream *)_data)->Seek(a,b);
}

int PS2ROM::Tell(u64 *a){
	if(!_data)
		return -1;
	//fread(PSXM(SWAP32(tmpHead.t_addr)), SWAP32(tmpHead.t_size), 1, (FILE *)_data);
	return ((RomStream *)_data)->Tell(a);
}

int PS2ROM::Close(){
	if(_data)
		delete (RomStream *)_data;
	_data=NULL;
	_type=-1;
	return 0;
}

int PS2ROM::Query(u32 w,void *pv){
	switch(w){
		case IGAME_GET_INFO:
			if(Game::Query(w,pv))
				return -1;
			if(_data){
				void *o;
				u32 d=0;

				o=NULL;
				((RomStream *)_data)->_getInfo(&d,&o);
				if(!o) return -1;

				*((u32 *)pv + IGAME_GET_INFO_SLOT)=((u32 *)o)[2];
				*((u32 *)pv + IGAME_GET_INFO_SLOT+4)=((u32 *)o)[1];
				*((u32 *)pv + IGAME_GET_INFO_SLOT+2)= ((u32 *)o)[0];
				*((u32 *)pv + IGAME_GET_INFO_SLOT+1) = ((u32 *)o)[3];
//printf("%x %p %x %x %x %x\n",d,o,((u32 *)o)[0],((u32 *)o)[1],((u32 *)o)[2],((u32 *)o)[3]);
				delete[]((u32 *)o);
			}
			return 0;
		default:
			return Game::Query(w,pv);
	}
}

RomStream::RomStream() : ISOStream(){
	_start=0;
	_data=NULL;
}

RomStream::RomStream(char *p) : ISOStream(p){
	_start=0;
	_data=NULL;
}

RomStream::~RomStream(){
	if(_data)
		delete (LElfFile*)_data;
	_data=NULL;
}

int RomStream::Parse(IGame *game,LElfFile **p){
	LElfFile *f;

	if(FileStream::Open((char *)_filename.c_str()))
		return -1;
	if(!(f=new LElfFile(this)))
		return -2;
	if(f->Open()){
		delete f;
		return -3;
	}
	*p=f;
	_data=f;
	game->AddFile((char *)_filename.c_str(),1);
	return 0;
}

int RomStream::SeekToStart(s64 a){
	return FileStream::Seek(_start+a,SEEK_SET);
}

int RomStream::_getInfo(u32 *psz,void **o){
	if(!_data || !psz)
		return -1;
	return ((LElfFile *)_data)->_getInfo(psz,o);
	//ProgramHeader[i].offset
}

};