#include "cps3dev.h"
#include "cps3.h"
#include "cps3m.h"

#define DLOG(a,...){\
	u32 pc;\
	cpu->Query(ICORE_QUERY_PC,&pc);\
	LOGF(a STR(\x20%x\x20%u\n),## __VA_ARGS__,pc,__cycles);\
}

static FILE *fp=0;

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
	for(int i=0;i<8;i++)
		_flashs[i]=new struct __flash(i);
	_flashs[0]->_bus=1;
	_flashs[1]->_bus=1;

	_char_dma._mem = &g._memory[MI_DUMMY+MI_CHRAM];
	_palette_dma._mem = &g._memory[MI_DUMMY+MI_PRAM];

	_eeprom.init(g);
	_cdrom.init(g);
	Create();

	return 0;
}

int CPS3DEV::Reset(){
	if(fp) fclose(fp);
	fp=0;
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
	int tranfercount = 0;
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
	_src=MAKEWORD(PPU_REG16_(regs,0xa0),PPU_REG16_(regs,0xa2)&0x7ff);
	_dst=MAKEWORD(PPU_REG16_(regs,0xa4),PPU_REG16_(regs,0xa6)&1);
	_fade=MAKEWORD(SR(PPU_REG16_(regs,0xaa),1),PPU_REG16_(regs,0xa8)&0x7f);
	_len=MAKEWORD(PPU_REG16_(regs,0xae),PPU_REG16_(regs,0xac)&1);
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
	//fprintf(stderr,"pal dma %x %x %x %x\t\t",_src,_dst,_len,_fade);
	_src= SL(_src,1)-0x400000;
	dst=(u16 *)_mem + _dst;
	src =&src[SR(_src,1)];
	for(u32 i=0; i<_len; i++) {
		u16 coldata = src[i];

		//coldata = (coldata << 8) | (coldata >> 8);
		if(coldata & 0x8000)
			printf("%x ",coldata);
		unsigned int r = (coldata & 0x001F);
		unsigned int g = (coldata & 0x03E0) >>  5;
		unsigned int b = (coldata & 0x7C00) >> 10;

		if (_fade!=0) {
			int fade;

			fade = (_fade & 0xff000000)>>24;
			r = SR(r*fade,8);
			if (r>0x1f)
				r = 0x1f;
			fade = (_fade & 0x00ff0000)>>16;
			g = SR(g*fade,8);
			if (g>0x1f)
				g = 0x1f;
			fade = (_fade & 0x000000ff);
			b = SR(b * fade,8);
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
	_mem=0;
	_value=0;
	_idx=n;
	_bus=0;
}

CPS3DEV::__flash::~__flash(){
	if(_mem)
		delete []_mem;
	_mem=0;
}

void CPS3DEV::__flash::reset(){
	_cycles=_data=0;
	_reset();
}

void CPS3DEV::__flash::_reset(){
	_timeout=1600;
	_mode=FM_NORMAL;
	_status=0x80;
	_command=0;
}

void CPS3DEV::__flash::write(u32 a,u32 v,u32 f){
	u32 elapsed= __cycles > _cycles?__cycles-_cycles:(25000000-_cycles)+__cycles;

	_cycles=__cycles;
//	if(_idx>1){//61388c8
		DLOG("FLASH W:%x %u %d %x",a,elapsed,_mode,v);
		//EnterDebugMode(DEBUG_BREAK_OPCODE);
	//}

	//if(elapsed > _timeout)
		//_reset();
	switch(_mode){
		case FM_NORMAL:
		case FM_READSTATUS:
		case FM_READID:
			switch((u8)v){
				case 0xf0:
				case 0xff:  // reset chip mode
					_reset();
			//		DLOG("FLASH W:MODE %x",v)
				break;
				case 0x90:  // read ID
					_mode=FM_READID;
					_timeout=1600;
					DLOG("FLASH W: FM_READID MODE %x %x",a,v);
					_command=v;
				break;
				case 0xa0:
					DLOG("FLASH W:MODE %x %x",a,v);
					_command=v;
				break;
				case 0x50:
				case 0x70:
					_mode=FM_READSTATUS;
					_command=v;
					DLOG("FLASH W: FM_READSTATUS MODE %x %x",a,v);
				break;
				case 0x10:
					_command=v;
				break;
				case 0:
				case 8:
				break;
				case 9:
					//_mode=FM_READSTATUS;
					//DLOG("FLASH W:%x %u %d %x",a,elapsed,_mode,v);
					if(_prev==8){
						//_timeout=7000;
						_mode=FM_READSTATUS;
						_command=v;
						DLOG("command %x ",_command);
					}
				break;
				default:
					//_reset();
					//DLOG("FLASH W:%x %u %d %x",a,elapsed,_mode,v);
				break;
			}
		break;
		default:
			//LOGD("FLASH %x %x %x\n",pc,a,v);
		break;
	}
	_prev=(u8)v;
}
//61338ae	61388c0
int CPS3DEV::__flash::read(u32 a,u32 v,u32 f){
	u32 elapsed= __cycles > _cycles ? __cycles-_cycles:(25000000-_cycles)+__cycles;

	//if(_idx>1)
		DLOG("FLASH R:%x %u %d %x",a,elapsed,_mode,_data);
	switch(_mode){
		case 0:
			return 1;
		case FM_READID:
			DLOG("FLASH R:%x %u %d %x",a,elapsed,_mode,_data);
			EnterDebugMode(DEBUG_BREAK_OPCODE);
			if(!_bus){
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
			_data=_status;
			DLOG("FLASH R: STATUS %x %d %x",a,_mode,_data);
			return 0;
		break;
	}
	return 1;
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
	int div_tab[4] = { 3, 5, 7, 0 };
	int wdtclk_tab[8] = { 1, 6, 7, 8, 9, 10, 12, 13 };

	int i=div_tab[SR(IOREG__(mem,5),8) & 3];
	_prescaler=BV(i);
	if(IOREG__(mem,5) & 0x10)
		_ocrb = SR(IOREG__(mem,5),16);
	else
		_ocra = SR(IOREG__(mem,5),16);
	_status=(IOREG__(mem,4)&0xFF00FFFF)|(_status & 0xff0000 & IOREG__(mem,4));
	//_count=(u16)_status;
	return _ic&0xe ? 0 : 1;
}

int CPS3DEV::__timer::Run(u8 *mem,int cyc){
	int irq;

	if(!_prescaler)
		return 0;
	_step+=cyc;
	if(_step < _prescaler)
		return 0;
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
	_adr=a;
	if((a&0xfff) < 0x201){
		_data=(a&0x100) ? *((u16 *)&_mem[a&0x7f]) : 0;
		*((u16 *)&_mem[0x202])=_status;//status port
		*((u16 *)&_mem[0x200])=_data;
	}
	return 1;
}

void CPS3DEV::__eeprom::write(u32 a,u32 v,u32 f){
	if(!(a&0x100))
		*((u16 *)&_mem[a&0x7f])=v;
	*((u16 *)&_mem[0x202])=_status;
}

#define CD_SET_COUNT(a)\
_regs[0x12]=SR(a,16);\
_regs[0x13]=SR(a,8);\
_regs[0x14]=a;

#define CD_GET_COUNT() (SL(_regs[0x12],16) | SL(_regs[0x13], 8) | _regs[0x14]);

CPS3DEV::__cdrom::__cdrom(){
	_status=0;
	_regs=_buffer=0;
}

CPS3DEV::__cdrom::~__cdrom(){
	if(_regs)
		delete []_regs;
	_regs=_buffer=0;
}

void CPS3DEV::__cdrom::reset(){
	_status=0;
	_data=0;
	if(_regs){
		memset(_regs,0,32);
		_regs[R_STATUS]=1;
	}
	_cycles=0;
}

int CPS3DEV::__cdrom::init(CPS3M &g){
	if(!(_regs=new u8[_size_buffer+50]))
		return -1;
	_buffer=_regs+32;
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
	DLOG("cdrom Rr:%x %x",_reg_idx,_regs[_reg_idx]);
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
	DLOG("cdrom Wr:%x %x",_reg_idx,v);
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

int CPS3DEV::__cdrom::Run(u8 *,int cyc){
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

static int to_msf(int frame){
	int m = frame / (75 * 60);
	int s = (frame / 75) % 60;
	int f = frame % 75;

	return (m << 16) | (s << 8) | f;
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
			printf(" %x %x ",_command_len,_count);
			for(int i=0;i<_command_len;i++)
				printf("%x ",_regs[i+3]);
			printf(" %u\n",__cycles);
		break;
		case 0x1e:
				//EnterDebugMode();
		case 0:
			_cw=0;
			_cr=0;
		break;
		case 0x28:{
			u32 r,lba = (_regs[2+3]<<24) | (_regs[3+3]<<16) | (_regs[4+3]<<8) | _regs[5+3];
			_blocks = (_regs[7+3] << 8) | _regs[8+3];
			printf("READ10 %u %u %u\n",lba,_blocks,_count);
			r=0;
			if(!fp)
				fp=fopen("roms/redearth/","rb");

			if(!fseek(fp,lba*2448,SEEK_SET))
				r=fread(_buffer,1,_count,fp);
			_blocks--;
			_cr=0;
			if((_cw=r)){
				_regs[R_AUX_STATUS] |= 1;
				_regs[R_COMMAND_STEP]=0x30;
				_state=2;
			}
			else
				_regs[R_STATUS]=0x28;

		}
		break;
		case 0x43://reaad toc
		{
			int msf = (_regs[1+3] & 0x2) != 0;
			u16 size = (_regs[7+3] << 7) | _regs[8+3];
			u8 format = _regs[2+3] & 15;

			if(!format)
				format = (_regs[3+9] >> 6) & 3;
			_cw=0;
			_cr=0;
			//printf("CDROM READTOC %d ",format);
			switch (format) {
				case 0: {
					int start_track = _regs[6];
					int end_track = start_track+1;
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
							tstart = to_msf(tstart+150);

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
		case 0x12:
			switch(_regs[3+2]){
				case 0:
				memset(_buffer,0,_size_buffer);
				_buffer[0]=5;
				_buffer[1]=0x80;
				_buffer[2]=0;
				_buffer[3]=2;
				_buffer[4]=0x20;
				_buffer[7]=5;
				strcpy((char *)&_buffer[8],"cazzo");
				strcpy((char *)&_buffer[16],"troia");
				strcpy((char *)&_buffer[32],"fic");
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
		case 0x25://capacity
		{
			u32 v=(MB(60) / 1) -1;
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

}