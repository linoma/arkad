#include "c64dev.h"

namespace c64{

c64dev::c64dev(){
}

c64dev::~c64dev(){
}

int c64dev::Destroy(){
	return 0;
}

int c64dev::Reset(){
	for(int i=0;i<sizeof(_cia)/sizeof(__cia);i++)
		_cia[i].reset();
	_keyboard.reset();
	for(int i=0;i<sizeof(_joy)/sizeof(__joystick);i++)
		_joy[i].reset();
	_ports[4]=0x7;
	_ports[3]=0x17;
	return 0;
}

int c64dev::Init(int,void *a,void *b,u32 f){
	for(int i=0;i<sizeof(_cia)/sizeof(__cia);i++)
		_cia[i].Init(i,a,b,f);
	if(_keyboard.Init(0,a,b,f))
		return -7;
	for(int i=0;i<sizeof(_joy)/sizeof(__joystick);i++)
		_joy[i].Init(i,a,b,f);
	_cia[0].Connect(&_keyboard,0,0);
	_cia[0].Connect(&_joy[0],0,0);
	_cia[0].Connect(&_joy[1],0,0);
	return 0;
}

c64dev::__cia::__cia(){
	_ioreg=NULL;
	_mem=NULL;
}

int c64dev::__cia::reset(){
	memset(_regs,0,sizeof(_regs));
	_regs[TA_LO]=0xff;
	_regs[TA_HI]=0xff;
	_regs[TB_LO]=0xff;
	_regs[TB_HI]=0xff;
	_timers[0].reset();
	_timers[1].reset();
	_tod.reset();
	_sdr.reset();
	if(_idx==B)
		_ioreg[0xd00]=_regs[PRA]=3;
	return 0;
}

int c64dev::__cia::enterIRQ(u8 v){
	_regs[ICR] |= v;
	if(!(_regs[IMR] & v))
		return 1;
	_regs[ICR] |= 0x80;
	machine->OnEvent(_idx+2,1);
	return 0;
}

int c64dev::__cia::_remap(void *a,void *b){
	_ioreg=(u8 *)b;
	_mem=(u8 *)a;
	return 0;
}

int c64dev::__cia::Init(int n,void *a,void *b,u32 freq){
	_remap(a,b);
	_idx=n;
	_sdr._idx=n;
	_sdr._freq=freq;
	_tod._sync=n;
	_tod._freq=freq;
	_timers[0]._freq=freq;
	_timers[1]._freq=freq;
	_regs[PRA]=0xc0;
	return 0;
}

int c64dev::__cia::Connect(IDevice *dev,u32 devId,u32 portId){
	_devices.push_back(dev);
	return 0;
}

int c64dev::__cia::Trigger(u32 what,u32 devId,u32 portId,void *b){
	enterIRQ((u8)(u64)b);
	return 0;
}

int c64dev::__cia::write(u32 a,u8 v){
	u8 b;

	DLOG("CIA%d W %x %x",_idx,a&0xf,v);
	switch((b=a&0xf)){
		case CRA:
			_regs[b]=v;
			_timers[0]._control._value=v;
			if(_timers[0]._control.a._load){
				_timers[0].start();
			}
		break;
		case CRB:
			_regs[b]=v;
			_timers[1]._control._value=v;
			//if((v&0x80) && !_tod._ae) printf("alarn %x\n",_pc);
			_tod._ae=SR(v,7);
			_tod._changed=1;
			if(_timers[1]._control.b._load){
				_timers[1].start();
			}
		break;
		case TA_LO:
			_timers[0]._load=(_timers[0]._load&0xff00)|v;
		break;
		case TA_HI:
			_timers[0]._load=(_timers[0]._load&0xff)|SL(v,8);
		/*	if(_timers[0]._control.a._rmode){
				_timers[0].start();
				_regs[CRA] |= 1;
				EnterDebugMode(DEBUG_BREAK_DEVICE);
			}*/
		break;
		case TB_LO:
			_timers[1]._load=(_timers[1]._load&0xff00)|v;
		break;
		case TB_HI:
			_timers[1]._load=(_timers[1]._load&0xff)|SL(v,8);
		/*	if(_timers[1]._control.b._rmode){
				_timers[1].start();
				_regs[CRB] |= 1;
				EnterDebugMode(DEBUG_BREAK_DEVICE);
			}*/
		break;
		case TOD_10THS:
			_regs[b]=v;
			_tod._changed=1;
			if(!(_regs[CRB] & 0x80)){
				_tod.enable(1);
				_tod._count=(_tod._count&0xffff00)|v;
				if(!_tod.alarm(_tod._count)) enterIRQ(4);
			}
			else
				_tod.reg[4] = v;
		break;
		case TOD_SEC:
			if(!(_regs[CRB] & 0x80))
				_tod._count=(_tod._count&0xff00ff)|SL(v,8);
			else
				_tod.reg[4+1] = v;
			_tod._changed=1;
		break;
		case TOD_MIN:
			_regs[b]=v;
			_tod._changed=1;
			if(!(_regs[CRB] & 0x80)){
				_tod.enable(0);
				_tod._count=(_tod._count&0xffff)|SL(v,16);
			}
			else
				_tod.reg[4+2]=v;
		break;
		case ICR://13
			if(v & 0x80){
				_regs[IMR] |= v & 0x1f;
				if(_regs[ICR] & _regs[IMR]){
					_regs[ICR] |= 0x80;
					machine->OnEvent(_idx+2,1);
				}
			}
			else
				_regs[IMR] &= ~v;
		break;
		case SDR:
			_sdr.write(v);
		default:
			_regs[b]=v;
		break;
	}
	return 0;
}

int c64dev::__cia::read(u32 a,u8 *m){
	u8 b;

	switch((b=a&0xf)){
		case PRA:
			switch(_idx){
				case B:
					//printf("CIA B PRA\n");
					*m=0x3f;
					return 0;
				case A:{
					u8 k[16],r,t,joy1,joy2;

					_devices[1]->Read(&joy1,1);
					joy2=0xff;
					r=_regs[PRA] | ~_regs[DDRA];
					t=(_regs[PRB] | ~_regs[DDRB]) & joy1;//joystick 1
					_devices[0]->Read(k,16);

					if (!(t & 0x01)) r &= k[0];	// AND all active columns
					if (!(t & 0x02)) r &= k[1];
					if (!(t & 0x04)) r &= k[2];
					if (!(t & 0x08)) r &= k[3];
					if (!(t & 0x10)) r &= k[4];
					if (!(t & 0x20)) r &= k[5];
					if (!(t & 0x40)) r &= k[6];
					if (!(t & 0x80)) r &= k[7];
					*m=r & joy2;//joystick2
					}
					return 0;
			}
		break;
		case PRB:
			switch(_idx){
				case A:{
					u8 k[16],r,t,joy1,joy2;

					_devices[0]->Read(k,16);
					joy2=0xff;
					_devices[1]->Read(&joy1,1);
					t=(_regs[PRA] | ~_regs[DDRA]) & joy2;//joystick2
					r= ~_regs[DDRB];

					if (!(t & 0x01)) r &= k[0+8];	// AND all active columns
					if (!(t & 0x02)) r &= k[1+8];
					if (!(t & 0x04)) r &= k[2+8];
					if (!(t & 0x08)) r &= k[3+8];
					if (!(t & 0x10)) r &= k[4+8];
					if (!(t & 0x20)) r &= k[5+8];
					if (!(t & 0x40)) r &= k[6+8];
					if (!(t & 0x80)) r &= k[7+8];

					*m= (r|(_regs[PRB] & _regs[DDRB])) & joy1;//joystick1
					//EnterDebugMode();
					return 0;
				}
				break;
			}
		break;
		case TA_LO:
		case TA_HI:
			_timers[0].update();
			_regs[TA_LO]=(u8)_timers[0]._count;
			_regs[TA_HI]=SR(_timers[0]._count,8);
			*m=_regs[b];
		break;
		case TB_LO:
		case TB_HI:
			_timers[1].update();
			_regs[TB_LO]=(u8)_timers[1]._count;
			_regs[TB_HI]=SR(_timers[1]._count,8);
			*m=_regs[b];
		break;
		case TOD_10THS:
			_tod.update();
			_regs[TOD_10THS]=_tod._count;
			if(_tod._latch)
				_tod.latch(0);
			*m=_regs[b];
		break;
		case TOD_SEC:
			_tod.update();
			_regs[TOD_SEC]=SR(_tod._count,8);
			*m=_regs[b];
		break;
		case TOD_MIN:
			_tod.update();
			_regs[TOD_MIN]=SR(_tod._count,16);
			if(!_tod._latch){
				_tod.latch(1);
			}
			*m=_regs[b];
		break;
		case ICR:
			*m=_regs[ICR];
			_regs[ICR]=0;
			machine->OnEvent(_idx+2,2);
			break;
		case SDR:
			if(_idx==A){
				//_extdevices.keyboard->read(&_regs[SDR]);
			}
		default://PRA CIA1 serial/par status
			*m=_regs[b];
		break;
	}
	//DLOG("CIA R %d %x %x",_idx,SR(a,8)&15,*m);
	return 0;
}

int c64dev::__cia::flush(){
	update(0);

	_regs[TA_LO]=(u8)_timers[0]._count;
	_regs[TA_HI]=SR(_timers[0]._count,8);
	_regs[TB_LO]=(u8)_timers[1]._count;
	_regs[TB_HI]=SR(_timers[1]._count,8);
	_regs[TOD_10THS]=_tod._count;
	_regs[TOD_SEC]=SR(_tod._count,8);
	_regs[TOD_MIN]=SR(_tod._count,16);
	_regs[TOD_HR]=SR(_tod._count,24);
	_regs[TOD_A10THS]=_tod._count;
	_regs[TOD_ASEC]=SR(_tod._alarm,8);
	_regs[TOD_AMIN]=SR(_tod._alarm,16);
	_regs[TOD_AHR]=SR(_tod._alarm,24);
	return 0;
}

int c64dev::__cia::update(int cyc){
	if(!_timers[0].update(cyc)){
		enterIRQ(1);
		_regs[CRA] = (_regs[CRA] & ~1) | _timers[0]._control.c._start;
		if(_regs[CRA]&0x40){
			if(!_sdr.update())
				enterIRQ(8);
		}
	}
	if(!_timers[1].update(cyc)){
		enterIRQ(2);
		_regs[CRB] = (_regs[CRB] & ~1) | _timers[1]._control.c._start;
	}
	if(!_tod.update(cyc))
		enterIRQ(4);
	//_sdr.update(cyc);
	return 0;
}

int c64dev::__cia::__timer::reset(){
	_control._value=0;
	_load=_count=0xffff;
	return 0;
}

int c64dev::__cia::__timer::update(int){
	u16 count;

	if(!_control.a._start)
		return -1;
	u32 d=D_CYCLES(_cycles,_freq,__cycles);	//__cycles >= _cycles ? __cycles - _cycles : (_freq - _cycles) + __cycles;
	//if(_control.b._inmode) fixme todo
//	printf("timer %x %u %x %x\n",_control.b._inmode,d,_count,_load);
	if(!d)
		return 1;
	_cycles=__cycles;
	count=_count;
	_count -= d;
	if(_count < count)
		return 1;
	if(_control.c._rmode) _control.c._start=0;
	if(_control.c._load) _count=_load;//-(d-count);
	return 0;
}

int c64dev::__cia::__timer::start(){
	_control.a._start=1;
	_count=_load;
	_cycles=__cycles+1;
	return 0;
}

int c64dev::__cia::__tod::reset(){
	_cycles=__cycles;
	_count=0;
	_enabled=0;
	_ae=0;
	_changed=0;
	*(u64 *)reg=0;
	return 0;
}

int c64dev::__cia::__tod::enable(int  v){
	_enabled=v;
	_changed=1;
	_cycles=__cycles;
	return 0;
}

int c64dev::__cia::__tod::update(int){
	u32 count;

	//cia a vsync
	//cia b hsync
	if(!_enabled)
		return -1;
	if(_changed){
		if(_ae){
			_alarm=*(u32 *)&reg[4];
		}
		_changed=0;
	}
	u32 d=__cycles >= _cycles ? __cycles-_cycles: (_freq-_cycles) + __cycles;
	d /= 20280;
	if(!d)
		return 1;
	_cycles=__cycles;
	count=_count;
	_count = (_count + d) & 0xffffff;
	if(!alarm(count)) return 0;
	return 2;
}

int c64dev::__cia::__tod::latch(int v){
	if((_latch=v&1))
		_countl=_count;
	return 0;
}

int c64dev::__cia::__tod::alarm(u32 count){
	if(!_ae) return 1;
	if((_alarm & ~7) == (_count & ~7) || (_count < count && _alarm < _count))
		return 0;
	return -1;
}

int c64dev::__cia::__sdr::write(u8){
	if(!_shift) _shift=15;
	return -1;
}

int c64dev::__cia::__sdr::reset(){
	_cycles=__cycles;
	_shift=0;
	return 0;
}

int c64dev::__cia::__sdr::update(int){
	//u32 d=__cycles >= _cycles ? __cycles - _cycles:_freq-_cycles + __cycles;
	//_cycles=__cycles;
	if(_shift && !--_shift) return 0;
	return 1;
}

c64dev::__keyboard::__keyboard() : ADevice(){
	_cols=&_rows[8];
}

c64dev::__keyboard::~__keyboard(){
}

int c64dev::__keyboard::reset(){
	_cycles=__cycles;
	_status=0;
	memset(_rows,0xff,sizeof(_rows));
	clear();
	return 0;
}

int c64dev::__keyboard::update(int cyc){
	u32 d=D_CYCLES(_cycles,_freq,__cycles);
	//if(d<65536) return 0;
	_cycles=__cycles;
	if(!size()){
		memset(_rows,0xff,sizeof(_rows));
		return 0;
	}
	__message &m=front();
	erase(begin());
	_translate(m._buf[1],m._buf[0]|SL(m._buf[2],1));
	return 0;
}

int c64dev::__keyboard::Init(int n,void *a,void *b,u32 f){
	ADevice::Init(n,a,b,f);
	/*_ciaa=(IDevice *)CIAA;
	cpu->Query(ICORE_QUERY_IO_DEVICE,&_ciaa);
	_ciaareg=(u8 *)CIAA;
	cpu->Query(ICORE_QUERY_IO_PORT,&_ciaareg);*/
	return 0;
}

int c64dev::__keyboard::Read(void *p,u32,u32 *r){
	if(!p) return -1;
	memcpy(p,&_rows[8],8);
	memcpy((u8 *)p+8,&_rows[0],8);
	//memcpy(p,_rows,sizeof(_rows));
	return 0;
}

#define MATRIX(a,b) (((a) << 3) | (b))

int c64dev::__keyboard::_translate(u32 &key,u32 flags){
	//printf("%x %x\n",key,flags);
	int c64_key = -1;
	switch (toupper(key)) {
		case 'A': c64_key = MATRIX(1,2); break;
		case 'B': c64_key = MATRIX(3,4); break;
		case 'C': c64_key = MATRIX(2,4); break;
		case 'D': c64_key = MATRIX(2,2); break;
		case 'E': c64_key = MATRIX(1,6); break;
		case 'F': c64_key = MATRIX(2,5); break;
		case 'G': c64_key = MATRIX(3,2); break;
		case 'H': c64_key = MATRIX(3,5); break;
		case 'I': c64_key = MATRIX(4,1); break;
		case 'J': c64_key = MATRIX(4,2); break;
		case 'K': c64_key = MATRIX(4,5); break;
		case 'L': c64_key = MATRIX(5,2); break;
		case 'M': c64_key = MATRIX(4,4); break;
		case 'N': c64_key = MATRIX(4,7); break;
		case 'O': c64_key = MATRIX(4,6); break;
		case 'P': c64_key = MATRIX(5,1); break;
		case 'Q': c64_key = MATRIX(7,6); break;
		case 'R': c64_key = MATRIX(2,1); break;
		case 'S': c64_key = MATRIX(1,5); break;
		case 'T': c64_key = MATRIX(2,6); break;
		case 'U': c64_key = MATRIX(3,6); break;
		case 'V': c64_key = MATRIX(3,7); break;
		case 'W': c64_key = MATRIX(1,1); break;
		case 'X': c64_key = MATRIX(2,7); break;
		case 'Y': c64_key = MATRIX(3,1); break;
		case 'Z': c64_key = MATRIX(1,4); break;

		case '0': case ')':c64_key = MATRIX(4,3); break;
		case '1': case '!': c64_key = MATRIX(7,0); break;
		case '2': case '@': c64_key = MATRIX(7,3); break;
		case '3': case '#':c64_key = MATRIX(1,0); break;
		case '4': case '$':c64_key = MATRIX(1,3); break;
		case '5': case '%':c64_key = MATRIX(2,0); break;
		case '6': case '^':c64_key = MATRIX(2,3); break;
		case '7': case '&':c64_key = MATRIX(3,0); break;
		case '8': case '*':c64_key = MATRIX(3,3); break;
		case '9': case '(':c64_key = MATRIX(4,0); break;

		case ' ': c64_key = MATRIX(7,4); break;
		case ',': c64_key = MATRIX(5,7); break;
		case '=': c64_key = MATRIX(6,5); break;
		case '-': c64_key = MATRIX(5,0); break;
		case ':': c64_key = MATRIX(5,5); break;		// :
		case ';': c64_key = MATRIX(6,2); break;		// ;
		case '.': c64_key = MATRIX(5,4); break;
		case '[': c64_key = MATRIX(5,6); break;
		case ']': c64_key = MATRIX(6,1); break;
		case '/': c64_key = MATRIX(6,7); break;
		case '\\': c64_key = MATRIX(5,3); break;
		case '`': c64_key = MATRIX(7,1); break;

		case GDK_KEY_F1: c64_key = MATRIX(0,4); break;
		case GDK_KEY_F2: c64_key = MATRIX(0,4) | 0x80; break;
		case GDK_KEY_F3: c64_key = MATRIX(0,5); break;
		case GDK_KEY_F4: c64_key = MATRIX(0,5) | 0x80; break;
		case GDK_KEY_F5: c64_key = MATRIX(0,6); break;
		case GDK_KEY_F6: c64_key = MATRIX(0,6) | 0x80; break;
		case GDK_KEY_F7: c64_key = MATRIX(0,3); break;
		case GDK_KEY_F8: c64_key = MATRIX(0,3) | 0x80; break;
		case GDK_KEY_Return: c64_key = MATRIX(0,1);break;
		case GDK_KEY_Delete:
		case GDK_KEY_BackSpace:c64_key = MATRIX(0,0); break;			// INS/DEL
		case GDK_KEY_Shift_L: c64_key = MATRIX(6,4); break;
		case GDK_KEY_Control_L: c64_key = MATRIX(7,2); break;
		case GDK_KEY_Alt_L: c64_key = MATRIX(7,5); break;
		case GDK_KEY_Home: c64_key = MATRIX(6,3); break;
		case GDK_KEY_End: c64_key = MATRIX(6,0); break;
		case GDK_KEY_Up: c64_key = MATRIX(0,7)|0x80; break;
		case GDK_KEY_Down: c64_key = MATRIX(0,7); break;
		case GDK_KEY_Right: c64_key = MATRIX(0,2); break;
		case GDK_KEY_Left: c64_key = MATRIX(0,2)|0x80; break;
		case GDK_KEY_Escape: c64_key = MATRIX(7,7); break;
		/*case SDL_SCANCODE_GRAVE: c64_key = MATRIX(7,1); break;			// ←
		case SDL_SCANCODE_BACKSLASH: c64_key = MATRIX(6,6); break;		// ↑

		case SDL_SCANCODE_ESCAPE: c64_key = MATRIX(7,7); break;			// RUN/STOP
		case SDL_SCANCODE_BACKSPACE:
		case SDL_SCANCODE_DELETE: c64_key = MATRIX(0,0); break;			// INS/DEL
		case SDL_SCANCODE_INSERT: c64_key = MATRIX(0,0) | 0x80; break;
		case SDL_SCANCODE_HOME: c64_key = MATRIX(6,3); break;			// CLR/HOME
		case SDL_SCANCODE_END: c64_key = MATRIX(6,0); break;			// £
		case SDL_SCANCODE_PAGEUP: c64_key = MATRIX(6,6); break;			// ↑
		case SDL_SCANCODE_PAGEDOWN: c64_key = MATRIX(6,5); break;		// =

		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_TAB:
		case SDL_SCANCODE_RCTRL: c64_key = MATRIX(7,2); break;
		case SDL_SCANCODE_LSHIFT: c64_key = MATRIX(1,7); break;
		case SDL_SCANCODE_RSHIFT: c64_key = MATRIX(6,4); break;
		case SDL_SCANCODE_LALT: c64_key = MATRIX(7,5); break;			// C=
		case SDL_SCANCODE_RALT: c64_key = MATRIX(7,5); break;			// C=

		case SDL_SCANCODE_UP: c64_key = MATRIX(0,7)| 0x80; break;
		case SDL_SCANCODE_DOWN: c64_key = MATRIX(0,7); break;
		case SDL_SCANCODE_LEFT: c64_key = MATRIX(0,2) | 0x80; break;
		case SDL_SCANCODE_RIGHT: c64_key = MATRIX(0,2); break;

		case SDL_SCANCODE_KP_0:
		case SDL_SCANCODE_KP_5: c64_key = 0x10 | 0x40; break;
		case SDL_SCANCODE_KP_1: c64_key = 0x06 | 0x40; break;
		case SDL_SCANCODE_KP_2: c64_key = 0x02 | 0x40; break;
		case SDL_SCANCODE_KP_3: c64_key = 0x0a | 0x40; break;
		case SDL_SCANCODE_KP_4: c64_key = 0x04 | 0x40; break;
		case SDL_SCANCODE_KP_6: c64_key = 0x08 | 0x40; break;
		case SDL_SCANCODE_KP_7: c64_key = 0x05 | 0x40; break;
		case SDL_SCANCODE_KP_8: c64_key = 0x01 | 0x40; break;
		case SDL_SCANCODE_KP_9: c64_key = 0x09 | 0x40; break;*/

		default: break;
	}

	if (c64_key < 0)
		return -1;

	// Handle joystick emulation
	if (c64_key & 0x40) {
		c64_key &= 0x1f;
	/*	if (key_up) {
			*joystick |= c64_key;
		} else {
			*joystick &= ~c64_key;
		}*/
		return 0;
	}

	// Handle other keys
	int shifted = c64_key & 0x80;
	int c64_byte = (c64_key >> 3) & 7;
	int c64_bit = c64_key & 7;
	if (flags & 1) {
		if (shifted) {
			_rows[6] |= 0x10;
			_cols[4] |= 0x40;
		}
		_rows[c64_byte] |= (1 << c64_bit);
		_cols[c64_bit] |= (1 << c64_byte);
	}
	else {
		if (shifted) {
			_rows[6] &= 0xef;
			_cols[4] &= 0xbf;
		}
		_rows[c64_byte] &= ~(1 << c64_bit);
		_cols[c64_bit] &= ~(1 << c64_byte);
	}
	return 0;
}

int c64dev::__joystick::reset(){
	_status=0;
	memset(_buf,0,sizeof(_buf));
	_cycles=__cycles;
	clear();
	return 0;
}

int c64dev::__joystick::update(int cyc){
	u32 d,bit;

	d=(__cycles >= _cycles ? __cycles - _cycles : (_freq-_cycles) + __cycles);
//	if(!SR(d,1))
//		goto Z;
	_cycles=__cycles;
	if(size()){
		struct __message &m=front();
		erase(begin());
		memcpy(_buf,m._buf,4*sizeof(u32));
	}
	else
		memset(_buf,0,sizeof(_buf));
Z:
	/*if((_ciareg[__cia::DDRA] & bit)==0){
		bit=SL(0x40,_idx);
		if(!(_buf[2] & 1))
			_ciareg[__cia::PRA] |= bit;
		else
			_ciareg[__cia::PRA] &= ~bit;
		//printf("%x\n",_ciaareg[__cia::PRA]);
	}*/
	return 0;
}

int c64dev::__joystick::Init(int i,void *a,void *b,u32 f){
	if(ADevice::Init(i,a,b,f)) return -1;
	_cia=(IDevice *)(u64)i + 1;
	cpu->Query(ICORE_QUERY_IO_DEVICE,&_cia);
	_ciareg=(u8 *)(u64)i + 1;
	cpu->Query(ICORE_QUERY_IO_PORT,&_ciareg);
	return 0;
}

int c64dev::__joystick::write(u32 a,u8 v){
	return 0;
}

int c64dev::__joystick::read(u32 a,u8 *p){
	u16 l,r,t,b,v;

	v =0xff;
	b=SR(_buf[3],3)&1;
	t=SR(_buf[3],2)&1;
	r=SR(_buf[3],1)&1;
	l=_buf[3]&1;

	v&=~t;
	v &= ~SL(l,2);//right
	v &= ~SL(r,3);//left
	v &= ~SL(t,1);//bottom
	v &= ~SL(_buf[2] & 1,4);
	//if(v!=0xff)	printf("joy %x\n",v);
	*(u8 *)p=v;
	return 0;
}

int c64dev::__joystick::Read(void *p,u32,u32 *r){
	if(!p) return -1;
	return read(0,(u8 *)p);
}

};