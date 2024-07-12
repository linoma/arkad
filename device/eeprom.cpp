#include "eeprom.h"

eeprom_device::eeprom_device(int db,int cab) :FileStream(){
	_buffer=NULL;
	_size=0;
	_status64=0;
	_data_bits=db;
	_command_bits=cab;
}

eeprom_device::~eeprom_device(){
	if(_buffer)
		delete []_buffer;
	_buffer=NULL;
}

int eeprom_device::_write(u8 v){
	int res=-1;

	_select=SR(v,2);
	_clock=SR(v,1);
	switch(_state){
		case STATE_IN_RESET:
			if(_select)
				_state=STATE_WAIT_FOR_START_BIT;
			res=0;
		break;
		case STATE_WAIT_FOR_START_BIT:
			if(_clock  && (v&1)){
				_state=STATE_WAIT_FOR_COMMAND;
				_data=0;
				_didx=0;
			}
			if(!_select)
				_state=STATE_IN_RESET;
			res=0;
		break;
		case STATE_WAIT_FOR_COMMAND:
			if(!_select)
				_state=STATE_IN_RESET;
			if(_clock){
				_data = (_data << 1) | (v & 1);
				_didx++;
				//printf("epro %x %u %u %x\n",_data,_didx,_state,v);
				if(_didx == (2+_command_bits)){
					_command=SR(_data,_command_bits);
					_address=_data&(BV(_command_bits)-1);
					_didx=0;
				//	printf("eeprrom %x %u %u\n",_command,_address,_state);
					switch(_command){
						case 1:
							_command=COMMAND_WRITE;
							_state=STATE_WAIT_FOR_DATA;
							_data=0;
							//EnterDebugMode();
						break;
						case 2:
							_command=COMMAND_READ;
							_state=STATE_READING_DATA;
							_data=0;
						break;
						case 0:
							switch(SR(_data,_command_bits)&3){
								case 0:
									_command=COMMAND_LOCK;
									_state=STATE_IN_RESET;
									_locked=1;
								break;
								default:
								printf("eeprom %x\n",_command);
									EnterDebugMode();
									break;
							}
							break;
						case 3:
							_state=STATE_WAIT_FOR_COMPLETION;
							_command = COMMAND_ERASE;
						break;
						default:
						printf("eeprom 2 %x\n",_command);
							EnterDebugMode();
							_state=STATE_IN_RESET;
						break;
					}
				}
			}
			res=0;
		break;
		case STATE_READING_DATA:
			//printf("eprord %x %u %u %x\n",_data,_didx,_state,_status);
			if(_clock){
				int i;

				if(!(i=_didx++) || !(i & 15)){
					read(i);
					//printf("eeprom read %u %u %x\n",_address + SR(i,3),i & 15,_data);
				}
				else
					_data=SL(_data,1) | 1;
			}
			if(!_select)
				_state=STATE_IN_RESET;
			res=0;
		break;
		case STATE_WAIT_FOR_DATA:
			if(!_select)
				_state=STATE_IN_RESET;
			if(_clock){
				_data=SL(_data,1) | (v&1);
				_didx++;
				if(_didx == _data_bits){
					//printf("eeprro write %x %x\n",_address,_data);
					write(0);
					_state=STATE_WAIT_FOR_COMPLETION;
				}
			}
			res=0;
		break;
		case  STATE_WAIT_FOR_COMPLETION:
			if(!_select)
				_state=STATE_IN_RESET;
			res=0;
		break;
	}
	return res;
}

int eeprom_device::_read(u8 *p){
	u8 res;

	res=1;
	if(_state == STATE_READING_DATA && (_data & 0x80000000) == 0)
		res=0;
	*p=res;
	return 0;
}

int eeprom_device::_reset(){
	_data=0;
	_status=0;
	return 0;
}

int eeprom_device::Read(){
	if(_buffer)
		return 0;
	_allocBuffer();
	FileStream::Read(_buffer,_size);
	return 0;
}

int eeprom_device::Write(){
	if(!_buffer || !_size) return -1;
	FileStream::Write(_buffer,_size);
	return 0;
}

int eeprom_device::_allocBuffer(u32 sz){
	void *p;

	if(!sz) sz =BV(8);
	p=_buffer && _size <= sz ? _buffer : NULL;
	if(!p){
		if(!(_buffer  = new u8[sz+1])) return  -1;
		_size=sz;
		memset(_buffer,0,sz+1);
	}
	return 0;
}

int eeprom_device::read(int i){
	Read();
	_data=SL(((u16 *)_buffer)[_address + SR(i,4)],32-_data_bits);
	return 0;
}

int eeprom_device::write(int i){
	_allocBuffer();
	((u16 *)_buffer)[_address + SR(i,4)] = (u16)_data;
	return 0;
}
