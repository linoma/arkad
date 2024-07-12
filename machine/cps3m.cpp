#include "cps3m.h"
#include "gui.h"

extern GUI gui;
//04040404		89898989	b0b0b0b0	383838c8	c2c2c2c2
namespace cps3{

static 	u8 _key_config[]={0,1,2,3,4,5,6,24,28,49,50};

CPS3M::CPS3M() : Machine(MB(250)),SH2Cpu(),CPS3GPU(),CPS3DAC(),CPS3DEV(){
	CCore::_freq=MHZ(25);
	_keys=_key_config;
}

CPS3M::~CPS3M(){
	//Destroy();
}

int CPS3M::Load(IGame *pg,char *path){
	FILE *fp;
	u32 u,ofs,i,d[10];
	u8 *p;

	if(!pg || pg->Open(path,0))
		return -1;
	Reset();
	if(pg->Query(IGAME_GET_INFO,d))
		return -2;

	_key1=d[IGAME_GIS_USER];
	_key2=d[IGAME_GIS_USER+1];

	pg->Read(_memory,MB(80),&u);

	for (i=0; i<0x80000; i+=4) {
		u32 xormask = _decrypt(i);
		u32 v,vv =*((u32 *)&_memory[i]);
		v = SL((u8)vv,24)|SL(vv&0xFF00,8)|SR(vv&0xFF0000,8)|SR(vv,24);
	//	if ( (i<0x1ff00) || (i>0x1ff6b) )
			v ^= xormask;
		*((u32 *)&M_BIOS[i])=v;
	}

	if(d[IGAME_GIS_ATTRIBUTE] & BV(1)){
		char c[1024];

		if(!pg->Query(IGAME_GET_DISK_IMAGE,c)){
			string s = GameManager::getBasePath(path);
			s += DPS_PATH;
			s += c;
			_cdrom.OnLoad((char *)s.c_str());
			memset(M_ROM,0xff,MB(16));
			goto X;
		}
	}

	//fp=fopen("uu5.rom","wb");
	p=&_memory[MI_USER5];
	for(ofs=0;ofs<16;ofs++){
		for (i = u=0; i < 0x200000;i+=2){
			u32 v;

			v=SL(p[i],8);
			v|=SL(p[i+1],24);
			v|=SL(p[0x200000+i],0);
			v|=SL(p[0x200000+i+1],16);

			*((u32 *)&M_ROM[u]) = v;
			u+=4;
		}
		memcpy(p,M_ROM,u);
		//fwrite(p,u,1,fp);
		p += u;
	}
	//fclose(fp);

	p=&_memory[MI_USER4];
	for(ofs=0;ofs<0x800000*2;ofs+=0x800000,p+=0x800000){
		for (i = 0; i < 0x800000; i +=4){
			u32 v,vv;

			vv=SL(p[i/4],24);
			vv|=SL(p[0x200000+i/4],16);
			vv|=SL(p[0x400000+i/4],8);
			vv|=SL(p[0x600000+i/4],0);

			*((u32 *)&M_ROM[i+ofs]) = vv;
		}
		memcpy(_mem,M_ROM+ofs,i);
		for (i = 0; i < 0x800000; i += 4){
			u32 xormask = _decrypt(i+0x6000000+ofs);
			*((u32 *)&M_ROM[i+ofs]) = *((u32 *)&_mem[i])^xormask;
		}
	}

	//((u32 *)M_BIOS)[0x1fed8/4]^=0x00000001; // clear region to 0 (invalid)
//    ((u32 *)M_BIOS)[0x1fed8/4]^=0x00000008;

//	SH2Cpu::Load(*((u32 *)M_BIOS),*((u32 *)&M_BIOS[4]));
X:
	_pc=*((u32 *)M_BIOS);
	REG_SP=*((u32 *)&M_BIOS[4]);

	//memset(_memory+MI_USER5,0xff,MB(50));
/*
 * cps3_region_address = 0x0001fec8;
	cps3_ncd_address    = 0x0001fecf;
*/
	return 0;
}

int CPS3M::Destroy(){
	SH2Cpu::Destroy();
	CPS3GPU::Destroy();
	CPS3DAC::Destroy();
	CPS3DEV::Destroy();
	Machine::Destroy();
	return 0;
}

int CPS3M::Reset(){
	Machine::Reset();
	CPS3DEV::Reset();
	SH2Cpu::Reset();
	CPS3DAC::Reset();
	CPS3GPU::Reset();
	_ports[0]=_ports[1]=0xffffffff;
	*((u32 *)&M_EXT[0])=_ports[0];
	*((u32 *)&M_EXT[4])=_ports[1];
	return 0;
}

int CPS3M::Init(){
	if(Machine::Init())
		return -1;
	_mem=&_memory[MI_DUMMY];

	_gpu_mem=M_VRAM;
	_gpu_regs=M_PPU;
	_pcm_regs=M_SPU;
	_pal_ram=M_PRAM;

	_tile_ram=_gpu_mem;
	_char_ram=M_CHRAM;

	_ss_regs=M_EXT+KB(5);
	_ss_ram=M_SSRAM;
	CPS3DEV::Init(*this);
	SH2Cpu::Init(0);
	CPS3GPU::Init();
	CPS3DAC::Init(*this);

	AddTimerObj((ICpuTimerObj *)this,_scan_cycles);

	SetIO_cb(0x04FFFE00,(CoreMACallback)&CPS3M::fn_gpu_device_w,(CoreMACallback)&CPS3M::fn_gpu_device_r);
	SetIO_cb(0x05FFFE00,(CoreMACallback)&CPS3M::fn_gfx_device_w,(CoreMACallback)&CPS3M::fn_gfx_device_r);
	SetIO_cb(0x06FFFE00,NULL,(CoreMACallback)&CPS3M::fn_crypted_rom_r);

	SetIO_cb(0xFFFFFE00,(CoreMACallback)&CPS3M::fn_device);

	SetIO_cb(0x24000000,(CoreMACallback)&CPS3M::fn_flash_w,	(CoreMACallback)&CPS3M::fn_flash_r);
	SetIO_cb(0x25000000,(CoreMACallback)&CPS3M::fn_flash_w,(CoreMACallback)&CPS3M::fn_flash_r);
	SetIO_cb(0x26000000,(CoreMACallback)&CPS3M::fn_flash_w,(CoreMACallback)&CPS3M::fn_flash_r);

	return 0;
}

int CPS3M::Exec(u32 status){
	int ret;

	ret=SH2Cpu::Exec(status);
	__cycles=SH2Cpu::_cycles;
	switch(ret){
		case -1:
			return 1;
		case -2:
			return 2;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),_ioreg);
	//419533/264=1589
	MACHINE_ONEXITEXEC(status,0);

}

int CPS3M::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case (u32)ME_ENDFRAME:
			CPS3DAC::Update();
			if(!OnFrame())
				CPS3GPU::Update();
			return 0;
		case (u32)-2:
			return 0;
		case 0:
			if(!_gic._irq_pending)
				return 0;
			ev=log2(_gic._irq_pending);
		break;
		case ME_MOVEWINDOW:
			{
				int x,y,w,h;

				va_start(arg, ev);
				x=va_arg(arg,int);
				y=va_arg(arg,int);
				w=va_arg(arg,int);
				h=va_arg(arg,int);
				CreateBitmap(w,h);
				Draw();
				va_end(arg);
			}
			return 0;
		case ME_REDRAW:
			CPS3GPU::Update();
			Draw();
			return 0;
		case ME_KEYUP:{
				u32 key;

				CALLEE(Machine::OnEvent,ev,key=,arg);
				if(key < 32)
					_ports[0] |= SL(1,key);
				else{
					_ports[1] |= SL(1,key-32);
				}
			}
			return 0;
		case ME_KEYDOWN:{
				u32 key;

				CALLEE(Machine::OnEvent,ev,key=,arg);
				if(key < 32){
					_ports[0] &= ~SL(1,key);
				}
				else{;
					_ports[1] &= ~SL(1,key-32);
				}
			}
			return 0;
		default:
			if(BVT(ev,31))
				CALLEE(Machine::OnEvent,ev,return,arg);
			{
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				if(i == 1){
					BS(_gic._irq_pending,BV(ev));
					return 1;
				}
			}
		break;
	}

	if(ev <= SR((u8)_sr,4)){
	//	EnterDebugMode();
		BS(_gic._irq_pending,BV(ev));
		return 1;
	}
	BC(_gic._irq_pending,BV(ev));
	if(_ppl.delay)
		return 2;
	switch(ev){
		case 5:
		{
			u8 a;

			if(_timer._ic & _timer.ICF)
				a = _gic._vectors[6];
			else if(_timer._ic & (_timer.OCFA|_timer.OCFB))
				a = _gic._vectors[5];
			else
				a = _gic._vectors[7];
			_enterIRQ(_gic._levels[5],a);
		}
		break;
		case 10:
		//if(_line <224)
			//EnterDebugMode();
		case 12:
			_enterIRQ(ev,0x40 + SR(ev,1));
		break;
	}
	return 0;
}

s32 CPS3M::fn_crypted_rom_w(u32 a,pvoid mem,pvoid data,u32 f){
	return 1;
}

s32 CPS3M::fn_crypted_rom_r(u32 a,pvoid mem,pvoid data,u32 f){
	if(_flashs[0]->_invalidate || _flashs[1]->_invalidate){
		u32 i,u;
		u8 *p;

		p=_mem;
		for(u=0;u<0x800000*2;u+=0x800000,p+=0x800000){
			for (i = 0; i < 0x800000; i += 4){
				u32 xormask = _decrypt(i+0x6000000+u);
				*((u32 *)&M_ROM[i+u]) = *((u32 *)&p[i])^xormask;
			}
		}
#ifdef _DEVEOP
		/*FILE *fp;

		if((fp=fopen("u5.rom","wb+"))){
			fwrite(&_memory[MI_USER5],MB(64),1,fp);
			fclose(fp);
		}*/
#endif
		if((p=(u8 *)malloc(MB(8)))){
			u8 *pp=&_memory[MI_USER5];
			for(int n=0;n<9;n++,pp+=MB(4)){
				for(u=i=0;i<KB(512);i++){
					u32 vh,vl,v;

					vh = pp[2+i*8]|SL(pp[i*8],8)|SL(pp[i*8+6],16)|SL(pp[i*8+4],24);
					vl = pp[i*8+3]|SL(pp[i*8+1],8)|SL(pp[i*8+7],16)|SL(pp[i*8+5],24);

					v=SL((u8)vl,8)|SL(vl&0xff00,16)|(u8)vh|(SL(vh&0xff00,8));
					*((u32 *)&p[u+MB(4)]) = v;

					u+=4;
					vh>>=16;
					vl>>=16;

					v=SL((u8)vl,8)|SL(vl&0xff00,16)|(u8)vh|(SL(vh&0xff00,8));
					*((u32 *)&p[u+MB(4)]) = v;
					u+=4;
				}
				memcpy(pp,p+MB(4),MB(4));
			}
			free(p);
		}
#ifdef _DEVELOP
		printf("reay\n");
		p=&_memory[MI_USER5];
		for(int n=3;n<6;n++){
			for(i=0;i<8;i++,p+=MB(2)){
				char c[30];
				FILE *fp;

				sprintf(c,"simm%d.%d.rom",n,i);

				if((fp=fopen(c,"wb+"))){
					fwrite(p,MB(2),1,fp);
					fclose(fp);
				}
			}
		}
#endif
		_flashs[0]->_invalidate=_flashs[1]->_invalidate=0;
	}
	return 1;
}

s32 CPS3M::fn_gpu_device_r(u32 a,pvoid mem,pvoid data,u32 f){//04
//	fprintf(stderr,"fn_gpu_device_r %x\n",a);
	switch(SR(a,20) & 0xf){
		case 0:
		switch((u8)SR(a,16)){
			default:
			//printf("%x %x\n",a,_pc);
			break;
			case 0xe:
			break;
		}
		break;
		case 1:
				return 0;
		break;
		case 2:
		case 3:
		{
			u8 idx = _simm_bank;
			//if(idx){
				idx=SR(_simm_bank,3);
				printf("simm r %x %d %x\n",a,idx,_pc);
				//if(!_flashs[idx]->read(a,*(u32 *)data,f))
					//*((u32 *)data)=_flashs[idx]->_data;
			//}
		}
			return 0;
		break;
	}
	return 1;
}
//61337fc 6133822 06133c20
s32 CPS3M::fn_gpu_device_w(u32 a,pvoid mem,pvoid data,u32 m){//04
	switch(SR(a,20)&0xf){
		case 1:

			return 1;
		case 2:
		case 3:
		{
			u8 idx = _simm_bank;
			//if(idx){
				idx=SR(_simm_bank,3);
				printf("simm w %x %d %x\n",a,idx,_pc);
				//printf("flash w %x %x\n",idx,_pc);
				//_flashs[idx]->write(a,__data);
			//}
			return 0;
		}
		break;
		case 0:
			switch((u8)SR(a,16)){
				case 0xc:{
					u8 r=(u8)a;

					switch(r){
						case 0x0:
						break;
						case 2:
						break;
						case 0xe:
							PPU_REG16(0xe) &= ~6;
						break;
						case 0x82:
						case 0x80://sprite dma
							//_dmas[2]->_value=PPU_REG16(0x80);
							//printf("sprite dma %x %x %x %x\n",a,_dmas[3]._status,PPU_REG16(0x94),PPU_REG16(0x96));
							PPU_REG16(0xe) &= ~1;
						break;
						case 0x84:
						case 0x86:
							//_set_bank(PPU_REG16(0x84));
							//printf("_cram_bank %x\n",_cram_bank);
						break;
						case 0x88:
						case 0x8a:
						{
							u32 a = PPU_REG16(0x8a);
							if(a != _simm_bank){
								_simm_bank=a-2;
								//memcpy(M_SIMM,&_mem[SL(a,20)+MB(16)],MB(2));
								//memset(M_CHRAM,0,MB(2));
								//printf("_simm_bank %x\n",_simm_bank);
							}
						}
						break;
						case 0x98:
						case 0x9a://tile dma
							//_dmas[3]._status=PPU_REG16(0x9a);
							_dmas[3]->_enabled = SR(PPU_REG16(0x9a) & 0x40,6);
							_dmas[3]->_init(_gpu_regs);
							//if(!_dmas[3]->_transfer())
								//BC(PPU_REG16(0x9a),0x40);
							if(!_dmas[3]->_transfer())
								OnEvent(10,1);
							//printf("tile dma %x %x %x %x %x %x\n",a,_dmas[3]->_value,PPU_REG16(0x94),PPU_REG16(0x96),PPU_REG16(0x98),PPU_REG16(0x9a));
							PPU_REG16(0xe) &= ~2;
						break;
						case 0xac:
						case 0xae://palette dma
							_dmas[4]->_enabled = SR(PPU_REG16(0xac) & 2,1);
							_dmas[4]->_init(_gpu_regs);
							//_dmas[4]->_transfer();
							if(!_dmas[4]->_transfer())
								OnEvent(10,1);
							PPU_REG16(0xe) &= ~4;
							_gpu_status |= BV(1);
							EnterDebugMode(DEBUG_BREAK_OPCODE);
							//printf("palette dma %x %x %x %x %x\n",a,PPU_REG16(0xa8),PPU_REG16(0xaa),PPU_REG16(0xac),PPU_REG16(0xae));
						break;
						default:
							if(!(r<0x20 || r > 0x5f)){
							//	for(int i=0;i<sizeof(_layers)/sizeof(struct __layer);i++)
								//	_layers[i].Invalidate();
							}
						break;
					}
				}
				break;
				case 0xe:
					//printf("spu reg %x %x\n",a,_pc);
					CPS3DAC::write_reg(a,*(u32 *)data);
				break;
			}
		break;
	}
	return 1;
}

s32 CPS3M::fn_gfx_device_r(u32 a,pvoid,pvoid data,u32 f){//05
	//printf("fn_gfx_device_r %x D:%x F:%x ",a,*((u32 *)data),f);
	switch(a&0x100000){
		case 0:
			switch(SR(a,16)&0xf){
				case 0:
					switch(SR(a,12)&0xf){
						case 1:
							if(!_eeprom.read(a,*(u32 *)data,f))
								*((u32 *)data)=_eeprom._data;
						break;
						case 0:
							*((u32 *)&M_EXT[0]) = _ports[0];
							*((u32 *)&M_EXT[4]) = _ports[1];
							if((a&0xf00) == 0xa00)
								*((u32 *)data)=0xffff;
							//EnterDebugMode();
						break;
					}
				break;
			}
		break;
		default:
			switch((u8)SR(a,16)){
				case 0x40:
				break;
			}
		break;
	}
	//printf("\n");
	return 1;
}

s32 CPS3M::fn_gfx_device_w(u32 a,pvoid m,pvoid data,u32 f){//05
	//if(a&0x20000000 == 0) return fn_gpu_device_w(a,m,data,f);
//	fprintf(stderr,"%x %x fn_gfx_device_w\n",a,SR(a,20) & 0xf);
	switch(a&0x100000){
		case 0:
			switch(SR(a,16)&0xf){
				case 0:
					switch(SR(a,12)&0xf){
						case 1:
							_eeprom.write(a,*(u32 *)data,f);
						break;
					}
				break;
				case 5:
					//printf("ssr %x %x\n",a,_pc);
				case 4:
					_gpu_status |= 1;
				break;
			}
		break;
		default:
			switch((u8)SR(a,16)){
				case 0:
				break;
				case 4:
				break;
				case 5:
				break;
				case 0x10:
				break;
				case 0x11:
				break;
				case 0x12:
				break;
				case 0x14:
				break;
			}
		break;
	}
	return 1;
}

s32 CPS3M::fn_flash_w(u32 a,pvoid,pvoid data,u32 f){
	u8 idx;//6163752

	//printf(stderr,"fn_flash_w %x %x\n",a,SR(a,24) & 0xf);
	switch((idx = SR(a,24) & 0xf)){
		case 0x4:
			idx=3+(_simm_bank/8);//  (idx-4)+SR(a&0x800000,23);//gfx rom
			a=(a&0x1FFFFF)|SL(_simm_bank&7,21);
		break;
		case 0x5:
			if((a&0xFFFFFc) == 0x140000){
				int i;

				switch(i=_cdrom.write(a,__data,AM_WRITE)){
					case 0:
					break;
					default:
						AddTimerObj(&_cdrom,i);
						break;
				}
				return 1;
			}
			return 0;
		case 0x6:
			idx=SR(a&0x800000,23);
			a &= 0x0fffffff;
		break;
		case 0x7:
			idx=2;
			a &= 0x0fffffff;
		break;
		default:
			return 1;
	}
	_flashs[idx]->write(a,__data,f);
	return 0;
}

s32 CPS3M::fn_flash_r(u32 a,pvoid mem,pvoid data,u32 f){
	u8 idx;

	//printf("%x %x\n",a,_pc);
	switch((idx = SR(a,24) & 0xf)){
		case 0x4:
			idx=3+(_simm_bank/8);//idx=(idx-4) + SR(a&0x800000,23);
			a=(a&0x1FFFFF)|SL(_simm_bank&7,21);
		break;
		case 0x5:
			if((a&0xFFFFFc) == 0x140000){
				if(_cdrom.read(a,__data,AM_READ))
					*((u32 *)data)=_cdrom._data;
				return 1;
			}
			return 0;
		case 0x6:
			idx=SR(a&0x800000,23);
			a &= 0x0fffffff;
		break;
		case 0x7:
			idx=2;
			a &= 0x0fffffff;
		break;
		default:
			return 1;
	}
//	if(!mem) return 1;
	if(!_flashs[idx]->read(a,*(u32 *)data,f)){
		*((u32 *)data)=_flashs[idx]->_data;
		//LOGD("FLASH %x %x %x\n",a,__data,_pc);
	}
	return 0;
}

s32 CPS3M::fn_device(u32 a,pvoid mem,pvoid data,u32){
	u8 idx;

//	fprintf(stderr,"fn_device %x \n",a);
	switch((idx = SR(a&0x1ff,2))){
		case 4:
		case 5:{
			if(!_timer.Init(_ioreg,idx))
				AddTimerObj((ICpuTimerObj *)&_timer,_timer._min_wait);
			else
				DelTimerObj((ICpuTimerObj *)&_timer);
			//LOGI("TIMER %x %x %x ic.%x sc:%x %u\n",a,IOREG(4),IOREG(5),_timer._ic,_timer._sc,_timer._count);
		}
		break;
		case 0x18:
		{
			//_gic.iprb=(u16)*((u16 *)data);
			u16 u=(u16)*((u16 *)data);
			for(int i=0;i<4;i++,u=SR(u,4))
				_gic._levels[7-i]=(u&0xf);
		}
		case 0x19:
		case 0x1a:
		{
			u32 v=IOREG(0x18);
			_gic._vectors[7]=(u8)v;
			_gic._vectors[6]=(u8)SR(v,8);
			v=IOREG(0x19);
			_gic._vectors[5]=(u8)v;
			_gic._vectors[4]=(u8)SR(v,8);
			_gic._vectors[3]=(u8)SR(v,16);
			_gic._vectors[2]=(u8)SR(v,24);
			v=IOREG(0x20);
			_gic._vectors[1]=(u8)v;
			_gic._vectors[0]=(u8)SR(v,8);
		}
		break;
		case 0x38:
		{
			u16 u=(u16)*((u16 *)data);
			//_gic.ipra=(u16)*((u16 *)data);
			for(int i=0;i<4;i++,u=SR(u,4))
				_gic._levels[3-i]=(u&0xf);
		}
		break;
		case 0x41:
		case 0x45://divisor
			LOGI("DIV %x\n",a);
		break;
		case 0x60:
		case 0x61:
		break;
		case 0x63://dma control ch 0
			_do_dma(0);
			//LOGD("DMA %x\n",a);
		break;
		case 0x67://dma control ch 1
			_do_dma(1);
			//LOGD("DMA %x\n",a);
		break;
		case 0x6c://dma master
			//*((u32 *)&_ioreg[0x18c]) |= BV(1);//transfer complete
			//LOGD("DMA %x %x PC:%x\n",a,IOREG(0x6c),_pc);
			_do_dma(0);
			_do_dma(1);
			LOGD("DMA MASTER %x\n",a);
		break;
		case 0x7c://rtcsr
		case 0x7d://rtcnt
		case 0x7e://rtcor
			//IOREG(idx) &= 0XFF;
			LOGD("rt %x\n",a);
		break;
		case 0x20:
			_wdt._status=*((u32 *)&_ioreg[a&0x1fc]);
			LOGD("WDT %x %x %x PC:%x\n",a,idx,*((u32 *)&_ioreg[a&0x1fc]),_pc);
		break;
		default:
			LOGD("%x %x %x PC:%x\n",a,idx,*((u32 *)&_ioreg[a&0x1fc]),_pc);
		break;
	}
	return 0;
}

int CPS3M::_do_dma(int ch){
	u32 src,dst,count,incs,incd;
	void *p,*pp;

	if(BT(DMA_CR(ch),BV(0))==0 || BT(IOREG(0x6c),BV(0))==0)
		return 0;

	src=DMA_SRC(ch);
	dst=DMA_DST(ch);
	count = DMA_COUNT(ch);
	incs=SR(DMA_CR(ch),12)&0x3;
	incd=SR(DMA_CR(ch),14)&0x3;

	LOGD("DMA %d %x S:%x D:%x C:%x M:%d %d\n",ch,DMA_CR(ch),src,dst,count,SR(DMA_CR(ch),10)&3,incd);
	//EnterDebugMode();

	switch(SR(DMA_CR(ch),10)&3){
		case 0:{
			for(;count > 0; count --){
				u32 data;

				if(incs == 2)
					src--;
				if(incd == 2)
					dst--;

				R_(src^3,data,R,L,u8,AM_BYTE|0x80);
				WB_(dst^3,data,W,L);

				if(incs == 1)
					src++;
				if(incd == 1)
					dst++;
			}
		}
		break;
		default:
			EnterDebugMode();
		break;
		case 2:
		{
			src &= ~3;
			dst &= ~3;
			for(;count > 0; count --){
				if(incs == 2)
					src -= 4;
				if(incd == 2)
					dst -= 4;
				u32 data;

				R_((((src))),data,R,L,u32,AM_DWORD|0x80);
				WL_((((dst))),data,W,L);
				if(incs == 1)
					src += 4;
				if(incd == 1)
					dst += 4;
			}
		}
		break;
	}
	BS(DMA_CR(ch),BV(1));
	return 0;
}

int CPS3M::Query(u32 what,void *pv){
	switch(what){
		//case ICORE_QUERY_OID:
			//return CPS3GPU::Query(what,pv);
		case ICORE_QUERY_NEW_WAITABLE_OBJECT:
			switch((u16)*(u32 *)pv){
				case 0xf:
					*((void **)((u32 *)pv + 1)) = &_cdrom;
					return 0;
			}
			return -1;
		case ICORE_QUERY_CURRENT_FRAME:
			*((u64 *)pv)=_frame;
			return 0;
		case ICORE_QUERY_CURRENT_SCANLINE:
			*((u32 *)pv)=__line;
			return 0;
		case IMACHINE_QUERY_MEMORY_ACCESS:
			{
				u32  a = *((u32 *)pv);

			//	printf("adr %x %p\n",a,&_mem[MB(16)]);
				void **p = (void **)(u64 *)((u8 *)pv + 4);
				*p = (void *)&_mem[MB(16)];
			}
			return 0;
		case ICORE_QUERY_SET_LOCATION:{
			if(!pv)
				return -1;
			u32 *p = (u32 *)pv;
			switch((u16)p[3]){
				case 3105:
					SS_REG_(_ss_regs,p[0])=p[1];
					return 0;
				case 3102:{
					FILE *fp=fopen("u5.rom","wb+");
					if(fp){
						fwrite(&_memory[MI_USER5],MB(64),1,fp);;
						fclose(fp);
					}
					//return SH2Cpu::Query(what,pv);
				}
			}
		}
			return -1;
		case ICORE_QUERY_GIC_LEVEL:
		{
			int level = _gic._levels[5];
			printf("GIC ");
			for(int i=0;i<8;i++)
				printf(" %d:%02x-%02x ",i,_gic._vectors[i],_gic._levels[i]);
			printf("\n");
			int vector=_gic._vectors[6];
			printf("TIMER IRQ %d %d %x\n",level,vector,HIWORD(IOREG__(_ioreg,4)));
		}
			return 0;
		case ICORE_QUERY_DBG_PAGE_COORD_TO_ADDRESS:{
				LPDEBUGGERPAGE p;

				if(!pv)
					return -1;
				p=(LPDEBUGGERPAGE)pv;
				switch(p->id){
					case 3105:
						p->message=1;
						p->res=0x22;
						return 0;
				//	default:
					//	return -2;
				}

				FILE *fp=fopen("palette.txt","wb");

				/*for(int i=0;i<0x10000;i++){
					if(!(i&31))
						fprintf(fp,"%04x ",i);
					fprintf(fp,"%02x ",_mem[0x1000000]);
					if((i&31)==31) fprintf(fp,"\n");
				}

				fprintf(fp,"\n\n");
				for(int i=0;i<0x10000 ;i++){
					if(!(i&31))
						fprintf(fp,"%04x ",i);
					fprintf(fp,"%02x ",M_SSRAM[i]);
					if((i&31)==31) fprintf(fp,"\n");
				}
				fprintf(fp,"\n\n");*/

				for(int i=0,ii=0;i<0x40000;i+=2){
					if(!ii)
						fprintf(fp,"%04x ",i);
					fprintf(fp,"%04x ",*((u16 *)&M_PRAM[i]));
					if((ii=(ii+1)&15)==0) fprintf(fp,"\n");
				}
				fclose(fp);
		}
			return -1;
		case ICORE_QUERY_MEMORY_FIND:{
				int i=0;
				u8 *pp,*p=(u8 *)pv;
				char *c = (char *)*((u64 *)p);
				p += sizeof(char *);

				RMAP_(((u32 *)p)[0],pp,R);
#ifdef _DEVELOP
				printf(" find %x %x %s\n",*((u32 *)p),((u32 *)p)[1],c);
#endif
				for(s32 i=0;i<((u32 *)p)[1];i++){
					s32 n;

					for(n=0;c[n];n++,i++){
						if(c[n] != pp[i])
							break;
					}
					if(!c[n]){
						((u32 *)p)[0]+=i-n;
						printf("%x\n",((u32 *)p)[0]);
						return 0;
					}
				}
#ifdef _DEVELOP
				printf(" not find %x %x %s\n",*((u32 *)p),((u32 *)p)[1],c);
#endif
				return 0;
			}

			return -2;
		case ICORE_QUERY_DBG_MENU_SELECTED:
			{
				u32 id =*((u32 *)pv);
printf("%x\n",id);
				switch((u16)id){
					case 7:
						return -1;
					case 6:
						_ss_layer._visible=id&0xFFFF0000?1:0;
						return 0;
					default:
						_layers[(u16)id-1]._visible=id&0xFFFF0000?1:0;
						return 0;
				}
			}
			return -1;
		case ICORE_QUERY_DBG_MENU:
			{
				char *p = new char[1000];
				((void **)pv)[0]=p;
				memset(p,0,1000);
				for(int i=0;i<4;i++){
					sprintf(p,"Layer %d",i);
					p+=strlen(p)+1;
					*((u32 *)p)=1+i;
					*((u32 *)&p[4])=0x102;
					p+=sizeof(u32)*2;
				}
				strcpy(p,"SS Layer");
				p+=strlen(p)+1;
				*((u32 *)p)=6;
				*((u32 *)&p[4])=0x102;
				p+=sizeof(u32)*2;
				strcpy(p,"Sprite Layer");
				p+=strlen(p)+1;
				*((u32 *)p)=7;
				*((u32 *)&p[4])=0x102;
				p+=sizeof(u32)*2;
				*((u64 *)p)=0;
			}
			return 0;
		case ICORE_QUERY_DBG_PAGE:
			{
				LPDEBUGGERPAGE p;

				if(!pv)
					return -1;
				*((LPDEBUGGERPAGE *)pv)=NULL;

				if(!(p = (LPDEBUGGERPAGE)malloc(9*sizeof(DEBUGGERPAGE))))
					return -2;
				*((LPDEBUGGERPAGE *)pv)=p;

				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Registers");
				strcpy(p->name,"3100");
				p->type=1;
				p->popup=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Memory");
				strcpy(p->name,"3102");
				p->type=2;
				p->editable=1;
				p->popup=1;
				p->clickable=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Ext");
				strcpy(p->name,"3103");
				p->type=1;
				p->clickable=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"PPU");
				strcpy(p->name,"3104");
				p->type=1;
				p->clickable=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"SPU");
				strcpy(p->name,"3105");
				p->type=1;
				p->clickable=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"EEPROM");
				strcpy(p->name,"3107");
				p->type=1;
				p->clickable=1;
				//p->popup=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Call Stack");
				strcpy(p->name,"3108");
				p->type=1;
				p->popup=1;

				p++;
				memset(p,0,sizeof(DEBUGGERPAGE));
			}
			return 0;
		case ICORE_QUERY_ADDRESS_INFO:
			{
				u32 adr,*pp,*p = (u32 *)pv;
				adr =*p++;
				pp=(u32 *)*((u64 *)p);
				switch(SR(adr,24)){
					case 0:
						pp[0]=0;
						pp[1]=KB(512);
						break;
					case 2:
						pp[0]=0x02000000;
						pp[1]=KB(512);
					break;
					case 4:
						pp[0]=0x040c0000;
						pp[1]=KB(1);
					break;
					case 6:
						pp[0]=0x06000000;
						pp[1]=MB(16);
					break;
					case 0x24:
						pp[0]=0x24000000;
						pp[1]=MB(16);
						break;
					case 0x25:
						pp[0]=0x25000000;
						pp[1]=MB(64);
						break;
					case 0xc0:
						pp[0]=0xc0000000;
						pp[1]=0x400;
						break;
					default:
						return -2;
				}
			}
			return 0;
		default:
			return SH2Cpu::Query(what,pv);
	}
}

int CPS3M::Dump(char **pr){
	int res,i;
	char *c,*cc,*p,*pp;
	u8 *mem;
	u32 adr;
	DEBUGGERDUMPINFO di;

	_dump(pr,&di);

	if((c = new char[600000])==NULL)
		return -1;

	*((u64 *)c)=0;
	cc = &c[590000];
	pp=&cc[900];
	res = 0;
	p=c;
	strcpy(p,"3100");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	((CCore *)cpu)->_dumpRegisters(p);
#ifdef _DEVELOP
	sprintf(cc,"\nCL:%u\tCPU:%u\tIRQ:%08X\tFP:%u",__line,CCore::_cycles,_gic._irq_pending,__frame);
	strcat(p,cc);
#endif
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3102");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	adr=di._dumpAddress;

	RMAP_L(adr,mem,R);
	_dumpMemory(p,mem,&di);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3103");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	adr=0xFFFFFE00;
	mem=_ioreg;

	for(int n=0,i=0;i<0x200;i+=4,mem+=4){
		*cc=0;
		sprintf(cc,"%3X: %08x ",i/4,*((u32 *)mem));
		strcat(p,cc);
		if(++n == 4){
			n=0;
			strcat(p,"\n");
		}
	}
	sprintf(cc,"\n\nINPUTS: %08X\tEXTRA: %08X\n",*((u32 *)M_EXT),*((u32 *)&M_EXT[4]));
	strcat(p,cc);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3106");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;

	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3108");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	((CCore *)cpu)->_dumpCallstack(p);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3104");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;

	*((u64 *)p)=0;
	mem=M_PPU;
	pp=&cc[900];
	for(int n=0,i=0;i<0x100;i+=2,mem+=2){
		*cc=0;
		sprintf(cc,"%3X: %04x ",i,*((u16 *)mem));
		strcat(p,cc);
		if(++n == 7){
			n=0;
			strcat(p,"\n");
		}
	}
	strcat(p,"\n\n\tSS Registers\n");
	mem=(u8 *)_ss_regs;
	for(int n=0,i=0;i<0x30;i+=2,mem+=2){
		*cc=0;
		sprintf(cc,"%3X: %04x ",i,*((u16 *)mem));
		strcat(p,cc);
		if(++n == 7){
			n=0;
			strcat(p,"\n");
		}
	}
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3105");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	mem=M_SPU;
	pp=&cc[900];
	for(int n=0,i=0;i<0x203;i+=4,mem+=4){
		*cc=0;
		sprintf(cc,"%3X: %08x ",i,*((u32 *)mem));
		strcat(p,cc);
		if(++n == 4){
			n=0;
			strcat(p,"\n");
			if((i&0x1f) == 0x1c)
				strcat(p,"\n");
		}
	}
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3107");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	mem=(u8 *)M_EEPROM;
	pp=&cc[900];
	for(int n=0,i=0;i<0x100;i+=2,mem+=2){
		*cc=0;
		sprintf(cc,"%3X: %04x ",i,*((u16 *)mem));
		strcat(p,cc);
		if(++n == 7){
			n=0;
			strcat(p,"\n");
		}
	}
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	*pr = c;
	return res;
}

int CPS3M::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]="384";
	m["height"]="224";
	CPS3DAC::LoadSettings(v);
	CPS3GPU::LoadSettings(v);
	return 0;
}

int CPS3M::SaveState(IStreamer *p){
	if(Machine::SaveState(p))
		return -1;
	if(SH2Cpu::SaveState(p))
		return -2;

	return 0;
}

int CPS3M::LoadState(IStreamer *p){
	if(Machine::LoadState(p))
		return -1;
	if(SH2Cpu::LoadState(p))
		return -2;

	return 0;
}

};