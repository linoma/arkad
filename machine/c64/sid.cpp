#include "sid.h"
#include "ccore.h"

namespace c64{

u32 SID::FREQ=0;
u16 *SID::_lfsr=0;

static uint8_t sid_random(){
	static uint32_t seed = 1;
	seed = seed * 1103515245 + 12345;
	return seed >> 16;
}

struct __voice;

static struct __voice{
	union {
		u16 _status;
		u8 _control;
		struct {
			unsigned int _gate:1;
			unsigned int _modulate:1;
			unsigned int _ring:1;
			unsigned int _test:1;
			unsigned int _wave:4;
			unsigned int _sync:1;
			unsigned int _changed:1;
			unsigned int _enabled:1;
			unsigned int _out:1;
			unsigned int _fake:1;
			unsigned int _idx:2;
		};
	};

	struct{
		enum :u8 {STOP,ATTACK=1,DECAY,SUSTAIN,RELEASE};

		union{
			u32 _status;
			u16 _control;
			struct{
				unsigned int _decay:4;
				unsigned int _attack:4;
				unsigned int _release:4;
				unsigned int _sustain:4;
				unsigned int _mode:3;
			};
		};

		u32 _step,_melapsed,_vstep,_mvelapsed;
		s32 _vinc,_vol;

		int step(int cyc){
			int res=0;

			if(_mode==STOP || _mode==SUSTAIN)
				return 0;
			_step += cyc;
			if(_step > _melapsed){
				_step=0;
				_mode++;
				res=1;
			}
			_vstep += cyc;
			if(_vstep > _mvelapsed){

				if(!((_vol==0 && _vinc < 0) || (_vol>=65535 && _vinc > 0))){
					if((_vol += _vinc) < 0){
						_vol=0;
						res=1;
					}
					else if(_vol > 65535)
						_vol=65535;
				}
				//printf("%u\n",_vol);
				_vstep=0;
			}

			return res;
		};

		int reset(){
			_step=0;
			_vol=0;
			_mode=STOP;
			_vstep=0;
			return 0;
		};

	} _eg;

	void adsr(int m){
		u16 _env[]={2,8,16,24,38,56,68,80,100,250,500,800,1000,3000,5000,8000};
		int i;

		if(_eg._mode==m)
			return;
		switch(_eg._mode=m){
			case _eg.ATTACK://attack
				i=_env[_eg._attack];
				_eg._vinc=256;
				_eg._vol=0;
				_out=1;
				break;
			case _eg.DECAY:{//decay
				i=_env[_eg._decay]*3;
				int v =SR(i*SID::SID_FREQ,10);
				v = (_eg._vol - SL(_eg._sustain,12)) / v;
				printf("%u %u %u %u\n",i,_eg._vol,_eg._sustain,(int)v);
				_eg._vinc=-v;
				_out=1;
			}
				break;
			case _eg.SUSTAIN://sustain
				i=0;
				_eg._vinc=0;
				_out=0;
				break;
			case _eg.RELEASE://release
				i=_env[_eg._release]*3;
				_eg._vinc=-256;
				_out=1;
				break;
			default:
				i=0;
				_eg._mode=_eg.STOP;
				_out=0;
			break;
		}
		_eg._melapsed=SR(i*SID::SID_FREQ,10);
		_eg._mvelapsed=SR(_eg._melapsed,8);
		_eg._vstep=0;
		//printf("en switch %u %u %u %.2f %.2f %d\n",_eg._mode,_eg._melapsed,_eg._mvelapsed,_eg._vinc/256.0f,_eg._vol/256.0f,i);
	};

	int init(int n,void *a,void *b){
		_mem=(u8 *)a;
		_ioreg=(u8 *)b;
		_idx=n;
		_enabled=1;
		_prev=_next=NULL;
		return reset();
	};

	int reset(){
		_duty=0;
		_period=0;
		_control=0;
		_out=0;
		_changed=0;
		_sync=0;
		_eg.reset();
		return 0;
	}

	int output(int &sample){
		u32 d;

		sample=0;
		if(_changed){
			_period=*(u16 *)&_ioreg[0x400+(_idx*7)];
			_inc = _period / (float)SID::SID_FREQ * SID::FREQ;
			_duty= SL(*(u16 *)&_ioreg[0x402+(_idx*7)] & 0xfff,12);
			_eg._control= *(u16 *)&_ioreg[0x405+(_idx*7)];
			_control= _ioreg[0x404+(_idx*7)];
			_next->_sync=_modulate;
			adsr(_gate ? _eg.ATTACK:_eg.RELEASE);
			_changed=0;
		}
		if(!_enabled || !_period)
			return 1;
		//d=D_CYCLES(_cycles,_cpu_freq,__cycles);
		if(!(d = _inc)) return 2;
		if(_eg.step(1)==1)
			adsr(_eg._mode);
		_step = (_step + d);
		if(_sync && _step >= 0x1000000 && _next)
			_next->_step=0;
		_step &= 0xffffff;
		sample=0;
		if((_test && !_fake) || !_out || (_idx==2 && (_ioreg[0x418]&0x80))) //test
			return 3;
		switch(_wave){
			case 1://triangle
				sample=_step>>8;
				if(sample >= 0x8000)
					sample = 0xffff-sample;
				sample = SL((s16)sample - 0x4000,1);
				//sample=0;
			break;
			case 2://saw
				sample=(s16)(_step >> 8);
				//sample=0;
			break;
			case 3:{
				int res=sample=_step>>8;
				if(res >= 0x8000)
					res=0xffff-res;
				if(sample >= 0x8000)
					sample = (s16)sample;
				sample = SR((s16)res*(s16)sample,16);
				//sample=0;
			}
			break;
			case 4:
				sample=_step > _duty ? 0 : 32767;
				//sample=0;
			break;
			case 5://trirect
				if(_step > _duty)
					sample=0;
				else{
					sample=_step>>8;
					if(sample >= 0x8000)
						sample=0xffff-sample;
					//sample=0;
				}
			break;
			case 6:
				if(_step > _duty)
					sample=0;
				else{
					sample=_step>>8;
					if(sample >= 0x8000)
						sample = (s16)sample;
				//sample=0;
				}
			break;
			case 7://trisawrect
				if(_step > _duty)
					sample=0;
				else{
					int res=sample=_step>>8;
					if(res >= 0x8000)
						res=0xffff-res;
					if(sample >= 0x8000)
						sample = (s16)sample;
					sample = SR((s16)res*(s16)sample,16);
				//sample=0;
				}
			break;
			case 8:{
				//printf("%d:%u-%u-%u ",_idx,_step,_period,cyc);
				if (_step >= 0x100000) {
					//noise=(s16)(sid_random()<<6);
					_sample=((sample=SID::_lfsr[_step % 2185]) >> (_step & 0xf)) & 1;
					_sample=_sample ? sample : -sample;
					_sample >>= 1;
					//printf("%d\n",noise);
					_step &= 0xfffff;
				}
				sample=(s16)_sample;
				//sample=0;
			}
			break;
			default:
			//	printf("not impl %u\n",_wave);
			break;
		}
		//if(_wave!=8) sample=0;
Z:
		sample=SR((s16)sample * _eg._vol,16);
		return 0;
	};

	int link(struct __voice *l,struct __voice *n){
		_prev=l;
		_next=n;
		return 0;
	}

	u16 _period;
	u32 _step,_duty,_inc;
	int _sample;
protected:
	u8 *_mem,*_ioreg;
	struct __voice *_prev,*_next;
} _voices[3], _voice;

static struct __filter{
	union{
		u8 _control;
		struct{
			unsigned int _voices:3;
			unsigned int _enabled:1;
			unsigned int _a:6;
			unsigned int _idx:2;
			unsigned int _changed:1;
		};
	};
	u16 _fc,_vol;
	u32 _step,_freq,_period;

	int init(int n,void *a,void *b){
		_mem=(u8 *)a;
		_ioreg=(u8 *)b;
		_idx=n;
		_freq=SID::SID_FREQ;
		return reset();
	};

	int reset(){
		_step=0;
		_control=0;
		_vol=_idx==0 ? 256 : 0;
		return 0;
	};

	int update(int){
		if(_changed){
			_fc=_ioreg[0x415] & 7;
			_fc |= SL(_ioreg[0x416],3);
			_enabled=SR(_ioreg[0x418],4+_idx);
			_voices=_ioreg[0x417];
			_period = ((_fc*5.8f)+30) * 8192.0f / _freq;
			//printf("filter freq %u %u %u %u %x\n",_fc,_enabled,_voices,_idx,_ioreg[0x418]);
			_changed=0;
		}
		if(!_enabled)
			return 0;
		_step += 4096;
		if(_step < _period)
			return 0;
		int i = SR(_step-_period,13);
		if(i > 256) i = 256;
		switch(_idx){
			case 0:
				_vol=256-i;
			break;
			case 2:
				_vol=i;
			break;
		}
		if(SR(_step,12) < _freq)
			return 0;
		switch(_idx){
			case 0:
				_vol=256;
			break;
			case 2:
				_vol=0;
				break;
		}
		_step -= SL(_freq,12);
		return 0;
	};

	int output(int &sample,int voice){
		if(!_enabled || !BVT(_voices,voice)) return 1;
		sample = SR(sample*_vol,8);
		return 0;
	};
protected:
	u8 *_mem,*_ioreg;
} _filters[3];

static struct {
	u8 *_pvol,*_pfil;
	u32 _wc,_size,_rc,_tw;

	int reset(){
		_wc=0;
		_rc=0;
		_tw=0;
		return 0;
	};

	int write(u8 v,u8 f){
		_pfil[_wc]=f;
		_pvol[_wc++]=v;
		_tw++;
		if(_wc>=_size)
			_wc -= _size;
		return 0;
	};

	int empty(){
		_tw=0;
		_rc=NFXD(_wc);
		return 0;
	};

	int read(u8 &v,u8 &f,int d){
		v=_pvol[IFXD(_rc)];
		_rc += d;
		if(IFXD(_rc) >= _size)
			_rc &= SM_FXD;
		return 0;
	}
} _sampler;

SID::SID() : PCMDAC(){
	_samples=NULL;
}

SID::~SID(){

}

int SID::Init(void *a,void *b,u32 f){
	_remap(a,b);
	SID::FREQ=f;
	if(PCMDAC::Open(2,SID_FREQ))
		return -1;
	if(!(_samples=new u16[SID_FREQ+2*312*2+KB(5)]))
		return -2;
	_sampler._pvol = (u8 *)&_samples[SID_FREQ];
	_sampler._pfil = &_sampler._pvol[312*2];
	_sampler._size=312*2;
	SID::_lfsr=(u16 *)&_sampler._pfil[312*2];

	for(u16 i=0,i1=0,vs=14,n=0x7fff;;i++){
		if(!(i++ % 15))
			SID::_lfsr[i1++] = n;
		u16 bit = (u16)(n & 1);
		bit = (u16)((bit ^ ((n >>= 1) & 1)) << vs);
		n |= bit;
		if(n==0x7fff) break;
	}

	//buf=(u8 *)&_samples[f];
	for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++)
		_voices[i].init(i,_mem,_ioreg);

	_voices[0].link(&_voices[2],&_voices[1]);
	_voices[1].link(&_voices[0],&_voices[2]);
	_voices[2].link(&_voices[1],&_voices[0]);

	_voice.init(2,_mem,_ioreg);
	for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++)
		_filters[i].init(i,_mem,_ioreg);
	SID::Reset();
	return Create();
}

int SID::_remap(void *a,void *b){
	_ioreg=(u8 *)b;
	_mem=(u8 *)a;
	return 0;
}

int SID::Reset(){
	for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++)
		_voices[i].reset();
	_voice.reset();
	_voice._fake=1;
	//_ioreg[0x401]=0x71;
	//_ioreg[0x404]=0x31;
	//_ioreg[0x405]=0x11;
	//_ioreg[0x406]=0x11;
	//_voices[0]._changed=1;
	return PCMDAC::Reset();
}

int SID::Update(){
	u32 n, count,nn;
	int sampleL,empty;
	s16 *p;
	u8 _fil;

	if(!(p=(s16 *)_samples))
		return -1;
	n=SPU_CYCLES(_cycles,SID::FREQ,SID_FREQ,__cycles);
	if(!n) return 0;
	_cycles=__cycles;

	nn=NFXD(_sampler._tw)/n;
	//printf("%u %u %u %u %u %u\n",SID_FREQ/n,cyc,n,_sampler._tw,SR(_sampler._rc,12),nn);
	empty=1;
	for(count=0;n && count<8000;n--){
		for(int i=0;i<sizeof(_filters)/sizeof(__filter);i++)
			_filters[i].update(1);
		sampleL=0;
		for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++){
			int sample=0;

			if(!_voices[i].output(sample)) empty=0;
			for(int m=0;m<sizeof(_filters)/sizeof(__filter);m++)
				_filters[m].output(sample,i);
			if((sampleL += sample) > 32767)
				sampleL=32767;
			else if(sampleL < -32768)
				sampleL=-32768;
		}
		_sampler.read(_vol,_fil,nn);
		if(empty)
			sampleL=SL(8-_vol,11);
		sampleL = SR(sampleL *_vol,4);
		*p++=(s16)sampleL;
		*p++=(s16)sampleL;
		count++;
	}
	Write(_samples,count);
	_sampler.empty();
	return 0;
}

int SID::Destroy(){
	if(_samples)
		delete []_samples;
	_samples=NULL;
	return PCMDAC::Destroy();
}

int SID::update(int cyc){
	int sample;

	//u32 n=D_CYCLES(__cycles,SID::FREQ,_cycles);
	//printf("%u %u\n",n,cyc);
	//_voice._changed=1;
	//_voice.output(sample,cyc);
	_sampler.write(_vol,_ioreg[0x417]);
	return 0;
}

int SID::write(u32 a,u8 v){
	switch(a & 0x1f){
		default:
			_voices[(a&31)/7]._changed=1;
		break;
		case 0x18:
			_vol=v&15;
		//break;
		case 0x15:
		case 0x16:
		case 0x17:
			for(int i=0;i<sizeof(_filters)/sizeof(__filter);i++)
				_filters[i]._changed=1;
		break;
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		break;
	}
	return 1;
}

int SID::read(u32 adr,u8 *p){
	switch(adr & 0x1f){
		case 0x1b:
			*p = sid_random();
		break;
		default:
			printf("sid read %x\n",adr);
		break;
	}
	return 0;
}

int SID::_dumpRegisters(char *p){
	char c[200];

	sprintf(c,"SID %02X %02X %02X %02X\n",_ioreg[0x418],_ioreg[0x417],_ioreg[0x415],_ioreg[0x416]);
	strcat(p,c);
	for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++){
		sprintf(c," %1d P:%04X D:%04X CTRL:%02X\n",i, *(u16 *)&_ioreg[0x400 + i*7],
			*(u16 *)&_ioreg[0x402 + i*7],
			_ioreg[0x404+i*7]
		);
		strcat(p,c);
	}
	return 0;
}

};