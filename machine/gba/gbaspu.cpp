#include "gbaspu.h"
#include "gbadev.h"

namespace gba{

gbaspu::gbaspu(): PCMDAC(){
	_samples=NULL;
}

gbaspu::~gbaspu(){

}

int gbaspu::Init(void *a,void  *b){
	_spu_regs=b;
	_spu_mem=a;
	if(PCMDAC::Open(2,44100))
		return -1;
	//printf("%s 5\n",__FUNCTION__);
	if(!(_samples=new u16[176400*4]))
		return -2;
	cpu->Query(ICORE_QUERY_CPU_FREQ,&_freq);

	for(int i=0;i<sizeof(_pcms)/sizeof(__pcm);i++){
		_pcms[i].Init(i,a,b,_freq);
		_pcms[i]._samples=(u8 *)&_samples[176400*i];
		_pcms[i]._size=176400;
	}
	for(int i=0;i<sizeof(_pwms)/sizeof(__pwm *);i++)
		_pwms[i]->Init(i,a,b,_freq);
	Reset();
	Create();
	return 0;
}

int gbaspu::Reset(){
	PCMDAC::Reset();
	_cycles=__cycles;
	for(int i=0;i<sizeof(_pcms)/sizeof(__pcm);i++)
		_pcms[i].reset();
	for(int i=0;i<sizeof(_pwms)/sizeof(__pwm *);i++)
		_pwms[i]->reset();

	((u8*)_spu_regs)[REG_NR10] = 0x80;
	((u8*)_spu_regs)[REG_NR11] = 0xBF;
	((u8*)_spu_regs)[REG_NR12] = 0xF3;
	((u8*)_spu_regs)[REG_NR14] = 0xBF;
	((u8*)_spu_regs)[REG_NR21] = 0x3F;
	((u8*)_spu_regs)[REG_NR22] = 0xF0;
	((u8*)_spu_regs)[REG_NR24] = 0xBF;
	((u8*)_spu_regs)[REG_NR30] = 0xFF;
	((u8*)_spu_regs)[REG_NR31] = 0xFF;
	((u8*)_spu_regs)[REG_NR32] = 0x1F;
	((u8*)_spu_regs)[REG_NR33] = 0xBF;
	((u8*)_spu_regs)[REG_NR41] = 0xFF;
	((u8*)_spu_regs)[REG_NR42] = 0xF0;
	((u8*)_spu_regs)[REG_NR43] = 0x00;
	((u8*)_spu_regs)[REG_NR44] = 0xBF;
	((u8*)_spu_regs)[REG_NR50] = 0x77;
	((u8*)_spu_regs)[REG_NR51] = 0xFF;
	((u8*)_spu_regs)[REG_NR52] = 0xF0;

	return 0;
}

int gbaspu::write(u32 a,u32 v){
	switch((u8)a){
		case REG_NR10:
		case REG_NR11:
		case REG_NR12:
		case REG_NR13:
		case REG_NR21:
		case REG_NR22:
		case REG_NR23:
		case REG_NR24:
		case REG_NR30:
		case REG_NR31:
		case REG_NR32:
		case REG_NR33:
		case REG_NR34:
		case REG_NR41:
		case REG_NR42:
		case REG_NR43:
		case REG_NR44:
			//printf("spu w %d %x %x\n",SR(a&0x1f,3),a,vì);
			return _pwms[SR(a&0x1f,3)]->write(a,v);
		break;
		case REG_NR50:
			for(int i = 0;i<sizeof(_pwms) / sizeof(__pwm *);i++){
				_pwms[i]->_le = v & BV(12+i);
				_pwms[i]->_re = v & BV(8+i);
			}
		break;
		case REG_SOUNDCNT_H:
			_pcms[0]._enabled=(SR(v,8)&3)?1:0;
			_pcms[0]._timer=SR(v,10);
			_pcms[0]._changed=1;
			_pcms[1]._timer=SR(v,14);
			_pcms[1]._enabled=(SR(v,12)&3)?1:0;
			_pcms[1]._changed=1;
			DLOG("SPU REG_SOUNDCNT_H %u %u %u %u",_pcms[0]._enabled,_pcms[0]._timer,_pcms[1]._enabled,_pcms[1]._timer);
		break;
	}
	return 1;
}

int gbaspu::Update(u32 flags){
	u32 n, count;
	int sampleL,sampleR;
	s16 *p;

	if(!(p = (s16 *)_samples))
		return -1;
	{
		u32 f=_freq;
		//f=_cpu.getFrequency();
		u32 c=__cycles;
		//c=_cpu.getTicks();

		n = SPU_CYCLES(_cycles,f,44100,c);
		_cycles=c;
		//printf("sample %u\n",n);
	}

	for(count=0;n>0 && count <44100;n--){
		sampleL=sampleR=0;
		for(int i=0;i<sizeof(_pcms)/sizeof(__pcm);i++)
			_pcms[i].output(1,sampleL,sampleR);

		for(int i=0;i<sizeof(_pwms)/sizeof(__pwm *);i++)
			_pwms[i]->output(1,sampleL,sampleR);

		*p++=(s16)sampleL;
		*p++=(s16)sampleR;
		count++;
	}
//	printf("%u\n",_xa_player._twc);
	Write(_samples,count);
	return 0;
}

int gbaspu::Destroy(){
	PCMDAC::Destroy();
	if(_samples)
		delete []_samples;
	_samples=NULL;
	return 0;
}

int gbaspu::__pwm::write(u32 a,u16 v){
	_changed=1;
	return 1;
}

int gbaspu::__pwm::Init(int n,void *a,void *b,u32 c){
	_idx=n;
	_ioreg=(u8 *)b;
	_mem=(u8 *)a;
	_pllHz=c;
	return 0;
}

int gbaspu::__pwm::reset(){
	_status=0;
	_pos=_fpos=_epos=_spos=0;
	return 0;
}

int gbaspu::__pwm::output(int cyc,int &sampleL,int &sampleR){
	if(_changed){
		switch(_idx){
			case 0:
				_enabled=SR(IOREG(REG_NR13),15);
				_freq=IOREG(REG_NR13);
				_duty=SR(IOREG(REG_NR11),6) & 3;
			break;
			case 1:
				_enabled=SR(IOREG(REG_NR23),15);
				_freq=IOREG(REG_NR23);
				_duty=SR(IOREG(REG_NR21),6) & 3;
			break;
		}
		//printf("pwm %d %x",_idx,_freq);
		_freq=SL(2048-(_freq&0x7ff),7);
		u16 percDuty[]={512,1024,2048,3072};
		_duty= SR(_freq * percDuty[_duty],12);
		//printf(" %u %u\n",_freq,_duty);
		_changed=0;
	}
	if(!_enabled) return 0;

	return 0;
}

gbaspu::__noiseg::__noiseg(): __pwm(){
	_lfsr7=NULL;
	_lfsr15=NULL;
}

gbaspu::__noiseg::~__noiseg(){
	if(_lfsr7)
		delete[]_lfsr7;
	_lfsr7=NULL;
	_lfsr15=NULL;
}

int gbaspu::__noiseg::Init(int n,void *a,void *b,u32 c){
	u16 noise,bit,value,valueShift;
	int i,i1;

	__pwm::Init(n,a,b,c);
	if(!(_lfsr7 = new u8[20+2190*2]))
		return -2;
	_lfsr15 = (u16 *)&_lfsr7[20];

	noise = value = 0x7F;
	i = i1 = 0;
	valueShift = 6;
	do{
		if(!(i++ % 7))
			_lfsr7[i1++] = (u8)noise;
		bit = (unsigned short)(noise & 1);
		bit = (unsigned short)((bit ^ ((noise >>= 1) & 1)) << valueShift);
		noise |= bit;
	}while(noise != value);

	noise = value = 0x7FFF;
	i = i1 = 0;
	valueShift = 14;
	do{
		if(!(i++ % 15))
			_lfsr15[i1++] = noise;
		bit = (unsigned short)(noise & 1);
		bit = (unsigned short)((bit ^ ((noise >>= 1) & 1)) << valueShift);
		noise |= bit;
	}while(noise != value);
	return 0;
}

int gbaspu::__noiseg::reset(){
	__pwm::reset();
	return 0;
}

int gbaspu::__noiseg::output(int,int &sampleL,int &sampleR){
	if(_changed){
		_enabled=SR(IOREG(REG_NR43),15);

		_freq=0;
		if(_enabled){
			u32 div,freqtab[] = {8192,4096,2048, 1365,1024,819,682,585};

			if((div = (u32)(IOREG(REG_NR43) >> 4)) > 13)
				div = 13;
			div += 13;
			div= (524288 * freqtab[IOREG(REG_NR43) & 7]) >> div;
			if(div)	_freq = _pllHz / div;
		//	printf("pwm %d %u %x\n",_idx,_freq,IOREG(REG_NR43));
		}
		_changed=0;
	}
	if(!_enabled)
		return 0;
	return 0;

}

int gbaspu::__waveg::Init(int n,void *a,void *b,u32 c){
	__pwm::Init(n,a,b,c);
	_wave=(u8 *)_ioreg +0x90;
	return 0;
}

int gbaspu::__waveg::reset(){
	__pwm::reset();
	return 0;
}

int gbaspu::__waveg::output(int,int &sampleL,int &sampleR){
	if(_changed){
		_enabled=SR(IOREG(REG_NR33),15);
//		printf("pwm %d %x\n",_idx,IOREG(REG_NR33));
		_changed=0;
	}
	if(!_enabled) return 0;

	return 0;
}

int gbaspu::__pcm::Init(int n,void *a,void *b,u32){
	_idx=n;
	_ioreg=(u8 *)b;
	return 0;
}

int gbaspu::__pcm::write(u16 v){
	if(!_samples)
		return -1;
	_samples[_cw++]=v>>_vol;
	if(_cw >= _size)
		_cw=0;
	return 0;
}

int gbaspu::__pcm::clock(int cyc){
	return _clock++ != 0;
}

int gbaspu::__pcm::reset(){
	_status=0;
	_cw=0;
	_cr=0;
	_step=0;
	return 0;
}

int gbaspu::__pcm::output(int,int &sampleL,int &sampleR){
	if(_changed){
		u16 v = IOREG(REG_SOUNDCNT_H);
		_vol=SR(v,2+_idx);
		_vol=1-_vol;
		_re=SR(v,8+(_idx*4));
		_le=SR(v,9+(_idx*4));
		_step = _freq*4096.0f / 44100.0f;
		_clock=0;
		_changed=0;
	}
	if(_le){
		sampleL += SL((s8)_samples[(_cr>>12)],8);
		if(sampleL > 32767)
			sampleL = 32767;
		else if(sampleL < -32768)
			sampleL=-32768;
	}
	if(_re){
		sampleR += SL((s8)_samples[(_cr>>12)],8);
		if(sampleR > 32767)
			sampleR = 32767;
		else if(sampleR < -32768)
			sampleR=-32768;
	}
	_cr += _step;
	if(SR(_cr,12) >= _size)
		_cr = _cr & 4095;
	return 0;
}

};