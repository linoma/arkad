#include "amiga500dev.h"

namespace amiga {

static FILE *lino[4]={0};

struct __achannel{
	__achannel(){_status=0;_enabled=1;_buf=NULL;_szb=0;};
	u32 _cycles,_period,_vol,_adr,_cadr,_freq,_twr,_step,_rstep,_pstep;
	s32 _length;
	u16 _cr,_cw,_szb;
	u8 *_buf,_dat[3];

	union{
		u32 _status;
		u16 _status16[2];
		u8 _status8[4];
		struct{
			unsigned int _dma:1;
			unsigned int _fm:1;
			unsigned int _am:1;
			unsigned int _manual:1;
			unsigned int _a0:4;
			unsigned int _changed:7;
			unsigned int _fetch:1;
			unsigned int _idx:2;
			unsigned int _enabled:1;
		};
	};

	int _push(u8 v){
		_buf[_cw++]=v;
		_twr++;
		//if(_twr >= _szb) printf("OOb %d %u %u\n",_idx,_twr,_szb);
		if(_cw >= _szb)
			_cw = 0;
		return 0;
	}

	int write(u16 sample,u32 flags=0){
		_manual = flags & 1;
		_dat[0]=(u8)sample;
		_dat[1]=(u8)SR(sample,8);
		return 0;
	};

	int reset(){
		_adr=0;
		_cadr=0;
		_period=0;
		_length=0;
		_vol=0;
		_cycles=0;
		_status16[0]=0;
		_cr=_szb/2;
		_cw=0;
		_twr=0;
		if(_buf)
			memset(_buf,0,_szb);
		return 0;
	};

	int init(int n,void *a,void *b,void *buf=NULL,u32 sz=0,u32 f=0){
		_buf=(u8 *)buf;
		_szb=sz;
		_mem=(u8 *)a;
		_ioreg=(u16 *)b;
		_idx=n;
		_enabled=1;
		_freq=f;
		return reset();
	};

	int output(int &sample,int inc){
		sample=0;
		if(!_enabled || !_twr || _am || _fm)
			return 1;

		u32 u=_rstep>>12;
		_rstep+=inc;
		sample=(int)(s16)(s8)_buf[_rstep>>12];
		sample=SR(sample * _vol,6);
#ifdef _DEVELOPa
		if(!lino[_idx]){ char c[30];sprintf(c,"lino%d.raw",_idx);lino[_idx]=fopen(c,"wb+");}
		if(lino[_idx]){
			s8 v=(s8)sample;
			fwrite(&v,1,1,lino[_idx]);
		}
#endif
		sample = SL(sample,7);
		_twr -= (_rstep >> 12) - u;
		if((_rstep>>12) >= _szb)
			_rstep &= 0xfff;
		return 0;
	};

	int dma(int v){
		if(v==_dma) return 0;
		//if(v && !_dma) machine->OnEvent(7+_idx,1);
		_dma=v;
		_changed |= 0x20;
		return 0;
	};

	int step(int cyc){
		int res=0;

		if(_changed & 0x1f){
			if(_changed & 3)
				_cadr=_adr=BELE32(ACHIPREG32_(_ioreg,REG_AUD0LCH+_idx*8));
			if(_changed & 4)
				_length=SWAP16(ACHIPREG_(_ioreg,REG_AUD0LEN+_idx*8));
			if(_changed & 0x28){
				_period=SWAP16(ACHIPREG_(_ioreg,REG_AUD0PER+_idx*8));//3579545 PAL=3546895
				_cycles=__cycles;
				_step=0;
				_pstep=0;
			}
			if(_changed&0x10)
				_vol=SWAP16(ACHIPREG_(_ioreg,REG_AUD0VOL+_idx*8));
			//printf("PAULA %d %d %x %u %u %u %u:%u\n",_idx,_dma,_adr,_period,_vol,_length,_fm,_am);
			_changed=0;
		}
		if(_length==0 || _period==0)
			return 0;
		u32 d=__cycles >= _cycles ? __cycles - _cycles:_freq-_cycles + __cycles;
		_step += d/2;
		_cycles=__cycles;
	//	if(!_idx && _twr) printf("s %u %u %u %u\n",d,_period,_twr,_step);
		for(;_step >= 124 && !res;_step -= 124){
			if((_pstep += 124) >= _period*2){
				_pstep -= _period*2;
				if(_dma){
					write(*(u16 *)&_mem[_cadr]);
					_cadr += 2;
					if(--_length==0){
						_changed |= 3|4|8;
						res=1;
					}
				}
			}
			if(_manual || _dma){
				_push(_dat[_pstep > _period ? 1 : 0]);
			}
			if(_manual)
				res = 1;
			_manual=0;
		}
		return res;
	}

	protected:
	u8 *_mem;
	u16 *_ioreg;
} _achannels[4];

int Paula::_dumpRegisters(char *p){
	char cc[200];

	for(int n=0;n<4;n++){
		sprintf(cc,"PCH:%d\tL:%d P:%d V:%d B:%u M:%08x E:%d\n",n,
			_achannels[n]._length,_achannels[n]._period,_achannels[n]._vol,
			_achannels[n]._twr,	_achannels[n]._cadr,_achannels[n]._dma);
		strcat(p,cc);
	}
	return 0;
}

Paula::Paula() : PCMDAC(){
	_samples=NULL;
}

Paula::~Paula(){
#ifdef _DEVELOP
	for(int i=0;i<sizeof(lino)/sizeof(FILE *);i++) if(lino[i]) fclose(lino[i]);
#endif
}

int Paula::Init(void *a,void *b,u32 f){
	u8 *buf;

	_ioreg=(u16 *)b;
	_mem=(u8 *)a;
	_freq=f;
	_clock=(u32)(_freq/124.0f/2.0f);
	_rinc=SL(_clock,12)/44100.0f;
	if(PCMDAC::Open(2,44100))
		return -1;
	if(!(_samples=new u16[88192+4*(2*1000*sizeof(u8))]))
		return -2;
	buf=(u8 *)&_samples[88192];
	for(int i=0;i<sizeof(_achannels)/sizeof(__achannel);i++)
		_achannels[i].init(i,_mem,_ioreg,&buf[1000*2*i],1000*2,_freq);
	Paula::Reset();
	return Create();
}

int Paula::update(int cyc){
	for(int i=0;i<sizeof(_achannels)/sizeof(__achannel);i++){
		if(_achannels[i].step(cyc))
			machine->OnEvent(7+i,3);
	}
	return 0;
}

int Paula::write(u32 a,u16 v){
	u8 r;
	int res=1;

	switch(r=(u8)SR(a,1)){
		case REG_ADKCON:
			for(int i=0;i<sizeof(_achannels)/sizeof(__achannel);i++){
				_achannels[i]._fm=SR(v,4+i);
				_achannels[i]._am=SR(v,i);
			}
		break;
		case REG_DMACON:
			for(int i=0;i<sizeof(_achannels)/sizeof(__achannel);i++){
				_achannels[i].dma( (v & (DMACON_DMAEN|SL(DMACON_AUD0EN,i))) == (DMACON_DMAEN|SL(DMACON_AUD0EN,i)) );
			}
		break;
		case REG_AUD0DAT: case REG_AUD1DAT: case REG_AUD2DAT: case REG_AUD3DAT:{
			int i = (r-REG_AUD0LCH)/8;
			_achannels[i].write(v,1);
		}
		break;
		case REG_AUD0LEN: case REG_AUD1LEN: case REG_AUD2LEN: case REG_AUD3LEN://4 2
			//printf("P %x %x %d %x\n",a,r,SR((r-REG_AUD0LCH),3),r&7);
		case REG_AUD0LCH: case REG_AUD0LCL:
		case REG_AUD1LCH: case REG_AUD1LCL:
		case REG_AUD2LCH: case REG_AUD2LCL:
		case REG_AUD3LCH: case REG_AUD3LCL:
		case REG_AUD0PER: case REG_AUD1PER: case REG_AUD2PER: case REG_AUD3PER://6 3
		case REG_AUD0VOL: case REG_AUD1VOL: case REG_AUD2VOL: case REG_AUD3VOL://8 4
		{
			int i = (r-REG_AUD0LCH) / 8;
			_achannels[i]._changed |= BV(r & 7 );
			//printf("P %x %x %d %x\n",a,r,i,((r-i*8)&15));
			DLOG("PCM W %d %x %x",i,r&7,v);
		}
		break;
		default:
			//printf("paula %x<%x\n",a,v);
			break;
	}
	return res;
}

int Paula::read(u32 a,u16 *v){
	return 0;
}

int Paula::Reset(){
	_cycles=0;
	for(int i=0;i<sizeof(_achannels)/sizeof(__achannel);i++)
		_achannels[i].reset();
	return PCMDAC::Reset();
}

int Paula::Update(){
	u32 n, count;
	int sampleL,sampleR;
	s16 *p;

	if(!(p=(s16 *)_samples))
		return -1;
	n = SPU_CYCLES(_cycles,_freq,44100,__cycles);
	_cycles=__cycles;
//	printf("%u %u %u\n",n,_rinc,_clock);
	for(count=0;n && count<n;){
		sampleL=sampleR=0;
	//	if(!_dmaen) goto A;
		int sample;

		_achannels[0].output(sampleL,_rinc);
		_achannels[1].output(sampleR,_rinc);
		_achannels[2].output(sample,_rinc);
		if((sampleL += sample) > 32767)
			sampleL=32767;
		else if(sampleL < -32768)
			sampleL=-32768;
		_achannels[3].output(sample,_rinc);
		if((sampleR += sample) > 32767)
			sampleR=32767;
		else if(sampleR < -32768)
			sampleR=-32768;
A:
		*p++=(s16)sampleL;
		*p++=(s16)sampleR;
		count++;
	}
	Write(_samples,count);
	return 0;
}

int Paula::Destroy(){
	PCMDAC::Destroy();
	if(_samples)
		delete []_samples;
	_samples=NULL;
	return 0;
}

};
