#include "ps1m.h"

namespace ps1{

#define P1SPU_REGA(a) ((u8*)_spu_regs + RMAP_IO(a))
#define PS1SPU_REG16(a) *((u16 *)P1SPU_REGA(a))
#define PS1SPU_STATUS	PS1SPU_REG16(0x1f801dae)

static FILE *fp=0;

typedef struct __adpcm_decoder{
	union{
		struct{
			unsigned int _shift:4;
			unsigned int _filter:4;
			unsigned int _flags:8;
		};
		u16 _header;
	};

	s16 _fb[2],_samples[28];
	u32 _rstep,_rpos;

	virtual int _decode(u8 *m){
		int f[5][2] = {{0,  0},{60,  0},{115, -52 },{   98, -55 },{  122, -60 } };

		_header=*(u16 *)m;
		m+=2;
		if(_filter>4) _filter=0;
		for(int i=0;i<(sizeof(_samples)/sizeof(s16))-1;){
			int d= *m++;
			int s = (int)(s16)SL(d & 0xf,12);
			int fa=SR(s,_shift);
			fa=fa + ((_fb[0] * f[_filter][0])>>6) + ((_fb[1] * f[_filter][1])>>6);
			if(fa>32767)
				fa=32767;
			else if(fa<-32768)
				fa=-32768;
			_samples[i++]=fa;
			_fb[1]=_fb[0];
			_fb[0]=fa;
			s=(int)(s16)SL(d&0xf0,8);
			fa=SR(s,_shift);
			fa=fa + ((_fb[0] * f[_filter][0])>>6) + ((_fb[1] * f[_filter][1])>>6);
			if(fa>32767)
				fa=32767;
			else if(fa<-32768)
				fa=-32768;
			_samples[i++]=fa;
			_fb[1]=_fb[0];
			_fb[0]=fa;
		}
		//printf("%u %u\n",_rpos>>12,_rstep);EnterDebugMode();
		_rpos &= 4095;
		return 0;
	};

	virtual int _reset(){
		_rpos=-1;
		_fb[0]=_fb[1]=0;
		_rstep=4096;
		memset(_samples,0,sizeof(_samples));
		return 0;
	};

	virtual int _output(int &sample){
		u32 p;

		if((p=SR(_rpos,12)) >= (sizeof(_samples)/sizeof(s16))-1)
			return -1;
		sample=(int)(s16)_samples[p];
		_rpos += _rstep;
		return 0;
	};
} ADPCMDECODER;

struct __voice{
	enum : u8{
		EG_ATTACK=1,
		EG_DECAY,
		EG_SUSTAIN,
		EG_RELEASE
	};

	union{
		struct{
			unsigned int _enabled:1;
			unsigned int _pitch:1;
			unsigned int _reverb:1;
			unsigned int _dummy:13;
			unsigned int _idx:5;
			unsigned int _changed:1;
			unsigned int _full:1;
		};
		u16 _control16;
		u64 _control;
	};

	struct{
		struct{
			union{
				struct{
					unsigned int _step:2;
					unsigned int _shift:5;
					unsigned int _dummy:5;
					unsigned int _phase:1;
					unsigned int _dir:1;
					unsigned int _mode:1;
				};
				u16 _control;
			};
			int _reset(){
				_control=0;
				return 0;
			};
		} _sweep[2];

		union{
			struct{
				unsigned int _enabled:1;
				unsigned int _state:3;
				unsigned int _level:14;
				unsigned int _changed:1;
			};
			u32 _control;
		};
		union{
			struct{
				unsigned int _mode:1;
				unsigned int _dir:1;
				unsigned int _shift:5;
				unsigned int _step:2;
				unsigned int _level:15;
			};
			u32 _control;
		} _states[4];

		int _reset(){
			_control=0;
			memset(_states,0,sizeof(_states));
			_states[EG_SUSTAIN-1]._level=32767;
			_sweep[0]._reset();
			_sweep[1]._reset();
			return 0;
		};

		int _clock(int){
			if(_state == EG_RELEASE)
			_state=0;
			return 0;
		};

	} _eg;

	struct __pcm : ADPCMDECODER{
		union{
			struct{
				unsigned int _noise:1;
			};
			u16 _control;
		};
	} _pcm;

	u16 _rate;
	u32 _pos,_loop;
	u8 *_mem;
	s16 _vol[2];

	__voice(){_control=0;};

	int _init(int n,void *m){
		_idx=n;
		_mem=(u8*)m;
		_eg._reset();
		return 0;
	}

	int _reset(){
		_control16=0;
		_eg._reset();
		_pcm._reset();
		_loop=0;
		_pos=(u32)-1;
		_rate=0;
		_vol[0]=_vol[1]=0;
		return 0;
	};

	int _write(u16 a,u16 v){
	//	printf("voice w %d %x %x\n",_idx,a,v);
		switch(a){
			case 0:
			case 2:
				_set_vol(a/2,v);
			break;
			case 4:
				_rate = v > 16384 ? 16384 : v;
				_rate =_rate*10;
				_pcm._rstep=_rate*4096/44100.0f;
				_pcm._rpos=-1;
		//		printf("r %d %u %u\n",_idx,_rate,_step);
			break;
			default:
				printf("v %d %x  %x\n",_idx,a,v);
			break;
			case 6:
				_pos = SL(v,3);
				_pcm._rpos=-1;
				//printf("voice %d  pos %x\n",_idx,_pos);
			break;
			case 8:
				//printf("v adsr %d 8  %x\n",_idx,v);
			break;
			case 0xa:
				//printf("v adsr %d a  %x\n",_idx,v);
			break;
			case 0xe:
			break;
		}
		return 0;
	};

	int _set_vol(int ch,u16 v){
		if(v&0x8000){
			_eg._enabled=0;
			_eg._sweep[ch]._control=v;
		}
		else{
			_eg._enabled=1;
			_vol[ch]=SL(v,1);
		}
	//	printf("vol %d %d %x\n",_idx,ch,v<<1);
		//_enabled=_vol != 0;
		return 0;
	}

	int _key(int v){
		_changed=1;
		_eg._state=v ? EG_ATTACK : EG_RELEASE;
		_enabled=v ? 1:0;
		return 0;
	}

	int _mode(int v){
		_pcm._noise=v;
		return 0;
	}

	int _output(int a,s16 *o){
		int sampleL,sampleR,res;

		sampleL=sampleR=0;
		res=-1;
		if(_changed){
			_changed=0;
		}
		if(!_enabled || _pos==-1)
			goto Z;
		res=0;
		_eg._clock(a);

		if(_pcm._output(sampleL))
			_full=0;
		if(!_full){
		//	printf("adpcm %x %u\t",_mem[_pos],_pos);
			_pcm._decode(&_mem[_pos]);
			if((_pcm._flags & 4))
				_loop=_pos;
			if(!(_pcm._flags&1)){
				_pos += 16;
			}
			else{
				if(!(_pcm._flags&2)){
					_enabled=0;
					//printf("end %d\n",_idx);
					if(!_idx && fp){
						fclose(fp);
						fp=0;
						return 1;
					}
				}
				_pos=_loop;
			}
			_full=1;
			if(_pos>=0x80000)
				printf("azzo\n");
		}
	//	if(!_idx&&!fp) fp=fopen("psx.pcm","wb");
		sampleR=sampleL;
Z:
		o[0]=SR(sampleL*_vol[0],15);
		o[1]=SR(sampleR*_vol[1],15);
		//printf("v %d %d %d adsr:%d\n",_idx,_pos,_rate,_eg._state);
		return res;
	};

	friend class PS1SPU;
} _voices[24];

struct __xa_decoder{
	struct __ch: ADPCMDECODER{
		union{
			struct{
			};
			u32 _control;
		};
	} _ch[2];

	union{
		struct{
			unsigned int _enabled:1;
		};
		u32 _control;
	};

	int _init(void *m){
		_control=0;
		_samples=(s16 *)m;
		return 0;
	};

	int _stop(){
		_twc=0;
		_wc=0;
		return 0;
	};

	int _reset(){
		for(int  i=0;i<sizeof(_ch)/sizeof(__ch);i++)
			_ch[i]._reset();
		_twc=0;
		_wc=0;
		_rpos=0;
		_rstep=4096;
		return 0;
	};

	int _output(int &l,int &r){
		if(!_twc) return -1;

		u32 p = SR(_rpos,12);
		if((l+=_samples[p]) > 32767)
			l=32767;
		else if(l<-32768)
			l=-32768;
		if((r+=_samples[p]) > 32767)
			r=32767;
		else if(r<-32768)
			r=-32768;
		_twc--;
		if((_rpos+=_rstep) >= 44100*4096)
			_rpos &= 4095;
		return 0;
	}

	int _decode_sector(u8 *data){
		u8 c[30],cc[30],*h;
		int i,n,j,r;

		_subq=*(u32 *)data;
		switch(_freq){
			case 0:
				r=37800;
			break;
			case 1:
				r=18900;
			break;
			default:
				printf("xa invalidfreq\n");
			break;
		}
		r=r*(4096.0f/44100.0f);
		data += 8;
		for (j=0; j < 18; j++) {
			u8 *h=data+j*128;
			u8 *p=h+16;
			for (i=0; i < 4; i++) {
				c[0]=h[i*2];
				c[1]=0;
				cc[0]=h[i*2+1];
				cc[1]=0;
				n=2;
				u8 *p0=p+i;
				for (int k=0; k < 7; k++,p0 += 16) {
					c[n]=p0[0]&0xf;
					cc[n]=SR(p0[0],4);
					c[n]|=SL(p0[4]&0xf,4);
					cc[n++]|=p0[4]&0xf0;

					c[n]=p0[8]&0xf;
					cc[n]=SR(p0[8],4);
					c[n]|=SL(p0[12]&0xf,4);
					cc[n++]|=p0[12]&0xf0;
				}
				_ch[0]._decode(c);
				_ch[1]._decode(cc);
				for(int k=0;SR(k,12)<28;k+=r){
					_samples[_wc++]=_ch[0]._samples[SR(k,12)];
					//_samples[_wc++]=_ch[1]._samples[ii];
					_twc++;
					if(_wc >=44100) _wc-= 44100;
				}
				/*FILE *fp=fopen("xad.bin","ab");
				if(fp){
					s16 s[2];

					for(int ii=0;ii<28;ii++){
						s[0]=_ch[0]._samples[ii];
						s[1]=_ch[1]._samples[ii];
						fwrite(s,1,sizeof(s),fp);
					}
					fclose(fp);
				}*/
			}
		}
		return 0;
	};
protected:
	s16 *_samples;
	u32 _rpos,_rstep,_wc,_twc;

	union{
		struct{
			unsigned int _fn:8;
			unsigned int _cn:8;
			unsigned int _sm:8;
			unsigned int _stereo:2;
			unsigned int _freq:2;
			unsigned int _bits:2;
			unsigned int _emp:1;
		};
		u32 _subq;
	};

	friend class PS1SPU;
} _xa_player;

struct __cdda_player{
	union{
		struct{
		};
		u32 _control;
	};
	__cdda_player(){
		_fp=0;
	};

	int _init(){
		_fp=0;
		_control=0;
		return 0;
	};

	int _play(char *fn,u32 pos){
		_stop();
		if(!(_fp=fopen(fn,"rb")))
			return -1;
		//fseek(_fp,pos,SEEK_SET);
		_pos=pos;
		return 0;
	};

	int _stop(){
		if(_fp)
			fclose(_fp);
		_fp=NULL;
		return 0;
	};

	int _reset(){
		_stop();
		_pos=0;
		return 0;
	};

	int _output(int &sampleL,int &sampleR){
		s16 s[2];

		if(!_fp)
			return -1;
		if(!fread(s,2,sizeof(s16),_fp))
			return -2;
		if((sampleL+= s[0]) >32767)
			sampleL=32767;
		else if(sampleL<-32768)
			sampleL=-32768;
		if((sampleR+= s[1]) >32767)
			sampleR=32767;
		else if(sampleR<-32768)
			sampleR=-32768;
		return 0;
	};
protected:
	FILE *_fp;
	u32 _pos;
} _cdda_player;

struct{
	union{
		struct{
			unsigned int _enabled:1;
			unsigned int _changed:1;
		};
		u32 _control;
	};

	int _output(int &l,int &r){
		if(!_enabled) return 1;
		if(_changed){
			_changed=0;
		}
		return 0;
	};

	int _reset(){
		_control=0;
		return 0;
	};

	int _init(void *m){
		return 0;
	};

	int _write(u8 a,u16 v){
		switch(a){
			case 0xa2:
				_start=SL(v,2);
				_changed=1;
			break;
		}
		return 0;
	};
protected:
	s16 *_samples;
	u32 _start;
} _reverber;

PS1SPU::PS1SPU() : PCMDAC(){
	_samples=NULL;
	_track._fp=0;

	/*FILE *fp=fopen("xa.bin","rb");
	if(fp){
		char b[3000];
		while(!feof(fp)){
			int i=fread(b,1,0x908,fp);
			_xa_decode_sector(0,b,0);
			printf("%u ",i);
		}
		fclose(fp);
	}*/
}

PS1SPU::~PS1SPU(){

}

int PS1SPU::Init(PS1M &g,void *r,void *m){
	_spu_regs=r;
	_spu_mem=m;
	if(PCMDAC::Open(2,44100))
		return -1;
	//printf("%s 5\n",__FUNCTION__);
	if(!(_samples=new u16[176400*2]))
		return -2;

	_reverbb=&_samples[44100];
	_xa_player._init(&_reverbb[44100]);
	_cdda_player._init();
	ps1::_reverber._init(m);
	for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++)
		_voices[i]._init(i,m);
	Reset();
	Create();
	return 0;
}

void PS1SPU::write_reg(u32 a,u32 v){
#ifdef _DEVELOP
	DLOG("SPU %x %x",v,a);
#endif
	switch((u16)a){
		case 0x1d88:
		case 0x1d8a:
			v=*(u32 *)P1SPU_REGA(0x1f801d88);
		//	printf("key on %x\t",v);
		//	EnterDebugMode();
			for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++,v>>=1){
				_voices[i]._key(v&1);
			//		printf("%d ",_voices[i]._idx);
			}
			//printf("\n");
			break;
		case 0x1d8c:
		case 0x1d8e:
			v=*(u32 *)P1SPU_REGA(0x1f801d8c);
			//printf("key off %x\n",v);
			for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++,v>>=1)
				_voices[i]._key(!(v&1));
			break;
		case 0x1d90:
		case 0x1d92:
			v=*(u32 *)P1SPU_REGA(0x1f801d90);
			for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++,v>>=1)
				_voices[i]._pitch=v;
			break;
		case 0x1dae:
		break;
		case 0x1d94:
		case 0x1d96:
			v=*(u32 *)P1SPU_REGA(0x1f801d94);
			for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++,v>>=1)
				_voices[i]._mode(v&1);
			break;
		case 0x1d98:
			for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++,v>>=1)
				_voices[i]._reverb=v;
			break;
		case 0x1da2:
			ps1::_reverber._write(0xa2,v);
		break;
		case 0x1da8://fifo
			*(u32 *)((u8 *)_spu_mem + _mem_adr_start)=v;
			_mem_adr_start+=4;//fixmee
		//	printf("ramtra %x %x %x\n",_rtm,_mem_adr_start,v);
		break;
		case 0x1daa:
			_control16=v;
			ps1::_reverber._enabled=SR(v,7);
			switch(_rtm){
				case 0:
				break;
				default:
					//printf("ramtra %x %x %x\n",_rtm,PS1SPU_REG16(0x1f801da6)<<3,PS1SPU_REG16(0x1f801dac));
					_mem_adr_start=SL(PS1SPU_REG16(0x1f801da6),3);
				break;
			}
			PS1SPU_STATUS=(PS1SPU_STATUS & ~0x1f)|(v&0x2f);
		//	printf("%x %x\n",a,v);
			//EnterDebugMode();
			break;
		default:
			if((u16)a < 0x1d80)
				_voices[SR(a&0x1ff,4)]._write(a&15,(u16)v);
	}
}

int PS1SPU::Reset(){
	PCMDAC::Reset();

	for(int i=0;i<sizeof(_voices)/sizeof(__voice);i++)
		_voices[i]._reset();
	_control=0;
	_mem_adr_start=0;
	_cycles=__cycles;
	_cdda_player._reset();
	_xa_player._reset();
	return 0;
}

int PS1SPU::Update(){
	int sampleL,sampleR;
	u32 n, count;
	s16 *p;

	{
		u32 f=MHZ(33.868);
		//f=_cpu.getFrequency();
		u32 c=__cycles;
		//c=_cpu.getTicks();

		n = SPU_CYCLES(_cycles,f,44100,c);
		//printf("sample %u %u %u\n",n,__cycles,_cycles);
		_cycles=c;
	}
	p=(s16 *)_samples;
	if(!p)
		return -1;
	for(count=0;n>0 && count < 22050;n--){
		sampleL=sampleR=0;

		for(int i=0;i<sizeof(_voices)/sizeof(__voice) && 0;i++){
			s16 s[2];

			if(_voices[i]._output(0,s))
				continue;
			if((sampleL += s[0]) > 32767)
				sampleL=32767;
			else if(sampleL<-32768)
				sampleL=-32768;
			if((sampleR += s[1]) >32767)
				sampleR=32767;
			else if(sampleR<-32768)
				sampleR=-32768;
		}
		_cdda_player._output(sampleL,sampleR);
		_xa_player._output(sampleL,sampleR);
		//sampleL=sampleR=0;
		*p++=(s16)sampleL;
		*p++=(s16)sampleR;
		count++;
	}
//	printf("%u\n",_xa_player._twc);
	Write(_samples,count);
	return 0;
}

int PS1SPU::Destroy(){
	PCMDAC::Destroy();
	if(_samples)
		delete []_samples;
	_samples=NULL;
	_closeAudioFile();
	return 0;
}

int PS1SPU::_playAudioFile(char *fn,u32 pos){
	_xa_player._reset();
	return _cdda_player._play(fn,pos);
}

int PS1SPU::_closeAudioFile(){
	_cdda_player._stop();
	return 0;
}

int PS1SPU::_xa_decode_sector(void *,void *data,u32){
	return _xa_player._decode_sector((u8*)data);
}

};