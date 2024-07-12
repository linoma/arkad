#include "1943.h"
#include "1943spu.h"

namespace M1943{

#ifdef _DEVELOP
static FILE *fp=0;
#endif


M1943Spu::M1943Spu() : PCMDAC(){
	//_cpu._freq=3579545;
	_samples=0;
	_cycles=0;
	_ipc=0;
}

M1943Spu::~M1943Spu(){
}

int M1943Spu::Init(M1943 &g){
	if(PCMDAC::Open(1,_freq))
		return -1;
	if(!(_samples=new u16[33000]))
		return -2;
	_cpu.Query(ICORE_QUERY_SET_MACHINE,(IObject *)(ICore *)&g);
	_ipc = &g._memory[MB(15)];
	_cpu.Init(_ipc);
	_cpu._spu=this;

	for(int  i=0;i<4;i++)
		_cpu.SetMemIO_cb(0xe000+i,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);

	_cpu.SetMemIO_cb(0xc800,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);
	_cpu.SetMemIO_cb(0xd800,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);
	_cpu.AddTimerObj((M1943Spu *)this,6508);//one tick 65,08

	_ym[0].init();
	_ym[1].init();

	__ym2203::initialize();

	Create();
	Reset();
	return 0;
}

int M1943Spu::Reset(){
	u32 i;

	_cpu.Reset();
	i=1;
	_cpu.Query(ICORE_QUERY_SET_STATUS,&i);
	_ym[0].reset();
	_ym[1].reset();
	if(_ipc)
		_ipc[0xc800]=0x8|0x4|0x10;
	_cycles=_cpu.getTicks();
	PCMDAC::Reset();
#ifdef _DEVELOPa
	if(!fp && !(fp=fopen("blktigerspu.txt","wb")))
		printf("errror\n");
	//if(fp)
		//fseek(fp,44,SEEK_SET);
#endif
	return 0;
}

int M1943Spu::Update(){
	u32 n, count;
	int sampleL,sampleR;
	u16 *p;

	{
		u32 f=_cpu.getFrequency();
		u32 c=_cpu.getTicks();

		n = c >= _cycles ? c-_cycles : (f-_cycles)+c;
		_cycles=c;
		n = n / (f / _freq);
	}

	//printf("so %u\n",n);
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
		if(_ym[0]._enabled)
			sampleL += _ym[0].update(2);
		if(_ym[1]._enabled)
			sampleL += _ym[1].update(2);

		if(sampleL<-32768)
			sampleL=-32768;
		else if(sampleL>32767)
			sampleL=32767;
		*p++=sampleL;
		count++;
	}
	//printf("write  %u\n",count);
	Write(_samples,count);
	return 0;
}

int M1943Spu::Destroy(){
	PCMDAC::Destroy();
	_cpu.Destroy();
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

int M1943Spu::Load(void *m){
	void *mem;

	if(m){
		mem=0;
		if(_cpu.Query(ICORE_QUERY_CPU_MEMORY,&mem))
			return -1;
		memcpy(mem,m,0x8000);
	}
	return 0;
}

int M1943Spu::Exec(u32 status){
	int ret=_cpu.Exec(status);
	return ret;
}

s32 M1943Spu::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 0:{
			//*((u8 *)port)=_ipc[0xc800]|0x8;
			_ipc[0xc800]=*((u8 *)data);
			//*((u8 *)data)|=0x8;
			//return 1;
		}
		break;
	}
	return 0;
}

int M1943Spu::Run(u8 *_mem,int cyc,void *obj){
	//EnterDebugMode();
	int i = _ym[0].step(cyc);
	i |= _ym[1].step(cyc);
	if(i){
		//EnterDebugMode();
		//_mem[0xe000]|=1;
		return 1;
	}
	return 0;
}

s32 M1943Spu::fn_mem_w(u32 a,pvoid,pvoid data,u32){
	//printf("%s %x %x\n",__FUNCTION__,a,*((u8 *)data));
	switch(a){
		case 0xc800:
			if(*((u8 *)data)){
				u32  i;

				_cpu.Restart();
				_ym[0].reset();
				_ym[1].reset();
				i=0;
				_cpu.Query(ICORE_QUERY_SET_STATUS,&i);
			}
		break;
		case 0xe000:
		case 0xe002:
			_ym[SR(a&3,1)]._reg_idx=*((u8 *)data);
		break;
		case 0xe001:
		case 0xe003:
			_ym[SR(a&3,1)].write(_ym[SR(a&3,1)]._reg_idx,*((u8 *)data));
		break;
		case 0xd800:
			printf("spu %x\n",*((u8 *)data));
		break;
	}
	return 0;
}

s32 M1943Spu::fn_mem_r(u32 a,pvoid,pvoid data,u32){
	switch(a){
		case 0xd800:
		case 0xc800:
			printf("mem read %x\n",a);
		break;
		case 0xe000:
		case 0xe002:
			*((u8 *)data) = _ym[SR(a&3,1)]._status;
		break;
		case 0xe001:
		case 0xe003:
			_ym[SR(a&3,1)].read(_ym[SR(a&3,1)]._reg_idx);
			*((u8 *)data) = _ym[SR(a&3,1)]._data;
		break;
	}
	return 0;
}

int M1943Spu::_dumpRegisters(char *p){
	char cc[1024];

	*p=0;
	for(int i=0;i<sizeof(_ym)/sizeof(struct __ym2203);i++){
		sprintf(cc,"YM2203 %d\n\n",i+1);
		strcat(p,cc);
		for(int n=0;n<256;n++){
			sprintf(cc,"%02X:%02X  ",n,_ym[i]._regs[n]);
			strcat(p,cc);
			if((n&7) == 7)
				strcat(p,"\n");
		}
		strcat(p,"\n\n");
	}
	return 0;
}

int M1943Spu::_enterIRQ(int n,int v,u32 pc){
	return _cpu._enterIRQ(n,v,pc);
}

s32 M1943Spu::__cpu::fn_mem_w(u32 a,pvoid b,pvoid c,u32 d){
	return _spu->fn_mem_w(a,b,c,d);
}

s32 M1943Spu::__cpu::fn_mem_r(u32 a,pvoid b,pvoid c,u32 d){
	return _spu->fn_mem_r(a,b,c,d);
}

int M1943Spu::__cpu::Exec(u32 status){
	int ret;

	switch((ret= Z80Cpu::Exec(status))){
		case -1:
		case -2:
			return ret;
	}
	EXECTIMEROBJLOOP(ret,_enterIRQ(i__,0);ret=-3;,_mem);
	return ret;
}

#include "ym2203.cpp.inc"

};