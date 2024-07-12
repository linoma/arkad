#include "cps3dev.h"
#include "cps3.h"
#include "cps3m.h"
#include "game.h"


namespace cps3{

CPS3DEV::CPS3DEV() : Runnable(){
}

CPS3DEV::~CPS3DEV(){
	Destroy();
}

int CPS3DEV::Destroy(){
	Runnable::Destroy();

	for(int i=0;i<8;i++){
		if(_flashs[i])
			delete _flashs[i];
		_flashs[i]=0;
	}

	for(int i=0;i<3;i++){
		if(_dmas[i])
			delete _dmas[i];
		_dmas[i]=0;
	}
	return 0;
}

int CPS3DEV::Init(CPS3M &g){
	u8 *m=&g._memory[MI_USER4];

	for(int i=0;i<8;i++){
		struct __flash *p=new struct __flash(i);
		if(!p)
			return -1;
		p->init(m);
		switch(i){
			case 0:
				m+=MB(8);
			break;
			case 1:
				m=NULL;
			break;
			case 2:
				m=&g._memory[MI_USER5];
			break;
			default:
				m+=MB(16);
			break;
		}
		_flashs[i]=p;
	}

	_flashs[0]->_bank=0;
	_flashs[1]->_bank=0;

	_flashs[2]->_enabled=0;

	_char_dma._mem = &g._memory[MI_DUMMY+MI_CHRAM];
	_palette_dma._mem = &g._memory[MI_DUMMY+MI_PRAM];

	_eeprom.init(g);
	_cdrom.init(g);
	Create();

	return 0;
}

int CPS3DEV::Reset(){
	_ports[0]=_ports[1]=0xffffffff;
	_eeprom.reset();
	_wdt._status=0;
	_timer.Reset();
	_gic.reset();
	_cdrom.reset();

	for(int i=0;i<5;i++){
		if(_dmas[i])
			_dmas[i]->_reset();
	}

	for(int i=0;i<8;i++){
		if(_flashs[i])
			_flashs[i]->reset();
	}
	_simm_bank=0;

	return 0;
}

void CPS3DEV::OnRun(){
}

int CPS3DEV::__dma::_transfer(){
	if(!_enabled)
		return 1;
	return 0;
}

void CPS3DEV::__dma::_reset(){
	_value=0;
	_dst=_src=0;
}

void CPS3DEV::__dma::_init(void *){
}

void CPS3DEV::__char_dma::_init(void *regs){
	_src=PPU_REG16_(regs,0x94);
}

int CPS3DEV::__char_dma::_transfer(){
	u32 *p;

	if(!_enabled)
		return 1;
	p=((u32 *)((u32 *)_mem + _src));
	for (int i = 0; i < 0x1000; i += 3){
		u32 dat1 = p[i];
		u32 dat2 = p[i + 1];
		u32 dat3 = p[i + 2];

		//printf("dma o %x %x %x\n",dat1,dat2,dat3);
		if (dat1 & 0x01000000)
			break;
		_len = SL(((dat1 & 0x1fffff) + 1) ,3);
		_dst = SL(dat2,3);
		_src = SL(dat3,1);
	//	printf("dmat %x %x %x\n",SR(dat1,20),_dst,_len);
		switch(dat1 & 0xe00000){
			case 0:
			{
				u32 d[5];

			//	printf("dma 0 %x %x\n",_dst,_len);
				d[0]=_src;
				cpu->Query(IMACHINE_QUERY_MEMORY_ACCESS,d);
				u8 *src=(u8 *)*((void **)&d[1]);
				_src-=0x400000;

				memcpy( (u8 *)_mem + _dst, src + (_src^1), _len );
				//printf("src %x %x\n",_src,_len);
			}
			break;
			case 0x400000://6bpp dma
				//fprintf(stderr,"dma 4 %x %x %x\n",_src^1,_dst,_len);
				_transfer6bpp();
				//fprintf(stderr,"\n\n");
			break;
			case 0x600000://8bpp dma
			//	printf("dma 6 %x %x\n",_dst,_len);
				_transfer8bpp();
			break;
			case 0x800000:
				_table_src=_src-0x400000;
			break;
			default:
				LOGE("char dma %x\n",dat1);
			break;
		}
	}
	_enabled=0;
	return 0;
}

void CPS3DEV::__char_dma::_transfer6bpp(){
	u32 length_processed;
	u8 *src;

	{
		u32 d[5];

		d[0]=_src;
		cpu->Query(IMACHINE_QUERY_MEMORY_ACCESS,d);
		src=(u8 *)*((void **)&d[1]);
	}

	_src-= 0x400000;
	_last_byte=0;

	for(;_len;){
		u8 data=src[_src^1];
		_src++;
		if(data & 0x80){
			u8 v;

			data &= 0x7f;
			for(int i = 0;i<2;i++){
				v = src[ (_table_src+data*2+i) ^ 1];
				length_processed = _decode6bpp( v, _dst, _len );
				if (length_processed >=_len)
					goto A;
				_len -= length_processed;
				_dst += length_processed;
				if (_dst > 0x7fffff)
					goto A;
			}
		}
		else{
			length_processed = _decode6bpp( data, _dst, _len );
			if (length_processed >=_len)
				goto A;
			_len -= length_processed;
			_dst += length_processed;
			if (_dst>0x7fffff)
				goto A;
		}
	}
A:
	_len=0;
}

void CPS3DEV::__char_dma::_transfer8bpp(){
	u8* px;
	u32 start = _dst;
	u32 ds = _dst;

	{
		u32 d[5];

		d[0]=_src;
		cpu->Query(IMACHINE_QUERY_MEMORY_ACCESS,d);
		px=(u8 *)*((void **)&d[1]);
	}
	_src-=0x400000;
	_lastb=0xfffe;
	_lastb2=0xffff;

	while(1) {
		u8 ctrl=px[ _src ^ 1 ];
 		_src++;

		for(int i=0;i<8;++i) {
			u8 p = px[ _src ^ 1 ];

			if(ctrl & 0x80) {
				u8 v;

				p &= 0x7f;
				v = px[ (_table_src+p*2+0) ^1];
				ds += _decode8bpp(v,ds);
				v = px[ (_table_src+p*2+1) ^1];
				ds += _decode8bpp(v,ds);
 			}
 			else
 				ds += _decode8bpp(p,ds);
 			_src++;
 			ctrl<<=1;
			if((ds-start) >= _len)
				break;
 		}
	}
}

u32 CPS3DEV::__char_dma::_decode8bpp(u8 b,u32 dst){
 	if(_lastb==_lastb2) {
 		int l,rle=(int)(u8)(b+1);

 		for(int i=0,l=0;i<rle;++i) {
			((u8 *)_mem)[(dst&0x7fffff) ^3] = _lastb;
			dst++;
 			l++;
 		}
 		_lastb2=0xffff;
 		return l;
 	}
 	_lastb2=_lastb;
 	_lastb=b;
	((u8 *)_mem)[(dst & 0x7fffff) ^ 3] = b;
	return 1;
}

u32 CPS3DEV::__char_dma::_decode6bpp(u8 v,u32 dst,u32 max_length){
	dst &= 0x7fffff;

	if(!(v&0x40)) {
		((u8 *)_mem)[(dst & 0x7fffff) ^ 3] = v;
		_last_byte = v;
		return 1;
	}
	u32 tranfercount = 0;
	for (int r= (v & 0x3f)+1;r;r--) {
		((u8 *)_mem)[((dst+tranfercount)&0x7fffff) ^ 3] = (_last_byte & 0x3f);
		tranfercount++;
		max_length--;
		if ((dst+tranfercount) > 0x7fffff)
			return max_length;
	}
	return tranfercount;
}

void CPS3DEV::__palette_dma::_init(void *regs){
	_src=MAKELONG(PPU_REG16_(regs,0xa0),PPU_REG16_(regs,0xa2)&0x7ff);
	_dst=MAKELONG(PPU_REG16_(regs,0xa4),PPU_REG16_(regs,0xa6)&1);
	_fade=MAKELONG(PPU_REG16_(regs,0xaa),PPU_REG16_(regs,0xa8));
	_len=MAKELONG(PPU_REG16_(regs,0xae),PPU_REG16_(regs,0xac)&1);
	//printf("pal %x %x %x %x\n",_src,_dst,_fade,_len);
	//if(_src && _dst) EnterDebugMode();
}

int CPS3DEV::__palette_dma::_transfer(){
	u16 *src,*dst;

	if(!_enabled)
		return 1;
	{
		u32 d[5];

		d[0]=_src;
		cpu->Query(IMACHINE_QUERY_MEMORY_ACCESS,d);
		src=(u16 *)*((void **)&d[1]);
	}
//	printf("pal dma %x %x %x %x\t\t",_src,_dst,_len,_fade);
	_src= SL(_src,1)-0x400000;
	dst=(u16 *)_mem + _dst;
	src =&src[SR(_src,1)];
	for(u32 i=0; i<_len; i++) {
		u16 coldata = src[i];

		//coldata = (coldata << 8) | (coldata >> 8);
		u32 r = (coldata & 0x001F);
		u32 g = (coldata & 0x03E0) >>  5;
		u32 b = (coldata & 0x7C00) >> 10;

		if (_fade!=0) {
			int fade;

			fade = (_fade & 0x3f000)>>12;
			r = SR(r*fade,6);
			if (r>0x1f)
				r = 0x1f;
			fade = (_fade & 0xfc0)>>6;
			g = SR(g*fade,6);
			if (g>0x1f)
				g = 0x1f;
			fade = (_fade & 0x3f);
			b = SR(b * fade,6);
			if (b>0x1f)
				b = 0x1f;
			//coldata = (r << 0) | (g << 5) | (b << 10);
		}
		dst[ + i] = RGB555(r,g,b);
//		fprintf(stderr,"%x ",RGB555(r,g,b));
	}
	//fprintf(stderr,"\n\n");
	_enabled=0;
	return 0;
}

CPS3DEV::__flash::__flash(int n){
	_mem=NULL;
	_value=0;
	_idx=n;
	_bank=1;
	_enabled=1;
}

CPS3DEV::__flash::~__flash(){
	if(_mem && !_shared)
		delete []_mem;
	_mem=0;
}

void CPS3DEV::__flash::init(void *m){
	if((_mem=(u8 *)m))
		_shared=1;
}

void CPS3DEV::__flash::reset(){
	_cycles=_data=0;
	_reset();
}

void CPS3DEV::__flash::_reset(){
	_timeout=1600;
	_mode=FM_NORMAL;
	_status=0x80;
}

void CPS3DEV::__flash::write(u32 a,u32 v,u32 f){
	u32 elapsed= __cycles > _cycles?__cycles-_cycles:(MHZ(25)-_cycles)+__cycles;

	_cycles=__cycles;
//	if(_idx>1){//61388c8
		DLOG("FLASH W:%x %u %d %x F:%x %d:%d\n",a,elapsed,_mode,v,f,_idx,SR(a,28));
	//}
//	if(elapsed > _timeout)
	//	_reset();
	switch(_mode){
		case FM_NORMAL:
		case FM_READSTATUS:
		case FM_READID:
			switch((u8)v){
				case 0xf0:
					_reset();
				break;
				case 0xff:  // reset chip mode
					_reset();
			//		DLOG("FLASH W:MODE %x",v)
				break;
				case 0x90:  // read ID
					_mode=FM_READID;
					_timeout=1600;
					DLOG("FLASH W: FM_READID MODE %x %x",a,v);
				break;
				case 0xa0:
					DLOG("FLASH W:MODE %x %x",a,v);
				break;
				case 0x50:
				case 0x70:
					_mode=FM_READSTATUS;
					DLOG("FLASH W: FM_READSTATUS MODE %x %x",a,v);
				break;
				case 0x10:
				break;
				case 0xaa:
					if((a&0xfff) == 0x554 || (a&0xfff) == 0xaa8){
						_mode=FM_COMMAND1;
					}
				break;
				default:
					//_reset();
					//DLOG("FLASH W:%x %u %d %x",a,elapsed,_mode,v);
				break;
			}
		break;
		case FM_COMMAND1:
		//EnterDebugMode();
			if((u8)v==0x55 && ((a&0xffc) == 0xaa8 || (a&0xffc) == 0x554))
				_mode=FM_COMMAND2;
		break;
		case FM_COMMAND2:
			switch(a&0xffc){
				case 0x554:
				case 0xaa8:
					switch((u8)v){
						case 0x90:
							_mode=FM_READID;
						break;
						case 0x80://Enter Extended Command
							_mode=FM_ERASEAMD1;
							//EnterDebugMode();
						break;
						case 0xf0:
							_mode=FM_NORMAL;//fixme
							//_status=0xff;
							//EnterDebugMode(DEBUG_BREAK_OPCODE);
						break;
						case 0xa0:
							_mode=FM_PROGRAM;
						break;
						default:
							EnterDebugMode();
						break;
					}
				break;
				default:
					_mode=FM_NORMAL;
					break;
			}
		break;
		case FM_ERASEAMD1:
			if((u8)v==0xaa && ((a&0xffc) == 0xaa8 || (a&0xffc) == 0x554))
				_mode=FM_ERASEAMD2;
		break;
		case FM_ERASEAMD2:
		if((u8)v==0x55 && ((a&0xffc) == 0xaa8 || (a&0xffc) == 0x554))
			_mode=FM_ERASEAMD3;//aa8 55
		break;
		case FM_ERASEAMD3:
			switch((u8)v){
				case 0x10:
				//	EnterDebugMode();
					_mode=FM_ERASEAMD4;//FM_ERASEAMD4 chip erase
					if(_mem)
						memset(_mem,0xff,MB(_bank*8 + 8));
					_invalidate=1;
				break;
				case 0x30://Erase Sector
				default:
					EnterDebugMode();
				break;
			}
		break;
		case FM_PROGRAM://write byte

			_invalidate=1;
			if(_mem){
				u32 b = a & (MB(_bank*8 + 8) - 1);
				if(_idx > 2 && 0){
					printf("%x %x %x %x  %d\n",a,b,v,f,_idx);
					EnterDebugMode();
				}
				_mem[b] = (u8)v;
				if(!(f&AM_BYTE))
					_mem[b + 1] = SR(v,8);
				if(f&AM_DWORD) {
					_mem[b + 2]=SR(v,16);
					_mem[b + 3]=SR(v,24);
					//EnterDebugMode();;
				}
			}
			_mode=FM_NORMAL;
		break;
		default:
			//LOGD("FLASH %x %x %x\n",pc,a,v);
		break;
	}
}
//61338ae	61388c0
int CPS3DEV::__flash::read(u32 a,u32 v,u32 f){
//	if((f&0x80) && _idx<2) return 0xFFFFFFFF;
	u32 elapsed= __cycles > _cycles ? __cycles-_cycles:(MHZ(25)-_cycles)+__cycles;

	//if(_idx>1)
		DLOG("FLASH R:%x %u %d %x F:%x %d:%d",a,elapsed,_mode,_data,f,_idx,SR(a,28));
	switch(_mode){
		case FM_NORMAL:
			if(_mem){
				u32 b =a & ((!_bank ? MB(8) : MB(16))-1);
				if(f&AM_DWORD)
					_data=*((u32 *)&_mem[b]);
				else if(f&AM_WORD)
					_data=*((u16 *)&_mem[b]);
				else
					_data=_mem[b];
			}
			else
				_data=0xffffffff;

			return 0;
		case FM_READID:
			DLOG("FLASH R:%x %u %d %x",a,elapsed,_mode,_data);
			if(!_bank){//2 bank 64mbit
				switch(a & 0xfffc) {
					case 0:
						_data=0x04040404;
					break;
					case 4:
						_data=0xadadadad;
					break;
					case 8:
						_data=0x00000000;
					break;
				}
			}
			else
				_data=0x0404adad;
			return 0;
		case FM_READSTATUS:
		case FM_WRITEPART1:
		//	if(_bus)
				_data=_status|(_status<<8)|(_status<<16)|(_status<<24);
			//else
				//_data=_status;
			DLOG("FLASH R: STATUS %x %d %x",a,_mode,_data);
			return 0;
		break;
		case FM_ERASEAMD1:
		case FM_ERASEAMD4:
			_data=0x80808080;
			_mode=FM_NORMAL;
			return 0;
	}
	return 1;
}

int CPS3DEV::__flash::Serialize(IStreamer *p){
	p->Write(&_value,sizeof(_value),0);
	return 0;
}

int CPS3DEV::__flash::Unserialize(IStreamer *p){
	p->Read(&_value,sizeof(_value),0);
	return 0;
}

CPS3DEV::__timer::__timer(){
	Reset();
}

CPS3DEV::__timer::~__timer(){
}

int CPS3DEV::__timer::Reset(){
	_status=0;
	_prescaler=_step=_ocra=_ocrb=0;
	return 0;
}

int CPS3DEV::__timer::Init(u8 *mem,int port){
	u32 v;
	int div_tab[4] = { 3, 5, 7, 0 };
	int wdtclk_tab[8] = { 1, 6, 7, 8, 9, 10, 12, 13 };

	int i=div_tab[SR(IOREG__(mem,5),8) & 3];
	_prescaler=BV(i);
	_status=(IOREG__(mem,4)&0xFF00FFFF)|(_status & 0xff0000 & IOREG__(mem,4));
	_min_wait = 0x10000-_count;
	if(IOREG__(mem,5) & 0x10){
		_ocrb = SR(IOREG__(mem,5),16);
		if(_ocrb && (v=_ocrb-_count) < _min_wait || !_min_wait)
			_min_wait=v;
	}
	else{
		_ocra = SR(IOREG__(mem,5),16);
		if(_ocra && (v=_ocra-_count) < _min_wait || !_min_wait)
			_min_wait=v;
	}
	if(_min_wait < _prescaler)
		_min_wait=_prescaler;
//	if((_ic & 0xe))
	//	printf("%u %u\n",_min_wait,_prescaler);
	return _ic & 0xe ? 0 : 1;
}

int CPS3DEV::__timer::Run(u8 *mem,int cyc,void *obj){
	int irq;


	if(!_prescaler)
		return 0;
	_step+=cyc;
	if(_step < _prescaler)
		return 0;
	//printf("timer %u %u %u %u\n",cyc,_step,_prescaler,_min_wait);
	irq=0;
	while(_step >=_prescaler){
		_step -=_prescaler;
		_count++;

		if(_count==_ocrb){
			_sc |= OCFB;
			irq=1;
		}
		if(!_count){
			//_count -= 65535;
			_sc |= OVF;
			irq=1;
		}
		if(_count==_ocra){
			irq=1;
			_sc |= OCFA;
			if(_ic & CCLRA)
				_count=0;
		}
	}
	IOREG__(mem,4)=_status;
	if(!irq)
		return 0;
	if(_ic & _sc & (ICF|OCFA|OCFB|OVF))
		return 5;
	return 0;
}

void CPS3DEV::__gic::reset(){
	*((u64 *)_vectors)=0;
	*((u64 *)_levels)=0;
	_irq_pending=0;
}

void CPS3DEV::__eeprom::init(CPS3M &g){
	_mem=&g._memory[MI_DUMMY+MI_EEPROM];
	reset();
}

void CPS3DEV::__eeprom::reset(){
	_data=0;
	_adr=0;
	_status=0;
}

int CPS3DEV::__eeprom::read(u32 a,u32 v,u32 f){
	DLOG("EEPROM R %x %x F:%x",a,v,f);
	_adr=a;
	if((a&0xfff) < 0x201){
		_data=(a&0x100) ? *((u16 *)&_mem[a&0x7f]) : 0;
		*((u16 *)&_mem[0x202])=_status;//status port
		*((u16 *)&_mem[0x200])=_data;
	}
	return 1;
}

void CPS3DEV::__eeprom::write(u32 a,u32 v,u32 f){
	DLOG("EEPROM W %x %x F:%x",a,v,f);
	if(!(a&0x100))
		*((u16 *)&_mem[a&0x7f])=v;
	*((u16 *)&_mem[0x202])=_status;
}

static int lino=0;

#define CD_SET_COUNT(a)\
_regs[0x12]=SR(a,16);\
_regs[0x13]=SR(a,8);\
_regs[0x14]=a;

#define CD_GET_COUNT() (SL(_regs[0x12],16) | SL(_regs[0x13], 8) | _regs[0x14]);

CPS3DEV::__cdrom::__cdrom(){
	_status=0;
	_regs=_buffer=0;
	fp=NULL;
}

CPS3DEV::__cdrom::~__cdrom(){
	if(_regs)
		delete []_regs;
	_regs=_buffer=0;
	if(fp)
		fclose(fp);
	fp=0;
}

int CPS3DEV::__cdrom::Query(u32,void *){
	return -1;
}

int CPS3DEV::__cdrom::Serialize(IStreamer *p){
	p->Write(_regs,32);
	p->Write(&_cycles,sizeof(u32));
	p->Write(&_status,sizeof(u64));
	p->Write(&_blocks,sizeof(u16));
	p->Write(&_data,sizeof(u8));
	return 0;
}

int CPS3DEV::__cdrom::Unserialize(IStreamer *p){
	p->Read(_regs,32);
	p->Read(&_cycles,sizeof(u32));
	p->Read(&_status,sizeof(u64));
	p->Read(&_blocks,sizeof(u16));
	p->Read(&_data,sizeof(u8));
	return 0;
}

void CPS3DEV::__cdrom::reset(){
	_status=0;
	_data=0;
	if(_regs){
		memset(_regs,0,32);
		_regs[R_STATUS]=1;
	}
	_cycles=0;
	_blocks=0;
	_size=0;
	if(fp)
		fclose(fp);
	fp=NULL;
}

int CPS3DEV::__cdrom::init(CPS3M &g){
	if(!(_regs=new u8[_size_buffer+50]))
		return -1;
	_buffer=_regs+32;
	reset();
	return 0;
}

int CPS3DEV::__cdrom::read(u32 a,u32 v,u32 f){
	switch(a&2){
		case 2:
			_data= _regs[R_AUX_STATUS] & 0x01 ? _regs[R_AUX_STATUS] & 0x7f : _regs[R_AUX_STATUS];
			DLOG("cdrom Raux:%5x",_data);
		break;
		case 0:
			_read_reg();
		break;
	}
	//EnterDebugMode();
	return 1;
}

void CPS3DEV::__cdrom::_read_reg(){
	//printf("cdrom Rr:%x %x\n",_reg_idx,_regs[_reg_idx]);
	switch(_reg_idx){
		case R_OWN_ID:
			_data=_command_len;
			//EnterDebugMode();
		break;
		case R_COMMAND_STEP:
			_data=_regs[_reg_idx];
			switch(_data){
				case 0x43:
					_regs[R_COMMAND_STEP]=0x80;
				break;
			}
		break;
		case 0x12:
		case 0x13:
		case 0x14:
			_data=_regs[_reg_idx++];
		break;
		case R_DATA:
	//	printf("data %x %d\n",_buffer[_cr],_cr);
			_data=_buffer[_cr++];
			if(_cr >= _size_buffer)
				_cr = _size_buffer-1;
			if(_cw)
				_cw--;
			if(!_cw){
				if(_state==2){
					if(_blocks){
						if(fp){
							fread(_buffer,1,_count,fp);
							_cw=_count;
						}
						_blocks--;
					}
					if(!_blocks)
						_state=1;
				}
				if(_state==1){
					_regs[R_AUX_STATUS] &= ~1;
					_regs[R_COMMAND_STEP]=0;
				}
			}
		break;
		case R_STATUS:
			//todo?
		default:
			_data=_regs[_reg_idx];
		break;
	}
}

int CPS3DEV::__cdrom::_write_reg(u32 v){
	//printf("cdrom Wr:%x %x\n",_reg_idx,v);
	//fflush(stderr);
	switch(_reg_idx){
		case 0x15:
			_regs[_reg_idx]=v;
			_regs[R_AUX_STATUS] =  0x40;
		//EnterDebugMode();
		break;
		case R_COMMAND_STEP:
			_regs[_reg_idx++]=v;
			switch(v){
				case 0x44://releselected
					_regs[R_STATUS]=0x80;
					_regs[R_AUX_STATUS] =  0x40;
				break;
			}
		break;
		case R_DATA:
			EnterDebugMode();
		break;
		case R_COMMAND://COMMAND
			//EnterDebugMode();
			switch(v){
				case 0://Reset
					_regs[R_AUX_STATUS] = 0x80;
				break;
				case 8:
				case 9://Transfer
					return _process_command(v);
				default:
				printf("CDROM COMMAND %X \n",v);
					break;
			}
		break;
		default:
			//printf("cdrom W:%x %x\n",_reg_idx,v);
			_regs[_reg_idx++]=v;
		break;
		case R_AUX_STATUS:
		case R_STATUS:
		break;
	}
	return 0;
}

int CPS3DEV::__cdrom::Run(u8 *,int cyc,void * obj){
	int res;

	_cycles+=cyc;
//	printf("run %u  %d\n",_cycles,cyc);
	if(_cycles < 150)
		return 0;
	_cycles-=150;
	res=0;
	switch(_state){
		case 1:
			switch(_regs[R_COMMAND_STEP]){
				case 0:
					_state=7;
					break;
			}
		break;
		case 7:
			_regs[R_STATUS]=0x85;
			res=-1;
		break;
	}
	return res;
}

int CPS3DEV::__cdrom::write(u32 a,u32 v,u32 f){
	switch(a&2){
		case 2:
			_reg_idx=v;
		break;
		case 0:
			return _write_reg(v);
		break;
	}
	return 0;
}

int CPS3DEV::__cdrom::_process_command(u8 v){
	_cycles=0;
	_regs[R_COMMAND_STEP]=0;
	_regs[R_AUX_STATUS] = 0x80;

	{
		u8 a[]={6,10,6,6,6,12,6,6,6};
		_command_len=a[SR(_regs[3],5)];
	}
	//_regs[R_AUX_STATUS] &= ~1;;
	_regs[R_STATUS]=0x16;//ok
	_regs[1]=0;

	_count = SL(_regs[0x12],16) | SL(_regs[0x13], 8) | _regs[0x14];

	_state = 1;
	switch(_regs[3]){
		default:
#ifdef _DEVELOP
			printf("cdrom %x %x ",_command_len,_count);
			for(int i=0;i<_command_len;i++)
				printf("%x ",_regs[i+3]);
			printf(" %u\n",__cycles);
#endif
		break;
		case 0x1e://prevent removal
				//EnterDebugMode();
		case 0:
			_cw=0;
			_cr=0;
		break;
		case 0x28:{
			u32 r,lba = (_regs[2+3]<<24) | (_regs[3+3]<<16) | (_regs[4+3]<<8) | _regs[5+3];
			_blocks = (_regs[7+3] << 8) | _regs[8+3];
			//printf("READ10 %u %u %u\n",lba,_blocks,_count);
			r=0;
			if(fp && !fseek(fp,lba*2448,SEEK_SET))
				r=fread(_buffer,1,_count,fp);
			_blocks--;
			_cr=0;
			if((_cw=r)){
				_regs[R_AUX_STATUS] |= 1;
				_regs[R_COMMAND_STEP]=0x30;
				_state=2;
			}
			else{
				_regs[R_STATUS]=0x28;
				_regs[R_AUX_STATUS]=0;
			}
		}
		break;
		case 0x43:{//read toc
			int msf = (_regs[1+3] & 0x2) != 0;
			u16 size = (_regs[7+3] << 7) | _regs[8+3];
			u8 format = _regs[2+3] & 15;

			if(!format)
				format = (_regs[3+9] >> 6) & 3;
			_cw=0;
			_cr=0;
			//printf("CDROM READTOC %d %x",format,_regs[6]);
			switch (format) {
				case 0: {
					int start_track = _regs[6];
					int end_track = fp ? start_track+1 : 0;
					int tracks;

					if(start_track == 0)
						tracks = end_track + 1;
					else if(start_track <= end_track)
						tracks = (end_track - start_track) + 2;
					else if(start_track <= 0xaa)
						tracks = 1;
					else
						tracks = 0;

					int len = 2 + (tracks * 8);

					//printf(" %d ",len);
					_buffer[_cw++] = (len>>8) & 0xff;
					_buffer[_cw++] = (len & 0xff);
					_buffer[_cw++] = 1;
					_buffer[_cw++] = end_track;

					if (start_track == 0)
						start_track = 1;

					for(int i = 0; i < tracks; i++) {
						int track = start_track + i;
						int cdrom_track = track - 1;
						if(i == tracks-1) {
							track = 0xaa;
							cdrom_track = 0xaa;
						}

						_buffer[_cw++] = 0;
						_buffer[_cw++] = 0x14;//cdrom->get_adr_control(cdrom_track);//0x10 audio 0x14  data
						_buffer[_cw++] = track;
						_buffer[_cw++] = 0;

						u32 tstart = 0xa200;//cdrom->get_track_start(cdrom_track);
						if(msf)
							tstart = SECTMFS2(tstart+150);

						_buffer[_cw++] = (tstart>>24) & 0xff;
						_buffer[_cw++] = (tstart>>16) & 0xff;
						_buffer[_cw++] = (tstart>>8) & 0xff;
						_buffer[_cw++] = (tstart & 0xff);
					}
				}
			}
			printf("%d\n",_cw);
			_regs[R_AUX_STATUS] |= 1;
			_regs[R_COMMAND_STEP]=0x30;
		}
		break;
		case 0x12://inquiry
			switch(_regs[3+2]){
				case 0:
					memset(_buffer,0,_size_buffer);
					_buffer[0]=5;
					_buffer[1]=0x80;
					_buffer[2]=0;
					_buffer[3]=2;
					_buffer[4]=0x20;
					_buffer[7]=5;
					strcpy((char *)&_buffer[8],"arkad");
					strcpy((char *)&_buffer[16],"arkad");
					strcpy((char *)&_buffer[32],"emu");
					for(int i=8;i<36;i++){
						if(!_buffer[i])
							_buffer[i]=0x20;
					}
					_cw=_count;
					_cr=0;
					_regs[R_AUX_STATUS] |= 1;
					_regs[R_COMMAND_STEP]=0x30;
				break;
			}
		break;
		case 0x25:{//capacity
			u32 v=_size;

			v=(v / 2048);
			memset(_buffer,0,_size_buffer);
			_buffer[0]=SR(v,24);
			_buffer[1]=SR(v,16);
			_buffer[2]=SR(v,8);
			_buffer[3]=v;
			v=2048;
			_buffer[6]=SR(v,8);
			_buffer[7]=v;
			_cw=_count;
			_cr=0;
			_regs[R_AUX_STATUS] |= 1;
			_regs[R_COMMAND_STEP]=0x30;
		}
		break;
	}
	return 150;
}

void CPS3DEV::__cdrom::_write_fifo(u8 d){
	_buffer[_cw++]=d;
}

void CPS3DEV::__cdrom::_write_fifo(void *p,u8 d){
	u8 *s = (u8 *)p;
	for(;d;d--)
		_buffer[_cw++]=*s++;
}

int CPS3DEV::__cdrom::OnLoad(char *path){
	string s = GameManager::getBasePath(path);
	s += DPS_PATH;
	s += "cdrom";
	fp=fopen(s.c_str(),"rb");
	_size=0;
	if(!fp)
		return -1;
	fseek(fp,0,SEEK_END);
	_size=ftell(fp);
	fseek(fp,0,SEEK_SET);
	return 0;
}

}