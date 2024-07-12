#include "cps2spu.h"

namespace cps2{

#ifdef _DEVELOP
static FILE *fp=0;
#endif

class __dsp{
public:
	__dsp(){_rom=NULL;_ram=NULL;};
	virtual ~__dsp(){};
	virtual int _reset(){return -1;};
	virtual int _write(u8 a){
		printf("dspwrite %x %x\n",a,_val);
		_ready=1;
		return  0;
	};
	virtual int _setBank(u32 v){
		_bank = v;
		_ready=1;
		return 0;
	};

	virtual int _step(int){
		return 0;
	};
public:
	virtual int _destroy(){return -1;};
	u8 *_rom,*_ram,_bank,*_samples;
	u16 _val;
	union{
		struct{
			unsigned int _dummy:7;
			unsigned int _ready:1;
		};
		u8 _status;
	};

	union __registers{
		struct {
			struct __pcm{
				u16 _bank,_addr,_phase,_rate,_loop,_end,_volume,_echo;
			} _pcms[16];
			struct __adpcm{
			} _adpmcs[3];
		} _channels;
		u16 _values[256];
	} _regs;

} _dsp16a;

#define __Z80F(a,...)

CPS2Spu::CPS2Spu() : PCMDAC(){
	_samples=NULL;
	_cycles=0;
}

CPS2Spu::~CPS2Spu(){
}

int CPS2Spu::Init(void *mem,void *ipc){
	if(PCMDAC::Open(1,_freq))
		return -1;
	if(!(_samples=new u16[_freq]))
		return -2;
	//_cpu.Query(ICORE_QUERY_SET_MACHINE,(IObject *)(ICore *)&g);
	_cpu.Init(mem);

	_cpu._spu=this;

	for(int  i=0;i<8;i++)
		_cpu.SetMemIO_cb(0xd000+i,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);

	for(int i=0;i<0x1000;i++){
	//	SetMemIO_cb(i,(CoreMACallback)&tehkanwc::fn_shared_mem_w);
		_cpu.SetMemIO_cb(i+0xc000,(CoreMACallback)&__cpu::fn_shared_mem_w);
		//_cpu.SetMemIO_cb(i+0xF000,(CoreMACallback)&__cpu::fn_shared_mem_w);
	}

	//SetMemIO_cb(0xda00,(CoreMACallback)&tehkanwc::fn_mem_w);
	//_cpu.SetMemIO_cb(0xda00,(CoreMACallback)&tehkanwc::fn_mem_w);
	_cpu._ipc=(u8 *)ipc - 0xb000;

	_dsp16a._ram=((u8 *)mem+0xf000);
	//_ipc=&_memory[MB(10)];

	//_cpu.SetMemIO_cb(0xc800,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);
	_cpu.AddTimerObj((CPS2Spu *)this,_cpu.getFrequency()/250);//one tick 65,08

	Create();
	Reset();

	return 0;
}

int CPS2Spu::Reset(){
	u32 i;

	_cpu.Reset();
	_dsp16a._reset();
	i=1;
	_cpu.Query(ICORE_QUERY_SET_STATUS,&i);

	_latch.reset();
	if(_samples) memset(_samples,0,sizeof(u16)*_freq);
	_cycles=_cpu.getTicks();
	PCMDAC::Reset();
#ifdef _DEVELOPa
	if(!fp && !(fp=fopen("cps2spu.txt","wb")))
		printf("errror\n");
	//if(fp)
		//fseek(fp,44,SEEK_SET);
#endif
	return 0;
}

int CPS2Spu::Update(){
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
		if(sampleL<-32768)
			sampleL=-32768;
		else if(sampleL>32767)
			sampleL=32767;
		_samples[count++]=sampleL;
	}
	Write(_samples,count);
	return 0;
}

int CPS2Spu::Destroy(){
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

int CPS2Spu::Load(void *m){
	void *mem;

	if(m){
		mem=0;
		if(_cpu.Query(ICORE_QUERY_CPU_MEMORY,&mem))
			return -1;
		memcpy(mem,m,0x8000);
	}
	_cpu.Stop();
	_dsp16a._samples =(u8 *)m + 0x20000;
	return 0;
}

int CPS2Spu::Exec(u32 status){
	int ret=_cpu.Exec(status);
	return ret;
}

s32 CPS2Spu::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 0:
			_latch.write(0,*((u8 *)data));
		break;
		case 4:
			if(*((u8 *)data) & 0x20){
				u32  i;

				_cpu.Restart();
			//	_ym[0].reset();
				//_ym[1].reset();
				i=0;
				_cpu.Query(ICORE_QUERY_SET_STATUS,&i);
			}
		break;
	}
	return 0;
}

int CPS2Spu::Run(u8 *_mem,int cyc,void *obj){
	//printf("CPS2Spu %u\n",cyc);
	int i = _dsp16a._step(cyc);
i=1;
	if(i){
		//EnterDebugMode();
		//_mem[0xe000]|=1;
		return 1;
	}
	return 0;
}

s32 CPS2Spu::fn_mem_w(u32 a,u8 data){
//	printf("%s %x %x\n",__FILE__,a,data);
	switch(a){
		case 0xd001:
			_dsp16a._val = (_dsp16a._val& 0xff00) | data;
			return 1;
		case 0xd000:
		_dsp16a._val = (u8)_dsp16a._val | SL(data,8);
			return 1;
		case 0xd002:
			_dsp16a._write(data);
		break;
		case 0xd003://ban
			_dsp16a._setBank(data&15);
			printf("setbank %x\n",data);
		break;
	}
	return 0;
}

s32 CPS2Spu::fn_mem_r(u32 a,u8 *data){
	//printf("%s %x %x\n",__FILE__,a,*data);
	switch(a){
		case 0xd007:
			*data =_dsp16a._status;
		break;
	}
	return 0;
}

int CPS2Spu::_dumpRegisters(char *p){
	char cc[1024];

	*p=0;

	return 0;
}

int CPS2Spu::_enterIRQ(int n,int v,u32 pc){
	return _cpu._enterIRQ(n,v,pc);
}

s32 CPS2Spu::__cpu::fn_mem_w(u32 a,pvoid,pvoid data,u32){
	return ((CPS2Spu *)_spu)->fn_mem_w(a,*((u8 *)data));
}

s32 CPS2Spu::__cpu::fn_mem_r(u32 a,pvoid,pvoid data,u32){
	return ((CPS2Spu *)_spu)->fn_mem_r(a,(u8 *)data);
}

int CPS2Spu::__cpu::Exec(u32 status){
	int ret;

	switch((ret= Z80CpuShared::Exec(status))){
		case -1:
		case -2:
			return ret;
	}
	EXECTIMEROBJLOOP(ret,_enterIRQ(i__,0);ret=-3;,_mem);
	return ret;
}

#include "z80.cpp.inc"

};