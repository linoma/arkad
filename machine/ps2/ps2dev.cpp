#include "ps2.h"

namespace ps2{

PS2DEV::PS2DEV() : R5900Cpu(){
}

PS2DEV::~PS2DEV(){
}

int PS2DEV::Init(PS2M &g){
	if(R5900Cpu::Init(&g._memory[MB(2)]))
		return -1;
	_io_regs=(u32 *)&_mem[MB(32)];
	//for(int i =0;i<sizeof(_timers)/sizeof(__timer);i++)
	//	_timers[i].init(i,_io_regs,_mem);
//	for(int i=0;i<sizeof(_dmas) / sizeof(struct __dma *);i++)
//		_dmas[i]->init(i,_io_regs,_mem);
	_dvdrom.init(0,_io_regs,_mem);
	return 0;
}

int PS2DEV::Reset(){
	R5900Cpu::Reset();
	//for(int i =0;i<sizeof(_timers)/sizeof(__timer);i++)
	//	_timers[i].reset();
	//for(int i=0;i<sizeof(_dmas)/sizeof(struct __dma *);i++)
	//	_dmas[i]->reset();
	_dvdrom.reset();
	CP0._reset();
	CP1._reset();
	return 0;
}

int PS2DEV::do_dma(struct __dma *dma){
/*	u32 dst,n=0;
	for(;dma->_count;dma->_count--,n++){
		u32 d;
		s32 res;

		RL(dma->_src,d);
		dst=dma->_dst;
		switch(dma->_do_transfer(d,&d)){
			case 0:
				WL(dst,d);
			break;
			case 2:
				__data=d;
				WIO(dst,0,&__data,AM_DWORD|AM_DMA,res=);
				if(res==(u32)-2)
					dma->_count=1;
			break;
		}
	}
	if(_io_regs[RMAP_IO(0x1f8010F4)]&BV(16+dma->_idx)){
		_io_regs[RMAP_IO(0x1f8010F4)] |= BV(24+dma->_idx);
		_io_regs[RMAP_IO(0x1f801070)] |= BV(3);
	}
	//if(dma->_idx == 2) printf("dma %d %u\n",dma->_idx,n);*/
	return 0;
}

PS2DEV::__timer::__timer(){
	_status=0;
}

int PS2DEV::__timer::init(int n,void *m,void *mm){
	_idx=n;
	_io_regs=(u32 *)m;
	_mem=(u8 *)mm;
	return 0;
}

int PS2DEV::__timer::reset(){
	_status=0;
	_elapsed=0;
	_counter=0;
	_step=0;
	return 0;
}

int PS2DEV::__timer::write(u32 a,u32 b){
	switch(a & 0xc){
		case 0:
		break;
		case 4:
		//	printf("timer %d %x %x D:%x\n",_idx,a,b,__data);
			//EnterDebugMode();
			_status=b;
			_count();
			//if(_imode) return _idx+1;
		break;
		case 8:
		break;
	}
	return 0;
}

int PS2DEV::__timer::read(u32 a,u32 *b){
	switch(a&0xc){
		case 0:
			switch(_mode){
				case 0:
					*b=_count();
				break;
				case 1:
				break;
			}
		break;
		case 4:
			_count();
			*b=(u32)_status;
			_reached=0;
		break;
	}
	return 0;
}

u32 PS2DEV::__timer::_count(){
	u32 v=_counter;

	switch(_source){
		case 2:
			switch(_idx){
				case 2:
					_counter = (u32)(u16)SR(__cycles,3);//sysem/8
					goto Z;
			}
		case 0:
			_counter = (u32)(u16)__cycles;
		break;
		case 1:
			switch(_idx){
				case 0:
					_counter = (u32)(u16)SR(__cycles,3+9);//dot
				break;
				case 1://hblank
					_counter = (u32)(u16)SR(__cycles,3);
				break;
				case 2:
					_counter = (u32)(u16)__cycles;
				break;
			}
		break;
		case 3:
			switch(_idx){
				case 0:
					_counter = (u32)(u16)SR(__cycles,3+9);//dot
				break;
				case 1://hblank
					_counter = (u32)(u16)SR(__cycles,3);
				break;
				case 2:
					_counter = (u32)(u16)SR(__cycles,3);//sysem/8
				break;
			}
		break;
	}
Z:
	if(v > _counter)
		_reached=1;
	return _counter;
}

int PS2DEV::__timer::Run(u8 *,int cyc,void *){
	return 0;
}

PS2DEV::__dma::__dma(){
	_status=0;
}

int PS2DEV::__dma::write(u32 r,u32 v,PS2DEV &g){
	//printf("dma write %d %x %x\n",_idx,r,v);
	switch(r){
		case 8:
			if(BVT(v,24)) _do_dma(g);
		break;
	}
	return 0;
}

int PS2DEV::__dma::reset(){
	_mode=0;
	_count=_src=_dst=0;
	return 0;
}

int PS2DEV::__dma::init(int n,void *m,void *mm){
	_io_regs=(u32 *)m;
	_mem=(u8 *)mm;
	_idx=n;
	return 0;
}

int PS2DEV::__dma::_do_transfer(u32 a,u32 *r){
	//printf("dma data %d %x\n",_idx,a);
	return 0;
}

int PS2DEV::__dma::_do_dma(PS2DEV &g){
	u32 v=_io_regs[RMAP_IO(0x1F801088+SL(_idx,4)) / 4];
	_mode=SR(v,9);
	_src=_io_regs[RMAP_IO(0x1F801080+SL(_idx,4)) / 4];
	_count=_io_regs[RMAP_IO(0x1F801084+SL(_idx,4)) / 4];
	//printf("do dma %d %x %x %x\n",_idx,_mode,_src,_count);
	if(!g.do_dma(this))
		_io_regs[RMAP_IO(0x1F801088+SL(_idx,4)) / 4] &= ~BV(24);
	return 0;
}

PS2DEV::__dvdrom::__dvdrom(){
}

int PS2DEV::__dvdrom::reset(){
	return 0;
}

int PS2DEV::__dvdrom::init(int,void *m,void *mm){
	_io_regs=(u32 *)m;
	_mem=(u8 *)mm;
	return 0;
}

int PS2DEV::__CP0::_rfe(u32*){
	return  -1;
}

int PS2DEV::__CP0::_reset(){
	memset(_regs,0,sizeof(_regs));
	_regs[SR] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	_regs[PRID] = 0x00000002; // PRevID = Revision ID, same as R3000A
	return 0;
}
int PS2DEV::__VFPU::_reset(){
	memset(&_regs,0,sizeof(_regs));
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