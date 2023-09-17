#include "blacktiger.h"
#include "blktigerspu.h"

namespace blktiger{

#ifdef _DEVELOP
static FILE *fp=0;
#endif

u16 BlkTigerSpu::_waveform[1024]={0};

static u32 attenuation_to_volume(u32 input){
	return SL(0x400|((255-(u8)input)*3),2) >> (input >> 8);
}

static u32 attenuation_increment(uint32_t rate, uint32_t index){
	return SR(rate+7,3);
}

BlkTigerSpu::BlkTigerSpu() : PCMDAC(){
	//_cpu._freq=3579545;
	_samples=0;
	_cycles=0;
	_ipc=0;
}

BlkTigerSpu::~BlkTigerSpu(){
}

int BlkTigerSpu::Init(BlackTiger &g){
	if(PCMDAC::Open(1,_freq))
		return -1;
	if(!(_samples=new u16[33000]))
		return -2;
	_cpu.Query(ICORE_QUERY_SET_MACHINE,(IObject *)&g);
	_ipc = &g._memory[MB(15)];
	_cpu.Init(_ipc);
	_cpu._spu=this;

	for(int  i=0;i<4;i++)
		_cpu.SetMemIO_cb(0xe000+i,(CoreMACallback)&__cpu::fn_mem_w,(CoreMACallback)&__cpu::fn_mem_r);

	_cpu.SetMemIO_cb(0xc800,(CoreMACallback)&__cpu::fn_mem_w);
	_cpu.AddTimerObj((BlkTigerSpu *)this,6508);//one tick 65,08

	_ym[0].init(*this);
	_ym[1].init(*this);

	u8 d[][2]={{8,1},{6,2},{5,2},{4,5},{3,10},{2,21},{1,44},{0,171}};
	u16 dt[256];
	float inc,f;
	for(int n=0,l=-1,i=0,step=0;i<sizeof(d)/2;n++){
		int v = d[i][0];
		if(v!=l){
			inc=1.0f / (step=d[i][1]);
			f=(v+1)-inc;
			l=v;
		}
		dt[n]=floor(f*256.0f);
		//printf("%x:%x ",dt[n],s_sin_table[n]);
		f-=inc;
		if(--step == 0)
			i++;
	}

	for (u32 index = 0; index < 1024; index++){
		u32 p=index;
		if(p&0x100)
			p=~p;
		_waveform[index] = dt[(u8)p]|((SR(index, 9)&1) << 15);//abs_sin_attenuation(index) | ((SR(index, 9)&1) << 15);
	//	printf("%d:%d  ",v,abs_sin_attenuation(index) | ((SR(index, 9)&1) << 15));
	}
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

int BlkTigerSpu::Update(){
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
	for(count=0;n>0;n--){
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
		if(_cpu.Query(ICORE_QUERY_CPU_MEMORY,&mem))
			return -1;
		memcpy(mem,m,0x8000);
	}
	return 0;
}

int BlkTigerSpu::Exec(u32 status){
	int ret=_cpu.Exec(status);
	return ret;
}

s32 BlkTigerSpu::fn_port_w(u32 a,pvoid port,pvoid data,u32){
	switch(a){
		case 0:{
			//*((u8 *)port)=_ipc[0xc800]|0x8;
			_ipc[0xc800]=*((u8 *)data);
			//*((u8 *)data)|=0x8;
			//return 1;
		}
		break;
		case 4:
			if(*((u8 *)data) & 0x20){
				u32  i;

				_cpu.Reset();
				_ym[0].reset();
				_ym[1].reset();
				i=0;
				_cpu.Query(ICORE_QUERY_SET_STATUS,&i);
			}
		break;
	}
	return 0;
}

int BlkTigerSpu::Run(u8 *_mem,int  cyc){
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
			printf("mem read %x  %p\n",_ipc[0xc800],_ipc);
		break;
		case 0xe000:
		case 0xe002:
			*data = _ym[SR(a&3,1)]._status;
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
	return _spu->fn_mem_w(a,*((u8 *)data));
}

s32 BlkTigerSpu::__cpu::fn_mem_r(u32 a,pvoid,pvoid data,u32){
	return _spu->fn_mem_r(a,(u8 *)data);
}

int BlkTigerSpu::__cpu::Exec(u32 status){
	int ret;

	switch((ret= Z80Cpu::Exec(status))){
		case -1:
		case -2:
			return ret;
	}
	EXECTIMEROBJLOOP(ret,_enterIRQ(i,0);ret=-3;,_mem);
	return ret;
}

BlkTigerSpu::__ym2203::__ym2203(){
	memset(_fm,0,sizeof(_fm));
	memset(_pwm,0,sizeof(_pwm));
	memset(_timer,0,sizeof(_timer));
	_enabled=1;
}

int BlkTigerSpu::__ym2203::set_clock_prescale(u8 v){
	_clock_prescale=v;
	return 0;
}

int BlkTigerSpu::__ym2203::init(BlkTigerSpu &g){
	for(int i=0;i<sizeof(_pwm)/sizeof(struct __pwm);i++)
		_pwm[i].init(i,*this);
	for(int i=0;i<sizeof(_fm)/sizeof(struct __fm);i++)
		_fm[i].init(i,*this);
	for(int i=0;i<sizeof(_timer)/sizeof(struct __timer);i++)
		_timer[i].init(i,*this);
	return 0;
}

int BlkTigerSpu::__ym2203::update(u32 cyc){
	int sample;

	if((_noise_count+=cyc) >= _noise_period){
		do{
			_noise_state ^= SL((_noise_state&1)^SR(_noise_state&8,3),17);
			_noise_state >>= 1;
			_noise_count -= _noise_period;
		}while(_noise_count>=_noise_period);
	}
	if((_env_count+=cyc) >= _env_period){
		do{
			_env_count-=_env_period;
			_env_state++;
		}while(_env_count>=_env_period);

		u8 a = SR(_regs[0xd],2) & 1;
		if( ((_regs[0xd]&1) | ((SR(_regs[0xd],3) & 1) ^ 1)) && _env_state >= 32){
			_env_state=32;
			_env_vol= ((a ^ (SR(_regs[0xd],1) & 1)) & (SR(_regs[0xd],3) & 1)) ? 31 : 0;
		}
		else{
			if(_regs[0xd] & 2)
				a ^= SR(_env_state,5) & 1;
			_env_vol=(_env_state & 31) ^ (a ? 0 : 31);
		}
	}

	if((++_counter & 3) == 3)
		_counter++;
	sample=0;

	for(int i=0;i<sizeof(_fm)/sizeof(struct __fm);i++){
		if((sample += _fm[i].output(_counter)) > 32767)
			sample=32767;
		else if(sample < -32768)
			sample=-32768;
	}
	for(int i=0;i<sizeof(_pwm)/sizeof(struct __pwm);i++){
		if((sample += _pwm[i].output(cyc,*this)) > 32767)
			sample=32767;
		else if(sample < -32768)
			sample=-32768;
	}
	return sample;
}

int BlkTigerSpu::__ym2203::reset(){
	_data=0;
	_reg_idx=0;
	_irq=0x1f;
	_status=0;
	_clock_prescale=6;
	_counter=0;

	_noise_period=1;
	_noise_state=1;
	_noise_count=0;

	_env_count=0;
	_env_period=1;
	_env_state=0;

	memset(_regs,0,sizeof(_regs));
	for(int i=0;i<sizeof(_pwm)/sizeof(struct __pwm);i++)
		_pwm[i].reset();
	for(int i=0;i<sizeof(_fm)/sizeof(struct __fm);i++)
		_fm[i].reset();
	for(int i=0;i<sizeof(_timer)/sizeof(struct __timer);i++)
		_timer[i].reset();
	return 0;
}

int BlkTigerSpu::__ym2203::step(int cyc){
	int irq=0;

	cyc = cyc/get_clock_prescale();
	for(int i=0;i<sizeof(_timer)/sizeof(struct __timer);i++){
		if(!_timer[i]._enabled) {
			goto  A;
		}
		if(_timer[i].step(cyc)){
			_status |= SL(1,i);
			irq |= SL(1,i);
			if(i==0 && (_regs[0x27] & 0xc0)==0x80){
				printf("csm\n");
			}
			continue;
		}
A:
		_status &= ~SL(1,i);
	}
	return irq;
}

int BlkTigerSpu::__ym2203::read(u8 a){
	printf("ym read %x\n",a);
	if(a<0x10){
	}
	else
		_data=0;
	return 0;
}

int BlkTigerSpu::__ym2203::write(u8 a,u8 v){
	_regs[a]=v;
	switch(a){
		default:
			if(a < 0x10){//pcm
				for(int i=0;i<sizeof(_pwm)/sizeof(__pwm);i++)
					_pwm[i].write(a,v);
			}
			else if(a > 0x2f)//fm
				_fm[a&3].write(a,v);
		break;
		case 6:
			if(!(_noise_period = SL(_regs[6] & 31,1)))
				_noise_period=1;
		break;
		case 7:
			for(int i=0;i<sizeof(_pwm)/sizeof(__pwm);i++)
				_pwm[i].write(a,v);
		break;
		case 0xb:
		case 0xc:
			if(!(_env_period=MAKEHWORD(_regs[0xb],_regs[0xc])))
				_env_period=1;
		break;
		case 0xd://env shape
			_env_state=0;
		break;
		case 0x24:
		case 0x25:{
			u16 vv=SL(_regs[0x24],2)|(_regs[0x25]&3);
			_timer[0].load(vv);
		}
		break;
		case 0x26:
			_timer[1].load(_regs[0x26]);
		break;
		case 0x27:
			_timer[0].write(_regs,0x27,v);
			_timer[1].write(_regs,0x27,v);
	/**		if(v&0x10)
				_timer_a=0;
			if(v&0x20)
				_timer_b=0;**/
		break;
		case 0x28:
			_fm[v&3].key(SR(v,4));
		break;
		case 0x2d:
			set_clock_prescale(6);
		break;
		case 0x2e:
			set_clock_prescale(3);
		break;
		case 0x2f:
			set_clock_prescale(2);
		break;
	}
	return 0;
}

int BlkTigerSpu::__ym2203::__pwm::init(int n,struct __ym2203  &g){
	_idx=n;
	_regs=g._regs;
	reset();
	return 0;
}

int BlkTigerSpu::__ym2203::__pwm::reset(){
	_enabled=0;
	_count=0;
	_state=0;
	_duty=1;
	_noise=0;
	_env=0;
	_vol=0;

	return 0;
}

int BlkTigerSpu::__ym2203::__pwm::output(u32 cyc,struct __ym2203 &g){
	int sample;

	if((_count += cyc) >= _duty){
		do{
			_state ^= 1;
			_count-=_duty;
		}while(_count>=_duty);
	}

	if(((_noise | (g._noise_state&1)) & (_state | _enabled)) == 0)
		return 0;
	if(_env)
		sample=g._env_vol;
	else
		sample=_vol;
	return SL(sample,9);
}

int BlkTigerSpu::__ym2203::__pwm::write(u8 r,u8 vv){
	switch(r){
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			if(!(_duty = MAKEHWORD(_regs[_idx*2],_regs[1+_idx*2]) & 0xfff))
				_duty=1;
		break;
		case 6:
		break;
		case 7:
			_enabled = SR(_regs[7],_idx);
			_noise=SR(_regs[7],_idx+3);
		break;
		case 0x8:
		case 0x9:
		case 0xa:
			_vol=SL(_regs[8+_idx] & 0xf,1);
			_env=SR(_regs[8+_idx],4);
		break;
	}
	return 0;
}

int BlkTigerSpu::__ym2203::__fm::init(int i,struct __ym2203  &g){
	_regs=g._regs;
	_idx=i;
	for(int n=0;n<sizeof(_op)/sizeof(struct __op);n++)
		_op[n].init(n,_idx,g);
	return 0;
}

int BlkTigerSpu::__ym2203::__fm::reset(){
	_enabled=0;
	_modified=0;
	for(int i=0;i<sizeof(_op)/sizeof(__op);i++)
		_op[i].reset();
	memset(_fb,0,sizeof(_fb));
	return 0;
}

int BlkTigerSpu::__ym2203::__fm::key(u8 m){
	for(int i=0;i<sizeof(_op)/sizeof(__op);i++,m >>= 1){
		//printf("key %d %d %d\n",i,_op[i]._key,(m&1));
		if(_op[i]._key_state != (m & 1)){
			_op[i]._key_state=m;
			_modified=1;
			_op[i]._modified=1;
		}
	}
	return 0;
}

int BlkTigerSpu::__ym2203::__fm::output(u32 counter){
	int sample;
	s16 opout[8];
	u32 fb;

	if(_modified){
		_modified=0;
	}

	_fb[0]=_fb[1];
	_fb[1]=_fb[2];

	fb=SR(_regs[0xb0|_idx],3)&7;
	if(fb)
		fb=SR(_fb[0]+_fb[1],10-fb);

	sample = _op[0].output(counter,fb);
	_fb[2]=sample;

	opout[0]=0;
	opout[1]=sample;

	opout[2] = _op[1].output(counter,fb);
	opout[5] = opout[1] + opout[2];

	fb = opout[SR(_alg_op, 1) & 7] >> 1;
	opout[3] = _op[2].output(counter,fb);
	opout[6] = opout[1] + opout[3];
	opout[7] = opout[2] + opout[3];
	fb = opout[SR(_alg_op, 4)&7] >> 1;

	sample=_op[3].output(counter,fb);
	if(_alg_op & 0x80){
		if((sample+=opout[1])>32767)
			sample=32767;
		else if(sample<-32768)
			sample=-32768;
	}
	if(_alg_op & 0x100){
		if((sample+=opout[2])>32767)
			sample=32767;
		else if(sample<-32768)
			sample=-32768;
	}
	if(_alg_op & 0x200){
		if((sample+=opout[3])>32767)
			sample=32767;
		else if(sample<-32768)
			sample=-32768;
	}
	if(sample==32767 || sample==-32768)
		return sample;

	return SL(SR(sample,8),7);
}

int BlkTigerSpu::__ym2203::__fm::write(u8 reg,u8 v){
	switch(SR(reg,4)){
		case 0xa:
			for(int i=0;i<sizeof(_op)/sizeof(__op);i++)
				_op[i]._modified=1;
			_modified=1;
		break;
		case 0xb:{
			u16 s_algorithm_ops[]={53,58,100,113,305,787,769,896,53,180,305,644};
			_alg_op=s_algorithm_ops[_regs[0xb0|_idx]&7];
			for(int i=0;i<sizeof(_op)/sizeof(__op);i++)
				_op[i]._modified=1;
			_modified=1;
		}
		break;
		case 5:
			//_op[SR(reg,2)&3]._set_rate(0,v);//attack
			_op[SR(reg,2)&3]._modified=1;
			_modified=1;
		break;
		case  6:
			_op[SR(reg,2)&3]._modified=1;
			_modified=1;
		break;
		case  7:
			_op[SR(reg,2)&3]._modified=1;
		break;
		case 8:
			_op[SR(reg,2)&3]._modified=1;
			_modified=1;
		break;
		case 0x9://envelope ssg
			_op[SR(reg,2)&3]._env._ssg=SR(v,4);
			_op[SR(reg,2)&3]._env._mode=v&0x7;
			_op[SR(reg,2)&3]._modified=1;
			_modified=1;
		break;
		case 4:
			_op[SR(reg,2)&3]._level=SL(v,3);
			_modified=1;
		break;
	}
	return 0;
}

int BlkTigerSpu::__ym2203::__fm::__op::init(int n,int ch,struct __ym2203 &g){
	_idx=n;
	_regs=g._regs;
	_ch=ch;
	reset();
	return 0;
}

int BlkTigerSpu::__ym2203::__fm::__op::reset(){
	_key=0;
	_key_state=0;
	_modified=0;
	_phase=0;
	_phase_step=0;

	_env._values=0;
	_env._enabled=1;
	_env._state=EG_RELEASE;
	_env._vol=0x3ff;
	for(int i=0;i<sizeof(_rates)/sizeof(u8);i++)
		_rates[i]=0;
	_sustain_level=0;
	_level=0;

	return 0;
}

int BlkTigerSpu::__ym2203::__fm::__op::attack(){
	_modified=1;
	if(_env._state == EG_ATTACK) return 1;
	_env._state = EG_ATTACK;
	if(_rates[0] > 61)
		_env._vol=0;
	_phase=0;
	_env._inverted=0;
	return 0;
}

int BlkTigerSpu::__ym2203::__fm::__op::release(){
	_modified=1;
	if(_env._state == EG_RELEASE) return 1;
	_env._state = EG_RELEASE;
	if(_env._inverted){
		_env._vol=(0x200-_env._vol)&0x3ff;
		_env._inverted=0;
	}
	return  0;
}

int BlkTigerSpu::__ym2203::__fm::__op::output(u32 _counter,u32 fb){
	int sample;

	if(_modified){
		u16 freq,opo,block,v=MAKEHWORD(_regs[0xa0|_ch],_regs[0xa4|_ch]);
		u32 kc,ksr;

		opo=_ch|SL(_idx,2);
		block = SR(v,10) & 0xf;
		kc=SL(block,1)|(SR(0xfe00,SR(v,7) & 0xf) & 1);
		ksr=SR(kc,(SR(_regs[0x50|opo],6)&3)^3);

		for(int i=0;i<sizeof(_rates)/sizeof(u8);i++){
			switch(i){
				case 0://attach
					_rates[i]=SL(_regs[0x50|opo]&0x1f,1);
				break;
				case 1://decay
					_rates[i]=SL(_regs[0x60|opo]&0x1f,1);
				break;
				case 2://sustain
					_rates[i]=SL(_regs[0x70|opo]&0x1f,1);
					_sustain_level=SR(_regs[0x80|opo],4) & 0xf;
					_sustain_level |= (_sustain_level+1) & 0x10;
					_sustain_level=SL(_sustain_level,5);
				break;
				case 3://release
					_rates[i]=SL(_regs[0x80|opo]&0xf,2)+2;
				break;
			}
			if(_rates[i]){
				//ksr
				if((_rates[i]+=ksr) > 63)
					_rates[i]=63;
			}
		}
		if(_key != _key_state){
			//("keyonoff %d %d %d\n",_ch,_idx,_key);
			_key=_key_state;
			if(_key)
				attack();
			else
				release();
#ifdef _DEVELOP
	if(_idx==0 && fp)
		fprintf(fp,"keyonoff %d:%x %x %d\n",_idx,_key,_key_state,_env._state);
#endif
		}
		freq = SL(v & 0x7ff,1);
		_phase_step = SR(SL(freq,SR(block,1)),2);
		//_phase_step += cache.detune;
		_phase_step &= 0x1ffff;
		v=SL(_regs[0x30|opo] & 0xf,1);
		if(!v) v=1;
		_phase_step = SR(_phase_step*v,1);

		_modified=0;//operator
	}
	if(_env._state == EG_RELEASE && _env._vol>0x380)
		return 0;
	if((_counter & 3) == 0){// clock enveloope / 3
#ifdef _DEVELOP
		if(fp) fprintf(fp,"ec %d:%d %u",_ch,_idx,_counter);
#endif
		_counter=SR(_counter,2);
		if(_env._ssg){
			//ssg eg clock
			if((_env._vol&0x200)){
				if(_env._mode&1){
					_env._inverted=SR(_env._mode&4,2)^SR(_env._mode&2,1);
				}
				else{
					_env._inverted^=SR(_env._mode&2,1);
				}
				if (_env._state == EG_RELEASE){
					_env._vol = 0x3ff;
				}
			}
		}
		else
			_env._inverted=0;

		if(_env._state == EG_ATTACK && _env._vol==0)
			_env._state=EG_DECAY;
		if(_env._state == EG_DECAY && _env._vol >= _sustain_level)
			_env._state=EG_SUSTAIN;

		u32 rate = _rates[_env._state-1];
		u32 rate_shift=SR(rate,2);
		u32 counter=SL(SR(_counter,2),rate_shift);

#ifdef _DEVELOP
		if(fp) fprintf(fp," %u %u V:%u %u S:%u",counter,_counter,_env._vol,_sustain_level,_env._state);
#endif
		if((counter & 0x7ff) != 0)
			goto A;
		u32 n=SR(counter,rate_shift > 11 ? rate_shift : 11) & 7;
		u32 inc = attenuation_increment(rate,n);//SR(rate,3);

#ifdef _DEVELOP
		if(fp) fprintf(fp," %u %u %u",rate,n,inc);
#endif
//_env._vol 4.6
		if(_env._state == EG_ATTACK){
			if (rate < 62)
				_env._vol += SR((~_env._vol * inc),4);
		}
		else{
			if(!_env._ssg)
				_env._vol += inc;
			else if(_env._vol < 0x200)
				_env._vol += SL(inc,2);
			if(_env._vol > 0x3ff)
				_env._vol=0x3ff;
		}
	}
#ifdef _DEVELOP
	if(fp) fprintf(fp," \n");
#endif
A:
	_phase += _phase_step;

	u32 e0=_waveform[(SR(_phase,10)+fb) & 0x3ff];
	int env = _env._vol;

	if(_env._inverted)
		env=(0x200-env) & 0x3ff;
	env=env+_level;
	if(env > 0x3ff)
		env=0x3ff;
	//total level 4.6 fixed point to 4.8

	int e1=attenuation_to_volume((e0 & 0x7fff) + SL(env,2));
	sample = e0 & 0x8000 ? -e1 : e1;
	return sample;
}

int BlkTigerSpu::__ym2203::__timer::init(int n,struct __ym2203 &g){
	//engine_=g;
	_idx=n;
	return 0;
}

int BlkTigerSpu::__ym2203::__timer::reset(){
	_enabled=0;
	_count=0;
	_load=0;
	return 0;
}

int BlkTigerSpu::__ym2203::__timer::write(u8* regs,u8 a,u8 v){
	switch(a){
		case 0x27:
			if(_idx==0){
				if((v&0x10))
					reset();
				_enabled=SR(v,2);
				load(SL(regs[0x24],2)|(regs[0x25]&3));
			}
			else if(_idx==1){
				if((v&0x20))
					reset();
				_enabled=SR(v,3);
				load(regs[0x26]);
			}
		break;
		default:
			return -1;
	}
	return 0;
}

int BlkTigerSpu::__ym2203::__timer::load(u32 v){
	if(_idx==0)
		_count=1024-v;
	else if(_idx==1)
		_count=SL(256-v,4);
	else
		_count=0;
	_count*= 12;
	_load=_count;
	//printf("timer cyc %d %d \n",_idx,_count);
	return 0;
}

int BlkTigerSpu::__ym2203::__timer::step(int cyc){
	u16 count = _count;

	//printf("timer step %d %u %u\n",_idx,cyc,_count);
	_count -= _count > cyc ? cyc : _count;
	if(_count)
		return 0;
	cyc -= count;
	_count=_load;
	_count += cyc;
	//if(!_count)
		_enabled=0;
	return 1;
}

};