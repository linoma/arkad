#include "gbam.h"
#include "gui.h"

extern GUI gui;

namespace gba{

#define SET_DP_HANDLE(base,ins){\
	for(int i=0;i<0x10;i += 2){\
		_opcodescb[base|i] = (CoreDecode)ins;\
		_opcodescb[base|0x200|i]  = _opcodescb[base|0x201|i]  = (CoreDecode)ins##_imm;\
		_disopcodescb[base|i] = _disopcodescb[base|0x200|i]  = _disopcodescb[base|0x201|i]  = (CoreDisassebler)ins##_dis;\
	}\
	for(int i=1;i<8;i += 2){\
		_opcodescb[base|i] = (CoreDecode)ins##_reg;\
		_disopcodescb[base|i]=(CoreDisassebler)ins##_dis;\
	}\
}

#define SET_HWDT_HANDLE(base,h,pfix){\
	_opcodescb[base]=(CoreDecode)h##_postdown##pfix;\
	_opcodescb[base|0x20]=(CoreDecode)h##_postdown##pfix;\
	_opcodescb[base|0x80]=(CoreDecode)h##_postup##pfix;\
	_opcodescb[base|0xA0]=(CoreDecode)h##_postup##pfix;\
	_opcodescb[base|0x100]=(CoreDecode)h##_predown##pfix;\
	_opcodescb[base|0x120]=(CoreDecode)h##_predown##pfix##wb;\
	_opcodescb[base|0x180]=(CoreDecode)h##_preup##pfix;\
	_opcodescb[base|0x1A0]=(CoreDecode)h##_preup##pfix##wb;\
	_disopcodescb[base]=_disopcodescb[base|0x20]=(CoreDisassebler)h##_dis;\
	_disopcodescb[base|0x80]=_disopcodescb[base|0xA0]=(CoreDisassebler)h##_dis;\
	_disopcodescb[base|0x100]=_disopcodescb[base|0x120]=(CoreDisassebler)h##_dis;\
	_disopcodescb[base|0x180]=_disopcodescb[base|0x1A0]=(CoreDisassebler)h##_dis;\
}

//ldr_postdownimm,ldr_postdownimm,ldr_postupimm,ldr_postupimm,ldr_predownimm,ldr_preupimm,ldr_predownimmwb,ldr_preupimmwb
// h, ht, h2, h2t, h3, h4, h3wb, h4wb
//h ht h2 h2t h3 h3wb h4 h4wb
#define SET_SDT_HANDLE(base,h,pfix)\
	for(int i=0;i<0x10;i++){\
		_opcodescb[base|i]=_opcodescb[base|0x20|i]=(CoreDecode)h##_postdown##pfix;\
		_opcodescb[base|0x80|i] = _opcodescb[base|0xA0|i]=(CoreDecode)h##_postup##pfix;\
		_opcodescb[base|0x100|i] = (CoreDecode)h##_predown##pfix;\
		_opcodescb[base|0x120|i] = (CoreDecode)h##_predown##pfix##wb;\
		_opcodescb[base|0x180|i] = (CoreDecode)h##_preup##pfix;\
		_opcodescb[base|0x1A0|i] = (CoreDecode)h##_preup##pfix##wb;\
		_disopcodescb[base|i]=_disopcodescb[base|0x20|i]=(CoreDisassebler)h##_dis;\
		_disopcodescb[base|0x80|i] = _disopcodescb[base|0xA0|i]=(CoreDisassebler)h##_dis;\
		_disopcodescb[base|0x100|i]=_disopcodescb[base|0x120|i]=_disopcodescb[base|0x180|i]=_disopcodescb[base|0x1a0|i]=(CoreDisassebler)h##_dis;\
	}

#define __ARMS(a,b,...)	sprintf(cc,"%s" "%s " STR(a),b,condition_strings[_opcode>>28],## __VA_ARGS__)
//#define __ARMF(a,b,...) printf(STR(%08X %08X %s%s\x20) STR(a) STR(\n),_pc,_opcode,b,condition_strings[_opcode>>28],## __VA_ARGS__);
#define __ARMF(a) if((gui._getStatus() & (S_PAUSE/*|S_DEBUG_NEXT*/)) == S_PAUSE){\
		char cc__[200];cc__[0]=0;_##a##_dis(cc__);printf("%08x %08x %s\n",_pc,_opcode,cc__);}

#define SET_ALU_FLAGS(r,a,b,ins){\
	u32 a__,r__,b__;\
	u8 n__,z__,c__=SR(_cpsr,C_SHIFT)&1,o__;\
	a__=(a);b__=(b);_cpsr &= ~(C_BIT|N_BIT|Z_BIT|V_BIT);\
	__asm__ __volatile__(\
		"mov %1,%%eax\n"\
		"mov %2,%%ecx\n"\
		ins \
		"setsb %3\n"\
		"setzb %4\n"\
		"setob %6\n"\
		"mov %%eax,%0\n"\
		: "=m"(r__) : "m"(a__), "m"(b__), "m"(n__), "m"(z__), "m"(c__), "m"(o__):"cc","eax","ecx","edx"\
	);\
	if(n__) _cpsr|=N_BIT;\
	if(z__) _cpsr|=Z_BIT;\
	if(c__) _cpsr|=C_BIT;\
	if(o__) _cpsr|=V_BIT;\
	(r)=r__;\
}


#define SET_SUB_FLAGS(r,a,b) SET_ALU_FLAGS(r,a,b,"subl %%ecx,%%eax\n setncb %5\n")
#define SET_ADD_FLAGS(r,a,b) SET_ALU_FLAGS(r,a,b,"addl %%ecx,%%eax\n setcb %5\n")
#define SET_SUBC_FLAGS(r,a,b) SET_ALU_FLAGS(r,a,b,"movzbl %5,%%edx\n xorl $1,%%edx\n btw $0,%%dx\n sbbl %%ecx,%%eax\n setncb %5\n")
#define SET_ADDC_FLAGS(r,a,b) SET_ALU_FLAGS(r,a,b,"btw $0,%5\n adcl %%ecx,%%eax\n setcb %5\n")

#define SET_DP_FLAGS(a){_cpsr &= ~(N_BIT|Z_BIT);u32 a__=(a);if(!a__) _cpsr |= Z_BIT; else if(a__&0x80000000) _cpsr |= N_BIT;}

#define DP_IMM_OPERAND(a){\
	u32 a__; __asm__ __volatile__ (\
		"mov %1,%%cx\n"	"movzbl %1,%%eax\n"	"and $0xf00,%%cx\n"	"shr $0x7,%%cx\n" "ror %%cl,%%eax\n" "mov %%eax,%0\n"\
		: "=m"(a__) : "m"(_opcode) : "eax","ecx","cc"); (a)=a__;}

#define DPS_IMM_OPERAND(a){\
	u32 a__;u8 c__; __asm__ __volatile__ (\
		"mov %1,%%cx\n"	"movzbl %1,%%eax\n"	"and $0xf00,%%cx\n"	"shr $0x7,%%cx\n" "ror %%cl,%%eax\n" "setcb %2\n" "mov %%eax,%0\n"\
		: "=m"(a__) : "m"(_opcode),"m"(c__) : "eax","ecx","cc");if(c__) _cpsr|=C_BIT; else _cpsr &= ~C_BIT; (a)=a__;\
}

#define DP_REG_OPERAND(a,b){\
	u8 shift;\
	switch((_opcode & 0x60)){\
		case 0:	(a) = OP_REG << b;break;\
		case 0x20:if((shift = (u8)b) == 0) (a)=0;else (a)=OP_REG >> shift;break;\
		case 0x40:if((shift = (u8)b) == 0){	if((OP_REG & 0x80000000)) (a)= 0xFFFFFFFF; else	(a)=0;}	else (a)=(s32)OP_REG >> shift;break;\
		case 0x60:if((shift = (u8)b) == 0)(a)= ((OP_REG >> 1)|SL(_cpsr&C_BIT,2)); else(a)=((OP_REG << (32-shift))|(OP_REG>>shift));	break;\
	}\
}

#define DPS_REG_OPERAND(a,b){\
	u32 res;u8 shift=b;\
	res = OP_REG_INDEX;	res = res==15 ? _pc+4 : REG_(res);\
	switch(_opcode & 0x60){\
		case 0:\
			if(!shift) goto CONCAT(A,__LINE__);\
			else if(shift < 32){res <<= (shift-1);if(res & 0x80000000)	_cpsr |= C_BIT; else _cpsr &= ~C_BIT;res <<=1;}\
			else if(shift == 32){if(res & 1) _cpsr |= C_BIT; else _cpsr &= ~C_BIT;res = 0;}\
			else{res = 0;_cpsr &= ~C_BIT;}\
		break;\
		case 0x20:\
			if(!shift){if(!(_opcode & 0x10)){if(res & 0x80000000)	_cpsr |= C_BIT; else _cpsr &= ~C_BIT;res = 0;}}\
			else if(shift < 32){ res >>= shift-1;if(res & 1) _cpsr |= C_BIT; else _cpsr &= ~C_BIT;res>>=1;}\
			else if(shift == 32){if(res & 0x80000000) _cpsr |= C_BIT; else _cpsr &= ~C_BIT;res = 0;}\
			else{res = 0;_cpsr &= ~C_BIT;}\
		break;\
		case 0x40:\
			if(!shift){if(!(_opcode & 0x10)){  if(res & 0x80000000){res = 0xFFFFFFFF;_cpsr |= C_BIT;} else{res = 0;_cpsr &= ~C_BIT;}}}\
			else if(shift < 32){res = (s32)res >> (shift-1);if(res & 1)	_cpsr |= C_BIT; else _cpsr &= ~C_BIT;res =(s32)res>>1;}\
			else{if((res & 0x80000000)){res = 0xFFFFFFFF;_cpsr |= C_BIT;} else{res = 0;_cpsr &= ~C_BIT;}}\
		break;\
		case 0x60:\
			if(!shift){if(!(_opcode & 0x10)){shift = (u8)(res & 1);res >>= 1;if(_cpsr & C_BIT) res |= 0x80000000;if(shift) _cpsr |= C_BIT; else _cpsr &= ~C_BIT;}}\
			else if(shift == 32){if(res & 0x80000000) _cpsr |= C_BIT; else _cpsr &= ~C_BIT;}\
			else{shift &= 0x1F; u32 a__=res;res>>=shift-1;if(res & 1) _cpsr |= C_BIT; else _cpsr &= ~C_BIT;res=(res>>1)|(res<<(32-shift));}\
		break;\
  	}\
CONCAT(A,__LINE__):	(a)=res;}

#define ARMSMDT_(a){\
	u8 s[20],*p;strcpy(cc,a);\
	if((_opcode & 0x800000)){if((_opcode & 0x1000000)) strcat(cc,"ib"); else strcat(cc,"ia");}\
	else{if((_opcode & 0x1000000)) strcat(cc,"db"); else strcat(cc,"da");}\
	strcat(cc,condition_strings[(_opcode>>28)]);strcat(cc," ");strcat(cc,register_strings[(_opcode>>16)&0xF]);\
	p = s;for(int i=0;i<16;i++){if((_opcode & (1 << i))) *p++ = (u8)i;}\
	*p = 0xFF;if((_opcode & 0x200000)) strcat(cc,"!");\
	strcat(cc,",");_fillMultipleRegisterString(s,cc);\
	if((_opcode & 0x400000)) strcat(cc,"^");\
}

#define ARMMSR_(a){\
	__ARM_COND_PRE(a);if(!(_opcode & (1 << 25)) && !(_opcode & (1 << 21))){\
       strcat(cc,register_strings[(_opcode >> 12) & 0xF]);if((_opcode & (1 << 22))) strcat(cc,",spsr"); else strcat(cc,",cpsr");\
	} else{ char s__[30]; if((_opcode & (1 << 22))) strcat(cc,"spsr"); else strcat(cc,"cpsr");\
		sprintf(s__,"_%s,%s",msr_fields[(_opcode >> 16) & 0xF],register_strings[_opcode & 0xF]);strcat(cc,s__);} }

#define ARMHSDT(a){\
	sprintf(cc,a "%s %s, [%s",condition_strings[SR(_opcode,28)], register_strings[DEST_REG_INDEX],register_strings[BASE_REG_INDEX]);\
	if((_opcode & 0x400000)){\
		int i;if((i = (((_opcode&0xF00)>>4)|(_opcode&0xF))) != 0){\
			char s__[20]; sprintf(s__,"0x%X",i);if((_opcode & 0x800000) != 0) strcat(cc,"+"); else strcat(cc,"-");strcat(cc,s__);}\
	} else{ if((_opcode & 0x800000) != 0) strcat(cc,"+"); else strcat(cc,"-"); strcat(cc,register_strings[_opcode&0xF]);} strcat(cc,"}");}

static u8 _key_config[]={6,7,5,4,1,2,3,8,9,0};

gbam::gbam() : Machine(MB(40)),gbabios(){
	_keys=_key_config;
}

gbam::~gbam(){
}

int gbam::Load(IGame *pg,char *fn){
	int  res;
	u32 sz;

	if(!pg && !fn)
		return -1;
	if(!pg){
		pg=new FileGame();
		pg->AddFile((char*)GameManager::getFilename(fn).c_str(),1);
	}
	if(!pg || pg->Open(fn,0))
		return -1;
	Reset();
	pg->Read(M_ROM,MS_ROM,&sz);
	printf("load %u\n",sz);
	_pc=0x8000000;
	delete (FileGame *)pg;
	Query(ICORE_QUERY_SET_FILENAME,fn);
	res=0;
	return res;
}

int gbam::Destroy(){
	Machine::Destroy();
	gbabios::Destroy();
	return 0;
}

static u32 lino=0;
#define REG_BANK(a) ((u32 *)_pregs + ((a)*17))

int gbam::Reset(){
	Machine::Reset();
	gbabios::Reset();

	REG_BANK(0)[13] = 0x380FEC0;
	REG_BANK(1)[13] = 0x380FFA0;
	REG_BANK(2)[13] = 0x380FFC0;
	lino=0;
	return 0;
}

int gbam::Init(){
	if(Machine::Init()) return -1;
	if(gbadev::Init(_memory,&_memory[MI_IO]))
		return -3;
	arm7::_ioreg=(u32 *)&_memory[MI_IO];
	_gpu_mem=&_mem[MI_VRAM];
	_gpu_regs=_ioreg;
	_pal_ram=&_mem[MI_PRAM];
	_sprite_ram=&_mem[MI_ORAM];
	_char_ram=_gpu_mem;
	_tile_ram=_gpu_mem;
	if(gbagpu::Init(arm7::_mem,arm7::_ioreg))
		return -4;
	if(gbaspu::Init(arm7::_mem,arm7::_ioreg))
		return -5;
	AddTimerObj(this,1232,this);//
	for(int i=0xB0;i<0xE0;i++)
		SetIO_cb(0x4000000|i,(CoreMACallback)&gbam::fn_write_dma);
	for(int i=0x100;i<0x10F;i++)
		SetIO_cb(0x4000000|i,(CoreMACallback)&gbam::fn_timerw,(CoreMACallback)&gbam::fn_timerr);
	for(int i=0;i<0x56;i++)
		SetIO_cb(0x4000000|i,(CoreMACallback)&gbam::fn_write_gpu);
	for(int i=0x60;i<0xa9;i++)
		SetIO_cb(0x4000000|i,(CoreMACallback)&gbam::fn_spuw);
	for(int i=0;i<KB(128);i++){
		SetIO_cb(0xE000000|i,(CoreMACallback)&gbam::fn_iow,(CoreMACallback)&gbam::fn_ior);
		SetIO_cb(0xA000000|i,(CoreMACallback)&gbam::fn_iow,(CoreMACallback)&gbam::fn_ior);
		SetIO_cb(0xB000000|i,(CoreMACallback)&gbam::fn_iow,(CoreMACallback)&gbam::fn_ior);
		SetIO_cb(0xC000000|i,(CoreMACallback)&gbam::fn_iow,(CoreMACallback)&gbam::fn_ior);
		SetIO_cb(0xD000000|i,(CoreMACallback)&gbam::fn_iow,(CoreMACallback)&gbam::fn_ior);
	}
	for(int i=0;i<KB(1);i++){
		//SetIO_cb(0x5000000|i,(CoreMACallback)&gbam::fn_iow);
		SetIO_cb(0x7000000|i,(CoreMACallback)&gbam::fn_iow);
	}
	for(int i=0;i<4;i++)
		SetIO_cb(0x4000200|i,(CoreMACallback)&gbam::fn_iow);
	SetIO_cb(0x4000301,(CoreMACallback)&gbam::fn_iow);
	return 0;
}

int gbam::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case ME_ENDFRAME:
	//		IOREG(REG_KEYINPUT) ^=1;
			gbagpu::Update(Machine::_status & MS_DRAW_FRAME !=0);
			OnFrame();
			gbaspu::Update();
			return 0;
		case ME_MOVEWINDOW:
			{
				int w,h;

				va_start(arg, ev);
				w=va_arg(arg,int);
				w=va_arg(arg,int);
				w=va_arg(arg,int);
				h=va_arg(arg,int);
				CreateBitmap(w,h);
				Draw();
				va_end(arg);
			}
			return 0;
		case ME_REDRAW:
			_startDrawFrame();
			for(__line=0;__line<161;__line++)
				RenderLine();
			gbagpu::Update();
			Draw();
			CALLEE(Machine::OnEventI,ev,return,arg);
			return 0;
		case ME_KEYUP:{
				int key;

				CALLEE(Machine::OnEventI,ev,key=,arg);
				IOREG(REG_KEYINPUT) |= BV(key);
			}
			return 0;
		case ME_KEYDOWN:{
			int key;

				CALLEE(Machine::OnEventI,ev,key=,arg);
				IOREG(REG_KEYINPUT) &= ~BV(key);
			}
			return 0;
		case 0:{//pending irq bit
			va_start(arg, ev);
			int i=va_arg(arg,int);
			va_end(arg);
			switch(i){
				case 0:
					ev=1;
					printf("irq 1\n");
				break;
				default:
				if(!_irq_pending) return -1;
				{
					u32 n;
					u16 v;

					n=_log2(_irq_pending);
					ev=BV(n);
				}
				break;
			}
		}
		break;
		default:
			if(BVT(ev,31)) return -1;
			{
				Resume();
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				if(i == 1){
					BS(_irq_pending,ev);
					return 1;
				}
			}
		break;
	}
	BC(_irq_pending,ev);
	switch(ev){
		case 2:
		case 1:
		case 4:
		case 0x100:
		case 0x200:
		case 0x400:
		case 0x800:
			if(gbabios::_enterIRQ(ev)){
				BS(_irq_pending,ev);
				return 1;
			}
			break;
	}
	return 0;
}

int gbam::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_EMU_MENU:{
			char *s,*p;

			if(!(s=new char[500])) return -2;
			p=s;
			for(int i=0;i<sizeof(SRAMID)/sizeof(__sramid);i++){
				char c[10];

				if(!SRAMID[i].ID) continue;
				sprintf(c,"%x",SRAMID[i].ID);
				strcpy(p,c);
				p+=strlen(c)+1;
				*(u32 *)p=SRAMID[i].ID;
				p+=sizeof(u32);
			}
			*(u32 *)p=0;
			*(char **)pv=s;
			return 0;
		}
		case ICORE_QUERY_DBG_OAM:{
			int res;

			res=-3;
			int idx = (int)**((int **)pv);
			if(idx < 0|| idx > 127)
				return 4;

			__oam *o=&__oams[idx];
			u32 *p=(u32 *)calloc(1,o->_width*o->_height*sizeof(u16)+10*sizeof(u32));

			if(p){
				u32 params[10];
				params[0]=idx;
				*((void **)pv)=p;

				//o->Init(0,*this);

				p[0]= MAKELONG(o->_width,o->_height);
				p[1]=10*sizeof(u32);
				p[2]=!o->_enabled ? 0 : 2|4|8;//flags
				p[3]=MAKELONG(o->_x,o->_y);//position
				p[4]=o->_palette ? 16 : 256;
				p[5]=o->_tileno;
				//o->_drawToMemory((u16 *)&p[10],params,*this);
				res=0;
			}
			//delete o;
			return res;
		}
		case ICORE_QUERY_DBG_PALETTE:{
				u32 *data,*p= (u32 *)calloc(sizeof(u32),0x400);
				data=p;
				*((void **)pv)=data;
				*data++=0x10;
				*data++=0x200;
				for(int i=0;i<0x200;i++){
					u16 col =((u16 *)&_mem[MI_PRAM])[i];
					u32 c = SL(col&31,19)|SL(col&0x3c0,6)|SR(col&0x7c00,7);
					p[i+0x10]=c;
				}
				return 0;
			}
			return -1;
		case ICORE_QUERY_DBG_MENU_SELECTED:
			{
				u32 id =*((u32 *)pv);
				printf("id %x\n",id);
				switch((u16)id){
					case 7:
						__oam::_data[__oam::_visible]=id&0xFFFF0000 ? 1:0;
						return 0;
					case 1:
					case 2:
					case 3:
					case 4:
						__layers[(u16)id-1]._visible=id&0xFFFF0000?1:0;
						_buildLayersList();
						return 0;
					case 10:
					case 11:
					break;
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
				strcpy(p,"OAM");
				p+=strlen(p)+1;
				*((u32 *)p)=7;
				*((u32 *)&p[4])=0x102;
				p+=sizeof(u32)*2;

				strcpy(p,"ARM");
				p+=strlen(p)+1;
				*((u32 *)p)=10;
				*((u32 *)&p[4])=0x103;

				p+=sizeof(u32)*2;
				strcpy(p,"THUMB");
				p+=strlen(p)+1;
				*((u32 *)p)=11;
				*((u32 *)&p[4])=0x3;

				p+=sizeof(u32)*2;
				*((u64 *)p)=0;
			}
			return 0;
		case IMACHINE_QUERY_DEBUG_NOTIFY:
			Machine::_status |= MS_NOTIFY_DEBUG;
			return 0;
		case ICORE_QUERY_CPUS:{
			((void **)pv)[0]=0;
			char *p = new char[500];
			if(!p)
				return -1;
			((void **)pv)[0]=p;
			memset(p,0,100);
			strcpy(p,"CPU");
		}
			return 0;
		case ICORE_QUERY_DBG_PAGE:{
			LPDEBUGGERPAGE p;

			if(!pv)
				return -1;
			*((LPDEBUGGERPAGE *)pv)=NULL;
			if(!(p = (LPDEBUGGERPAGE)malloc(9*sizeof(DEBUGGERPAGE))))
				return -2;
			*((LPDEBUGGERPAGE *)pv)=p;
			memset(p,0,9*sizeof(DEBUGGERPAGE));
			p->size=sizeof(DEBUGGERPAGE);
			strcpy(p->title,"IO");
			strcpy(p->name,"3103");
			p->type=1;
			p->clickable=1;

		}
		return 0;
		case ICORE_QUERY_ADDRESS_INFO:{
			LPMEMORYACCESS d =(LPMEMORYACCESS)pv;
			u32 adr=d->addr;
			switch(SR(adr,24)){
				case 8:
				case 9:
					d->addr=adr&~(MS_ROM-1);
					d->size=MS_ROM;
					break;
				case 3:
					d->addr=adr&~(MS_WRAM-1);
					d->size=MS_WRAM;
				break;
				case 2:
					d->addr=adr&~(MS_RAM-1);
					d->size=MS_RAM;
				break;
				case 4:
					d->addr=adr&~(MS_IO-1);
					d->size=MS_IO;
				break;
				case 5:
					d->addr=adr&~(MS_PRAM-1);
					d->size=MS_PRAM;
				break;
				case 6:
					d->addr=adr & ~(KB(128)-1);
					d->size=MS_VRAM;
				break;
				case 7:
					d->addr=adr&~(MS_ORAM-1);
					d->size=MS_ORAM;
				break;
				case 0:
					d->addr=adr & ~(MS_BIOS-1);
					d->size=MS_BIOS;
				break;
				case 0xd:
					d->addr=adr & ~0x3fff;
					d->size=0x4000;
				break;
				default:
					return -2;
			}
		}
		return 0;
		default:
			return arm7::Query(what,pv);
	}
	return -1;
}

int gbam::Exec(u32 status){
	int ret;

	ret=arm7::Exec(status);
	//if(_opcode ==0 || (__frame > 10000 && *((u32 *)&_mem[MI_WRAM+0x3cd0]) == 0) ) EnterDebugMode();
//	if(__frame==13961 && __line>=161 && !lino){lino=1; EnterDebugMode();}
	__cycles=arm7::_cycles;
	switch(ret){
		case -1:
		case -2:
			return -ret;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int gbam::Dump(char **pr){
	int res;
	char *c,*cc,*p;
	u8 *mem;
	u32 adr;
	DEBUGGERDUMPINFO di;

	_dump(pr,&di);
	if((c = new char[600000])==NULL)
		return -1;

	*((u64 *)c)=0;
	cc = &c[590000];
	*((u64 *)cc)=0;

	res = 0;
	p=c;
	strcpy(p,"3100");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;

	((CCore *)cpu)->_dumpRegisters(p);

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

	if(SR(adr,24) == 0xd){
		mem=_eeprom[1].rom_pack[0]._buffer;
		printf("dump %x %p\n",adr,mem);
	}
	else
		RMAP_(adr,mem,R);
	((CCore *)cpu)->_dumpMemory(p,mem,&di);

	res += strlen(p)+1;

	p= &c[res];
	strcpy(p,"3106");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	((CCore *)cpu)->_dumpCallstack(p);
	res += strlen(p)+1;

	p= &c[res];
	strcpy(p,"3103");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;

	sprintf(p,"DISPCNT %04X DISPSTAT %04X VCOUNT %04X\n\n",IOREG(REG_DISPCNT),IOREG(REG_DISPSTAT),IOREG(REG_VCOUNT));
	for(int i=0;i<4;i++){
		sprintf(cc,"BG%dNT %04X\t",i,IOREG(REG_BGCNT(i)));
		strcat(p,cc);
	}
#ifdef _DEVELOP
	gbagpu::Dump(&p);
#endif
	sprintf(cc,"\n\nMOSAIC %04X BLENDCNT %04X WININ %04X WINOUT %04X V %04X Y %04X",IOREG(REG_MOSAIC),IOREG(REG_BLENDCNT),IOREG(REG_WININ),
		IOREG(REG_WINOUT),IOREG(REG_BLENDV),IOREG(REG_BLENDY));
	strcat(p,cc);
	sprintf(cc,"\n\nIME %04X IE %04X IF %04X\n\n",IOREG(REG_IME),IOREG(REG_IE),IOREG(REG_IF));
	strcat(p,cc);
	for(int i=0;i<sizeof(_dmas)/sizeof(__dma);i++){
		sprintf(cc,"%d DMASAD %08X DMADAD %08X DMACNT %04X DMACOUNT %04X\n",i,IOREG32_(_ioreg,REG_DMASRC(i)),
			IOREG32_(_ioreg,REG_DMADST(i)),IOREG(REG_DMACNT(i)+2),IOREG(REG_DMACNT(i)));
		strcat(p,cc);
	}
	strcat(p,"\n\n");
	for(int i=0;i<sizeof(_timers)/sizeof(__timer);i++){
		sprintf(cc,"%d TIMER %04X TIMERCNT %04X\n",i,IOREG(REG_TIMERD(i)),IOREG(REG_TIMERCNT(i)));
		strcat(p,cc);
	}
	res += strlen(p)+1;
	if(Machine::_status & MS_NOTIFY_DEBUG){
		p= &c[res];
		strcpy(p,"INVALIDATE");
		p+=11;
		*((u32 *)p)=0;
		p+=4;
		res+=15;
		Machine::_status &= ~MS_NOTIFY_DEBUG;
	}

	p= &c[res];
	*((u64 *)p)=0;

	*pr = c;
	return res;
}

int gbam::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	return 0;
}

int gbam::OnException(u32 code,u32 num){
	switch(code){
		case 2:{
			switch(num){
				case 0xefefe:
					biosCall(REG_(12));
				break;
				default:
					if(num != 2)
						DLOG("BIOS swi %x %08X %08X %08X",num,REG_(0),REG_(1),REG_(2));
					switchmode(SUPERVISOR_MODE,1);
					REG_(14) = _pc;
					_pc = 0x8;
					if(!(_cpsr & T_BIT))
						_pc -= 4;
					else{
						REG_(14) -= 2;
						_pc-=2;
					}
					_cpsr &= ~T_BIT;
					_cpsr |= IRQ_BIT|FIQ_BIT;
					//EnterDebugMode();
					//return biosCall((u8)_opcode);
					return 0;
			}
		}
	}
	return -1;
}

int gbam::Run(u8 *,int cyc,void *obj){
	gbadev::Update(cyc);
	int res = gbagpu::Run(0,cyc,obj);
	if(__line < 160){
		if((res & 2))
			dma_do(2);
		//if(Machine::_status & MS_DRAW_FRAME){

		//}
	}
	else if(__line==160)
		dma_do(1);
	return res&1;
}

s32 gbam::fn_spuw(u32 a,void *,void *pdata,u32 am){
	switch((u8)a){
		case 0xa0:
		case 0xa4:{
			u8 *p=(u8 *)pdata;
			for(int i=0;i<4;i++)
				_pcms[SR(a&15,2)].write(*p++);
		}
			return 0;
		case REG_SOUNDCNT_H:{
			u16 v = *(u16 *)pdata;
			for(int i=0;i<sizeof(_pcms)/sizeof(__pcm);i++){
				int t=_pcms[i]._timer;
				_pcms[i]._freq= _timers[t].freq();
				_pcms[i]._changed=1;
			//	printf("fifo %d freq %d %p %u\n",i,_pcms[i]._freq,_pcms[i]._samples,_pcms[i]._size);
			}
		}
		default:
			return gbaspu::write(a,*(u16 *)pdata);
	}
}

s32 gbam::fn_ior(u32 a,void *,void *pdata,u32 am){
	switch(SR(a,24)){
		case 0xa:
		case 0xb:
			if(am & AM_WORD)
				return _eeprom[0].read(a,(u16 *)pdata);
		break;
		case 0xc:
		case 0xd:
			if(am & AM_WORD)
				return _eeprom[1].read(a,(u16 *)pdata);
		break;
		case 0xe:
			if(am & AM_BYTE)
				return _sram.read(a,(u8 *)pdata);
		break;
	}
	return 1;
}

s32 gbam::fn_iow(u32 a,void *,void *pdata,u32 am){
	switch(SR(a,24)){
		case 4:
			switch(a&0x3ff){
				case 0x200:
					if(am & AM_DWORD){
						u32 v=*(u32 *)pdata;
						IOREG(REG_IE) = (u16)v;
						v>>=16;
						IOREG(REG_IF) &= ~v;
						_irq_pending &= ~v;
						return 0;
					}
					return 1;
				case 0x202:
					IOREG(REG_IF) &= ~*(u16 *)pdata;
					_irq_pending &= ~*(u16 *)pdata;
					return 0;
				break;
				case 0x301:
					Sleep();
				break;
			}
			break;
		case 5:
		case 7:
			return gbagpu::write(a,*(u16 *)pdata);
		case 0xa:
		case 0xb:
			if(am & AM_WORD)
				return _eeprom[0].write(a,*(u16 *)pdata);
			return 0;
		break;
		case 0xc:
		case 0xd:
			if(am & AM_WORD)
				return _eeprom[1].write(a,*(u16 *)pdata);
			return 0;
		break;
		case 0xe:
			if(am & AM_BYTE) return _sram.write(a,*(u8 *)pdata);
		break;
	}
	return 1;
}

s32 gbam::fn_write_gpu(u32 a,void *,void *pdata,u32 am){
	if(am & AM_DWORD)
		gbagpu::write(a|2,SR(*(u32 *)pdata,16));
	return gbagpu::write(a,*(u16 *)pdata);
}

s32 gbam::fn_timerw(u32 a,void *,void *pdata,u32){
	return _timers[((a&0x1ff) - 0x100) / 4].write(a,*(u32 *)pdata);
}

s32 gbam::fn_timerr(u32 a,void *,void *pdata,u32){
	return _timers[((a&0x1ff) - 0x100) / 4].read(a,(u32 *)pdata);
}

s32 gbam::fn_write_dma(u32 a,void *pmem,void *pdata,u32 f){
	int i;

	a=(a & 0xfff) - 0xB0;
	_dmas[a / 12].write(a,*(u32 *)pdata);
	i=a%12;
	if(f & AM_DWORD && i==8){
		*(u32 *)pmem = *(u32 *)pdata;
		dma_do((u32)0);
		return 0;
	}
	else if(f & AM_WORD && i==10){
		*(u16 *)pmem = *(u16 *)pdata;
		dma_do((u32)0);
		return 0;
	}
	return 1;
}

static char condition_strings[][3] 	= {"eq","ne","cs","cc","mi","pl","vs","vc","hi","ls","ge","lt","gt","le","",""};
static char register_strings[][4]  	= {"r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","r12","sp","lr","pc"};
static char shift_strings[][4]     	= {"lsl","lsr","asr","ror"};
static char msr_fields[][5] 		= {"","c","x","xc","s","sc","sx","sxc","f","fc","fx","fxc","fs","fsc","fsx","fsxc"};

arm7::arm7() : CCore(){
	_regs=NULL;
	_ioreg=NULL;
	_freq=MHZ(7);
}

arm7::~arm7(){
}

int arm7::Destroy(){
	CCore::Destroy();
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int arm7::Reset(){
	CCore::Reset();
	if(_regs)
		memset(_regs,0,sizeof(u32)*7*16);
	_irq_pending=0;
	_cpsr = SYSTEM_MODE;
	_pregs=_regs;
	return 0;
}

int arm7::Init(void *m,u32 ss,u32 f){
	u32 n;

	if(!(_regs = new u8[n=7*16*sizeof(u32) + (ss * sizeof(CoreMACallback) * 2) +0x2000*sizeof(CoreDecode) + 0x2000*sizeof(CoreDisassebler)]))
		return -1;
	memset(_regs,0,n);
	_mem=(u8 *)m;
	_portfnc_write = (CoreMACallback *)((u32 *)_regs + 7*16);
	_portfnc_read=&_portfnc_write[ss];
	_opcodescb=(CoreDecode *)&_portfnc_read[ss];
	_disopcodescb=(CoreDisassebler *)&_opcodescb[0x2000];
	for(int i=0;i<0x2000;i++){
		OPCODE(i,&arm7::_empty_op);
	}

	SET_DP_HANDLE(0x0,&arm7::_and);
	SET_DP_HANDLE(0x10,&arm7::_ands);
	SET_DP_HANDLE(0x20,&arm7::_eor);
	SET_DP_HANDLE(0x30,&arm7::_eors);
	SET_DP_HANDLE(0x40,&arm7::_sub);
	SET_DP_HANDLE(0x50,&arm7::_subs);
	SET_DP_HANDLE(0x60,&arm7::_rsb);
	SET_DP_HANDLE(0x70,&arm7::_rsbs);
	SET_DP_HANDLE(0x80,&arm7::_add);
	SET_DP_HANDLE(0x90,&arm7::_adds);
	SET_DP_HANDLE(0xA0,&arm7::_adc);
	SET_DP_HANDLE(0xB0,&arm7::_adcs);
	SET_DP_HANDLE(0xD0,&arm7::_subcs);
	SET_DP_HANDLE(0x110,&arm7::_tst);
	SET_DP_HANDLE(0x150,&arm7::_cmp);
	SET_DP_HANDLE(0x170,&arm7::_cmn);
	SET_DP_HANDLE(0x180,&arm7::_orr);
	SET_DP_HANDLE(0x190,&arm7::_orrs);
	SET_DP_HANDLE(0x1a0,&arm7::_mov);
	SET_DP_HANDLE(0x1B0,&arm7::_movs);
	SET_DP_HANDLE(0x1C0,&arm7::_bic);
	SET_DP_HANDLE(0x1D0,&arm7::_bics);
	SET_DP_HANDLE(0x1E0,&arm7::_mvn);

	SET_SDT_HANDLE(0x400,&arm7::_str,imm);
	SET_SDT_HANDLE(0x410,&arm7::_ldr,imm);
	SET_SDT_HANDLE(0x450,&arm7::_ldrb,imm);
	SET_SDT_HANDLE(0x440,&arm7::_strb,imm);
	SET_SDT_HANDLE(0x600,&arm7::_str, );
	SET_SDT_HANDLE(0x610,&arm7::_ldr, );
	SET_SDT_HANDLE(0x650,&arm7::_ldrb, );
	SET_SDT_HANDLE(0x640,&arm7::_strb, );

	SET_HWDT_HANDLE(0xb,&arm7::_strh, );
	SET_HWDT_HANDLE(0x1b,&arm7::_ldrh, );
	SET_HWDT_HANDLE(0x1d,&arm7::_ldrsb,);
	SET_HWDT_HANDLE(0x4b,&arm7::_strh,imm);
	SET_HWDT_HANDLE(0x5b,&arm7::_ldrh,imm);
	SET_HWDT_HANDLE(0x5d,&arm7::_ldrsb,imm);
	SET_HWDT_HANDLE(0x1f,&arm7::_ldrsh, );
	SET_HWDT_HANDLE(0x5f,&arm7::_ldrsh,imm);

	for (int i=0; i<0x10; i++) {
		if(!(i & 1)){
			OPCODE(0x100|i,&arm7::_mrs_cpsr);
			OPCODE(0x120|i,&arm7::_msr_cpsr);
			OPCODE(0x140|i,&arm7::_mrs_spsr);
			OPCODE(0x160|i,&arm7::_msr_spsr);
		}
		OPCODE(0x320|i,&arm7::_msr_cpsr);
		OPCODE(0x360|i,&arm7::_msr_spsr);
		for(n=0;n<=0xF;n++) {
			OPCODE(0x800|(n<<5)|i,&arm7::_stm);
			OPCODE(0x810|(n<<5)|i,&arm7::_ldm);
		}
	}
	OPCODE(0x9,&arm7::_mul);
	OPCODE(0x19,&arm7::_muls);
	OPCODE(0x29,&arm7::_mla);
	OPCODE(0x89,&arm7::_mullu);
	OPCODE(0x121,&arm7::_bx);
	for (int i=0; i<0x100; i++) {
		OPCODE(0xa00|i,&arm7::_bmi);
		OPCODE(0xb00|i,&arm7::_blmi);
		OPCODE(0xf00|i,&arm7::_swi);
	}
	for (int i=0; i<0x8; i++) {
		OPCODE(0x1060|i,&arm7::_tadd_reg);
		OPCODE(0x1068|i,&arm7::_tsub_reg);
		OPCODE(0x1070|i,&arm7::_tadd_short_imm);
		OPCODE(0x1078|i,&arm7::_tsub_short_imm);
		OPCODE(0x1140|i,&arm7::_tstr_reg);
		OPCODE(0x1148|i,&arm7::_tstrh_reg);
		OPCODE(0x1150|i,&arm7::_tstrb_reg);
		OPCODE(0x1158|i,&arm7::_tldrsb_reg);
		OPCODE(0x1160|i,&arm7::_tldr_reg);
		OPCODE(0x1168|i,&arm7::_tldrh_reg);
		OPCODE(0x1170|i,&arm7::_tldrb_reg);
		OPCODE(0x1178|i,&arm7::_tldrsh_reg);
		OPCODE(0x12d0|i,&arm7::_tpush);
		OPCODE(0x12f0|i,&arm7::_tpop);
	}

	for (int i=0; i<0x20; i++){
		OPCODE(0x1000|i,&arm7::_tlsl_imm);
		OPCODE(0x1020|i,&arm7::_tlsr_imm);
		OPCODE(0x1040|i,&arm7::_tasr_imm);
		OPCODE(0x1080|i,&arm7::_tmov_imm);

		OPCODE(0x10a0|i,&arm7::_tcmp_imm);
		OPCODE(0x10c0|i,&arm7::_tadd_imm);
		OPCODE(0x10e0|i,&arm7::_tsub_imm);
		OPCODE(0x1120|i,&arm7::_tldr_pc);

		OPCODE(0x1180|i,&arm7::_tstr_imm);
		OPCODE(0x11a0|i,&arm7::_tldr_imm);
		OPCODE(0x11c0|i,&arm7::_tstrb_imm);
		OPCODE(0x11e0|i,&arm7::_tldrb_imm);
		OPCODE(0x1200|i,&arm7::_tstrh_imm);
		OPCODE(0x1220|i,&arm7::_tldrh_imm);
		OPCODE(0x1240|i,&arm7::_tstr_sp);
		OPCODE(0x1260|i,&arm7::_tldr_sp);
		OPCODE(0x1280|i,&arm7::_tadd_pc_reg);
		OPCODE(0x12A0|i,&arm7::_tadd_sp_reg);

		OPCODE(0x1300|i,&arm7::_tstm_ia);
		OPCODE(0x1320|i,&arm7::_tldm_ia);
		OPCODE(0x1380|i,&arm7::_tbu);
		OPCODE(0x13A0|i,&arm7::_tbl);
		OPCODE(0x13C0|i,&arm7::_tbl);
		OPCODE(0x13E0|i,&arm7::_tbl);
	}

	for(int i=0;i<0x3c;i++){
		OPCODE(0x1340|i,&arm7::_tb);
	}
	for(int i =0;i<4;i++){
		OPCODE(0x1110+i,&arm7::_tadd_hi);
		OPCODE(0x1118+i,&arm7::_tmov_hi);
		OPCODE(0x111c+i,&arm7::_tbx);
		OPCODE(0x1114+i,&arm7::_tcmp_hi);
		OPCODE(0x137c+i,&arm7::_tswi_imm);
	}
	for(int i =0;i<2;i++){
		OPCODE(0x12c2+i,&arm7::_tsub_sp);
		OPCODE(0x12c0+i,&arm7::_tadd_sp);
	}
	OPCODE(0x1100,&arm7::_tand_reg);
	OPCODE(0x1101,&arm7::_teor_reg);
	OPCODE(0x1102,&arm7::_tlsl_reg);
	OPCODE(0x1103,&arm7::_tlsr_reg);
	OPCODE(0x1104,&arm7::_tasr_reg);
	OPCODE(0x1106,&arm7::_tsbc_reg);
	OPCODE(0x1107,&arm7::_tror_reg);
	OPCODE(0x1108,&arm7::_ttst_reg);
	OPCODE(0x1109,&arm7::_tneg_reg);
	OPCODE(0x110A,&arm7::_tcmp_reg);
	OPCODE(0x110B,&arm7::_tcmn_reg);
	OPCODE(0x110C,&arm7::_tor_reg);
	OPCODE(0x110D,&arm7::_tmul_reg);
	OPCODE(0x110E,&arm7::_tbic_reg);
	OPCODE(0x110F,&arm7::_tmvn_reg);
	return 0;
}

int arm7::SetIO_cb(u32 a,CoreMACallback w,CoreMACallback r){
	//printf("%x %x\n",a,RMAPIO(a));
	if(_portfnc_write)
		_portfnc_write[RMAPIO(a)]=w;
	if(_portfnc_read)
		_portfnc_read[RMAPIO(a)]=r;
	return 0;
}

int arm7::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_NEXT_STEP:
			switch(*((u32 *)pv)){
				case 1:
					*((u32 *)pv)=4;
				break;
				default:
					*((u32 *)pv)=2;
				break;
			}
			return 0;
		case ICORE_QUERY_REGISTER:{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					if(strcmp(c,"CPSR") == 0)
						v=_cpsr;
					else{
						sscanf(&c[1],"%d",&v);
						v=REG_(v);
					}
				}
				else
					v=REG_(v);
				p[1]=v;
			}
			return 0;
		case ICORE_QUERY_SET_REGISTER:{
			u32 v,*p=(u32 *)pv;
			v=p[0];

			if(v == (u32)-1){
				char *c;
				int i;

				c=*((char **)&p[2]);
				if(strcmp(c,"CPSR") == 0)
					_cpsr=p[1];
				else{
					sscanf(&c[1],"%d",&i);
					REG_(i)=p[1];
				}
				return 0;
			}
			else
				REG_(v)=p[1];
		}
			return 0;
		case ICORE_QUERY_SET_LOCATION:{
			u32 *p=(u32 *)pv;
			void *__tmp;

			RMAP_(p[0],__tmp,W);
			if(!__tmp)
				return -2;
			switch(p[2]){
				case 0:
					memset(__tmp,(u8)p[1],p[4]);
					return 0;
				case 1:{
					u8 *c=(u8 *)__tmp;
					for(u32 i=p[4];i>0;i--)
						*c++=*c-1;
					return 0;
				}
				case 2:{
					u8 *c=(u8 *)__tmp;
					for(u32 i=p[4];i>0;i--)
						*c++=*c+1;
					return 0;
				}
			}
		}
			return -1;
		default:
			return CCore::Query(what,pv);
	}
}

int arm7::OnReturnFromException(u32){
	u8 i;
	u32 spsr;

	_pc=REG_(15);
	DLOG("IRQ Leave %x",_irq_pending);
	if((i=_cpsr & 0x1f) != SYSTEM_MODE && i != USER_MODE)
		switchmode(0,0);
	if(!_irq_pending || machine->OnEvent(0,-1)){
		return 0;
	}
	//REG_SPSR=spsr;
	REG_(14) += 4;
	_pc -= 4;
	return 0;
}

int arm7::OnException(u32,u32){
	return 0;
}

int arm7::_enterIRQ(int n,int v,u32 pc){
	if(_cpsr & IRQ_BIT)
		return 1;
	switchmode(IRQ_MODE,1);
	//if(REG_(13) != 0x3007fa0) EnterDebugMode();
	DLOG("IRQ Enter %x",n);
	_cpsr &= ~T_BIT;
	_cpsr |= IRQ_BIT;
	REG_(14) = _pc;
	_pc = 0x18;
	return 0;
}

int arm7::_dumpRegisters(char *p){
	char s[450];

	*p=0;
	sprintf(p,"CPSR:%08X T:%d Z:%d N:%d C:%d V:%d I:%d F:%d SPSR:%08X\n\n",_cpsr,SR(_cpsr,T_SHIFT)&1,
		SR(_cpsr,Z_SHIFT)&1,SR(_cpsr,N_SHIFT)&1,SR(_cpsr,C_SHIFT)&1,SR(_cpsr,V_SHIFT)&1,
		SR(_cpsr,IRQ_SHIFT)&1,SR(_cpsr,FIQ_SHIFT)&1,REG_SPSR);

	for(int i =0;i<15;i++){
		sprintf(s,"r%02d %08X ",i, REG_(i));
		strcat(p,s);
		if((i&3)==3) strcat(p,"\n");
	}
	sprintf(s,"PC %08X\n\nM:%s IP:%08X L:%u %u %u",_pc,"",_irq_pending,__line,__cycles,__frame);
	strcat(p,s);
	return 0;
}

int arm7::_exec(u32 status){
	int ret;

	ret=1;
	if(_cpsr & T_BIT)
		goto B;
	RLOP(_pc,_opcode);
	REG_(15)=_pc+8;
	switch(_opcode >> 28){
		case 0:
           if(!(_cpsr & Z_BIT))
               goto Z;
       break;
       case 1:
           if((_cpsr&Z_BIT))
              goto Z;
       break;
       case 2:
           if(!(_cpsr & C_BIT)) //CS
               goto Z;
       break;
       case 3:
           if((_cpsr & C_BIT)) //CC
               goto Z;
       break;
       case 4://bmi
           if(!(_cpsr & N_BIT))
               goto Z;
       break;
       case 5://pl
           if(_cpsr & N_BIT)
               goto Z;
       break;
       case 6:
          	if(!(_cpsr & V_BIT))
               goto Z;
       break;
       case 7://bvc
          	if((_cpsr & V_BIT))
				goto Z;
       break;
       case 8: // BHI
			//if(!(c_flag && !z_flag))
			if(! ((_cpsr & (C_BIT|Z_BIT)) == C_BIT) )
				goto Z;
		break;
		case 9: //BLS
			//if(!(!c_flag || z_flag))
			if(! ((_cpsr & C_BIT)==0 || (_cpsr&Z_BIT)))
				goto Z;
		break;
		case 10:{ //BGE
			u32 a;

			// if(v_flag == n_flag)
			a=_cpsr & (V_BIT|N_BIT);
			if(! (a==0 || a == (V_BIT|N_BIT)) )
				goto Z;
		}
		break;
		case 11:{ // BLT
			u32 a;
			//  if(v_flag != n_flag)
			a=_cpsr & (V_BIT|N_BIT);
			if( (a==0 || a == (V_BIT|N_BIT)) )
				goto Z;
		}
		break;
		case 12: //BGT
          ////if(!(!z_flag && v_flag == n_flag))
			{ // BGT
			u32 a;
			if(! ((_cpsr & Z_BIT)==0 && (a=(_cpsr & (V_BIT|N_BIT)) == 0 || a == (V_BIT|N_BIT))))
				goto Z;
		}
		break;
		case 13:{ // BLE
			u32 a;
			//if(!(z_flag || n_flag != v_flag))
			if(! ((_cpsr & Z_BIT) || ((a=(_cpsr & (N_BIT|V_BIT))) && a != (N_BIT|V_BIT))))
				goto Z;
		}
		break;
	}
	ret += (((CCore *)this)->*_opcodescb[((_opcode & 0x0FF00000)>>16)|(((u8)_opcode)>>4)])();
Z:
	_pc += 4;
	goto ZZ;
B:
	RWOP(_pc,_opcode);
	REG_(15)=_pc+4;
	ret += (((CCore *)this)->*_opcodescb[0x1000|SR(_opcode,6)])();
	_pc+=2;
ZZ:
	return ret;
}

int arm7::Disassemble(char *dest,u32 *padr){
	u32 op,pc;
	char c[400],*cc;

	*((u64 *)c)=0;
	cc=&c[200];
	*((u64 *)cc)=0;

	pc=_pc;
	op=_opcode;
	_pc = *padr;
	sprintf(c,"%08X ",_pc);

	if(_cpsr & T_BIT){
		RWOP(_pc,_opcode);
		sprintf(&c[8]," %04x ",_opcode);
		(((CCore *)this)->*_disopcodescb[0x1000|SR(_opcode,6)])(cc);
		_pc += 2;
	}
	else{
		RLOP(_pc,_opcode);
		sprintf(&c[8]," %8x ",_opcode);
		(((CCore *)this)->*_disopcodescb[((_opcode & 0x0FF00000)>>16)|(((u8)_opcode)>>4)])(cc);
		_pc += 4;
	}
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	*padr=_pc;
	_opcode=op;
	_pc=pc;
	return 0;
}

int arm7::_empty_op(){
	EnterDebugMode();
	printf("unk %x %x %x %x\n",_pc,_opcode,((_opcode & 0x0FF00000)>>16)|(((u8)_opcode)>>4),SR(_opcode,6));
	return 0;
}

int arm7::_empty_op_dis(char *cc){
	sprintf(cc,"UNK A:%x T:%x",((_opcode & 0x0FF00000)>>16)|(((u8)_opcode)>>4),(u16)SR(_opcode,6));
	return 0;
}

#define __ARM_COND_PRE(a) sprintf(cc,a"%s ",condition_strings[_opcode>>28]);

#define ARMSDP__(a){\
	if(_opcode & 0x2000000){\
		u32 temp__,sa__ = ((_opcode>>8)&0xF)<<1;\
		temp__ = ((u8)_opcode << (32-sa__) )|((u8)_opcode >> sa__);\
		sprintf(ss__,"0x%X",temp__);\
	}\
	else{\
		strcat(cc,register_strings[_opcode&0xF]);\
		if(_opcode & 0x10){sprintf(ss__,", %s %s",shift_strings[(_opcode>>5)&0x3],register_strings[(_opcode>>8)&0xF]);}\
		else {\
			u32 temp__ = (_opcode>>7)&0x1F;\
			if(!temp__){\
				switch((_opcode>>5) & 0x3) {\
					case 0:ss__[0]=0;break;\
					case 1:strcpy(ss__,", lsr 0x20");break;\
					case 2:strcpy(ss__,", asr 0x20");break;\
					case 3:strcpy(ss__,", rrx");break;\
				}\
			}\
			else {sprintf(ss__,",%s 0x%X",shift_strings[(_opcode>>5)&0x3],temp__);}\
		}\
	}\
	strcat(cc,ss__);\
}

#define ARMSDP_(a){\
	char ss__[20];\
	sprintf(cc,a "%s %s, %s, ",condition_strings[SR(_opcode,28)], register_strings[DEST_REG_INDEX],register_strings[BASE_REG_INDEX]);\
	ARMSDP__(a) }

#define ARMSDPS_(a){\
	char ss__[20];\
	sprintf(cc,a "%s %s, ",condition_strings[SR(_opcode,28)], register_strings[DEST_REG_INDEX]);\
	ARMSDP__(a) }

int arm7::_swi(){
	OnException(2,_opcode&0xfffff);
	__ARMF(swi);
	return 1;
}

int arm7::_swi_dis(char *cc){
	sprintf(cc,"swi%s 0x%08X",condition_strings[_opcode>>28], _opcode&0xfffff);
	return 0;
}

int arm7::_bmi(){
	int adr;

	if(((adr = (_opcode & 0xFFFFFF)) & 0x800000))
		adr = 0 - (0x1000000 - adr);
	if((_opcode >> 28) == 0xF){
		REG_(14) = _pc - 4;
		adr= (adr << 2) + (_opcode & 0x1000000 ? 4 : 2);
       _cpsr |= T_BIT;
	}
	else
		adr = (adr << 2) + 4;
	__ARMF(bmi);
	_pc += adr;
	return 1;
}

int arm7::_bmi_dis(char *cc){
	u32 adress;

	if(_opcode&0x800000)
       adress = (((_opcode&0xFFFFFF)<<2)-0x4000000) + 8;
	else
       adress = ((_opcode&0x7FFFFF)<<2) + 8;
	sprintf(cc,"b%s 0x%08X",condition_strings[_opcode>>28], _pc+adress);
	return 0;
}

int arm7::_blmi(){
	int adr;

	REG_(14)=_pc+4;
	STORECALLLSTACK(REG_(14));
	if(((adr = (_opcode & 0xFFFFFF)) & 0x800000))
		adr = 0 - (0x1000000 - adr);
	adr = (adr << 2) + 4;
	__ARMF(blmi);
	_pc += adr;
	return 1;
}

int arm7::_blmi_dis(char *cc){
	u32 adress;

	if(_opcode&0x800000)
       adress = (((_opcode&0xFFFFFF)<<2)-0x4000000) + 8;
	else
       adress = ((_opcode&0x7FFFFF)<<2) + 8;
	sprintf(cc,"bl%s 0x%08X",condition_strings[_opcode>>28], _pc+adress);
	return 0;
}

int arm7::_bx(){
	u32 value;

	value = OP_REG;
	if(value & 1){
		value = (value & ~1) - 2;
		if(!(_cpsr & T_BIT)){
			_cpsr |= T_BIT;
			value -= 2;
			Query(IMACHINE_QUERY_DEBUG_NOTIFY,0);
		}
	}
	else
		value -= 4;
	__ARMF(bx);
	_pc=value;
	return 1;
}

int arm7::_bx_dis(char *cc){
	__ARMS(%s,"bx",register_strings[OP_REG_INDEX]);
	return 0;
}

int arm7::_and(){
	u32 rd,a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	REG_((rd = (u8)DEST_REG_INDEX)) = BASE_REG & a;
	__ARMF(and);
	if(rd == 15)
       _pc += 4;
	return 1;
}

int arm7::_and_imm(){
	u32 rd,a;

	DP_IMM_OPERAND(a);
	REG_((rd = (u8)DEST_REG_INDEX)) = BASE_REG & a;
	__ARMF(and);
	if(rd == 15)
       _pc += 4;
	return 1;
}

int arm7::_and_reg(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,SHFT_AMO_REG);
	REG_((rd = (u8)DEST_REG_INDEX)) &= a;
	__ARMF(and);
	printf("%s load pc %x\n",__FUNCTION__,_pc);
	EnterDebugMode();
	return 1;
}

int arm7::_and_dis(char *cc){
	ARMSDP_("and");
	return 0;
}

int arm7::_ands(){
	u32 a,rd;

	DPS_REG_OPERAND(a,IMM_SHIFT);
	SET_DP_FLAGS(REG_((rd = DEST_REG_INDEX)) = BASE_REG & a);
	__ARMF(ands);
	return 1;
}

int arm7::_ands_imm(){
	u32 a,rd;

	DPS_IMM_OPERAND(a);
	SET_DP_FLAGS(REG_((rd = DEST_REG_INDEX)) = BASE_REG & a);
	__ARMF(ands);
	return 1;
}

int arm7::_ands_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ands_dis(char *cc){
	ARMSDP_("ands");
	return 0;
}

int arm7::_mov(){
	u8 rd;

	DP_REG_OPERAND(REG_((rd = (u8)DEST_REG_INDEX)),IMM_SHIFT);
	__ARMF(mov);
	if(rd == 15){
		//printf("%s %x\n",__FUNCTION__,_pc);
       _pc = REG_(15) - 4;
    }
	return 1;
}

int arm7::_mov_imm(){
	DP_IMM_OPERAND(DEST_REG);
	__ARMF(mov);
	return 1;
}

int arm7::_mov_reg(){
	DP_REG_OPERAND(DEST_REG,SHFT_AMO_REG);
	__ARMF(mov);
	return 1;
}

int arm7::_mov_dis(char *cc){
	ARMSDPS_("mov");
	return 0;
}

int arm7::_movs(){
	u32 a;
	u8 rd;

	DPS_REG_OPERAND(a,IMM_SHIFT);
	REG_((rd = (u8)DEST_REG_INDEX)) = a;
	SET_DP_FLAGS(a);
	__ARMF(movs);
	if(rd != 15)
		return 1;
	OnReturnFromException(0);
	return 2;
}

int arm7::_movs_imm(){printf("%s\n",__FUNCTION__);return 0;}
int arm7::_movs_reg(){printf("%s\n",__FUNCTION__);return 0;}
int arm7::_movs_dis(char *cc){
	ARMSDPS_("movs");
	return 0;
}

int arm7::_mvn(){printf("%s\n",__FUNCTION__);return 0;}
int arm7::_mvn_imm(){
	u32 a;

	DP_IMM_OPERAND(a);
	DEST_REG=~a;
	__ARMF(mvn);
	return 1;
}
int arm7::_mvn_reg(){printf("%s\n",__FUNCTION__);return 0;}
int arm7::_mvn_dis(char *cc){
	ARMSDPS_("mvn");
	return 0;
}

int arm7::_mvs(){printf("%s\n",__FUNCTION__);return 0;}
int arm7::_mvns_imm(){printf("%s\n",__FUNCTION__);return 0;}
int arm7::_mvns_reg(){printf("%s\n",__FUNCTION__);return 0;}
int arm7::_mvns_dis(char *cc){
	ARMSDPS_("mvns");
	return 0;
}


int arm7::_orr(){
	u32 rd,a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	REG_((rd = (u8)DEST_REG_INDEX)) = BASE_REG | a;
	__ARMF(orr);
	return 1;
}

int arm7::_orr_imm(){
	u32 a,rd;

	DP_IMM_OPERAND(a);
	DEST_REG = BASE_REG | a;
	__ARMF(orr);
	return 1;
}

int arm7::_orr_reg(){
	u32 a;

	DP_REG_OPERAND(a,SHFT_AMO_REG);
	DEST_REG = BASE_REG | a;
	__ARMF(orr);
	return 1;
}

int arm7::_orr_dis(char *cc){
	ARMSDP_("orr");
	return 0;
}

int arm7::_orrs(){
	u32 rd,a;

	DPS_REG_OPERAND(a,IMM_SHIFT);
	a|=BASE_REG;
	REG_((rd = (u8)DEST_REG_INDEX)) = a;
	SET_DP_FLAGS(a);
	__ARMF(orrs);
	if(rd == 15)
       _pc += 4;
	return 1;
}

int arm7::_orrs_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_orrs_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_orrs_dis(char *cc){
	ARMSDP_("orrs");
	return 0;
}

int arm7::_bic(){
	u32 rd,a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	REG_((rd = (u8)DEST_REG_INDEX)) = BASE_REG & ~a;
	__ARMF(bic);
	return 1;
}

int arm7::_bic_imm(){
	u32 a,rd;

	DP_IMM_OPERAND(a);
	REG_((rd = DEST_REG_INDEX)) = BASE_REG & ~a;
	__ARMF(bic);
	return 1;
}

int arm7::_bic_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_bic_dis(char *cc){
	ARMSDP_("bic");
	return 0;
}

int arm7::_bics(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_bics_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_bics_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_bics_dis(char *cc){
	ARMSDP_("bics");
	return 0;
}

int arm7::_eor(){
	u32 rd,a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	REG_((rd = (u8)DEST_REG_INDEX)) = BASE_REG ^ a;
	__ARMF(eor);
	return 1;
}

int arm7::_eor_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_eor_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_eor_dis(char *cc){
	ARMSDP_("eor");
	return 0;
}

int arm7::_eors(){
	u32 rd,a;

	DPS_REG_OPERAND(a,IMM_SHIFT);
	a ^= BASE_REG;
	REG_((rd = (u8)DEST_REG_INDEX)) = a;
	SET_DP_FLAGS(a);
	__ARMF(eors);
	if(rd == 15){
       _pc += 4;
		EnterDebugMode();
		printf("%s %x\n",__FUNCTION__,_pc);
	}
	return 1;
}

int arm7::_eors_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_eors_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_eors_dis(char *cc){
	ARMSDP_("eors");
	return 0;
}

int arm7::_sub(){
	u32 rd,a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	REG_((rd = (u8)DEST_REG_INDEX)) = BASE_REG - a;
	__ARMF(sub);
	return 1;
}

int arm7::_sub_imm(){
	u32 rd,a;

	DP_IMM_OPERAND(a);
	REG_((rd=DEST_REG_INDEX)) = BASE_REG-a;
	__ARMF(sub);
	return 1;
}

int arm7::_sub_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_sub_dis(char *cc){
	ARMSDP_("sub");
	return 0;
}

int arm7::_subs(){
	u32 a,rd;

	rd=DEST_REG_INDEX;
	DP_REG_OPERAND(a,IMM_SHIFT);
	SET_SUB_FLAGS(REG_(rd),BASE_REG,a);
	__ARMF(subs);
	if(rd != 15)
		return 1;
	OnReturnFromException(0);
	return 2;
}

int arm7::_subs_imm(){
	u32 rd,a;

	DP_IMM_OPERAND(a);
	SET_SUB_FLAGS(REG_((rd = DEST_REG_INDEX)),BASE_REG,a);
	__ARMF(subs);
	if(rd == 15)
		OnReturnFromException(0);
	return 1;
}

int arm7::_subs_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_subs_dis(char *cc){
	ARMSDP_("subs");
	return 0;
}

int arm7::_subc(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_subc_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_subc_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_subc_dis(char *cc){
	ARMSDP_("subc");
	return 0;
}

int arm7::_subcs(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_subcs_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_subcs_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_subcs_dis(char *cc){
	ARMSDP_("subcs");
	return 0;
}

int arm7::_rsb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsb_imm(){
	u32 rd,a;

	DP_IMM_OPERAND(a);
	REG_((rd=DEST_REG_INDEX)) = a-BASE_REG;
	__ARMF(rsb);
	return 1;
}

int arm7::_rsb_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsb_dis(char *cc){
	ARMSDP_("rsb");
	return 0;
}

int arm7::_rsbs(){
	u32 a,rd;

	rd=DEST_REG_INDEX;
	DP_REG_OPERAND(a,IMM_SHIFT);
	SET_SUB_FLAGS(REG_(rd),a,BASE_REG);
	__ARMF(rsbs);
	if(rd != 15)
		return 1;
	EnterDebugMode();
	printf("%s %x\n",__FUNCTION__,_pc);
	OnReturnFromException(0);
	return 2;
}

int arm7::_rsbs_imm(){
	u32 rd,a;

	DP_IMM_OPERAND(a);
	SET_SUB_FLAGS(REG_(rd),a,BASE_REG);
	__ARMF(rsbs);
	if(rd != 15)
		return 1;
	EnterDebugMode();
	printf("%s %x\n",__FUNCTION__,_pc);
	OnReturnFromException(0);
	return 2;
}

int arm7::_rsbs_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsbs_dis(char *cc){
	ARMSDP_("rsbs");
	return 0;
}

int arm7::_rsbc(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsbc_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsbc_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsbc_dis(char *cc){
	return 0;
}

int arm7::_rsbcs(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsbcs_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsbcs_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_rsbcs_dis(char *cc){
	return 0;
}

int arm7::_add(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	REG_((rd = (u8)DEST_REG_INDEX)) = BASE_REG + a;
	__ARMF(add);
	return 1;
}

int arm7::_add_imm(){
	u32 rd,a;

	rd=DEST_REG_INDEX;
	DP_IMM_OPERAND(a);
	REG_(rd) = BASE_REG+a;
	__ARMF(add);
	return 1;
}

int arm7::_add_reg(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,SHFT_AMO_REG);
	rd = (u8)DEST_REG_INDEX;
	REG_(rd) = a+BASE_REG;
	__ARMF(add);
	return 1;
}

int arm7::_add_dis(char *cc){
	ARMSDP_("add");
	return 0;
}

int arm7::_adds(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	SET_ADD_FLAGS(a,BASE_REG,a);
	REG_((rd = (u8)DEST_REG_INDEX)) = a;
	__ARMF(adds);
	return 1;
}

int arm7::_adds_imm(){
	u8 rd;
	u32 a;

	DP_IMM_OPERAND(a);
	SET_ADD_FLAGS(a,BASE_REG,a);
	REG_((rd = (u8)DEST_REG_INDEX)) = a;
	__ARMF(adds);
	return 2;
}

int arm7::_adds_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_adds_dis(char *cc){
	ARMSDP_("adds");
	return 0;
}

int arm7::_adc(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	rd = (u8)DEST_REG_INDEX;
	REG_(rd) = a+BASE_REG+((_cpsr & C_BIT) >> C_SHIFT);
	__ARMF(adc);
	if(rd == 15){
       printf("%s load pc %x\n",__FUNCTION__,_pc);
	}
	return 1;
}

int arm7::_adc_imm(){
	u8 rd;
	u32 a;

	DP_IMM_OPERAND(a);
	DEST_REG = a+BASE_REG+((_cpsr & C_BIT) >> C_SHIFT);
	__ARMF(adc);
	return 2;
}

int arm7::_adc_reg(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,SHFT_AMO_REG);
	DEST_REG  = a+BASE_REG+((_cpsr & C_BIT) >> C_SHIFT);
	__ARMF(adc);
	return 1;
}

int arm7::_adc_dis(char *cc){
	ARMSDP_("adc");
	return 0;
}

int arm7::_adcs(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	SET_ADDC_FLAGS(a,BASE_REG,a);
	rd = (u8)DEST_REG_INDEX;
	REG_(rd) = a;
	__ARMF(adcs);
	if(rd == 15){
       printf("%s load pc %x\n",__FUNCTION__,_pc);
	}
	return 1;
}

int arm7::_adcs_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_adcs_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_adcs_dis(char *cc){
	ARMSDPS_("adcs");
	return 0;
}

int arm7::_tst(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_tst_imm(){
	u32 a;

	DPS_IMM_OPERAND(a);
	a &= BASE_REG;
	SET_DP_FLAGS(a);
	__ARMF(tst);
	return 1;
}

int arm7::_tst_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_tst_dis(char *cc){
	ARMSDPS_("tst");
	return 0;
}

int arm7::_tsts(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_tsts_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_tsts_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_tsts_dis(char *){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teq(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teq_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teq_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teq_dis(char *){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teqs(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teqs_imm(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teqs_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_teqs_dis(char *cc){
	ARMSDP_("teqs");
	return 0;
}

int arm7::_cmp(){
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	SET_SUB_FLAGS(a,BASE_REG,a);
	__ARMF(cmp);
	return 1;
}

int arm7::_cmp_imm(){
	u32 a;

	DP_IMM_OPERAND(a);
	SET_SUB_FLAGS(a,BASE_REG,a);
	__ARMF(cmp);
	return 1;
}

int arm7::_cmp_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_cmp_dis(char *cc){
	ARMSDPS_("cmp");
	return 0;
}

int arm7::_cmn(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_cmn_imm(){
	u32 a;

	DP_IMM_OPERAND(a);
	SET_ADD_FLAGS(a,BASE_REG,a);
	__ARMF(cmn);
	return 1;
}

int arm7::_cmn_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);return 1;
}

int arm7::_cmn_dis(char *cc){
	ARMSDPS_("cmn");
	return 0;
}

int arm7::_mul(){
	u32 value;

	BASE_REG = OP_REG * (value = SHFT_AMO_REG);
	__ARMF(mul);
	return 1;
}

int arm7::_mul_dis(char *cc){
	sprintf(cc,"mul%s %s, %s, %s",condition_strings[SR(_opcode,28)], register_strings[BASE_REG_INDEX],
	register_strings[OP_REG_INDEX],register_strings[SHFT_AMO_REG_INDEX]);
	return 0;
}

int arm7::_muls(){
	u32 value;

	SET_DP_FLAGS(BASE_REG = OP_REG * (value = SHFT_AMO_REG));
	__ARMF(muls);
	return 1;
}

int arm7::_muls_dis(char *cc){
	sprintf(cc,"muls%s %s, %s, %s",condition_strings[SR(_opcode,28)], register_strings[BASE_REG_INDEX],
		register_strings[OP_REG_INDEX],register_strings[SHFT_AMO_REG_INDEX]);
	return 0;
}

int arm7::_mla(){
	BASE_REG = (u32)OP_REG * (u32)SHFT_AMO_REG + DEST_REG;
	return 1;
}

int arm7::_mla_dis(char *cc){
	sprintf(cc,"mla%s %s, %s, %s",condition_strings[SR(_opcode,28)], register_strings[BASE_REG_INDEX],
		register_strings[OP_REG_INDEX],register_strings[SHFT_AMO_REG_INDEX]);
	return 0;
}

int arm7::_mlas(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mlas_dis(char *cc){
	return 0;
}

int arm7::_mull(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mull_dis(char *cc){
	return 0;
}

int arm7::_mulls(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mulls_dis(char *cc){
	return 0;
}

int arm7::_mlal(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mlal_dis(char *cc){
	return 0;
}

int arm7::_mlals(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mlals_dis(char *cc){
	return 0;
}

int arm7::_mullu(){
	u64 a=(u64)(OP_REG) * (u64)(SHFT_AMO_REG);
	LO_REG = (u32)a;
	HI_REG = SR(a,32);
	__ARMF(mullu);
	return 3;
}

int arm7::_mullu_dis(char *cc){
	sprintf(cc,"mullu%s %s, %s, %s, %s",condition_strings[SR(_opcode,28)], register_strings[DEST_REG_INDEX],register_strings[BASE_REG_INDEX],
		register_strings[OP_REG_INDEX],register_strings[SHFT_AMO_REG_INDEX]);
	return 0;
}

int arm7::_mullus(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mullus_dis(char *cc){
	return 0;
}

int arm7::_mlalu(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mlalu_dis(char *cc){
	return 0;
}

int arm7::_mlalus(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_mlalus_dis(char *cc){
	return 0;
}

int arm7::_ldrb_postdown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldrb_postup(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldrb_predown(){
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	RB(BASE_REG-a,DEST_REG);
	__ARMF(ldrb);
	return 2;
}

int arm7::_ldrb_preup(){
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	RB(BASE_REG + a,DEST_REG);
	__ARMF(ldrb);
	return 2;
}

int arm7::_ldrb_predownwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldrb_preupwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_strb_postdown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_strb_postup(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_strb_predown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_strb_preup(){
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	WB(BASE_REG + a,(u8)DEST_REG);
	__ARMF(strb);
	return 2;
}

int arm7::_strb_predownwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_strb_preupwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldrb_postdownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrb_postupimm(){
	u32 *regs;

	regs = &BASE_REG;
	RB(*regs,DEST_REG);
	*regs += _opcode & 0xFFF;
	__ARMF(ldrb);
	return 2;
}

int arm7::_ldrb_predownimm(){
	RB(BASE_REG - (_opcode & 0xfff),DEST_REG);
	__ARMF(ldrb);
	return 2;
}

int arm7::_ldrb_preupimm(){
	RB(BASE_REG + (_opcode & 0xfff),DEST_REG);
	__ARMF(ldrb);
	return 2;
}

int arm7::_ldrb_predownimmwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrb_preupimmwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_ldrb_dis(char *cc){
	__ARM_COND_PRE("ldrb");
	return _sdt_dis(cc);
}

int arm7::_strb_postdownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_strb_postupimm(){
	u32 *regs;

	regs = &BASE_REG;
    WB(*regs,DEST_REG);
	*regs += _opcode & 0xFFF;
	__ARMF(strb);
	return 2;
}

int arm7::_strb_predownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_strb_preupimm(){
	WB(BASE_REG + (_opcode & 0xFFF),DEST_REG);
	return 2;
}

int arm7::_strb_predownimmwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_strb_preupimmwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_strb_dis(char *cc){
	__ARM_COND_PRE("strb");
	return _sdt_dis(cc);
}

int arm7::_ldrsb_postdownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_ldrsb_postupimm(){
	u32 *regs;
	s8 a;

	regs = &BASE_REG;
	RB(*regs,a);
	DEST_REG=(u32)(s32)a;
	*regs += ((_opcode&0xF00)>>4)|(_opcode&0xF);
	__ARMF(ldrsb);
	return 2;
}

int arm7::_ldrsb_predownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsb_preupimm(){
	s8 a;

	RB(BASE_REG + (((_opcode&0xF00)>>4)|(_opcode&0xF)),a);
	DEST_REG = (u32)(s32)a;
	__ARMF(ldrsb);
	return 2;
}
int arm7::_ldrsb_predownimmwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsb_preupimmwb(){
	u32 base,*regs;
	s8 a;

	base = *(regs = &BASE_REG) + (((_opcode&0xF00)>>4)|(_opcode&0xF));
	RB(base,a);
	DEST_REG=(u32)(s32)a;
	*regs = base;
	__ARMF(ldrsb);
	return 2;
}

int arm7::_ldrsb_postdown(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsb_postup(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsb_predown(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsb_preup(){
	s8 a;

	RB(BASE_REG + OP_REG,a);
	DEST_REG = (u32)(s32)a;
	__ARMF(ldrsb);
	return 2;
}

int arm7::_ldrsb_predownwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsb_preupwb(){
	u32 base,*regs;
	s8 a;

	base = *(regs = &BASE_REG) + OP_REG;
	RB(base,a);
	DEST_REG = (u32)(s32)a;
	*regs = base;
	__ARMF(ldrsb);
	return 2;
}

int arm7::_ldrsb_dis(char *cc){
	__ARM_COND_PRE("ldrsb");
	return _sdt_dis(cc);
}

int arm7::_ldrsh_postdownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_ldrsh_postupimm(){
	u32 *regs;
	s16 a;

	RW(*(regs = &BASE_REG),a)
	DEST_REG = (u32)a;
	*regs += ((_opcode&0xF00)>>4)|(_opcode&0xF);
	__ARMF(ldrsh);
	return 2;
}

int arm7::_ldrsh_predownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_ldrsh_preupimm(){
	s16 a;

	RW(BASE_REG + (((_opcode&0xF00)>>4)|(_opcode&0xF)),a);
	DEST_REG = (u32)a;
	__ARMF(ldrsh);
	return 2;
}

int arm7::_ldrsh_predownimmwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsh_preupimmwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_ldrsh_postdown(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsh_postup(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsh_predown(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsh_preup(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsh_predownwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}
int arm7::_ldrsh_preupwb(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_ldrsh_dis(char *cc){
	__ARM_COND_PRE("ldrsh");
	return _sdt_dis(cc);
}

int arm7::_ldrh_postdown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ldrh_postup(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ldrh_predown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ldrh_preup(){
	RW(BASE_REG + OP_REG,DEST_REG);
	__ARMF(ldrh);
	return 2;
}

int arm7::_ldrh_predownwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ldrh_preupwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_postdown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_postup(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_predown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_preup(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_predownwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_preupwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ldrh_postdownimm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ldrh_postupimm(){
	u32 *regs,a;

	regs = &BASE_REG;
	RW(*regs,a);
	*regs += ((_opcode&0xF00) >> 4)|(_opcode & 0xF);
	DEST_REG = a;
	__ARMF(ldrh);
	return 2;
}

int arm7::_ldrh_predownimm(){
	RW(BASE_REG - (((_opcode&0xF00)>>4)|(_opcode&0xF)),DEST_REG);
	__ARMF(ldrh);
	return 2;
}

int arm7::_ldrh_preupimm(){
	RW(BASE_REG + (((_opcode&0xF00)>>4)|(_opcode&0xF)),DEST_REG);
	__ARMF(ldrh);
	return 2;
}

int arm7::_ldrh_predownimmwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_ldrh_preupimmwb(){
	RW(BASE_REG - (((_opcode&0xF00)>>4)|(_opcode&0xF)),DEST_REG);
	__ARMF(ldrh);
	return 2;
}

int arm7::_ldrh_dis(char *cc){
	ARMHSDT("ldrh");
	return 0;
}

int arm7::_strh_postdownimm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_postupimm(){
	u32 *regs;

	WW(*(regs = &BASE_REG),(u16)DEST_REG);
	*regs += ((_opcode&0xF00)>>4)|(_opcode&0xF);
	__ARMF(strh);
	return 1;
}

int arm7::_strh_predownimm(){
	WW(BASE_REG - (((_opcode&0xF00)>>4)|(_opcode&0xF)),(u16)DEST_REG);
	__ARMF(strh);
	return 2;
}

int arm7::_strh_preupimm(){
	WW(BASE_REG + (((_opcode&0xF00)>>4)|(_opcode&0xF)),(u16)DEST_REG);
	__ARMF(strh);
	return 2;
}

int arm7::_strh_predownimmwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_preupimmwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_strh_dis(char *cc){
	ARMHSDT("strh");
	return 0;
}

int arm7::_ldr_postdown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldr_postup(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldr_predown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldr_preup(){
	u32 base,*regs,a;
	u8 rd;

	DP_REG_OPERAND(a,IMM_SHIFT);
	base = *(regs = &BASE_REG);
	rd=DEST_REG_INDEX;
	RL(base + a,REG_(rd));
	*regs=base;
	__ARMF(ldr);
	if(rd != 15)
       return 3;
printf("%s load pc %x\n",__FUNCTION__,_pc);
	EnterDebugMode();
	REG_(15) += 4;
	return 2;
}

int arm7::_ldr_predownwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldr_preupwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 0;
}

int arm7::_ldr_postdownimm(){printf("%s %x\n",__FUNCTION__,_pc);return 0;}

int arm7::_ldr_postupimm(){
	u32 *regs;
	u8 rd;

	regs = &BASE_REG;
	rd = (u8)DEST_REG_INDEX;
	RL(*regs,REG_(rd));
	*regs += _opcode & 0xFFF;
	if(rd != 15)
       return 2;
    _pc = REG_(15) - 4;
	if(_pc & 1){
printf("%s load pc %x\n",__FUNCTION__,_pc);
		EnterDebugMode();
		_cpsr |= T_BIT;
	}
	return 3;
}

int arm7::_ldr_predownimm(){
	u32 rd,a;

	rd = (u8)DEST_REG_INDEX;
	RL(BASE_REG - (_opcode & 0xFFF),a);
	REG_(rd)=a;
	__ARMF(ldr);
	if(rd != 15)
		return 2;
	//EnterDebugMode();
	_pc = REG_(15) - 4;
	return 3;
}

int arm7::_ldr_preupimm(){
	u32 base,*regs,a;
   u8 rd;

	rd = (u8)DEST_REG_INDEX;
	base = BASE_REG + (_opcode & 0xFFF);
	RL(base,REG_(rd));
	__ARMF(ldr);
	return 1;
}

int arm7::_ldr_predownimmwb(){
	u32 base,*regs,a;
	u8 rd;

	__ARMF(ldr);
	rd = (u8)DEST_REG_INDEX;
	base = *(regs = &BASE_REG) - (_opcode & 0xFFF);
	RL(base,a);
	REG_(rd)=a;
	*regs = base;
	if(rd != 15)
       return 3;
	_pc += 4;
printf("%s load pc %x\n",__FUNCTION__,_pc);
	EnterDebugMode();
	return 3;
}

int arm7::_ldr_preupimmwb(){
	u32 *regs,base,a;
	u8 rd;

	__ARMF(ldr);
	rd = (u8)DEST_REG_INDEX;
	base = *(regs = &BASE_REG) + (_opcode & 0xFFF);
	RL(base,a)
	REG_(rd) = a;
	*regs = base;

	if(rd != 15)
		return 2;
printf("%s load pc %x\n",__FUNCTION__,_pc);
	EnterDebugMode();
	return 4;
}

int arm7::_ldr_dis(char *cc){
	__ARM_COND_PRE("ldr");
	return _sdt_dis(cc);
}

int arm7::_str_postdown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_postup(){
	printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_predown(){
	printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_preup(){
	u8 rd;
	u32 a;

	DP_REG_OPERAND(a,IMM_SHIFT);
	if((rd = (u8)DEST_REG_INDEX) != 15){
       WL(BASE_REG + a,REG_(rd));
    }
	else
       printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_predownwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_preupwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_postdownimm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_postupimm(){
	u32 *regs,rd;

	regs = &BASE_REG;
	if((rd = DEST_REG_INDEX) != 15){
       WL(*regs,REG_(rd));
	}
	else{
       WL(*regs,_pc);
	}
	*regs += _opcode & 0xFFF;
	__ARMF(str);
	return 2;
}

int arm7::_str_predownimm(){
	printf("%s %x\n",__FUNCTION__,_pc);
	__ARMF(str);
	return 2;
}

int arm7::_str_preupimm(){
	u8 rd;

	if((rd = (u8)DEST_REG_INDEX) != 15){
		WL(BASE_REG + (_opcode & 0xFFF),REG_(rd));
	}
	else{
		WL(BASE_REG + (_opcode & 0xFFF),REG_(15)+4);
	}
	return 1;
}

int arm7::_str_predownimmwb(){
   u32 base,*regs;

   base = *(regs = &BASE_REG) - (_opcode & 0xFFF);
   WL(base,DEST_REG);
   *regs = base;
   __ARMF(str);
   return 2;
}

int arm7::_str_preupimmwb(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_str_dis(char *cc){
	__ARM_COND_PRE("str");
	return _sdt_dis(cc);
}

int arm7::_stm(){
	u32 *regs,base,stack,*p3,*p2,reg1[16];
	u8 r;

	stack = *(regs = &(p3 = (u32 *)_pregs)[(_opcode >> 16) & 0xF]);
	p2 = reg1;
	if((_opcode & 0x400000) && (_cpsr & 0x1F) != USER_MODE){
		for(u16 p1 = 1;p1 > 0;p1 <<= 1,p3++){
			if((_opcode & p1)){
				if(p1 == 0x2000){
					*p2++ = ((u32 *)_regs)[13];
					continue;
				}
				else if(p1 == 0x4000){
					*p2++ = ((u32 *)_regs)[14];
					continue;
				}
				*p2++ = *p3;
			}
		}
	}
	else{
		for(u16 p1 = 1;p1 > 0;p1 <<= 1,p3++){
			if((_opcode & p1))
				*p2++ = *p3;
		}
	}
	r = (u8)(p2 - reg1);
	if((_opcode & 0x800000)){
		base = stack + (r << 2);
		if((_opcode & 0x1000000))
			stack += 4;
	}
	else{
		stack = (base = stack - (r << 2));
		if(!(_opcode & 0x1000000))
			stack += 4;
	}
	if((_opcode & 0x200000))
		*regs = base;
	stack &= ~3;
	p2 = reg1;
	for(u8 i = r,r=0;i > 0;i--){
		WL(stack,*p2++);
		stack += 4;
		r += (u8)1;
	}
	return r+1+1;
}

int arm7::_stm_dis(char *cc){
	ARMSMDT_("stm");
	return 0;
}

int arm7::_ldm(){
	u32 *regs,base,stack,*p3;
	u8 r,i;
	u16 p1;
	u64 reg1[16],*p2;

	stack = *(regs = &(p3 = (u32 *)_pregs)[(_opcode >> 16) & 0xF]) & ~3;
	p2 = reg1;
	if((_opcode & 0x408000) == 0x400000 && (_cpsr & 0x1F) != USER_MODE){
		for(p1 = 1;p1 > 0;p1 <<= 1,p3++){
			if(_opcode & p1){
				if(p1 == 0x2000){
					*p2++ = (u64)&((u32 *)_regs)[13];
					continue;
				}
				else if(p1 == 0x4000){
					*p2++ = (u64)&((u32 *)_regs)[14];
					continue;
				}
				*p2++ = (u64)p3;
			}
		}
	}
	else{
		for(p1 = 1;p1 > 0;p1 <<= 1,p3++){
			if(_opcode & p1)
				*p2++ = (u64)p3;
		}
	}
	r = (u8)(p2 - reg1);
	if((_opcode & 0x800000)){
		base = stack + (r << 2);
		if(_opcode & 0x1000000)
			stack += 4;
	}
	else{
		stack = (base = stack - (r << 2));
		if(!(_opcode & 0x1000000))
			stack += 4;
	}
	p2 = reg1;
	i = (u8)(r-1);
	r = 0;
	RL(stack,*((u32 *)*p2));
	stack += 4;
	if((_opcode & 0x200000))
		*regs = base;
	for(p2++;i > 0;i--,p2++){
		RL(stack,*((u32 *)*p2));
		stack += 4;
		r += (u8)1;
	}
	if(_opcode & 0x8000){
		//printf("%x %s load pc %x\n",_pc,__FUNCTION__,REG_(15));
	//	if(REG_(15) == 0x30050a4) EnterDebugMode();
		if(_opcode & 0x400000){
			EnterDebugMode();
		}
		else{
			if(REG_(15) & 1){
				EnterDebugMode();
				REG_(15) &= ~1;
				_cpsr |= T_BIT;
				REG_(15) -= 2;
			}
			else
				REG_(15) -= 4;
		}
		_pc=REG_(15);
		LOADCALLSTACK(_pc+4);
	}
	__ARMF(ldm);
	return r+1+1+1;
}

int arm7::_ldm_dis(char *cc){
	ARMSMDT_("ldm");
	return 0;
}

int arm7::_sdt_dis(char *cc){
	u32 temp,adress=_pc;
	char string [33], string2 [33];

	//standard_debug_handle(arm_opcode_strings,op,adress,dest,cpu);
	strcat(cc,register_strings[(_opcode>>12)&0xF]);
	if((_opcode & 0x4000000) == 0) goto A;

	if((_opcode & 0x2000000)) {
		strcat(cc, ", [");
		strcat(cc, register_strings[(_opcode>>16)&0xF]);
		if(!(_opcode&0x1000000))
           strcat(cc, "]");
		strcat(cc, ", ");
		strcat(cc, register_strings[_opcode&0xF]);
		strcpy(string, ", ");
		strcat(string, shift_strings[(_opcode>>5)&0x3]);
		temp = (_opcode>>7)&0x1F;
 		if(!temp){
			switch((_opcode>>5)&0x3) {
				case 0:
               break;
				case 1:
                   strcpy(string2, ", ");
                   strcat(string2, "lsr 0x20");
                   strcat(cc, string2);
               break;
				case 2:
                   strcpy(string2, ", ");
                   strcat(string2, "asr 0x20");
                   strcat(cc, string2);
               break;
				case 3:
                   strcpy(string2, ", ");
                   strcat(string2, "rrx");
                   strcat(cc, string2);
               break;
			}
		}
		else{
			strcat(string," ");
			sprintf(string2,"0x%08X",temp);
			strcat(string,string2);
			strcat(cc,string);
		}
		if((_opcode&0x1000000)){
			strcat(cc, "]");
			if((_opcode&0x200000))
               strcat(cc, "!");
		}
	}
	else{
		temp = (_opcode&0xFFF);
		if(temp){
			if((((_opcode>>16)&0xF)==15) && (_opcode&0x100000)) {
				if(_opcode&0x800000)
                   temp = adress+8+temp;
				else
                   temp = adress+8-temp;
				if(_opcode&0x400000)
                   RBPC(temp,temp)
				else
                   RLPC(temp,temp);
				sprintf(string,", 0x%X",temp);
				strcat(cc, string);
			}
			else {
				strcat(cc, ", [");
				strcat(cc, register_strings[(_opcode>>16)&0xF]);
				if(!(_opcode&0x1000000))
                   strcat (cc, "]");
				if((_opcode&0x800000))
                   strcat(cc, ", ");
				else
                   strcat(cc, ", -");
				sprintf(string,"0x%08X",temp);
				strcat(cc, string);
				if((_opcode&0x1000000)) {
					strcat(cc,"]");
					if((_opcode&0x200000))
                       strcat(cc,"!");
				}
			}
		}
		else {
			strcat(cc,", [");
			strcat(cc,register_strings[(_opcode>>16)&0xF]);
			strcat(cc,"]");
		}
	}
	return 0;
A:
	strcat(cc, ", [");
	strcat(cc, register_strings[(_opcode>>16)&0xF]);
	if((_opcode & 0x400000)){
		if((temp = (((_opcode&0xF00)>>4)|(_opcode&0xF))) != 0){
			sprintf(string,"0x%X",temp);
			if((_opcode & 0x800000) != 0)
				strcat(cc,"+");
			else
				strcat(cc,"-");
			strcat(cc,string);
		}
	}
	else{
		if((_opcode & 0x800000) != 0)
			strcat(cc,"+");
		else
			strcat(cc,"-");
		strcat(cc,register_strings[_opcode&0xF]);
	}
	strcat(cc,"]");
	return 0;
}

int arm7::_msr_cpsr(){
	u32 value;
	u8 field;

	if((_opcode & 0x2000000)){
       DP_IMM_OPERAND(value);
	}
	else
       value = OP_REG;
	field = (u8)((_opcode >> 16) & 0xF);
	if((_cpsr & 0x1F) != USER_MODE){
		if((field & 1)){
			if((_cpsr & 0x1F) != (value & 0x1F))
				switchmode((u8)(value & 0x1F),1);
			_cpsr = (_cpsr & 0xFFFFFF00) | (value & 0xDF);
		}
		if((field & 2))
			_cpsr = (_cpsr & 0xFFFF00FF) | (value & 0x0000FF00);
		if((field & 4))
			_cpsr = (_cpsr & 0xFF00FFFF) | (value & 0x00FF0000);
		if((field & 8))
			_cpsr = (_cpsr & 0xFFFFFF) | (value & 0xFF000000);
	}
	return 0;
}

int arm7::_msr_cpsr_dis(char *cc){
	ARMMSR_("msr");
	return 0;
}

int arm7::_msr_spsr(){
	u32 value;
	u8 field;

	if((_opcode & 0x2000000)){
       DP_IMM_OPERAND(value);
	}
	else
		value = OP_REG;
	field = (u8)((_opcode >> 16) & 0xF);
	if((field & 8))
		REG_SPSR = (REG_SPSR & ~0xFF000000) | (0xFF000000 & value);
	if((field & 4))
		REG_SPSR = (REG_SPSR & ~0x00FF0000) | (0x00FF0000 & value);
	if((field & 2))
		REG_SPSR = (REG_SPSR & ~0x0000FF00) | (0x0000FF00 & value);
	if((field & 1))
		REG_SPSR = (REG_SPSR & ~0x000000FF) | ((u8)value);
	return 1;
}

int arm7::_msr_spsr_dis(char *cc){
	ARMMSR_("msr");
	return 0;
}

int arm7::_mrs_cpsr(){
	DEST_REG=_cpsr;
	__ARMF(mrs_cpsr);
	return 1;
}

int arm7::_mrs_cpsr_dis(char *cc){
	ARMMSR_("mrs");
	return 0;
}

int arm7::_mrs_spsr(){
	DEST_REG = REG_SPSR;
	__ARMF(mrs_spsr);
	return 1;
}

int arm7::_mrs_spsr_dis(char *cc){
	ARMMSR_("mrs");
	return 0;
}

int arm7::_tbx(){
	if((_opcode & 0x80))
		REG_(14) = (_pc - 2) | 1;
	_pc = REG_((_opcode>> 3) & 0xf);
	if(!(_pc & 1)){
		_pc = (_pc & ~3) - 2;
		_cpsr &= ~T_BIT;
		Query(IMACHINE_QUERY_DEBUG_NOTIFY,0);
		//_status |= 0x100000000;
	}
	else
		_pc = (_pc & ~1) -2;
	LOADCALLSTACK(_pc+2);
	__ARMF(tbx);
	return 3;
}

int arm7::_tbx_dis(char *cc){
	sprintf(cc,"bx %s",register_strings[(_opcode>>3)&0xf]);
	return 0;
}

int arm7::_tbl(){
	int temp;
   u8 h,res;

   res = (u8)5;
   h = (u8)((_opcode >> 11) & 0x3);
   if(h == 3){
       temp = REG_(14);
       REG_(14) = (_pc - 2) | 1;
       _pc = (u32)temp + ((_opcode & 0x7FF) << 1);
       if(!(_pc & 1)){
           _pc = (_pc & ~3) + 4;
           _cpsr &= ~T_BIT;
       }
       else
           _pc = (_pc & ~1) + 2;
       return res;
   }
   STORECALLLSTACK(_pc+4);
   REG_(14) = REG_(15) | 1;
   temp = ((_opcode & 0x7ff) << 12);
   RWPC(_pc+2,_opcode);
   h = (u8)((_opcode >> 11) & 0x3);
   if(((temp |= ((_opcode & 0x7ff) << 1)) & 0x400000))
       temp = -(0x800000 - temp);
   _pc += temp;
   if(h == 1){
       _pc = (_pc & ~3) + 4;
       _cpsr &= ~T_BIT;
   }
   else
       _pc += 2;
   return res;
}

int arm7::_tbl_dis(char *cc){
	int temp;

	if(((_opcode >> 11) & 3) == 2){
		temp = ((_opcode & 0x7ff) << 12);
		RWPC(_pc+2,_opcode);
		if(((temp |= ((_opcode & 0x7ff)<<1)) & 0x400000))
			temp = -(0x800000-temp);
		sprintf(cc,"bl 0x%08X",_pc+temp+4);
	}
	else
		sprintf(cc,"**blh 0x%08X**",(_opcode & 0x7ff));
	return 0;
}

int arm7::_tbu(){
   int offset;

	if((offset = (_opcode & 0x7ff)) & 0x400)
		offset = 0 - (0x800 - offset);
	_pc += (offset << 1) + 2;
	__ARMF(tbu);
	return 1;
}

int arm7::_tbu_dis(char *cc){
	int offset;

	if((offset = (_opcode & 0x7ff)) & 0x400)
		offset = 0 - (0x800 - offset);
	sprintf(cc,"b 0x%08X",_pc + (offset << 1) + 4);
	return 0;
}

int arm7::_tb(){
	int offset;

	__ARMF(tb);
	switch(((_opcode>>8) & 0xF)){
		case 0: // beq
			if(!(_cpsr & Z_BIT))
				return 1;
		break;
		case 1: //BNE
			if((_cpsr & Z_BIT))
				return 1;
		break;
		case 2: //BCS
			if(!(_cpsr & C_BIT))
				return 1;
		break;
		case 3: //BCC
			if((_cpsr & C_BIT))
				return 1;
		break;
		case 4: //BMI
			if(!(_cpsr & N_BIT))
				return 1;
		break;
		case 5://BPL
			if((_cpsr & N_BIT))
				return 1;
		break;
		case 6: //BVS
			if(!(_cpsr & V_BIT))
				return 1;
		break;
		case 7: //BVC
			if((_cpsr & V_BIT))
				return 1;
		break;
		case 8: // BHI
			//if(!(c_flag && !z_flag))
			if(! ((_cpsr & (C_BIT|Z_BIT)) == C_BIT) )
				return 1;
		break;
		case 9: //BLS
			//if(!(!c_flag || z_flag))
			if(! ((_cpsr & C_BIT) == 0 || (_cpsr & Z_BIT)) )
				return 1;
		break;
		case 10:{ //BGE
			u32 a;

			// if(v_flag == n_flag)
			a=_cpsr & (V_BIT|N_BIT);
			if(! (a==0 || a == (V_BIT|N_BIT)) )
				return 1;
		}
		break;
		case 11:{ // BLT
			u32 a;
			//  if(v_flag != n_flag)
			a=_cpsr & (V_BIT|N_BIT);
			if( (a==0 || a == (V_BIT|N_BIT)) )
				return 1;
		}
		break;
		case 12:{ // BGT
			u32 a;

			//if(!z_flag && n_flag == v_flag)
			if(! ((_cpsr & Z_BIT)==0 && (a=(_cpsr & (V_BIT|N_BIT)) == 0 || a == (V_BIT|N_BIT))) )
				return 1;
		}
		break;
		case 13:{ // BLE
			u32 a;

			//if(z_flag || n_flag != v_flag)
			if(! ((_cpsr & Z_BIT) || ((a=(_cpsr & (N_BIT|V_BIT))) && a != (N_BIT|V_BIT)) ))
				return 1;
		}
		break;
	}
	if(((offset = ((u8)_opcode) << 1) & 0x100))
		offset = -(0x200 - offset);
	_pc += offset + 2;
	return 4;
}

int arm7::_tb_dis(char *cc){
	int offset;

	if(((offset = ((u8)_opcode) << 1) & 0x100))
       offset = -(512 - offset);
	sprintf(cc,"b%s 0x%08X",condition_strings[SR(_opcode,8)&0xf],_pc+offset + 4);
	return 0;
}

int arm7::_tldr_pc(){
	RLPC((_pc & ~3) + SL((u8)_opcode,2) + 4,REG_((_opcode >> 8) & 7));
	__ARMF(tldr_pc);
	return 4;
}

int arm7::_tldr_pc_dis(char *cc){
	strcpy(cc,"ldr ");
	return _treg_dis(cc);
}

int arm7::_tadd_pc_reg(){
	REG_((_opcode >> 8) & 7) = (_pc + 4) + (((u8)_opcode)<<2);
	__ARMF(tadd_pc_reg);
	return 1;
}

int arm7::_tadd_pc_reg_dis(char *cc){
	strcpy(cc,"add ");
	return _treg_dis(cc);
}

int arm7::_tldr_sp(){
	RL(REG_(13) + (((u8)_opcode)<<2), REG_((_opcode >> 8) & 7));
	__ARMF(tldr_sp);
	return 1;
}

int arm7::_tldr_sp_dis(char *cc){
	strcpy(cc,"ldr ");
	return _treg_dis(cc);
}

int arm7::_tstr_sp(){
	WL(REG_(13) + (((u8)_opcode)<<2), REG_((_opcode >> 8) & 7));
	__ARMF(tstr_sp);
	return 1;
}

int arm7::_tstr_sp_dis(char *cc){
	strcpy(cc,"str ");
	return _treg_dis(cc);
}

int arm7::_tmov_imm(){
	SET_DP_FLAGS((REG_((_opcode >> 8) & 7) = (u8)_opcode));
	__ARMF(tmov_imm);
	return 1;
}

int arm7::_tmov_imm_dis(char *cc){
	 sprintf(cc,"mov %s,0x%02X",register_strings[(_opcode>>8)&0x7],(u8)_opcode);
	 return 0;
}

int arm7::_tadd_imm(){
	SET_ADD_FLAGS(REG_((_opcode >> 8) & 7),REG_((_opcode >> 8) & 7),(u8)_opcode);
	__ARMF(tadd_imm);
	return 1;
}

int arm7::_tadd_imm_dis(char *cc){
	sprintf(cc,"add %s,0x%02X",register_strings[(_opcode>>8)&0x7],(u8)_opcode);
	return 0;
}

int arm7::_tadd_reg(){
	SET_ADD_FLAGS(REG_(_opcode & 7),REG_((_opcode >> 3) & 7),REG_((_opcode >> 6) & 7));
	__ARMF(tadd_reg);
	return 1;
}

int arm7::_tadd_reg_dis(char *cc){
	sprintf(cc,"add %s, [%s, %s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[SR(_opcode,6)&7]);
	return 0;
}

int arm7::_tadd_short_imm(){
	SET_ADD_FLAGS(REG_(_opcode & 7) ,REG_((_opcode >> 3) & 7),((_opcode >> 6) & 7));
	__ARMF(tadd_short_imm);
	return 1;
}

int arm7::_tadd_short_imm_dis(char *cc){
	sprintf(cc,"add %s,[%s, 0x%1X]",register_strings[_opcode&7], register_strings[(_opcode>>3)&0x7],((_opcode >> 6) & 7));
	return 0;
}

int arm7::_tsub_reg(){
	SET_SUB_FLAGS(REG_(_opcode & 7),REG_((_opcode >> 3) & 7),REG_((_opcode >> 6) & 7));
	__ARMF(tsub_reg);
	return 1;
}

int arm7::_tsub_reg_dis(char *cc){
	sprintf(cc,"sub %s, [%s, %s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[SR(_opcode,6)&7]);
	return 0;
}

int arm7::_tsub_imm(){
	SET_SUB_FLAGS(REG_((_opcode >> 8) & 7),REG_((_opcode >> 8) & 7),(u8)_opcode);
	__ARMF(tsub_imm);
	return 1;
}

int arm7::_tsub_imm_dis(char *cc){
	sprintf(cc,"sub %s,0x%02X",register_strings[(_opcode>>8)&0x7],(u8)_opcode);
	return 0;
}

int arm7::_tsub_short_imm(){
	SET_SUB_FLAGS(REG_(_opcode & 7) ,REG_((_opcode >> 3) & 7),((_opcode >> 6) & 7));
	__ARMF(tsub_short_imm);
	return 1;
}

int arm7::_tsub_short_imm_dis(char *cc){
	sprintf(cc,"sub %s,[%s, 0x%1X]",register_strings[_opcode&7], register_strings[(_opcode>>3)&0x7],((_opcode >> 6) & 7));
	return 0;
}

int arm7::_tsub_hi(){
	u8 rd;

	if((rd = (u8)(((_opcode & 0x80) >> 4) | (_opcode & 7))) == 15){
       _pc -= REG_((_opcode >> 3) & 0xF) + 2;
       return 4;
	}
	REG_(rd) -= REG_((_opcode >> 3) & 0xF);
	return 1;
}

int arm7::_tsub_hi_dis(char *cc){
	sprintf(cc,"sub %s,%s",register_strings[((_opcode & 0x80) >> 4)|(_opcode&0x7)],register_strings[(_opcode >> 3) & 0xF]);
	return 0;
}

int arm7::_tand_reg(){
	SET_DP_FLAGS(	REG_(_opcode & 7) &= REG_(SR(_opcode,3)&7)	);
	__ARMF(tand_reg);
	return 1;
}

int arm7::_tand_reg_dis(char *cc){
	sprintf(cc,"and %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tcmp_imm(){
	u32 a;

	SET_SUB_FLAGS(a,REG_(SR(_opcode,8)&0x7),(u8)_opcode);
	__ARMF(tcmp_imm);
	return 1;
}

int arm7::_tcmp_imm_dis(char *cc){
	 sprintf(cc,"cmp %s,0x%02X",register_strings[(_opcode>>8)&0x7],(u8)_opcode);
	 return 0;
}

int arm7::_tcmp_reg(){
	u32 a;

	SET_SUB_FLAGS(a,REG_(_opcode&7),REG_(SR(_opcode,3)&0x7));
	__ARMF(tcmp_reg);
	return 1;
}

int arm7::_tcmp_reg_dis(char *cc){
	 sprintf(cc,"cmp %s,%s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	 return 0;
}

int arm7::_tadd_hi(){
	u8 rd;

	if((rd = (u8)(((_opcode & 0x80) >> 4) | (_opcode & 7))) == 15){
       _pc += REG_((_opcode >> 3) & 0xF) + 2;
       return 4;
	}
	REG_(rd) += REG_((_opcode >> 3) & 0xF);
	return 1;
}

int arm7::_tadd_hi_dis(char *cc){
	sprintf(cc,"add %s,%s",register_strings[((_opcode & 0x80) >> 4)|(_opcode&0x7)],register_strings[(_opcode >> 3) & 0xF]);
	return 0;
}

int arm7::_tmov_hi(){
	u8 rd;

	rd = (u8)(((_opcode & 0x80) >> 4) | (_opcode & 7));
	REG_(rd) = REG_((_opcode >> 3) & 0xF);;
	if(rd==15){
		_pc=(REG_(15) & ~1)-2;
		return 4;
	}
	return 1;
}

int arm7::_tmov_hi_dis(char *cc){
	sprintf(cc,"mov %s,%s",register_strings[((_opcode & 0x80) >> 4)|(_opcode&0x7)],register_strings[(_opcode >> 3) & 0xF]);
	return 0;
}

int arm7::_tsub_sp(){
	REG_(13) -= SL(_opcode &0x7f,2);
	return 1;
}

int arm7::_tsub_sp_dis(char *cc){
	strcpy(cc,"sub ");
	return _treg_dis(cc);
}

int arm7::_tadd_sp(){
	REG_(13) += SL(_opcode &0x7f,2);
	return 1;
}
int arm7::_tadd_sp_dis(char *cc){
	strcpy(cc,"sub ");
	return _treg_dis(cc);
}

int arm7::_tadd_sp_reg(){
	REG_((_opcode >> 8) & 7) = REG_(13) + (((u8)_opcode)<<2);
	__ARMF(tadd_sp);
	return 1;
}

int arm7::_tadd_sp_reg_dis(char *cc){
	strcpy(cc,"add ");
	return _treg_dis(cc);
}

int arm7::_tlsl_imm(){
	u32 temp1,sh;

	temp1=REG_((_opcode >> 3) & 7);
	_cpsr &= ~(C_BIT|Z_BIT|N_BIT);
	if((sh = ((_opcode>>6) & 0x1F))){
		temp1 = SL(temp1, sh - 1);
		if(temp1 & 0x80000000)
			_cpsr |= C_BIT;
		temp1=SL(temp1,1);
	}
	if(!temp1)
		_cpsr |= Z_BIT;
	else if(temp1 & 0x80000000)
		_cpsr |= N_BIT;
	REG_(_opcode & 7) = temp1;
	__ARMF(tlsl_imm);
	return 1;
}

int arm7::_tlsl_imm_dis(char *cc){
	sprintf(cc,"lsl %s, [%s, 0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tlsl_reg(){
	u8 shift;
	u32 *regd;
	u32 temp1;

	temp1 = *(regd = &REG_(_opcode & 7));
	shift = (u8)REG_((_opcode >> 3) & 7);
	if(shift && shift < 32){
		_cpsr &= ~(C_BIT|Z_BIT|N_BIT);
		temp1=SL(temp1,shift-1);
		if(temp1 & 0x80000000)
			_cpsr |= C_BIT;
		temp1=SL(temp1,1);
		if(!temp1)
			_cpsr |= Z_BIT;
		else if(temp1 & 0x80000000)
			_cpsr |= N_BIT;
	}
	else if(shift > 31){
		_cpsr &= ~(C_BIT|Z_BIT|N_BIT);
		if((temp1=SL(temp1,31)))
			_cpsr |= C_BIT|N_BIT;
		else
			_cpsr |= Z_BIT;
	}
	*regd=temp1;
	__ARMF(tlsl_reg);
	return 1;
}

int arm7::_tlsl_reg_dis(char *cc){
	sprintf(cc,"lsl %s %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tlsr_imm(){
	u32 temp1,sh;

	sh = ((_opcode>>6) & 0x1F);
	temp1=REG_((_opcode >> 3) & 7);
	if(sh){
		temp1 = SR(temp1, sh-1);
		_cpsr &= ~(C_BIT|N_BIT|Z_BIT);
		if(temp1 & 1) _cpsr |= C_BIT;
		temp1=SR(temp1,1);
	}
	if(!temp1)
		_cpsr |= Z_BIT;
	REG_(_opcode & 7) = temp1;
	__ARMF(tlsr_imm);
	return 1;
}

int arm7::_tlsr_imm_dis(char *cc){
	sprintf(cc,"lsr %s, [%s, 0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tlsr_reg(){
	u8 shift;
	u32 *regd,temp1;

	temp1 = *(regd = &REG_(_opcode & 7));
	shift = (u8)REG_((_opcode >> 3) & 7);
	if(shift && shift < 32){
		_cpsr &= ~(C_BIT|Z_BIT|N_BIT);
		temp1=SR(temp1,shift-1);
		if(temp1 & 1)
			_cpsr |= C_BIT;
		temp1=SR(temp1,1);
		if(!temp1)
			_cpsr |= Z_BIT;
		else if(temp1 & 0x80000000)
			_cpsr |= N_BIT;
	}
	else if(shift > 31){
		_cpsr &= ~(C_BIT|Z_BIT|N_BIT);
		temp1=0;
		_cpsr |= Z_BIT;
	}
	*regd=temp1;
	__ARMF(tlsr_reg);
	return 1;
}

int arm7::_tlsr_reg_dis(char *cc){
	sprintf(cc,"lsr %s %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tasr_imm(){
	u32 sh;
	s32 temp1;

	temp1=(s32)REG_((_opcode >> 3) & 7);
	sh = (_opcode>>6) & 0x1F;
	if(sh){
		temp1 = SR(temp1, sh-1);
		_cpsr &= ~(C_BIT|N_BIT|Z_BIT);
		if(temp1 & 1) _cpsr |= C_BIT;
		if(!(temp1=SR(temp1,1)))
			_cpsr |= Z_BIT;
	}
	REG_(_opcode & 7) = temp1;
	__ARMF(tasr_imm);
	return 1;
}

int arm7::_tasr_imm_dis(char *cc){
	sprintf(cc,"asr %s, [%s, 0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tasr_reg(){
	u8 shift;
	u32 *regd;
	s32 temp1;

	temp1 = *(regd = &REG_(_opcode & 7));
	shift = (u8)REG_((_opcode >> 3) & 7);
	if(shift && shift < 32){
		_cpsr &= ~(C_BIT|Z_BIT|N_BIT);
		temp1=SR(temp1,shift-1);
		if(temp1 & 1) _cpsr |= C_BIT;
		temp1=SR(temp1,1);
		if(!temp1)
			_cpsr |= Z_BIT;
		else if(temp1 & 0x80000000)
			_cpsr |= N_BIT;
	}
	else if(shift > 31){
		_cpsr &= ~(C_BIT|Z_BIT|N_BIT);
		if((temp1=SR(temp1,31)))
			_cpsr |= C_BIT|N_BIT;
		else
			_cpsr |= Z_BIT;
	}
	*regd=temp1;
	__ARMF(tasr_reg);
	return 1;
}

int arm7::_tasr_reg_dis(char *cc){
	sprintf(cc,"asr %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tor_reg(){
	SET_DP_FLAGS(	REG_(_opcode& 7) |= REG_(SR(_opcode,3)&7)	);
	__ARMF(tor_reg);
	return 1;
}

int arm7::_tor_reg_dis(char *cc){
	sprintf(cc,"or %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_teor_reg(){
	SET_DP_FLAGS(	REG_(_opcode& 7) ^= REG_(SR(_opcode,3)&7)	);
	__ARMF(teor_reg);
	return 1;
}

int arm7::_teor_reg_dis(char *cc){
	sprintf(cc,"eor %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tneg_reg(){
	SET_SUB_FLAGS(REG_(_opcode& 7),0, REG_(SR(_opcode,3)&7));
	__ARMF(tneg_reg);
	return 1;
}

int arm7::_tneg_reg_dis(char *cc){
	sprintf(cc,"neg %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tror_reg(){
	u8 s;
	u32 value,*regd;

	value = *(regd = &REG_(_opcode & 7));
	s = (u8)REG_((_opcode >> 3) & 7);
	if(s){
		s &= 0x1F;
		_cpsr &= ~C_BIT;
		if(s == 0){
			if(value & 0x80000000)
				_cpsr |= C_BIT;
		}
		else{
			u32 a = SR(value,s-1);
			if(a&1)
				_cpsr |= C_BIT;
			value = SL(value,31-s)|SR(a,1);
		}
	}
	SET_DP_FLAGS(*regd = value);
	return 1;
}

int arm7::_tror_reg_dis(char *cc){
	sprintf(cc,"ror %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tmul_reg(){
	SET_DP_FLAGS(	REG_(_opcode& 7) *= REG_(SR(_opcode,3)&7)	);
	__ARMF(tmul_reg);
	return 1;
}

int arm7::_tmul_reg_dis(char *cc){
	sprintf(cc,"mul %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tbic_reg(){
	SET_DP_FLAGS(	REG_(_opcode& 7) &= ~REG_(SR(_opcode,3)&7)	);
	__ARMF(tbic_reg);
	return 1;
}

int arm7::_tbic_reg_dis(char *cc){
	sprintf(cc,"bic %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tmvn_reg(){
	SET_DP_FLAGS(	REG_(_opcode& 7) = ~REG_(SR(_opcode,3)&7)	);
	__ARMF(tmvn_reg);
	return 1;
}

int arm7::_tmvn_reg_dis(char *cc){
	sprintf(cc,"mvn %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tcmn_reg(){
	u32 a;

	SET_ADD_FLAGS(a,REG_(_opcode&7),REG_(SR(_opcode,3)&0x7));
	__ARMF(tcmn_reg);
	return 1;
}

int arm7::_tcmn_reg_dis(char *cc){
	sprintf(cc,"cmn %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_ttst_reg(){
	SET_DP_FLAGS(	REG_(_opcode& 7) & REG_(SR(_opcode,3)&7)	);
	__ARMF(ttst_reg);
	return 1;
}

int arm7::_ttst_reg_dis(char *cc){
	sprintf(cc,"tst %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tsbc_reg(){
	SET_SUB_FLAGS(REG_(_opcode& 7),REG_(_opcode& 7),REG_(SR(_opcode,3)&7));
	__ARMF(tsbc_reg);
	return 1;
}

int arm7::_tsbc_reg_dis(char *cc){
	sprintf(cc,"sbc %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}
int arm7::_tadc_reg(){
	printf("%s %x\n",__FUNCTION__,_pc);
	return 1;
}

int arm7::_tadc_reg_dis(char *cc){
	sprintf(cc,"adc %s, %s",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7]);
	return 0;
}

int arm7::_tswi_imm(){
	OnException(2,(u8)_opcode);
	__ARMF(tswi_imm);
	return 1;
}

int arm7::_tswi_imm_dis(char *cc){
	sprintf(cc,"swi %X",(u8)_opcode);
	return 0;
}

int arm7::_tcmp_hi(){
	u32 a;

	SET_SUB_FLAGS(a,REG_(((_opcode & 0x80) >> 4)|(_opcode&0x7)),REG_((_opcode >> 3) & 0xF));
	return 1;
}

int arm7::_tcmp_hi_dis(char *cc){
	sprintf(cc,"cmp %s, %s",register_strings[((_opcode & 0x80) >> 4)|(_opcode&0x7)],register_strings[SR(_opcode,3)&0xf]);
	return 0;
}

int arm7::_tldrb_imm(){
	RB(REG_(SR(_opcode,3) & 7) + (((_opcode>>6)&0x1F)),REG_(_opcode & 7));
	__ARMF(tldrb_imm);
	return 1;
}

int arm7::_tldrb_imm_dis(char *cc){
	sprintf(cc,"ldrb %s,[%s,0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tstrb_imm(){
	WB(REG_(SR(_opcode,3) & 7) + (((_opcode>>6)&0x1F)),REG_(_opcode & 7));
	__ARMF(tstrb_imm);
	return 1;
}

int arm7::_tstrb_imm_dis(char *cc){
	sprintf(cc,"strb %s,[%s,0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tstrb_reg(){
	WB(REG_((_opcode >> 3) & 7) + REG_((_opcode >> 6) & 7),REG_(_opcode & 7));
	return 2;
}

int arm7::_tstrb_reg_dis(char *cc){
	sprintf(cc,"strb %s,[%s %s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[(_opcode >> 6) & 7]);
	return 0;
}

int arm7::_tldrsb_reg(){
	s8 a;

	RB(REG_((_opcode >> 3) & 7) + REG_((_opcode >> 6) & 7),a);
	REG_(_opcode&7)=(u32)(s32)a;
	__ARMF(tldrsb_reg);
	return 1;
}

int arm7::_tldrsb_reg_dis(char *cc){
	sprintf(cc,"ldrsb %s,[%s %s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[(_opcode >> 6) & 7]);
	return 0;
}

int arm7::_tldrh_imm(){
	RW(REG_(SR(_opcode,3) & 7) + SL(((_opcode>>6)&0x1F),1),REG_(_opcode & 7));
	__ARMF(tldrh_imm);
	return 1;
}

int arm7::_tldrh_imm_dis(char *cc){
	sprintf(cc,"ldrh %s,[%s,0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tstrh_imm(){
	WW(REG_(SR(_opcode,3) & 7) + SL(((_opcode>>6)&0x1F),1),REG_(_opcode & 7));
	__ARMF(tstrh_imm);
	return 1;
}

int arm7::_tstrh_imm_dis(char *cc){
	sprintf(cc,"strh %s,[%s,0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tldr_imm(){
	RL(REG_(SR(_opcode,3) & 7) + SL(((_opcode>>6)&0x1F),2),REG_(_opcode & 7));
	__ARMF(tldr_imm);
	return 1;
}

int arm7::_tldr_imm_dis(char *cc){
	sprintf(cc,"ldr %s,[%s,0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tldr_reg(){
	RL(REG_(SR(_opcode,3) & 7) + REG_(SR(_opcode,6)&7),REG_(_opcode & 7));
	__ARMF(tldr_reg);
	return 1;
}

int arm7::_tldr_reg_dis(char *cc){
	sprintf(cc,"ldr %s,[%s,%s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[SR(_opcode,6)&7]);
	return 0;
}

int arm7::_tstr_imm(){
	WL(REG_(SR(_opcode,3) & 7) + SL((_opcode>>6)&0x1F,2),REG_(_opcode & 7));
	__ARMF(tstr_imm);
	return 1;
}

int arm7::_tstr_imm_dis(char *cc){
	sprintf(cc,"str %s,[%s,0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tstr_reg(){
	WL(REG_((_opcode >> 3) & 7) + REG_((_opcode >> 6) & 7),REG_(_opcode & 7));
	return 2;
}

int arm7::_tstr_reg_dis(char *cc){
	sprintf(cc,"str %s,[%s %s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[(_opcode >> 6) & 7]);
	return 0;
}

int arm7::_tstrh_reg(){
	WW(REG_((_opcode >> 3) & 7) + REG_((_opcode >> 6) & 7),REG_(_opcode & 7));
	return 2;
}

int arm7::_tstrh_reg_dis(char *cc){
	sprintf(cc,"strh %s,[%s %s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[(_opcode >> 6) & 7]);
	return 0;
}

int arm7::_tldrh_reg(){
	u16 a;

	RW(REG_((_opcode >> 3) & 7) + REG_((_opcode >> 6) & 7),a);
	REG_(_opcode&7)=(u32)a;
	__ARMF(tldrh_reg);
	return 1;
}

int arm7::_tldrh_reg_dis(char *cc){
	sprintf(cc,"ldrh %s,[%s,%s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[(_opcode >> 6) & 7]);
	return 0;
}

int arm7::_tldrb_reg(){
	RB(REG_((_opcode >> 3) & 7) + REG_((_opcode >> 6) & 7),REG_(_opcode&7));
	__ARMF(tldrb_reg);
	return 1;
}

int arm7::_tldrb_reg_dis(char *cc){
	sprintf(cc,"ldrb %s,[%s,0x%02X]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],SR(_opcode,6)&0x1f);
	return 0;
}

int arm7::_tldrsh_reg(){
	u16 a;

	RW(REG_((_opcode >> 3) & 7) + REG_((_opcode >> 6) & 7),a);
	REG_(_opcode&7)=(u32)(s16)a;
	__ARMF(tldrsh_reg);
	return 1;
}

int arm7::_tldrsh_reg_dis(char *cc){
	sprintf(cc,"ldrsh %s,[%s,%s]",register_strings[_opcode&7],register_strings[SR(_opcode,3)&7],register_strings[SR(_opcode,6)&7]);
	return 0;
}

int arm7::_tpush(){
	u8 p,n;
	u32 *reg,adress;

	adress = (reg = &REG_(7))[6];
	n=0;
	if((_opcode & 0x100)){
		adress -= 4;
		WL(adress,REG_(14));
		n += (u8)1;
	}
	for(p = 0x80;p > 0;p >>= 1,reg--){
		if(!(_opcode & p))
			continue;
		adress -= 4;
		WL(adress,*reg);
		n += (u8)1;
	}
	REG_(13) = adress;
	__ARMF(tpush)
	return (u8)((n - 1) + 1 + 1);
}

int arm7::_tpush_dis(char *cc){
	u8 *p,s1[30];

	p = s1;
	if((_opcode & 0x800)){
		for(int i=0;i<8;i++){
			if((_opcode & (1 << i)))
				*p++ = (u8)i;
		}
		if((_opcode & 0x100))
			*p++ = 15;
	}
	else{
		if((_opcode & 0x100))
			*p++ = 14;
		for(int i=0;i<8;i++){
			if((_opcode & (1 << i)))
				*p++ = (u8)i;
		}
	}
	*p = 0xFF;
	strcpy(cc,"push ");
	_fillMultipleRegisterString(s1,cc);
	return 0;
}

int arm7::_tpop(){
	u8 n;
	u32 *reg,adress;
	u16 p;

	n = 0;
	adress = (reg = (u32 *)_pregs)[13];
	for(p = 1;p < 0x100;p <<= 1,reg++) {
       if(!(_opcode & p))
           continue;
       RL(adress,*reg);
       adress += 4;
       n += (u8)1;
	}
	if((_opcode & 0x100)){
       RL(adress,_pc);
       adress += 4;
       _pc = (_pc & ~1) - 2;
       LOADCALLSTACK(_pc+2);
	}
	REG_(13) = adress;
	__ARMF(tpop);
	return (1 + n + 1 + 1);
}

int arm7::_tpop_dis(char *cc){
	u8 *p,s1[30];

	p = s1;
	if((_opcode & 0x800)){
		for(int i=0;i<8;i++){
			if((_opcode & (1 << i)))
				*p++ = (u8)i;
		}
		if((_opcode & 0x100))
			*p++ = 15;
	}
	else{
		if((_opcode & 0x100))
			*p++ = 14;
		for(int i=0;i<8;i++){
			if((_opcode & (1 << i)))
				*p++ = (u8)i;
		}
	}
	*p = 0xFF;
	strcpy(cc,"pop ");
	_fillMultipleRegisterString(s1,cc);
	return 0;
}

int arm7::_tldm_ia(){
	u8 n;
	u32 address,*regd,*reg;
	u16 p;

	address = *(regd = &(reg = (u32 *)_pregs)[(_opcode >> 8) & 7]) & ~3;
	for(p=1,n=0;p < 0x100;reg++,p <<= 1){
       if(!(_opcode & p))
           continue;
       RL(address,*reg);
       if(reg == regd)
           regd = NULL;
       address += 4;
       n += (u8)1;
	}
	if(regd)
       *regd = address;
	__ARMF(tldm_ia);
	return n +1+1;
}

int arm7::_tldm_ia_dis(char *cc){
	u8 *p,s1[30];

	p = s1;
	sprintf(cc,"ldmia %s!,",register_strings[SR(_opcode,8)&7]);
	for(int i=0;i<8;i++){
       if((_opcode & (1 << i)))
           *p++ = (u8)i;
	}
	*p = 0xFF;
	_fillMultipleRegisterString(s1,cc);
	return 0;
}

int arm7::_tstm_ia(){
	u8 n;
	u32 address,*regd,*reg;
	u16 p;

	address = *(regd = &(reg = (u32 *)_pregs)[(_opcode >> 8) & 7]) & ~3;
	n = 0;
	for(p=1;p < 0x100;p <<= 1,reg++){
		if(!(_opcode & p))
			continue;
		WL(address,*reg);
		address += 4;
		n += (u8)1;
	}
	*regd = address;
	__ARMF(tstm_ia);
	return n + 1+1+1;
}

int arm7::_tstm_ia_dis(char *cc){
	u8 *p,s1[30];

	p = s1;
	sprintf(cc,"stmia %s!,",register_strings[SR(_opcode,8)&7]);
	for(int i=0;i<8;i++){
       if((_opcode & (1 << i)))
           *p++ = (u8)i;
	}
	*p = 0xFF;
	_fillMultipleRegisterString(s1,cc);
	return 0;
}

int arm7::_treg_dis(char *cc){
	char s[100];

	if((_opcode >> 11) == 0x9){
		u32 a;

		RLPC((_pc & ~3) + 4 + (((u8)_opcode) << 2),a);
		sprintf(s,"%s,0x%08X ==%08X",register_strings[(_opcode >> 8)&0x7],a,(_pc & ~3) + 4 + (((u8)_opcode) << 2));
	}
	else if(!(_opcode & 0x2000)) //ldr,str PC
		sprintf(s,"%s,sp,0x%03X",register_strings[(_opcode >> 8)&0x7],((u8)_opcode) << 2);
	else{
		if((_opcode & 0x1000))
			sprintf(s,"sp,sp,0x%03X",(_opcode & 0x7F)<<2);
		else{
			if((_opcode & 0x800))
				sprintf(s,"%s,sp,0x%03X",register_strings[(_opcode >> 8)&0x7],((u8)_opcode) << 2);
			else
				sprintf(s,"%s,pc,0x%03X",register_strings[(_opcode >> 8)&0x7],((u8)_opcode) << 2);
		}
	}
	strcat(cc,s);
	return 0;
}

void arm7::_fillMultipleRegisterString(u8 *s1,char *dest){
   u8 *p,enterLoop,*p1;

   enterLoop = 0;
   strcat(dest,"{");
   p = s1;
   while(*p != 0xFF){
       if(!enterLoop){
           if(((u64)p - (u64)s1) > 0)
               strcat(dest,",");
           strcat(dest,register_strings[*p]);
           p1 = p;
       }
       if(abs(*(p+1) - *p) == 1)
           enterLoop = 1;
       else{
           if(enterLoop){
				if(abs(*p1 - *p) > 1)
					strcat(dest,"-");
				else
					strcat(dest,",");
               strcat(dest,register_strings[*p]);
               enterLoop=0;
           }
       }
       p++;
   }
   strcat(dest,"}");
}

int arm7::cpu_mode(u8 m,char *p,u32 **pr){
	static char c[][15]={"USER","FIQ","IRQ","SUPERVISOR","UNDEFINED","ABORT","SYSTEM"};
	switch(m){
		case USER_MODE:
			if(p) p=c[0];
			if(pr) *pr=(u32 *)_regs;
		break;
		case FIQ_MODE:
			if(p) p=c[1];
			if(pr) *pr=&((u32 *)_regs)[51];
		break;
		case IRQ_MODE:
			if(p) p=c[2];
			if(pr) *pr=&((u32 *)_regs)[17];
		break;
		case SUPERVISOR_MODE:
			if(p) p=c[3];
			if(pr) *pr=&((u32 *)_regs)[34];
		break;
		case UNDEFINED_MODE:
			if(p) p=c[4];
		break;
		case ABORT_MODE:
			if(p) p=c[5];
		break;
		case SYSTEM_MODE:
			if(p) p=c[6];
			if(pr) *pr=(u32 *)_regs;
		break;
		default:
			return -1;
	}
	return 0;
}

void arm7::switchmode(u8 mode,u8 to){
	u32 *p,ss;
	int i;

	p = (u32 *)_pregs;
	if(!to)
		mode = (u8)((ss = REG_SPSR) & 0x1F);
	if((_cpsr & 0x1F) == (mode & 0x1F))
		goto switchmode_1;
	i = 13;
	switch(mode){
		case USER_MODE:
		case SYSTEM_MODE:
			p = (u32 *)_regs;
		break;
		case IRQ_MODE:
			p = (u32 *)&((u32 *)_regs)[17];
		break;
		case SUPERVISOR_MODE:
			p = (u32 *)&((u32 *)_regs)[34];
		break;
		case FIQ_MODE:
			p = (u32 *)&((u32 *)_regs)[51];
			i = 8;
		break;
	}
	memcpy(p,_pregs,sizeof(u32)*i);
	p[15] = _pc;
switchmode_1:
	if(to){
	    p[16] = _cpsr;
		_cpsr = (_cpsr & ~0x1F) | (mode & 0x1F);
	}
	else
		_cpsr=ss;
	//memcpy(_pregs,p,sizeof(u32)*16);
	_pregs=(u8 *)p;
}

};