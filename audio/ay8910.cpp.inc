
int __ay8910::reset(){
	_enabled=0;
	_reg=0;
	memset(_regs,0,sizeof(_regs));
	memset(&_noise,0,sizeof(_noise));
	_noise._number=1;
	_noise._period=1;
	for(int i=0;i<sizeof(_channels)/sizeof(struct __tone);i++)
		_channels[i].reset();
	return 0;
}

int __ay8910::write(u8 v){
	_regs[_reg]=v;
	switch(_reg){
		case 0:
		case 1:
		case 8:
		case 11:
		case 12:
			_channels[0]._invalidate=1;
		break;
		case 2:
		case 3:
		case 9:
			_channels[1]._invalidate=1;
		break;
		case 4:
		case 5:
		case 10:
			_channels[2]._invalidate=1;
		break;
		case 6:
			if(!(_noise._period=v&0x1f))
				_noise._period=1;
		break;
	}
	return 0;
}

int __ay8910::read(u8 *p){
	switch(_reg){
		default:
			*p=_regs[_reg];
			break;
	}
	return 0;
}

int __ay8910::output(int cyc){
	int sample;

	if((_noise._count+=cyc) >=_noise._period){
		do{
			u32 i = _noise._period;
			if(i >_noise._count)
				i=_noise._count;
			_noise._count -= i;
			if(!(_noise._prescale ^= 1))
				_noise._number = (_noise._number >> 1) | (((_noise._number&1) ^ SR(_noise._number&8, 3)) << 16);
		}while(_noise._count >= _noise._period );
	}

	for(int i=sample=0;i<sizeof(_channels)/sizeof(struct __tone);i++){
		if((sample+=_channels[i].output(cyc,*this)) > 32767)
			sample=32767;
		else if(sample < -32768)
			sample = -32768;
	}
	return sample;
}

int __ay8910::init(int n){
	_value=0;
	_idx=n;
	for(int i=0;i<sizeof(_channels)/sizeof(struct __tone);i++)
		_channels[i].init(n,i,*this);
	return 0;
}

int __ay8910::__tone::output(int cyc,struct __ay8910 &g){
	int sample=0;

	if(_invalidate){
		_noise_enable=SR(_regs[7],3+_idx);
		_enabled=SR(_regs[7],_idx);
		_env=SR(_regs[_idx+8],4);
		_vol=_regs[_idx+8];
		if(!(_period=((u16 *)_regs)[_idx] & 0xfff))
			_period=1;
		//printf("NOISE %d:%d %u NE:%x E:%x V:%d %d\n",_channel,_idx,_period,_noise_enable,_enabled,_vol,_env);
		_invalidate=0;
	}

	if((_count+=cyc) >= _period){
		do{
			u32 i =_period;
			if(i>_count) i=_count;
			_duty = (_duty-1) & 0x1f;
			_count-= i;
		} while(_count>=_period);
	}
	if(_envelope._holding==0){
		if((_envelope._count+=cyc) >= SL(_envelope._holding,1)){
			_envelope._count=0;
			_envelope._step--;
			if(_envelope._step < 0){
				if(_envelope._hold){
					_envelope._holding=1;
					_envelope._step=0;
				}
				else{
				}
			}
		}
	}
	if(( (_enabled|(_duty & 1)) & (_noise_enable|(g._noise._number&1)) ) == 0)
		return 0;
	_envelope._vol=_envelope._step^_envelope._attack;
	if(_env){
		sample=0;
	}
	else
		sample = SL(_vol,10);
	return sample;
}

int __ay8910::__tone::reset(){
	_count=_period=0;
	_vol=_duty=0;
	_enabled=0;
	_noise_enable=0;
	_env=0;

	_invalidate=1;
	memset(&_envelope,0,sizeof(_envelope));
	return 0;
}

int __ay8910::__tone::init(int ch,int n,struct __ay8910 &g){
	_regs=g._regs;
	_idx=n;
	_channel=ch;
	return 0;
}
