#include "cps3dac.h"
#include "cps3m.h"

namespace cps3{


#ifdef _DEVELOP
static FILE *fp=0,*flog=0;
static u32 lino;
#endif

CPS3DAC::CPS3DAC():PCMDAC(){
	_samples=0;
	_data=NULL;
	_pcm_regs=NULL;
}

CPS3DAC::~CPS3DAC(){
}

int CPS3DAC::Init(CPS3M &g){
	_pcm_regs=&g._memory[MI_DUMMY+MI_SPU];
	if(PCMDAC::Open(2,_freq))
		return -1;
	//printf("%s 5\n",__FUNCTION__);
	if(!(_samples=new u16[33000]))
		return -2;
	_data=&g._memory[MI_USER5];
	for(int i=0;i<sizeof(_channels)/sizeof(struct __channel);i++)
		_channels[i].init(i,*this);
	Reset();

	Create();
#ifdef _DEVELOPa
	//if(!(fp=fopen("mepase.wav","rb")))
		//printf("error\n");
	flog=fopen("cp3dac.log","wb");
#endif
	//printf("%s 9\n",__FUNCTION__);
	return 0;
}

void CPS3DAC::write_reg(u32 a,u32 v){
	switch(a&0xfffc){
		case 0x200:
			{
				u16 data=HIWORD(*(u32 *)((u8 *)_pcm_regs+0x200));

#ifdef _DEVELOP
				lino++;
				if(flog){
				//	printf("spu w %x %u\n",v,lino);
				//	fprintf(flog,"spu W %x %x %x %u %u\n",a,data,v,lino,D_CYCLES(_cycles,MHZ(25)));
				}
#endif
				for(int i=0;i<sizeof(_channels)/sizeof(struct __channel);i++,data >>= 1)
					_channels[i].enable(data & 1);
			}
		break;
		default:
			if((u16)a < 0x202)
				_channels[SR((u16)a,5)].write_reg(SR(a&0x1f,2),*(u32 *)((u8 *)_pcm_regs+(a & 0x1fc)));
		break;
	}
}

int CPS3DAC::Reset(){
	PCMDAC::Reset();
	for(int i=0;i<sizeof(_channels)/sizeof(__channel);i++)
		_channels[i].reset();
	_cycles=__cycles;
#ifdef _DEVELOP
	if(fp)
		fseek(fp,44,SEEK_SET);
	lino=0;
#endif
	return 0;
}

int CPS3DAC::Update(){
	u32 n,count,f_;
	int sampleL,sampleR;
	s16 *p;

	p=(s16 *)_samples;
	n = SPU_CYCLES(_cycles,MHZ(25),_freq,__cycles);
	_cycles=__cycles;

	for(count=0;n>0;n--){
		sampleL=sampleR=0;

		for(int i=0;i<sizeof(_channels)/sizeof(struct __channel);i++){
			if(!_channels[i]._enabled)
				continue;
			if(!_channels[i]._init)
				_channels[i].start();
			if(!_channels[i]._play)
				continue;
			int s=((s8 *)_data)[SR(_channels[i]._pos,12)+_channels[i]._seek];
			sampleL += SR(s*_channels[i]._volL,8);
			if(sampleL>32767)
				sampleL=32767;
			else if(sampleL<-32768)
				sampleL=-32768;

			sampleR += SR(s*_channels[i]._volR,8);
			if(sampleR>32767)
				sampleR=32767;
			else if(sampleR<-32768)
				sampleR=-32768;

			_channels[i]._pos += _channels[i]._step;
#ifdef _DEVELOP
			if(!count && flog)
				fprintf(flog,"%d N:%u %x %u %u %x L:%x vol:%d\n",i,n,_channels[i]._seek,SR(_channels[i]._pos,12),_channels[i]._step,_channels[i]._end,_channels[i]._loop_start,_channels[i]._volR);
#endif
			if((SR(_channels[i]._pos,12)+_channels[i]._seek) >= _channels[i]._end){
				if(_channels[i]._loop){
					_channels[i]._seek=_channels[i]._loop_start;
					_channels[i]._pos=0;
				}
				else{
					_channels[i]._play=0;
				}
				//_channels[i]._loop=0;
			}
		}

#ifdef _DEVELOP
		if(fp){
			s16 s;

			fread(&s,1,2,fp);
			sampleL=s;
			fread(&s,1,2,fp);
			sampleR=s;
		}
#endif
	//if(lino>28000) sampleL=sampleR=0;
		*p++ = (s16)sampleL;
		*p++ = (s16)sampleR;
		count++;
	}
	return Write(_samples,count);
}

int CPS3DAC::Destroy(){
	PCMDAC::Destroy();
	if(_samples)
		delete []_samples;
	_samples=NULL;
#ifdef _DEVELOP
	if(fp)
		fclose(fp);
	fp=0;
#endif
	return 0;
}

void CPS3DAC::__channel::reset(){
	memset(_regs,0,sizeof(_regs));
	_init=0;
	_play=0;
	_pos=0;
	_enabled=0;
	_start=_end=_seek=_loop_start=0;
}

void CPS3DAC::__channel::enable(int v){
	if((v&1) == _enabled)
		return;
	if(_enabled)
		_pos=0;
	_enabled=v;
	_play=0;
	_init=0;
}

void CPS3DAC::__channel::write_reg(u8 i,u32 v){
	_regs[i]=SL((u16)v,16)|SR(v,16);
	_init=0;
}

void CPS3DAC::__channel::start(){
	u32 start;

	if((start = _regs[1]) > 0x400000)
		start -= 0x400000;
	else
		_start=start=0;
	if((_end = _regs[5]))
		_end -= 0x400000;
	if((_loop=SR(_regs[2],16) )){
		_loop_start=(_regs[4]&0xffff0000)|HIWORD(_regs[3]);
		if(_loop_start > 0x400000)
			_loop_start -= 0x400000;
		else
			_loop=0;
	}
	_volL=(s16)LOWORD(_regs[7]);
	_volR=(s16)HIWORD(_regs[7]);;
	_step=LOWORD(_regs[3]);
	if(start != _start){
#ifdef _DEVELOP
		if(flog)
			fprintf(flog,"voice restart %d %x %u\n",_idx,_start,lino);
#endif
		_seek=_start=start;
		_pos=0;
	}
	_init=1;
	_play=_start && _step ? 1 : 0;
}

int CPS3DAC::__channel::init(int n,CPS3DAC &){
	_idx=n;
	reset();
	return 0;
}

};