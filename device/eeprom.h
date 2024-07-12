#include "arkad.h"

#ifndef __EEPROMH__
#define __EEPROMH__


class eeprom_device : FileStream{
public:
	eeprom_device(int db=16,int cab=6);
	virtual ~eeprom_device();
	virtual int _write(u8 v);
	virtual int _read(u8 *p);
	virtual int _reset();
	virtual int Read();
	virtual int Write();
	const char  *buffer(){return (char  *)_buffer;};
protected:
	u32 _data,_size;
	u8 *_buffer;

	int _allocBuffer(u32 sz=0);
	virtual int read(int i);
	virtual int write(int i);

	union{
		struct{
			unsigned int _value:1;
			unsigned int _clock:1;
			unsigned int _select:1;
			unsigned int _state:3;
			unsigned int _didx:6;
			unsigned int _locked:1;
			unsigned int _address:8;
			unsigned int _command:5;
			unsigned int _data_bits:6;
			unsigned int _command_bits:5;
		};
		u16 _status;
		u64 _status64;
	};

	enum eeprom_state : u8{
		STATE_IN_RESET,
		STATE_WAIT_FOR_START_BIT,
		STATE_WAIT_FOR_COMMAND,
		STATE_READING_DATA,
		STATE_WAIT_FOR_DATA,
		STATE_WAIT_FOR_COMPLETION
	};

	enum eeprom_command : u8{
		COMMAND_INVALID,
		COMMAND_READ,
		COMMAND_WRITE,
		COMMAND_ERASE,
		COMMAND_LOCK,
		COMMAND_UNLOCK,
		COMMAND_WRITEALL,
		COMMAND_ERASEALL,
		COMMAND_COPY_EEPROM_TO_RAM,
		COMMAND_COPY_RAM_TO_EEPROM
	};
};

#endif