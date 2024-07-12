#include "tehkanwc.h"
#include "tehkanwcspu.h"


#ifdef _DEVELOP
static FILE *fp=0;
#endif

namespace tehkanwc{

tehkanwcSpu::tehkanwcSpu() : PCMDAC() {
}

tehkanwcSpu::~tehkanwcSpu(){
}

int tehkanwcSpu::Init(tehkanwc &g){
	if(PCMDAC::Open(1,__msm5205::_freq))
		return -1;
	if(!(_samples=new u16[33000]))
		return -2;
	_cpu.Init(&g._memory[MB(15)]);
	_cpu._spu=this;
	_cpu.Query(ICORE_QUERY_SET_MACHINE,(IObject *)(ICore *)&g);

	for(int i =0;i<255;i++)
		_cpu.SetIO_cb(i,(CoreMACallback)&tehkanwcSpu::__cpu::fn_port_w,(CoreMACallback)&tehkanwcSpu::__cpu::fn_port_r);
	_cpu.SetMemIO_cb(0xc000,(CoreMACallback)&tehkanwcSpu::__cpu::fn_mem_w,(CoreMACallback)&tehkanwcSpu::__cpu::fn_mem_r);
	for(int i =0;i<255;i++)
	_cpu.SetMemIO_cb(0x8000+i,(CoreMACallback)&tehkanwcSpu::__cpu::fn_mem_w,(CoreMACallback)&tehkanwcSpu::__cpu::fn_mem_r);
	//_cpu.AddTimerObj((tehkanwcSpu *)this,6508);//one tick 65,08

	for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++)
		_pwms[i].init(i);

	_adpcm.init(0,*this);
	Create();
	Reset();
	return 0;
}

int tehkanwcSpu::Reset(){
	u32 i;

	_cpu.Reset();
	_nmi=0;
	for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++)
		_pwms[i].reset();
	_adpcm.reset();

	i=1;
	_cpu.Query(ICORE_QUERY_SET_STATUS,&i);
	_cycles=_cpu.getTicks();
	PCMDAC::Reset();
#ifdef _DEVELOPa
	if(!fp && !(fp=fopen("tehkanwcSpu.txt","wb")))
		printf("errror\n");
	//if(fp)
		//fseek(fp,44,SEEK_SET);
#endif
	return 0;
}

int tehkanwcSpu::Update(){
	u32 n, count;
	int sampleL,sampleR;
	u16 *p;

	{
		u32 f=_cpu.getFrequency();
		u32 c=__cycles;//_cpu.getTicks();

		n = c >= _cycles ? c-_cycles : (f-_cycles)+c;
		_cycles=c;
		n = n / (f / __msm5205::_freq);

		//printf("samples %u %u\n",n,f);
	}

	p=_samples;
	for(count=0;n>0 && count < 33000;n--){
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
		sampleL=_adpcm.output(1);
		for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++){
			sampleL += _pwms[i].output(12);
			if(sampleL<-32768)
				sampleL=-32768;
			else if(sampleL>32767)
				sampleL=32767;
		}
		*p++ = sampleL;
		count++;
	}
	//printf("write  %u\n",count);
	Write(_samples,count);
	return 0;
}

int tehkanwcSpu::Destroy(){
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

int tehkanwcSpu::Load(void *m){
	void *mem;

	if(m){
		mem=0;
		if(_cpu.Query(ICORE_QUERY_CPU_MEMORY,&mem))
			return -1;
		memcpy(mem,m,0x4000);
		_adpcm._mem=(u8 *)m + 0x40000;
	}
	return 0;
}

int tehkanwcSpu::Exec(u32 status){
	int ret=_cpu.Exec(status);
	if(_nmi&1){
		_cpu._enterNMI();
		_nmi=0;
	}
	return ret;
}

s32 tehkanwcSpu::fn_port_r(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 0:
		case 2:
			_pwms[SR(a,1)].read(((u8 *)data));
		break;
	}
	return 1;
}

s32 tehkanwcSpu::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 1:
		case 3:
			_pwms[SR(a,1)]._reg=*((u8 *)data);
			//printf("port %x %x\n",a,*((u8 *)data));
		break;
		case 0:
		case 2:
			_pwms[SR(a,1)].write(*((u8 *)data));
			//if(*((u8 *)data)>10)	printf("port %x %x\n",a,*((u8 *)data));
		break;
		case 0x50:
			if(*((u8 *)data)){
				u32  i;

				_cpu.Restart();
				for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++)
					_pwms[i].reset();
				_adpcm.reset();
				//i=0;
				//_cpu.Query(ICORE_QUERY_SET_STATUS,&i);

				_latch.reset();
			}
		break;
		default:
			printf("%s %x\n",__FUNCTION__,a);
		break;
	}
	return 0;
}

u32 tehkanwcSpu::_readLatch(){
	return _latch.read(1);
}

void tehkanwcSpu::_writeLatch(u32 v){
	_latch.write(0,v);
	//printf("LATCH %x %x %x\n",v,_latch._data[0],_latch._data[1]);
	_nmi|=1;
}

s32 tehkanwcSpu::fn_mem_w(u32 a,u8 data){
	switch(a){
		case 0xc000:
			/*
			 * write 0x80
			 * read
			 * write 0
			 */
			_latch.write(1,data);
			//EnterDebugMode();
		break;
		case 0x8001:
		break;
		case 0x8002://watchdog
		break;
		case 0x8003://clear nmi port
		break;
		default:
		break;
	}
	return 0;
}

s32 tehkanwcSpu::fn_mem_r(u32 a,u8 *data){
	switch(a){
		case 0xc000:
			*data=_latch.read(0);
		break;
		default:
			printf("msm read %x\n",a);
		break;
	}
	return 0;
}

int tehkanwcSpu::_dumpRegisters(char *p){
	char cc[1024];

	*p=0;
	for(int i=0;i<sizeof(_pwms)/sizeof(struct __ay8910);i++){
		sprintf(cc,"AY8910 %d\n\n",i+1);
		strcat(p,cc);
		for(int n=0;n<sizeof(_pwms[i]._regs);n++){
			sprintf(cc,"%02X:%02X  ",n,_pwms[i]._regs[n]);
			strcat(p,cc);
			if((n&7) == 7)
				strcat(p,"\n");
		}
		strcat(p,"\n\n");
	}
	return 0;
}

int tehkanwcSpu::_enterIRQ(int n,int v,u32 pc){
	return _cpu._enterIRQ(n,v,pc);
}

s32 tehkanwcSpu::__cpu::fn_mem_w(u32 a,pvoid,pvoid data,u32){
	return _spu->fn_mem_w(a,*((u8 *)data));
}

s32 tehkanwcSpu::__cpu::fn_mem_r(u32 a,pvoid,pvoid data,u32){
	return _spu->fn_mem_r(a,(u8 *)data);
}

s32 tehkanwcSpu::__cpu::fn_port_w(u32 a,pvoid mem,pvoid data,u32 f){
	return _spu->fn_port_w(a,mem,data,f);
}

s32 tehkanwcSpu::__cpu::fn_port_r(u32 a,pvoid mem,pvoid data,u32 f){
	return _spu->fn_port_r(a,mem,data,f);
}

int tehkanwcSpu::__cpu::Exec(u32 status){
	int ret;

	switch((ret= Z80Cpu::Exec(status))){
		case -1:
		case -2:
			return ret;
	}
	EXECTIMEROBJLOOP(ret,_enterIRQ(i__,0);ret=-3;,_mem);
	return ret;
}

int tehkanwcSpu::__msm5205::reset(){
	_value=0;
	return 0;
}

int tehkanwcSpu::__msm5205::output(int){
	return 0;
}

int tehkanwcSpu::__msm5205::init(int,tehkanwcSpu &){
	int step, nib;
	const int nbl2bit[16][4] ={
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	for (step = 0; step <= 48; step++){
		int stepval = floor (16.0 * pow (11.0 / 10.0, (double)step));

		for (nib = 0; nib < 16; nib++){
			_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
					stepval/2 * nbl2bit[nib][2] +
					stepval/4 * nbl2bit[nib][3] +
					stepval/8);
		}
	}
	_mem=NULL;
	return 0;
}

#include "ay8910.cpp.inc"
};