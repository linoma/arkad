#include "ps1m.h"

namespace ps1{


PS1DEV::PS1DEV() : R3000Cpu(),PS1GPU(),PS1SPU(){
}

PS1DEV::~PS1DEV(){
}

int PS1DEV::Init(PS1M &g){
	for(int i =0;i<sizeof(_timers)/sizeof(__timer);i++)
		_timers[i].init(i,_io_regs,_mem);
	for(int i=0;i<sizeof(_dmas) / sizeof(struct __dma *);i++)
		_dmas[i]->init(i,_io_regs,_mem);
	_cdrom.init(0,_io_regs,_mem);
	_mdec.init(0,_io_regs,_mem);
	_joy.init(_io_regs,_mem);
	return 0;
}

int PS1DEV::Reset(){
	R3000Cpu::Reset();
	CP0._reset();
	PS1IOREG(0x1F801000)=0x1F000000;
	PS1IOREG(0x1F801018)=0x20843;
	PS1IOREG(0x1F8010f0)=0x07654321;
	for(int i =0;i<sizeof(_timers)/sizeof(__timer);i++)
		_timers[i].reset();
	for(int i=0;i<sizeof(_dmas)/sizeof(struct __dma *);i++)
		_dmas[i]->reset();
	_cdrom.reset();
	_mdec.reset();
	GTE._reset();
	_joy._reset();

	return 0;
}

int PS1DEV::do_sync(u32 dev,...){
	va_list arg;
	int res;

	res=0;
	va_start(arg, dev);
	switch(dev&0xf){
		case 1://mdec
			if((dev&0x100) && !_mdec._dof)
				res |= 0x100;
			if((dev&0x200) && !_mdec._dif)
				res |= 0x200;
		break;
		case 2://GPU
			return PS1IOREG(0x1f801814) & BV(26) ? 0x100 :0;
			switch(SR(dev,4)&15){
				case 2:
					return 0x100;
				case 1:
					return 0x100;
			}
		break;
	}
	va_end(arg);
	return res;
}

int PS1DEV::do_dma(struct __dma *dma){
	u32 dst,n,d;
	int  res;

#ifdef _DEVELOP
	DLOG("DMA do dma %d %x %x=>%x %x",dma->_idx,dma->_sync,dma->_src,dma->_dst,dma->_count);
#endif
	for(n=0;dma->_count;dma->_count--,n++){
		RL(dma->_src,d);
		dst=dma->_dst;
		switch(dma->_do_transfer(d,&d)){
			case 0:
				WL(dst,d);
			break;
			case 2:{
				s32 r_;

				__data=d;
				WIO(dst,0,&__data,AM_DWORD|AM_DMA,r_=);
				if(r_== (u32)-2 && n)
					dma->_count=1;
			}
			break;
			case 3://cdrom
				WB(dst,d);
				if(!_cdrom._dfe) dma->_count=1;
			break;
		}
	}
	dma->_end_transfer(0,*((PS1M *)this));
	res=0;
	if(PS1IOREG(0x1f8010F4) & BV(16 + dma->_idx)){
		PS1IOREG(0x1f8010F4) |= BV(24+dma->_idx);
	}
	if(
		(PS1IOREG(0x1f8010F4) & BV(15)) || ((PS1IOREG(0x1f8010F4) & BV(23)) && (PS1IOREG(0x1f8010F4) & 0x7f000000))
		 ){
		PS1IOREG(0x1f8010F4) |= BV(31);
		//PS1IOREG(0x1f8010F4) |= BV(31);
		res=1;
	}
	else
		PS1IOREG(0x1f8010F4) &= ~BV(31);
	return res;
}

PS1DEV::__timer::__timer(){
	_status=0;
}

int PS1DEV::__timer::init(int n,void *m,void *mm){
	_idx=n;
	_io_regs=(u32 *)m;
	_mem=(u8 *)mm;
	return 0;
}

int PS1DEV::__timer::reset(){
	_status=0;
	_elapsed=0;
	_counter=0;
	_step=0;
	return 0;
}

int PS1DEV::__timer::write(u32 a,u32 b){
	DLOG("TIMER %d  %x %x",_idx,a,b);
	switch(a & 0xc){
		case 0:
			_counter=(u16)b;
			_elapsed=__cycles;
		break;
		case 4:
			//printf("timer %d %x %x D:%x\n",_idx,a,b,__data);
			//EnterDebugMode();
			_status=b;
			_elapsed=__cycles;
			_counter=0;
			_count();
			//if(_imode) return _idx+1;
		break;
		case 8:
		break;
	}
	return 0;
}

int PS1DEV::__timer::read(u32 a,u32 *b){
	switch(a&0xc){
		case 0:
			*b=_count();
		break;
		case 4:
			_count();
			*b=(u32)_status;
			_reached=0;
		break;
	}
	return 0;
}

u32 PS1DEV::__timer::_count(){
	u32 vv,v;

	vv=__cycles;
	if(_elapsed)
		vv=_elapsed <__cycles ? __cycles-_elapsed: (MHZ(33)-_elapsed)+__cycles;
	switch(_source){
		case 2:
			switch(_idx){
				case 2:
					v = (u32)(u16)SR(vv,3);//sysem/8
					goto Z;
			}
		case 0:
			v = (u32)(u16)vv;
		break;
		case 1:
			switch(_idx){
				case 0:
					v = (u32)(u16)SR(vv,3+9);//dot
				break;
				case 1://hblank
					v = (u32)(u16)SR(vv,11);
				break;
				case 2:
					v = (u32)(u16)vv;
				break;
			}
		break;
		case 3:
			switch(_idx){
				case 0:
					v = (u32)(u16)SR(vv,3+9);//dot
				break;
				case 1://hblank
					v = (u32)(u16)SR(vv,12);
				break;
				case 2:
					v = (u32)(u16)SR(vv,3);//sysem/8
				break;
			}
		break;
	}
Z:
	switch(_idx){
		case 0:
		break;
		case 1:
		break;
		case  2:
		break;
	}
	if(!v)
		return _counter;
	vv=_counter;
	_counter += v;
	if(vv > _counter){
		_reached=1;
	}
	_elapsed=__cycles;
	return _counter;
}

int PS1DEV::__timer::Run(u8 *,int cyc,void *){
	return 0;
}

int PS1DEV::__joy::_reset(){
	memset(_pads,0xff,sizeof(_pads));
	_pads[0]._buf[0]=0;
	_pads[1]._buf[0]=0xff;
	_pads[0]._buf[1]=0x41;
	_pads[1]._buf[1]=0x41;
	_pads[0]._buf[2]=0x5a;
	_pads[1]._buf[2]=0x5a;
	for(int i=0;i<sizeof(_cards)/sizeof(__card);i++)
		_cards[i]._reset();
	return 0;
}

int PS1DEV::__joy::_read(u32 a,u32 *v){
	*v=PS1IOREG(0x1f01040)=_pads[_ch]._buf[_cr++];
	return 0;
}

int PS1DEV::__joy::_write(u32 a,u32 v){
	//printf("joy w %x %x\n",a,v);
	switch((u8)a){
		case 0x40:
			_buf[_cw++]=(u8)v;
			_tx1=1;
			switch(_buf[0]){
				case 1:
					_rx=1;
				break;
				case 0x81:
					printf("crd\n");
				break;
			}
		break;
		case 0x4a:
			_ch=SR(v&0x2000,13);
			_cw=_cr=0;
			_tx1=1;
		break;
	}
	PS1IOREG(0x1F801044)=_status;
	return 0;
}

int PS1DEV::__joy::init(void *io,void *m){
	_io_regs=(u32 *)io;
	for(int i=0;i<sizeof(_cards)/sizeof(__card);i++)
		_cards[i]._init(io,m);
	return 0;
}

int PS1DEV::__joy::_key_event(u32 k,u32 a){
	if(!(a&1))
		_pads[0]._buttons |= BV(k);
	else
		_pads[0]._buttons  &= ~BV(k);
	return 0;
}

int PS1DEV::__joy::__card::_reset(){return -1;}

int PS1DEV::__joy::__card::_write(u32,u32){return -1;}

int PS1DEV::__joy::__card::_read(u32,u32 *){return -1;}

int PS1DEV::__joy::__card::_init(void *,void *){return -1;}

int PS1DEV::__joy::__card::_open(char *fn,u32 a){
	printf("open %x %s\n",a,fn);return -1;
}

PS1DEV::__dma::__dma(){
	_status64=0;
}

int PS1DEV::__dma::write(u32 r,u32 v,PS1M &g){
	DLOG("DMA W %d %x %x",_idx,r,v);
	switch(r){
		case 8:{
			int res;

			res=0;
			/*if(_state) {
				_transfer(g);
				g.DelTimerObj(this);
				printf("flsuda %d\n",_idx);
			}*/
			_status=v;
			_src= PS1IOREG(0x1F801080+SL(_idx,4));
			_count=PS1IOREG(0x1F801084+SL(_idx,4));
			_inc= _step ? -4 : 4;

			//v &= ~BV(28);
			_prepare_dma();
			//printf("da %d %x %x\n",_idx,_start,PS1IOREG(0x1f8010f0));
			if(_start && (PS1IOREG(0x1f8010f0) & BV(3+SL(_idx,2)) ) ){
				switch(_sync){
					case 0:
				//	case 2:
						_state=0;
						v &= ~(BV(24)|BV(28));
						if(g.do_dma(this))
							g.OnEvent(3,1);
					break;
					default:
						//PS1IOREG(0x1f801814) &= ~BV(28);
						res=0xc002;
						_state=1;
					break;
				}
			}
			PS1IOREG(0x1f801088+SL(_idx,4))=v;
			return res;
		}
		break;
	}
	return 1;
}

int PS1DEV::__dma::reset(){
	_status=0;
	_state=0;
	_count=_src=_dst=0;
	return 0;
}

int PS1DEV::__dma::init(int n,void *m,void *mm){
	_io_regs=(u32 *)m;
	_mem=(u8 *)mm;
	_idx=n;
	return 0;
}

int PS1DEV::__dma::_prepare_dma(){
	switch(_idx){
		case 3:{//CDROM
			if(!_dir){
				u32 v=_src;
				_src=0x1f801802;
				_dst=v;
			}
			_count=SL((u16)_count,2);
		}
		break;
	}
	return 0;
}

int PS1DEV::__dma::_transfer(PS1M &g){
	PS1IOREG(0x1f801088+SL(_idx,4)) &= ~(BV(28));
	if(g.do_dma(this))
		g.OnEvent(3,1);
	//if(_idx==0 && (PS1IOREG(0x1f801088+SL(_idx,4)) & 0x200)) EnterDebugMode();
	PS1IOREG(0x1f801088+SL(_idx,4)) &= ~(BV(24));
	_state=0;
	return 0;
}

int PS1DEV::__dma::_end_transfer(u32,PS1M &g){
	switch(_sync){
		case 2:
			PS1IOREG(0x1f801080+SL(_idx,4))=0x00ffffff;
		break;
		case 1:
			PS1IOREG(0x1f801080+SL(_idx,4))=_dst;
		break;
	}
	EnterDebugMode(DEBUG_BREAK_DEVICE);
	return 0;
}

int PS1DEV::__dma::Run(u8 *,int cyc,void *obj){
	u32 v;

	PS1M *p=(PS1M *)((LPCPUTIMEROBJ)obj)->param;
	if(_state==2)
		goto B;
	v=_sync<<4;
//	printf("da sync %d\n",_idx);
	switch(_idx){
		case 0:
			v|=0x201;
			if(! (((PS1DEV *)p)->do_sync(v) & 0x200))
				return 0;
		break;
		case 1:
			v|=0x101;
			if(! (((PS1DEV *)p)->do_sync(v) & 0x100))
				return 0;
		break;
		case 2:
			v|=0x102;
			if(! (((PS1DEV *)p)->do_sync(v) & 0x100))
				return 0;
		break;
		default:
		break;
	}
	PS1IOREG(0x1f801088+SL(_idx,4)) &= ~(BV(28));
	_state=2;
	//((LPCPUTIMEROBJ)obj)->elapsed= _count*4;
	return 0x80000000|(SL(_count,3));
B:
	_transfer(*(p));
	return -1;
}

int PS1DEV::__dma::_do_transfer(u32 a,u32 *r){
	switch(_idx){
		case 3:
			_dst++;
			return 3;
	}
	return 0;
}

int PS1DEV::__gpu_dma::_prepare_dma(){
	_dst=0x1F801810;
	switch(_sync){
		case 1:
			_count=SR(_count,16)*(u16)_count;
		break;
		default:
			_count=1;
		break;
	}
//	printf("gpu dma %x %x %x\n",_count,_sync,_src);
	return 0;
}

int PS1DEV::__gpu_dma::_do_transfer(u32 v,u32 *r){
	switch(_sync){
		case 0:
		case 1:
		//	_dst+=4;
			_src += _inc;
			return 0;
		default:
		//	_count++;
			_src += _inc;
		break;
	}
	return 2;
}

int PS1DEV::__gpu_dma::_transfer(PS1M &g){
	u32 n,_srcL,d,i;

//	if(lino){ if(_src==0x800d95f0) EnterDebugMode();printf("gpudma  %x  %x\n",_src,_sync);}
	switch(_sync){
		case 0:
		case 1:
			return __dma::_transfer(g);
	}

	_srcL=_src & 0x1fffff;
	n=_srcL;
	PS1IOREG(0x1f801088+SL(_idx,4)) &= ~(BV(28));
	//printf("%x;%x;\t",n,*(u32 *)(_mem + n));

	for(d=0,i=0;d != 0xffffff;i++){
		d= *(u32 *)(_mem + n);
		_count=SR(d,24);
		_src=n+4;
		n = d & ~0xff000000;
		//if(_count) printf("%x;%x;%x %x\n",n,d,_src,_count);
		if(_count && g.do_dma(this))
			g.OnEvent(3,1);
		///if(d==n) break;
		d=n;
	//	break;
	}
Z:
	//if(i>1) EnterDebugMode();
///	printf("%u\n",i);
	PS1IOREG(0x1f801088+SL(_idx,4)) &= ~(BV(24));
	_state=0;
	return 0;
}

int PS1DEV::__otc_dma::_prepare_dma(){
	_count=(u16)_count;
	_dst=_src;///+SL(_count,2);
	_inc=-4;
	//printf("do __otc %d %x %x %x %x %x\n",_idx,_mode,_src,v,_count,_dst);
	return 0;
}

int PS1DEV::__otc_dma::_do_transfer(u32 v,u32 *r){
	if(_count == 1)
		*r=0xffffff;
	else{
		_dst += _inc;
		*r =(_dst & 0x1fffff);
	}
	return 0;
}

int PS1DEV::__spu_dma::_prepare_dma(){
	_dst=0x1F801DA8;
	switch(_sync){
		case 1:
			_count=SR(_count,16)*(u16)_count;
		break;
		default:
			_count=1;
		break;
	}
	//printf("do __spu %d %x %x %x %x %x\n",_idx,_sync,_src,v,_count,_dst);
	return 0;
}

int PS1DEV::__spu_dma::_do_transfer(u32 v,u32 *r){
	_src += 4;
	//*r = _count == 1 ? 0xffffff : _dst;
	return 0;
}

int PS1DEV::__spu_dma::_end_transfer(u32 a,PS1M &m){
	*(u16 *)(PS1IOREG8_(0x1f801dae,_io_regs)) &=~0x20;
	//PS1IOREG(0x1f801dae) &=~0x20;
	//printf("end sppudma %x\n",PS1IOREG(0x1f801dae));
	return __dma::_end_transfer(a,m);
}

int PS1DEV::__mdec_dma::_prepare_dma(){
	_dst=0x1F801820;
	if(_idx == 1){
		u32 s=_src;
		_src = _dst;
		_dst = s;
		//EnterDebugMode();
	}
	switch(_sync){
		case 0:
			_count=(u16)_count;
		break;
		case 1:
			_count=SR(_count,16)*(u16)_count;
		break;
	}
#ifdef _DEVELOPa
	printf("do __mdec_dma %d %x %x %x %x %x\n",_idx,_sync,_src,v,_count,_dst);
#endif
	return 0;
}

int PS1DEV::__mdec_dma::_do_transfer(u32 v,u32 *r){
	switch(_idx){
		case 0:
			_src += _inc;
		break;
		case 1:
			_dst += _inc;
		break;
	}
	//*r = _count == 1 ? 0x00ffffff : _dst;
	return 0;
}

//#define CD_SET_INT(a) {if(_regs[13] & 7) _irqp=a;else _regs[13]=(_regs[13] & 0xe0)|a;}
#define CD_SET_INT(a) {_regs[REG_ISTAT]=(_regs[REG_ISTAT] & 0xe0)|a;_regs[15]=_regs[REG_ISTAT];}
#define CD_CLEAR_INT() CD_SET_INT(0)
#define CD_RET_IRQ(a) return _regs[REG_IMASK] != 0 ?  1 : 0;

#define CMD_START_()\
	switch(_state++){\
		case 1:\
			_rfe=0;\
			_dfe=0;\
			_cr=0;\
			_cw=0;\
			_regs[REG_WANT]=0;\
			return 0x10002;\

#define CMD_START(a)\
	CMD_START_()\
		case 2:\
			CD_SET_INT(a);\
			CD_RET_IRQ(a);\

#define CMD_END()\
	default:\
		_param=0;\
		return -1;\
	}


PS1DEV::__cdrom::__cdrom(){
	_regs=_buffer=_params=NULL;
}

PS1DEV::__cdrom::~__cdrom(){
	if(_regs) delete []_regs;
	_regs=_buffer=NULL;
}

int PS1DEV::__cdrom::reset(){
	_control=0;
	if(!_regs)
		return 0;
	memset(_regs,0,5000);
	_regs[9]=0x1f;
	_status=0;
	_track=0;
	return 0;
}

int PS1DEV::__cdrom::Run(u8 *mem,int cyc,void *obj){
	int res;

	if(!_state)
		return -1;
	//printf("cdom %u \n",cyc);
	switch((u8)(res=_execCommand(1))){
		case 2:
			res = SR(res,8) | 0x80000000;
		case 0:
		case 0xff:
		break;
		default:
			return 4;
	}
	return res;
}

int PS1DEV::__cdrom::Query(u32 what,void *pv){
	switch(what){
		case IGAME_GET_ISO_FILEI:
			if(!_game)
				return -1;
			return _game->Query(what,pv);
	}
	return -1;
}

int PS1DEV::__cdrom::init(int,void *m,void *mm){
	_io_regs=(u32 *)m;
	_mem=(u8 *)mm;
	if(!(_regs= new u8[5000]))
		return  -1;
	_params=_regs+0x40;
	_subq=(struct __subq *)(_params+0x10);
	_buffer = (u8 *)(_subq + 2);
	return 0;
}

int PS1DEV::__cdrom::update(u32){
	return 0;
}

int PS1DEV::__cdrom::write(u32 a,u32 v,PS1M &){
	//DLOG("CDROM W %x;%x=>%x",a,_pidx,v);
	switch(a&3){
		case 0:
			_pidx=v;
			_regs[0]=(_regs[0] & 0xf8) | (v&7);
			return 0;
		case 1:
			switch(_pidx){
				case 0:
#ifdef _DEVELOP
	DLOG("CDROM _execCommand %x %x %x",v,_param,_mode);
#endif
					_regs[4]=v;
					_state=1;
					CD_CLEAR_INT();
					return _execCommand(0);
				default:
					return 0;
			}
		break;
		case 2:
			switch(_pidx){
				case 0:
					_params[_param++]=v;
				break;
			}
		break;
		case 3:
			switch(_pidx){
				case 1://irq clear
					DLOG("CDROM IRQ ACK %x %x %x %x %x",_regs[13],v,_state,_regs[4],_mode);
					_regs[REG_ISTAT] &= ~v;
					_regs[15]=_regs[REG_ISTAT];

					if(_play &&_report)
						return _do_report(0);
					if(_state==3) _state=4;
					return 0;
			}
		break;
	}
	_regs[(a&3)*4+_pidx]=(u8)v;
	return 0;
}

int PS1DEV::__cdrom::read(u32 a,u32 *pv){
	switch(a&3){
		case 0:
			*(u8 *)pv=(u8)_control;
		break;
		case 1://read response
			*(u8 *)pv=_buffer[_cr++];
			//if(_cr >= 4096)
				//_cr = 4095;
			if(_cw) _cw--;
			//_rfe=0;
			if(!_cw){
				_rfe=0;
				_param=0;

				if(_reading && (_regs[4]==CMD_READS || _regs[4]==CMD_READN)){
					_state=5;
					_execCommand(0);
				}
			}
		break;
		case 2://read data
			*(u8 *)pv=_buffer[_cr++];

			if(_cw)
				_cw--;
			//_rfe=0;

			if(!_cw){
				_rfe=0;
				_dfe=0;
				//_state=0;
				_param=0;
			}
		break;
		case 3:
			*(u8 *)pv=_regs[(a&3)*4+_pidx];
		break;
	}
//	printf("cdrom read %x %x %x\n",a,_pidx,*pv);
	return 0;
}

int PS1DEV::__cdrom::_do_report(int w){
	_regs[4]=-1;
	_state=1;
	CD_CLEAR_INT();
	return 2|SL(0x90000,6);
}

int PS1DEV::__cdrom::_execCommand(int){
	switch(_regs[4]){
		case CMD_NOP://sta-nop
			CMD_START_()
				case 2:
					_rfe=1;
					_cw=1;
					_buffer[0]=_status;
					CD_SET_INT(3);
					CD_RET_IRQ(3);
			CMD_END();
		case CMD_SETLOC://setloc 0x2
			CMD_START_()
				case 2:{
					_pos=MSF2SECT(btoi(_params[0]),btoi(_params[1]),btoi(_params[2]));
#ifdef _DEVELOP
				for(int i=0;i<_param;i++)
					printf("%x ",_params[i]);
				printf("\tM:%x %x %llu %llx\n",_mode,_param,_pos,_pos*CD_FRAMESIZE_RAW+CDIO_CD_SYNC_SIZE);
			if(_pos==112824){
				//lino=1;
				//EnterDebugMode();
				}
#endif
					_pos=_pos*CD_FRAMESIZE_RAW+CDIO_CD_SYNC_SIZE;
					if(_game) _game->Seek(_pos,SEEK_SET);//fixme
					_rfe=1;
					_cw=1;
					_status=0;
					_spinning=1;
					//_seek=1;
					_buffer[0]=_status;
					//EnterDebugMode();
					CD_SET_INT(3);
					CD_RET_IRQ(3);
				}
			CMD_END()
		break;
		case CMD_SETMODE://setmode 0xe
			CMD_START_()
				case 2:{
				//for(int i=0;i<_param;i++) printf("%x ",_params[i]);
			//	printf("\t%x\n",_param);
				_mode=_params[0];
				u32 a=(!(_mode&0x60)?0x800:0x924)+CDIO_CD_SYNC_SIZE ;
				_game->Query(IGAME_SET_ISO_MODE,&a);
				_param=0;
				_rfe=1;
				_cw=1;
				_buffer[0]=_status;
				CD_SET_INT(3);
				CD_RET_IRQ(3);
			}
			CMD_END()
		break;
		case CMD_MUTE:
		case CMD_DEMUTE:
			CMD_START(3)
				case 3:
					_rfe=1;
					_cw=1;
					_buffer[0]=_status;
					return 0;
			CMD_END();
		break;
		case CMD_READS://readS
		case CMD_READN://readN
			CMD_START_()
				case 2:
					_rfe=1;
					_cw=1;
					_status=0;
					_spinning=1;
					_fxas=1;
					_buffer[0]=_status;if(_mode & 1) printf("cdda\n");
					CD_SET_INT(3);
					CD_RET_IRQ(3);
				case 3:
					_state=3;
					return 0;
				case 4:
					_reading=1;
					_game->Seek(_pos,SEEK_SET);
					_game->Read(&_buffer[1],!(_mode & 0x60) ? 0x80c:0x930);
					_game->Tell(&_pos);
				//	DLOG("CDROM READ%c %x %x %x %x",_regs[4]==6?78:83,_buffer[1],_buffer[2],_buffer[3],_buffer[7]);
					if((_mode & 0x40)){
						if(_regs[4]==CMD_READS && _buffer[7] & 4){
							//printf("%x  %x %x\n",_buffer[1],_buffer[2],_buffer[3]);
							machine->OnEvent(PS1ME_PLAY_XA_SECTOR,&_buffer[5],_fxas);
							_fxas=0;
							_state=4;
							return 2|SL(90000,8);//fixme
						}
					}
					//if(_regs[4]==6) _pos+=12;
					_rfe=1;
					_dfe=0;
					_cw=1;
					_cr=0;
					_buffer[0]=_status;
					CD_SET_INT(1);
					CD_RET_IRQ(1);
				case 5:{
					if(!_regs[REG_WANT]){
						_state=5;
						return 0;
					}
					_dfe=1;
					_cw=0x800+CDIO_CD_SYNC_SIZE;
					_cr=1;
					//memcpy(_buffer,&_buffer[1],0x800);
					if(!(_mode&0x20)){
						//memcpy(&_buffer[1],&_buffer[CDIO_CD_SYNC_SIZE+1],0x924);
						_cr +=  CDIO_CD_SYNC_SIZE;
						//if(!(_mode&0x40))
						_cw -= CDIO_CD_SYNC_SIZE;
					}
					return 2|SL(90000,8);//fixme
				}
				case 6:
					//_spinning=0;
					//_reading=0;
					if(_cw) {
						_state=6;
						return 0;
					}
					_state=4;
					return 0;
			CMD_END()
		break;
		case CMD_PAUSE:
		case CMD_INIT://init
			//printf("cdrom a %u %u %x\n",_state,__cycles,_regs[9]);
			CMD_START_()
				case 2:
					_rfe=1;
					_cw=1;
					_buffer[0]=_status;
					if(_play)
						machine->OnEvent(PS1ME_PLAYTRACK,0,0,0);
					_status=0;
					_spinning=1;
					CD_SET_INT(3);
					CD_RET_IRQ(3);
				case 3:
					_state=3;
					return 0;
				case 4:
					_rfe=1;
					_cw=1;
					_cr=0;
					_buffer[0]=_status;
					CD_SET_INT(2);
					CD_RET_IRQ(2);
			CMD_END()
		break;
		case CMD_GETTN://8008a078
			CMD_START_()
				case 2:{
					u32 d[10]={0};
					_rfe=1;
					_cw=1;
					_buffer[0]=1;
					d[0]=(u32)-1;
					if(_game && !_game->Query(IGAME_GET_ISO_INFO,d)){
						//d[2]=d[1]+2;
						_buffer[0]=0;
						_buffer[1]=itob(d[1]);
						_buffer[2]=itob(d[2]);
						_cw=3;
					}
					//EnterDebugMode();
					CD_SET_INT(3);
					CD_RET_IRQ(3);
				}
			CMD_END()
		break;
		case CMD_GETTD:
			CMD_START_()
				case 2:{

					u32 d[10]={0};
					int i;
				//for(int i=0;i<_param;i++) printf("%x ",_params[i]);
				//printf("\t%x\n",_param);
					_param=0;
					_rfe=1;
					_buffer[0]=1;
					_cw=1;
					d[0]=btoi(_params[0]);//8008a078
					if(_game && !(i=_game->Query(IGAME_GET_ISO_INFO,d))){
						if(_params[0]==0)
							d[4]=d[5];
						else if(_params[0]>1)
							d[4]+=CD_FRAMESIZE_RAW*75*2;
						d[4] /= CD_FRAMESIZE_RAW;
						//printf("sect  %u %u %u:%u:%u\n",d[0],d[4],d[4]/(75*60),(d[4]/75)%60,d[4]%75);
						_cw=3;
						_buffer[0]=0;
						u8*c=&_buffer[1];
						SECTMSF2B(d[4],c);
						for(int i=0;i<_cw;i++)
							_buffer[i]=itob(_buffer[i]);
					}
					else
						printf("error %x %x %d\n",_regs[4],_params[0],i);
					//EnterDebugMode();
					CD_SET_INT(3);
					CD_RET_IRQ(3);
				}
			CMD_END()
		break;
		case CMD_SEEKP:
		case CMD_SEEKL:
			CMD_START_()
				case 2:
					_rfe=1;
					_cw=1;
					_buffer[0]=_status;
					_status=0;
					CD_SET_INT(3);
					CD_RET_IRQ(3);
				case 3:
					_state=3;
					return 0;
				case 4:
					//if(_game)
					//	_game->Seek(_pos*CD_FRAMESIZE_RAW,SEEK_SET);//
					///printf("seekl\n");
					//EnterDebugMode();
					_rfe=1;
					_cw=1;
					_cr=0;
					_spinning=1;
				//	_seeking=1;
					_buffer[0]=_status;

					if(_regs[4]==CMD_SEEKP)	{
						u32 d[5000];

						memset(d,0,40);
						d[3]=1;
						d[0]=_pos;
						d[3]|=2;
						d[1]=0;
						if(_game && !(_game->Query(IGAME_GET_ISO_INFO,d))){
							memset(_subq,0,sizeof(__subq));
							_track=d[6];
							_subq->_track=_track;
							_subq->_index=0;
							u32 pp = ((_pos)-CDIO_CD_SYNC_SIZE)/CD_FRAMESIZE_RAW;
							SECTMSF2B(pp,_subq->_abs);
						}
					}
					CD_SET_INT(2);
					CD_RET_IRQ(2);
			CMD_END()
		break;
		case CMD_PLAY:
			CMD_START_()
				case 2:{
					u32 d[5000];
					int i;

					_rfe=1;
					_cw=1;
					memset(d,0,40);
					d[3]=1;
					if(_param)
						d[0]=btoi(_params[0]);
					else{
						d[0]=_pos;
						d[3]|=2;
					}
					_status=0;
					_spinning=1;
					d[1]=0;
					if(_game && !(i=_game->Query(IGAME_GET_ISO_INFO,d))){
						char *p= (char *)&d[7];
						_track=d[6];
						machine->OnEvent(PS1ME_PLAYTRACK,d[0],p,_param ? 0:_pos-d[4]);
						_play=1;
						memset(_subq,0,sizeof(__subq));
						_subq->_track=_track;
						_subq->_index=1;
						u32 pp = ((_pos)-CDIO_CD_SYNC_SIZE)/CD_FRAMESIZE_RAW;
						SECTMSF2B(pp,_subq->_abs);
					}
					_buffer[0]=_status;
					CD_SET_INT(3);
					CD_RET_IRQ(3);
				}
				case 3:
					if(_play && _report)
						return _do_report(0);
			CMD_END()
		break;
		case CMD_GETLOCP:
			CMD_START_()
				case 2:

					memcpy(_buffer,_subq,sizeof(__subq));
					//u32 p = ((_pos)-CDIO_CD_SYNC_SIZE)/CD_FRAMESIZE_RAW;
					//printf("getloccp %u %u %u\n",_subq->_track,_subq->_index,_buffer[0]);
					_cw=8;
					for(int i=0;i<_cw;i++)
						_buffer[i]=itob(_buffer[i]);
					_cr=0;
					_rfe=1;
					CD_SET_INT(3);
					CD_RET_IRQ(3);
			CMD_END()
		break;
		case 0xff:
			CMD_START_()
				case 2:
				{
					u8 *c;

					_pos += 176400;
					u32 p = ((_pos)-CDIO_CD_SYNC_SIZE)/CD_FRAMESIZE_RAW;
					//_pos+=2352;
					memset(_buffer,0,9);

					c=&_buffer[6];
					SECTMSF2B(p,c);
					//printf("Cd REPORT %u %u %u\n",c[0],c[1],c[2]);
				//	EnterDebugMode();
					_buffer[1]=2;
					_cw=9;
					for(int i=0;i<_cw;i++)
						_buffer[i]=itob(_buffer[i]);
					_cr=0;
					_rfe=1;
					//_status=0;
					//_spinning=1;
					//_fxas=1;
					_buffer[0]=_status;
					CD_SET_INT(1);
					CD_RET_IRQ(1);
				}
			CMD_END()
		break;
		default:
			printf("_execCommand %x\n",_regs[4]);
			EnterDebugMode();
			//_regs[13]=(_regs[13]&~7)|3;
		break;
	}
	return -1;
}

int PS1DEV::__cdrom::_load(IGame *g){
	_game=g;
	return 0;
}

int PS1DEV::__CP0::_reset(){
	memset(_regs,0,sizeof(_regs));
	_regs[SR] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	_regs[PRID] = 0x00000002; // PRevID = Revision ID, same as R3000A
	return 0;
}

int PS1DEV::__CP0::_rfe(u32 *pc){
	_regs[SR]=(_regs[SR] & ~0xf)|SR(_regs[SR]&0x3c,2);
	*pc=_regs[EPC];
	return 0;
}

int PS1DEV::_enterIRQ(int n,u32 pc){
	if(!(PS1IOREG(0x1f801074) & n))
		return 1;
	if(R3000Cpu::_enterIRQ(n,pc))
		return 2;
	if(!(CP0._regs[CP0.SR] & 1))
		return 3;
	return 0;
}

};