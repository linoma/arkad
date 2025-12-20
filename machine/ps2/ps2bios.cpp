#include "ps2bios.h"

namespace ps2{

struct __resources{
	u32 addr,_sema,_thread,_cr_thread;
	struct  __heap{
		u32 addr,end,last;
	} heap;
	u32 _threads[256],_semas[256];
	struct{
		u32 addr,args;
	} _irqs[32][5];
} *_resources;

extern u32 *_sif_regs;

PS2BIOS::PS2BIOS() : PS2DEV() {
}

PS2BIOS::~PS2BIOS(){
}

int PS2BIOS::Reset(){
	if(PS2DEV::Reset())
		return -1;
	_resources=(struct __resources *)&_mem[0x1000];
	_resources->heap.addr=MB(3);
	_resources->heap.end=MB(32);
	_cb._reset();
	return 0;
}

int PS2BIOS::Init(PS2M &g){
	if(PS2DEV::Init(g))
		return -1;
	return 0;
}

int PS2BIOS::Run(u8 *a,int b,void *c){
	return PS2DEV::Run(a,b,c);
}

int PS2BIOS::_free_mem(u32 a){
	u8 *p;

	a=(a & ~3)-4;
	RMAP_(a,p,NOARG);
	//printf("free %x %x %d\n",a,*(u32 *)p,a==_heap.last);
	*p = (*p & ~1)|2;
	if(a==_resources->heap.last)
		_resources->heap.last--;
	return 0;
}

u32 PS2BIOS::_alloc_mem(u32 f,u32 size){
	u32 a,sz;

//printf("malloc %x %x %x %x\t",f,size,_heap.addr,_heap.end);
	if(!(a=_resources->heap.addr) || !size)
		return 0;

	for(a+=4,sz=(size + 3) & ~3;a < _resources->heap.end;){
		u8 *p;

		p=&_mem[a&(MS_RAM-1)];
		u32 attr = *((u32 *)p);

	//	printf(" %x:%x ",a,attr);
		//fflush(stdout);
		if(!(attr & 1) || a > _resources->heap.last){
			if(f & 1)
				memset(p+4,0,size);
			*((u32 *)p) = sz|1;
			a=(0x00000000|a) + 4;
			if(a > _resources->heap.last)
				_resources->heap.last=a-4;
			goto Z;
		}
		a += (attr & ~3) + 4;
	}
	a=0;
Z:
	//printf(" r=%x\n",a);
	return a;
}

int PS2BIOS::_biosCall(){
	s32 id = (s32)REG_(REGI_V0+1);
	DLOG("SYSCALL %x",REG_(REGI_V0+1));
	//	EnterDebugMode();
	switch(abs(id)){
		case 2://SetGsCrt = 2
			printf("syscall 2 %08x %08x %08x\n",(u32)REG_(REGI_A0),(u32)REG_(REGI_A0+1),(u32)REG_(REGI_A0+2));
			return 0;
		case 0x10:
			_resources->_irqs[REG_(REGI_A0)][0].addr=REG_(REGI_A0+1);
			printf("syscall 10 %08x %08x %08x\n",REG_(REGI_A0),REG_(REGI_A0+1),REG_(REGI_A0+2));
			//EnterDebugMode();
			return 0;
		case 0x12://AddDmacHandler
			_resources->_irqs[16+REG_(REGI_A0)][0].addr=REG_(REGI_A0+1);
			_resources->_irqs[16+REG_(REGI_A0)][0].args=REG_(REGI_A0+3);
			printf("syscall 12 %08x %08x %08x\n",(u32)REG_(REGI_A0),(u32)REG_(REGI_A0+1),(u32)REG_(REGI_A0+3));
//			EnterDebugMode();
			return 0;
		case 0x16:{//_EnableDmac
			u32 a =PS2IOREG(0x1000e010);
			a|= BV((u32)REG_(REGI_A0) + 16);
			WL(0x1000e010,a);
			printf("16 %x\n",(u32)REG_(REGI_A0));
		}
			return 0;
		case 0x20:{
			struct ThreadParam *p=(struct ThreadParam *)&_mem[REG_(REGI_A0) & (MS_RAM-1)];
			u32 a=sizeof(struct TCB);
			if(!p->stack)
				a+= p->stack_size;
			a = _alloc_mem(1,a);
			REG_(REGI_V0)=a;
			_resources->_thread++;
			printf("20 %x %x\n",REG_(REGI_A0),REG_(REGI_A0+1));
		}
			return 0;
		case 0x22:{
			struct TCB *p=(struct TCB *)&_mem[REG_(REGI_A0)];
		//	EnterDebugMode();
		}
			return 0;
		case 0x2f:
			REG_(REGI_V0)=_resources->_cr_thread;
			return 0;
		case 0x3c:{
			printf("3c %x %x %x %x %x\n",REG_(REGI_A0),REG_(REGI_A0+1),REG_(REGI_A0+2),REG_(REGI_A0+3)
				,REG_(REGI_A0+4));
			u32 c,b,a=sizeof(struct TCB);

			c=b=(u32)REG_(REGI_A0+1);
			if(b==-1){
				a += REG_(REGI_A0+2);
				c = (c + a + 4) & ~3;
			}
			a = _alloc_mem(1,a);
			_resources->_cr_thread=a;
			if(b==-1)
				c += a;
			REG_(REGI_V0)=c;
		}
			return 0;
		case 0x3d:{
			struct TCB *p=(struct TCB *)&_mem[_resources->_cr_thread];
			u32 b = REG_(REGI_A0+1),a=REG_(REGI_A0);
			if(b == -1)
				b = (MB(32) - a - 1) & ~3;
			if(a !=-1)
				p->heap_base = a+b;
			printf("3d %08x %08x %08x\n",_resources->heap.addr,REG_(REGI_A0),REG_(REGI_A0+1));
			//_resources->heap.addr=REG_(REGI_A0);
			}
			return 0;
		case 0x3e:{
			struct TCB *p=(struct TCB *)&_mem[_resources->_cr_thread];
			REG_(REGI_V0) = p->heap_base;
			//EnterDebugMode();
		}
			return 0;
		break;
		case 0x40:{
			void *p;

			p=&_mem[REG_(REGI_A0)&(MS_RAM-1)];
			u32 a=_alloc_mem(1,sizeof(struct sema));
			_resources->_sema++;
			//printf("create sema %08X %x %x %x\n",a,*(u32 *)(p+0),*(u32 *)(p+4),*(u32 *)(p+8));
			REG_(REGI_V0)=a;
		}
		break;
		case 0x41:{
			u32 a;
			void *p;

			a=REG_(REGI_A0)&(MS_RAM-1);
			p=&_mem[a];
			_free_mem(a);
			//printf("delete sema %08X %08x\n",(u32)REG_(REGI_A0),*((u32 *)p - 1));
		}
		break;
		case 0x42:{
			struct sema *p=(struct sema *)&_mem[REG_(REGI_A0)&(MS_RAM-1)];
		}
			return 0;
		case 0x44:{
			struct sema *p=(struct sema *)&_mem[REG_(REGI_A0)&(MS_RAM-1)];
			//EnterDebugMode();
		}
			return 0;
		case 0x4a:
		case 0x4b:{
			u32 *p=(u32 *)&_mem[REG_(REGI_A0)&(MS_RAM-1)];
			//EnterDebugMode();
		}
			return 0;
		case 0x56://SetTLBEntry
			return 0;
		case 0x64:
			return 0;
		case 0x75:
			printf(" %x %x\n",REG_(REGI_A0),REG_(REGI_A0+1));
			return 0;
		case 0x77:{//isceSifSetDma
			printf("77 %x %x\n",REG_(REGI_A0),REG_(REGI_A0+1));
			u32 *p=(u32 *)&_mem[REG_(REGI_A0) & (MS_RAM-1)];

			u32 a =PS2IOREG(0x1000e010);
			a |= BV(16+6);
			WL(0x1000e010,a);
			p += (REG_(REGI_A0+1)-1)*4;
			for(u32 n=0;n<REG_(REGI_A0+1);n++){
				printf("\t%x %x %x %x\n",p[0],p[1],p[2],p[3]);
				u32 *pp=(u32 *)&_mem[p[0] & (MS_RAM-1)];
				WL(0x1900c410,p[0]);
				WL(0x1000c420,SR(p[2]+5,3));
				WL(0x1000c400,0x101);
				p-=4;
				printf("\n");
			}
			printf("\n");
		//	EnterDebugMode();
		}
			return 0;
		case 0x79://sceSifSetReg
			//printf("79 %x %x\n",REG_(REGI_A0),REG_(REGI_A0+1));
			switch((u32)REG_(REGI_A0)){
				case 0x80000000://SIF_SYSREG_SUBADDR
					_sif_regs[4]=(u32)REG_(REGI_A0+1);
				break;
				case 0x80000001://SIF_REG_MAINADDR
					_sif_regs[5]=(u32)REG_(REGI_A0+1);
				break;
			}
			return 0;
		case 0x7a://sceSifGetReg
		//	printf("7a %x\n",(u32)REG_(REGI_A0));
			switch((u32)REG_(REGI_A0)){
				case 0x80000000://SIF_SYSREG_SUBADDR
					REG_(REGI_V0)=_sif_regs[4];//SIF_STAT_CMDINIT
					break;
				case 4://SIF_REG_SMFLAG
					REG_(REGI_V0)=_sif_regs[2];
					break;
				case 2:
					REG_(REGI_V0)=_sif_regs[1];
				break;
				case 1://SIF_REG_MAINADDR
				case 3://SIF_REG_MSFLAG
				break;
				case 0x80000002://SIF_SYSREG_RPCINIT
					REG_(REGI_V0)=_sif_regs[6];
					break;
			}
			return 0;
		case 0x7f:
			REG_(REGI_V0)=MB(32);
			return 0;
		default:
			printf("syscall %08x:%d %08x %08x\n",(u32)REG_(REGI_V0+1),(s32)REG_(REGI_V0+1),(u32)REG_(REGI_A0),_pc);
		break;
	}
	return -1;
}

int PS2BIOS::ReturnFromCall(){
	_cb._returnFromCall(&_pc,(RSZU *)_regs);
	if(_cb._doCall(&_pc,(RSZU *)_regs)){
		memcpy(CCore::_regs,_regs_copy,sizeof(_regs_copy));
		CP0._rfe(&_pc);
		//EnterDebugMode();
		return 1;
	}
	return 0;
}

int PS2BIOS::__calls::_addCall(u32 a,u32 pc,RSZU *r){
	cb.push_back({a,pc,r});
//	printf("addcall\n");
	return 0;
}

int PS2BIOS::__calls::_returnFromCall(u32 *pc,RSZU *r){
	__handler &h=cb.front();
	cb.pop_back();
	*pc=h._regs[0];
	memcpy(r,&h._regs[1],sizeof(h._regs)-4);
	return 0;
}

int PS2BIOS::__calls::_doCall(u32 *pc,RSZU *r){
	if(cb.size()==0) return 1;
	__handler &h=cb.front();
	*pc=h._adr;
	//memcpy(r,&h._regs[1],sizeof(h._regs)-4);
	r[REGI_RA]=0x1000;
	//EnterDebugMode();
	return 0;
}

int PS2BIOS::__calls::_reset(){
	cb.clear();
	return 0;
}

PS2BIOS::__calls::__handler::__handler(u32 a,u32 pc,RSZU *r,u32 attr){
	_adr=a;
	_regs[0]=pc;
	_attr=attr;
	if(r)
		memcpy(&_regs[1],r,sizeof(_regs)-sizeof(RSZU));
}

int PS2BIOS::Load(char *){
	char *p;
	u32 sz;

	FILE *fp=fopen("/home/lino/capcom/roms/ps2/bios/PS2 Bios 30004R V6 Pal.bin","rb");
	p=(char *)&_mem[MI_BIOS];
	sz=fread(p,1,MB(4),fp);

	for(u32 i=0;i<KB(512);i++){
		if(p[i]!='R')
			continue;
		if(!strstr(&p[i],"RESET"))
			continue;
		u32 ii=0;
		do{
			//printf("%s %08x %08x\n",&p[i],*(u32 *)&p[i+12],ii);
			if(strcmp(&p[i],"ROMVER")==0){
			//	printf("romver %llx\n",*(u64 *)&p[ii+8]);
			}
			else if(strcmp(&p[i],"KERNEL")==0){
			//	printf("kernel %llx\n",*(u64 *)&p[ii]);
				memcpy(p,&p[ii],KB(512));
			}
			ii+=*(u32 *)&p[i+12];
			i+= 16;
		}while(p[i]);
		break;
	}
	fclose(fp);

	//3C1A8001 FF5952B8 40196800 3C1A8001 3339007C 0359D021
	return 0;
}

int PS2BIOS::_enterIRQ(int n,u32 pc){
	if(PS2DEV::_enterIRQ(n,pc))
		return -10;
	//CP0._regs[CP0.EPC]=_pc;
	memcpy(_regs_copy,_regs,sizeof(_regs_copy));
	REG_SP=0x93380;
	switch(n){
		case 16:{
			for(u32 ev=0;ev<10;ev++){
				if(PS2IOREG(0x1000e010) & BV(ev)){
					if(_resources->_irqs[16+ev][0].addr){
						_cb._addCall(_resources->_irqs[16+ev][0].addr,_pc,(RSZU *)CCore::_regs);
						break;
					}
				}
			}
		}
		break;
	}
	if(!_cb._doCall(&_pc,(RSZU *)CCore::_regs)){
		//*(u32 *)&CCore::_mem[0x900]=0x42000018;
		goto Z;
	}
	memcpy(_regs,_regs_copy,sizeof(_regs_copy));
	CP0._rfe(&_pc);
Z:
	return 0;
}

};