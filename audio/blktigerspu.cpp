#include "blacktiger.h"
#include "blktigerspu.h"

namespace blktiger{

#ifdef _DEVELOP
static FILE *fp=0;
#endif

BlkTigerSpu::BlkTigerSpu() : PCMDAC(){
	//_cpu._freq=3579545;
	_samples=NULL;
	_cycles=0;
}

BlkTigerSpu::~BlkTigerSpu(){
}

int BlkTigerSpu::Init(BlackTiger &g){
	if(PCMDAC::Open(1,_freq))
		return -1;
	if(!(_samples=new u16[_freq]))
		return -2;
	_cpu.Query(ICORE_QUERY_SET_MACHINE,(IObject *)(ICore *)&g);

	_cpu.Init(&g._memory[MB(15)]);
	_cpu._spu=this;

	for(int  i=0;i<4;i++)
		_cpu.SetMemIO_cb(0xe000+i,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);

	_cpu.SetMemIO_cb(0xc800,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);
	_cpu.AddTimerObj((BlkTigerSpu *)this,6508);//one tick 65,08

	_ym[0].init();
	_ym[1].init();

	__ym2203::initialize();

	Create();
	Reset();
	return 0;
}

int BlkTigerSpu::Reset(){
	u32 i;

	_cpu.Reset();

	i=1;
	_cpu.Query(ICORE_QUERY_SET_STATUS,&i);
	_ym[0].reset();
	_ym[1].reset();

	_latch.reset();
	if(_samples) memset(_samples,0,sizeof(u16)*_freq);
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

int BlkTigerSpu::Update(){
	u32 n, count;
	int sampleL;

	if(!_samples)
		return -1;
	{
		u32 f=MHZ(6);
		//f=_cpu.getFrequency();
		u32 c=__cycles;
		//c=_cpu.getTicks();

		n = SPU_CYCLES(_cycles,f,_freq,c);
		_cycles=c;
	//	printf("sample %u\n",n);
	}

	for(count=0;n>0 && count <_freq;n--){
		sampleL=0;
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
			sampleL += _ym[0].update(1);
		if(_ym[1]._enabled)
			sampleL += _ym[1].update(1);

		if(sampleL<-32768)
			sampleL=-32768;
		else if(sampleL>32767)
			sampleL=32767;
		_samples[count++]=sampleL;
	}
	Write(_samples,count);
	return 0;
}

int BlkTigerSpu::Destroy(){
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

int BlkTigerSpu::Load(void *m){
	void *mem;

	if(m){
		mem=0;
		if(_cpu.Query(ICORE_QUERY_CPU_MEMORY,&mem))
			return -1;
		memcpy(mem,m,0x8000);
	}
	_cpu.Stop();
	return 0;
}

int BlkTigerSpu::Exec(u32 status){
	int ret=_cpu.Exec(status);
	return ret;
}

s32 BlkTigerSpu::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 0:
			_latch.write(0,*((u8 *)data));
		break;
		case 4:
			if(*((u8 *)data) & 0x20){
				u32  i;

				_cpu.Restart();
				_ym[0].reset();
				_ym[1].reset();
				i=0;
				_cpu.Query(ICORE_QUERY_SET_STATUS,&i);
			}
		break;
	}
	return 0;
}

int BlkTigerSpu::Run(u8 *_mem,int cyc,void *obj){
	//printf("BlkTigerSpu %u\n",cyc);
	int i = _ym[0].step(cyc);
	i |= _ym[1].step(cyc);
	if(i){
		//EnterDebugMode();
		//_mem[0xe000]|=1;
		return 1;
	}
	return 0;
}

s32 BlkTigerSpu::fn_mem_w(u32 a,u8 data){
	switch(a){
		case 0xc800:
			printf("%s %x %x\n",__FILE__,a,data);
		break;
		case 0xe000:
		case 0xe002:
			_ym[SR(a&3,1)]._reg_idx=data;
		break;
		case 0xe001:
		case 0xe003:
			_ym[SR(a&3,1)].write(_ym[SR(a&3,1)]._reg_idx,data);
		break;
	}
	return 0;
}

s32 BlkTigerSpu::fn_mem_r(u32 a,u8 *data){
	switch(a){
		case 0xc800:
			//printf("mem read %x\n",a);
			*data = _latch.read();
		break;
		case 0xe000:
		case 0xe002:
			*data = _ym[SR(a&3,1)]._status;
#ifdef _DEVELOP
		//	if(_ym[SR(a&3,1)]._status==2) EnterDebugMode();
		//	printf("S %x\n",_ym[SR(a&3,1)]._status);
#endif
		break;
		case 0xe001:
		case 0xe003:
			_ym[SR(a&3,1)].read(_ym[SR(a&3,1)]._reg_idx);
			*data = _ym[SR(a&3,1)]._data;
		break;
	}
	return 0;
}

int BlkTigerSpu::_dumpRegisters(char *p){
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

int BlkTigerSpu::_enterIRQ(int n,int v,u32 pc){
	return _cpu._enterIRQ(n,v,pc);
}

s32 BlkTigerSpu::__cpu::fn_mem_w(u32 a,pvoid,pvoid data,u32){
	return ((BlkTigerSpu *)_spu)->fn_mem_w(a,*((u8 *)data));
}

s32 BlkTigerSpu::__cpu::fn_mem_r(u32 a,pvoid,pvoid data,u32){
	return ((BlkTigerSpu *)_spu)->fn_mem_r(a,(u8 *)data);
}

int BlkTigerSpu::__cpu::Exec(u32 status){
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