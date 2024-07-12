#include "popeye.h"
#include "popeyespu.h"

namespace POPEYE{

#ifdef _DEVELOP
static FILE *fp=0;
#endif

PopeyeSpu::PopeyeSpu() : PCMDAC(){
	//_cpu._freq=3579545;
	_samples=0;
}

PopeyeSpu::~PopeyeSpu(){
}

int PopeyeSpu::Init(Popeye &g){
	if(PCMDAC::Open(1,_freq))
		return -1;
	if(!(_samples=new u16[33000]))
		return -2;
	//for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++)
		_pwms.init(0);
	Create();
	Reset();
	return 0;
}

int PopeyeSpu::Reset(){
	_cycles=0;
	//for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++)
		_pwms.reset();
	PCMDAC::Reset();
#ifdef _DEVELOPa
	if(!fp && !(fp=fopen("blktigerspu.txt","wb")))
		printf("errror\n");
	//if(fp)
		//fseek(fp,44,SEEK_SET);
#endif
	return 0;
}

int PopeyeSpu::Update(){
	u32 n, count;
	int sampleL,sampleR;
	u16 *p;

	{
		u32 f=MHZ(4);
		u32 c=__cycles;

		n = c >= _cycles ? c-_cycles : (f-_cycles)+c;
		_cycles=c;
		n = n / (f / _freq);
	}

//	printf("so %u\n",n);
	p=_samples;
	for(count=0;n>0 && count<33000;n--){
		sampleL=sampleR=0;
#ifdef _DEVELOPa
		if(fp){
			s16  cc;
			fread(&cc,1,2,fp);
			sampleL=cc;
			fread(&cc,1,2,fp);
			sampleR=cc;
		}
#endif
	///	for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++){
			sampleL += _pwms.output(6);
			if(sampleL<-32768)
				sampleL=-32768;
			else if(sampleL>32767)
				sampleL=32767;
//		}

		*p++=sampleL;
		count++;
	}
	//printf("write  %u\n",count);
	Write(_samples,count);
	return 0;
}

int PopeyeSpu::Destroy(){
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


#include "ay8910.cpp.inc"

};