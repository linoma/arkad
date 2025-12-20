#include "gbadev.h"

namespace gba{

struct gbadev::__sramid gbadev::SRAMID[8]={{0xd4bf,0,0},{0x1cc2,0,0},{0x1b32,0,0},{0x3d1f,0,0},{0x1362,0,1},{0x09c2,0,1},{0xd5bf,0,1},{0}};

gbadev::gbadev() : arm7(),gbagpu(){
	CCore::_freq=MHZ(16.78);
}

gbadev::~gbadev(){
}

int gbadev::Init(void *a,void *b){
	if(arm7::Init(a,0x3000))
		return -2;
	_sram._buffer=&_mem[MI_SRAM];
	for(int i=0;i<sizeof(_dmas)/sizeof(__dma);i++)
		_dmas[i].Init(i,a,b,CCore::_freq);
	for(int i=0;i<sizeof(_timers)/sizeof(__timer);i++)
		_timers[i].Init(i,a,b,CCore::_freq);
	_sram._buffer=&_mem[MI_SRAM];
	_sram.size=MS_SRAM;
	return 0;
}

int gbadev::Destroy(){
	arm7::Destroy();
	return gbagpu::Destroy();
}

int gbadev::Reset(){
	arm7::Reset();
	gbagpu::Reset();
	gbaspu::Reset();
	for(int i=0;i<sizeof(_dmas)/sizeof(__dma);i++)
		_dmas[i].reset();
	for(int i=0;i<sizeof(_timers)/sizeof(__timer);i++)
		_timers[i].reset();
	IOREG(REG_KEYINPUT)=0xffff;
	_sram._idxId=5;
	_sram.reset();
	_eeprom[0].reset();
	_eeprom[1].reset();
	return 0;
}

int gbadev::_enterIRQ(int n,u32 pc){
	IOREG(REG_IF) |= n;
	if(!(IOREG(REG_IME) & 1))
		return 3;
   	if(!(IOREG(REG_IE) & n))
		return 4;
	return arm7::_enterIRQ(n,pc);
}

int gbadev::Update(u32 cyc){
	int ovr;

	for(int i=0;i<sizeof(_timers)/sizeof(__timer);i++){
		_timers[i].update(cyc);
		if(_timers[i]._ovr){
		//	printf("timer ovr %d %u %u %u %u %u %u\n",i,_pcms[0]._enabled,_pcms[0]._timer,_pcms[1]._enabled,_pcms[1]._timer,_dmas[1]._reload,_dmas[2]._reload);
			if(_pcms[0]._enabled && _pcms[0]._timer==i && !_pcms[0].clock(_timers[i]._ovr))
				dma_do(&_dmas[1]);
			if(_pcms[1]._enabled && _pcms[1]._timer==i && !_pcms[1].clock(_timers[i]._ovr)){
				dma_do(&_dmas[2]);
			}
		}
	}
	return 0;
}

int gbadev::EnableBackupMemory(u32 id,u32 attr){
	return -1;
}

int gbadev::dma_do(struct __dma *p){
	u32 dst,src;

	p->update(0);
	if(!p->_enabled)
		return 0;
	if(p->_idx ==3 || p->_idx==0)
		DLOG("DMA %d %x %x %08X %u %u %u %u %u",p->_idx,p->_dst,p->_src,IOREG32_(_ioreg,0xB8+(p->_idx*12)),p->_count,p->_mode,p->_reload,p->_repeat,p->_start);

	//if(SR(p->_dst,24)==7) EnterDebugMode();
	if(p->_mode){
		dst = p->_dst & ~3;
		src = p->_src & ~3;
	    for(u32 i1 = p->_count;i1 > 0;i1--){
			u32 a;

			RL_(src,a,R, );
			WL_(dst,a,W, );
			dst += p->_incD;
			src += p->_incS;
		}
	}
	else{
		dst = p->_dst & ~1;
		src = p->_src & ~1;
		for(u32 i1 = p->_count;i1 > 0;i1--){
			u16 a;

			RW_(src,a,R, );
			WW_(dst,a,W, );
			dst += p->_incD;
			src += p->_incS;
		}
	}

	if(!p->_repeat){
		p->_enabled = 0;
		IOREG((0xBA+(p->_idx*12))) &= ~0x8000;
	}
	else //if(!p->_reload)
		p->_src = src;

	if(p->_reload){
       //p->_dst = dst;
		//p->_enabled = 1;
		//IOREG32_(_ioreg,(0xB8+(p->_idx*12))) |= 0x80000000;
	}

	if(p->_irq)
		machine->OnEvent(BV(8+p->_idx),0);
	return 0;
}

int gbadev::dma_do(u32 idx){
	for(int i=0;i<sizeof(_dmas)/sizeof(__dma);i++){
		_dmas[i].update(0);
		if(_dmas[i]._start == idx)
			dma_do(&_dmas[i]);
	}
	return 0;
}

int gbadev::__sram::reset(){
	com=0;
	size=0;
	mode=0;
	block=0;
	return 0;
}

int gbadev::__sram::read(u32 a,u8 *p){
	//EnterDebugMode();
	//printf("%s %x %x\n",__FUNCTION__,a,*p);
	a = (u16)a;
	switch(com){
		case 0x80:
			com = 0;
			mode = 0;
			*p=0xff;
			return 0;
		case 0x90:
			if(a < 2){
				_id= &gbadev::SRAMID[_idxId];
				if((a & 1))
                   *p= *((u8 *)(&_id->ID) + 1);
				else
                   *p= *((u8 *)(&_id->ID));
				return 0;
			}
		break;
	}
	//*p=0xff;
	*p=_buffer[(block << 16) | a];
	return 0;
}

int gbadev::__sram::write(u32 a,u8 data){
//	EnterDebugMode();
	a=(u16)a;
	if(a == 0x5555 && com != 0xA0){
		if(data == 0xF0 || data == 0xAA){
			if(mode != 2 || (com != 0x80 && com != 0xA0)){
               com = 0;
               mode = 0;
			}
			return 0;
		}
		if(mode == 1){
			com = (u16)data;
			mode = 2;
			if(data == 0xF0){
				com = 0;
				mode = 0;
			}
			return 0;
		}
	}
	else if(a == 0x2AAA && com != 0xA0){
		if(mode == 0){
			if(data == 0x55){
				mode = 1;
				return 0;
			}
		}
		else if(mode == 2){
			if(data == 0x55 && com == 0x80)
				mode = 3;
			return 0;
		}
	}
	else if(mode == 3 && com != 0xA0){
		if(data == 0x30){
           //erase sector
			a = (block << 16) | (a & 0xF000);
			memset(&_buffer[a],0,0x1000);
		}
		else
			memset(_buffer,/*IsSRAM128K() ? 0x20000 : 0x10000*/0,0x10000);
		mode = 0;
		return 0;
	}
	else if(mode == 2 && com == 0xB0){
		block = (u16)(u8)data;
		mode = 0;
		return 0;
	}
	a |= (block << 16);
	if((s32)a > size)
		size = a;
	_buffer[a] = (u8)data;
	if(mode == 2 && com == 0xA0){
       mode = 0;
       com = 0;
	}
	if(com || mode) PRINTF("SRAM W %x %x\n",a,data);
	return 0;
}

int gbadev::__eeprom::write(u32 a,u16 data){
	u8 *p;

	if((a = (u32)(u16)a) == 0){
		com = 0;
		mode = 0;
		blocco = 0;
		byteIndex = 0;
		bitIndex = 0;
		bytesWrite = 0;
    }
  //  printf("%s %x %x %x %x %x\n",__FUNCTION__,a,data,sizeCommand,com,blocco);
    if(a < sizeCommand){
        if(data & 1)
            com |= (u32)(1 << (a >> 1));
        if(a == (sizeCommand - 2)){
            blocco = (u16)(((com & 0x01FE000) >> 13));
            if(rom_pack[blocco]._buffer == NULL){
                rom_pack[blocco]._buffer = (u8 *)new u8[0x4000];
                memset(rom_pack[blocco]._buffer,0,0x4000);
                rom_pack[blocco].size = 0;
            }
            byteIndex = (u32)((com & (0x1FFF & ~bitCommand)) << 1);
            bitIndex = (com & bitCommand) == eecWrite ? bitCommand+1 : 0;
            bytesWrite = 0;
            DLOG("EEPROM W %u B:%u %u %u",(com & bitCommand),blocco,byteIndex,bitIndex);
        }
    }
    else{
        switch((com & bitCommand)){
            case eecSeek:
            break;
            case eecWrite:
                if(bytesWrite > 63){
                    isUsed = 1;
                    mode = 1;
                    break;
                }
                p = rom_pack[blocco]._buffer;
                if((data & 1))
                    p[byteIndex] |= (u8)(1 << bitIndex);
                else
                    p[byteIndex] &= (u8)~(1 << bitIndex);
                if((bitIndex = (u8)((bitIndex + 1) & 0x7)) == 0)
                    byteIndex++;
                if(byteIndex > rom_pack[blocco].size)
                    rom_pack[blocco].size = byteIndex;
                bytesWrite++;
            break;
        }
    }
	return 0;
}

int gbadev::__eeprom::read(u32,u16 *p){
	//printf("%s %u %u %u %u\n",__FUNCTION__,mode,byteIndex,bitIndex,blocco);
	switch(mode){
		case 1:
			*p=1;
			break;
		case 0:{
			u8 *m;
			u32 value;
			//EnterDebugMode();
			if((m = rom_pack[blocco]._buffer) == NULL)
				value=0;
			else{
				if((m[byteIndex] & (1 << bitIndex)) != 0)
					value = 1;
				else
					value = 0;
				if((bitIndex = (u8)((bitIndex + 1) & 0x7)) == 0)
					byteIndex++;
			}
			*p=value;
		}
		break;
	}
	return 0;
}

int gbadev::__eeprom::reset(){
	for(int i =0;i<sizeof(rom_pack)/sizeof(__packrom);i++)
		rom_pack[i]._free();
	com=byteIndex=0;
	blocco=0;
	bitIndex=bytesWrite=isUsed=mode=0;
	bitCommand=3;
	sizeCommand=32;
	return 0;
}

void gbadev::__packrom::_free(){
	if(_buffer) delete []_buffer;
	_buffer=NULL;
	size=0;
}

int gbadev::__timer::Init(int i,void *a,void *b,u32 fr){
	_mem=(u8 *)a;
	_ioreg=b;
	_idx=i;
	_pllHz=fr;
	return 0;
}

u32 gbadev::__timer::freq(){
	return (float)SR(_pllHz,_freq) / (65536.0f - _reset);
}

int gbadev::__timer::reset(){
	_status=0;
	_count=0;
	_reset=0;
	return 0;
}

int gbadev::__timer::update(int){
	if(_changed){
		u16 v = IOREG((0x102+(_idx*4)));
		if(_enabled=SR(v,7)) {
			_cycles=__cycles;
			_count=_reset;
		}
		_irq=SR(v,6);
		if((_freq=SL(v&3,1)) > 1)
			_freq+=4;
		_cascade=SR(v,2);
		_changed=0;
		DLOG("TIMER W %d %x %u %x",_idx,v,_freq,_reset);
	}
	if(!_enabled) return -1;
	_ovr=0;
	u32 d = __cycles >= _cycles ? __cycles-_cycles:(_pllHz-__cycles)+_cycles;
	if(!(d=SR(d,_freq)))
		return 0;
	_cycles=__cycles;

	u16 dd = 65535 - _count;
	u16 i = dd > d ? d :dd;
	_count +=i;
	d-=i;

	for(;d;d-=i){
		_count = _reset;
		_ovr++;
		dd = 65535 - _count;
		i = dd > d ? d :dd;
		_count +=i;
	}
	if(_ovr && _irq)
		machine->OnEvent(BV(3+_idx),1);
	IOREG((0x100+(_idx*4)))=_count;
	return 0;
}

int gbadev::__timer::write(u32 a,u32 v){
	switch(a&3){
		case 0:
			_reset=v;
			printf("timer %u %x\n",_idx,v);
		case 2:
			_changed=1;
		break;
	}
	return 1;
}

int gbadev::__timer::read(u32 a,u32 *p){
	switch(a&3){
		case 0:
			update(0);
			printf("timer read %d %u\n",_idx,_count);
			break;
	}
	return 0;
}

int gbadev::__dma::Init(int i,void *a,void *b,u32){
	_mem=(u8 *)a;
	_ioreg=b;
	_idx=i;
	return 0;
}

int gbadev::__dma::reset(){
	_status=0;
	return 0;
}

int gbadev::__dma::update(int){
	if(_changed){
		_dst=IOREG32_(_ioreg,(0xB4+(_idx*12)));
		_src=IOREG32_(_ioreg,(0xB0+(_idx*12)));
		u32 v=IOREG32_(_ioreg,(0xB8+(_idx*12)));
		_mode=SR(v,10+16);
		_start=SR(v,12+16);
		_irq=SR(v,14+16);
		_repeat=SR(v,9+16);
		_enabled=SR(v,15+16);
		_count=(u16)v;
		_reload=0;
		if(_enabled){
			//DLOG("DMA %d %x %x %08X %u",_idx,_dst,_src,v,_count);
			switch((v & 0x600000) >> (5+16)){
				case 0:
					_incD = 4;
				break;
				case 1:
					_incD = -4;
				break;//84400004
				case 2:
					_incD = 0;
				break;
				case 3:
					_reload = 1;
					_incD = 4;
				break;
			}
			switch((v & 0x1800000) >> (7+16)){
				case 0:
					_incS = 4;
				break;
				case 1:
					_incS = -4;
				break;
				default:
					_incS = 0;
				break;
			}
			if(_start==3){
				switch(_idx){
					case 1:
					case 2:
						_incD=0;
						_mode=1;
						_count=4;
					break;
				}
			}
			if(!_mode){
				_incS=SR(_incS,1);
				_incD=SR(_incD,1);
			}
		}
		_changed=0;
	}
	return 0;
}

int gbadev::__dma::write(u32 a,u32 v){
	_changed=1;
	return 1;
}

int gbadev::__dma::read(u32,u32 *){return -1;}

};
