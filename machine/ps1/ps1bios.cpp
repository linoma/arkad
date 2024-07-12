#include "ps1m.h"

namespace ps1{

#define GetEv() \
	ev = (REG_(REGI_A0) >> 24) & 0xf; \
	if (ev == 0xf) ev = 0x5; \
	ev*= 32; \
	ev+= REG_(REGI_A0)&0x1f;


#define GetSpec() \
	spec = 0; \
	switch (REG_(REGI_A0+1)) { \
		case 0x0301: spec = 16; break; \
		case 0x0302: spec = 17; break; \
		default: \
			for (i=0; i< 16; i++) if (REG_(REGI_A0+1) & (1 << i)) { spec = i; break; } \
			break; \
	}

#define EvStUNUSED	0x0000
#define EvStWAIT	0x1000
#define EvStACTIVE	0x2000
#define EvStALREADY 0x4000

#define EvMdINTR	0x1000
#define EvMdNOINTR	0x2000

PS1BIOS::PS1BIOS() : PS1DEV(){
}

PS1BIOS::~PS1BIOS(){
}

int PS1BIOS::Init(PS1M &g){
	Event = (EvCB *)&_mem[0x1000];
	HwEV = Event;
	EvEV = Event + 32;
	RcEV = Event + 32 * 2;
	UeEV = Event + 32 * 3;
	SwEV = Event + 32 * 4;
	ThEV = Event + 32 * 5;
	return PS1DEV::Init(g);
}

int PS1BIOS::Reset(){
	PS1DEV::Reset();

	CP0._regs[CP0.SR] |=0x5;
	memset(&_heap,0,sizeof(__heap));
	//Event=NULL;
	_pads._reset();
	_files._reset();
	_cb._reset();
	_hook_irq=NULL;
	memset(_regs_copy,0,sizeof(_regs_copy));
	memset(SysIntRP,0,sizeof(SysIntRP));
	return 0;
}

int PS1BIOS::_free(u32 a){
	u8 *p;

	a=(a & ~3)-4;
	RMAP_(a,p,NOARG);
	//printf("free %x %x %d\n",a,*(u32 *)p,a==_heap.last);
	*p = (*p & ~1)|2;
	if(a==_heap.last)_heap.last--;
	return 0;
}

u32 PS1BIOS::_malloc(u32 f,u32 size){
	u32 a,sz;

//printf("malloc %x %x %x %x\t",f,size,_heap.addr,_heap.end);
	if(!(a=_heap.addr) || !size)
		return 0;

	for(a+=4,sz=(size + 3) & ~3;a<_heap.end;){
		u8 *p;

		RMAP_(a,p,NOARG);
		u32 attr = *((u32 *)p);

	//	printf(" %x:%x ",a,attr);
		fflush(stdout);
		if(!(attr & 1) || a>_heap.last){
			if(f & 1)
				memset(p+4,0,size);
			*((u32 *)p) = sz|1;
			a=(0x80000000|a) + 4;
			if(a>_heap.last) _heap.last=a-4;
			goto Z;
		}
		a += (attr & ~3) + 4;
	}
	a=0;
Z:
	//printf(" r=%x\n",a);
	return a;
}

#define PS1_DELIVERY_EVENT(ev,spec){\
	if (Event[ev][spec].status == EvStACTIVE){\
		if (Event[ev][spec].mode == EvMdINTR) {\
			printf("de %x\n",ev);\
		}\
		else Event[ev][spec].status = EvStALREADY;\
	}\
}

int PS1BIOS::ExecA0(u32 a){
#ifdef _DEVELOP
	DLOG("BIOS call %X %x %x",0xA0,a,REG_(REGI_A0));
#endif
	switch(a){
		case 0://open
			return ExecB0(0x32);
		case 4://close
			return ExecB0(0x36);
		case 3://fwite
			return ExecB0(0x35);
		case 5://ioctl
		break;
		case 0x13:{//setjmp
			void *p;

			RMAP_(REG_(REGI_A0),p,NOARG);
			((u32 *)p)[0] = REG_(REGI_RA)+4;
			((u32 *)p)[1] = REG_(REGI_SP);
			((u32 *)p)[2] = REG_(REGI_FP);
			for (int i = 0; i < 8; i++) // s0-s7
				((u32 *)p)[3+i] = REG_(REGI_S0+i);
			((u32 *)p)[11] = REG_(REGI_GP);
			REG_(REGI_V0)=0;
		}
		break;
		case 0x15:{
			void *p,*p1;

			RMAP_(REG_(REGI_A0),p,NOARG);
			RMAP_(REG_(REGI_A0+1),p1,NOARG);
			strcat((char *)p,(char *)p1);
			REG_(REGI_V0) = REG_(REGI_A0);
		}
		break;
		case 0x17:{
			void *p,*p1;

			RMAP_(REG_(REGI_A0),p,NOARG);
			RMAP_(REG_(REGI_A0+1),p1,NOARG);
			REG_(REGI_V0) = strcmp((char *)p,(char *)p1);
		}
		break;
		case 0x18:{
			void *p,*p1;

			RMAP_(REG_(REGI_A0),p,NOARG);
			RMAP_(REG_(REGI_A0+1),p1,NOARG);
			REG_(REGI_V0) = strncmp((char *)p,(char *)p1,REG_(REGI_A0+2));
		}
		break;
		case 0x19:{
			void *p,*p1;

			RMAP_(REG_(REGI_A0),p,NOARG);
			RMAP_(REG_(REGI_A0+1),p1,NOARG);
			REG_(REGI_V0) = REG_(REGI_A0);
			strcpy((char *)p,(char *)p1);
		}
		break;
		case 0x1c:
		case 0x1e:{
			u8 *p,a;

			RMAP_(REG_(REGI_A0),p,NOARG);
			a=(u8)REG_(REGI_A0+1);
			for(int i=0;*p;i++){
				if(*p++ == a){
					REG_(REGI_V0) =REG_(REGI_A0) +i;
					return 0;
				}
			}
			REG_(REGI_V0) =0;
		}
		break;
		case 0x28:{
			u8 *p;

			RMAP_(REG_(REGI_A0),p,NOARG);
			for(u32 a =REG_(REGI_A0+1);a;a--)
				*p++=0;
		}
		break;
		case 0x2a:{
			void *p,*p1;

			RMAP_(REG_(REGI_A0),p,NOARG);
			RMAP_(REG_(REGI_A0+1),p1,NOARG);
			memcpy(p,p1,REG_(REGI_A0+2));
			REG_(REGI_V0)=REG_(REGI_A0);
		}
		break;
		case 0x2b:{
			u8 *p;

			RMAP_(REG_(REGI_A0),p,NOARG);
			memset(p,REG_(REGI_A0+1),REG_(REGI_A0+2));
			REG_(REGI_V0)=REG_(REGI_A0);
		}
		break;
		case 0x2c:{//memmove
			u8 *p,*p1;
			u32 len;

			RMAP_(REG_(REGI_A0),p,NOARG);
			RMAP_(REG_(REGI_A0+1),p1,NOARG);
			if(REG_(REGI_A0+1) <= REG_(REGI_A0) && (REG_(REGI_A0+1)+(len=REG_(REGI_A0+2)))>REG_(REGI_A0)){
				p+=++len;
				p1+=len;
				while(len--) *--p1=*--p;
			}
			else
				while(len--) *p1++=*p++;

			REG_(REGI_V0)=REG_(REGI_A0);
		}
		break;
		case 0x2f:{//rand
			u32 s = *(u32 *)&_mem[0x9010] * 1103515245 + 12345;
			REG_(REGI_V0) = (s >> 16) & 0x7fff;
			*(u32 *)&_mem[0x9010] = SWAP32(s);
		}
		break;
		case 0x30:
			*(u32 *)&_mem[0x9010] = SWAP32(REG_(REGI_A0));
		break;
		case 0x33://malloc
			REG_(REGI_V0)=_malloc(0,REG_(REGI_A0));
		break;
		case 0x34://free
			_free(REG_(REGI_A0));
		break;
		case 0x37://calloc
			REG_(REGI_V0)=_malloc(1,REG_(REGI_A0)*REG_(REGI_A0+1));
		break;
		case 0x39:{//InitHeap(void *block , int n)
			u32 size;
			u8 *p;

			if (((REG_(REGI_A0) & 0x1fffff) + REG_(REGI_A0+1)) >= 0x200000)
				size = 0x1ffffc - (REG_(REGI_A0) & 0x1fffff);
			else
				size = REG_(REGI_A0+1);
			size &= ~3;

			_heap.addr = REG_(REGI_A0);
			_heap.end = _heap.addr + size;
			RMAP_(_heap.addr,p,NOARG);
			//memset(p,0,size);
			*(u32*)(p)=size;
			*(u32*)(p)=0;

			//_mem[0]=0;
			//printf("he %x %x\n",_heap.addr,_heap.end);
			//EnterDebugMode();
		}
		break;
		case 0x3b:{
			int c=getchar();
			if(c==10) c=13;
			REG_(REGI_V0)=c;
		}
		break;
		case 0x3c:{
			int c=REG_(REGI_A0);
			//putchar(c==0xa?'>':c);
		}
		break;
		case 0x3e:{
			u8 *p;

			RMAP_(REG_(REGI_A0),p,NOARG);
			printf("%s",p);
			fflush(stdout);
		}
		break;
		case 0x3f:{//printf
			u32 a,n,i,u,sp[4];
			char c[1024],cc[1024],s[1024];

			EnterDebugMode(DEBUG_BREAK_OPCODE);
			for(i=0;i<4;i++)
				RL(REG_SP+i*4,sp[i]);
			WL(REG_SP,REG_(REGI_A0));
			WL(REG_SP+4,REG_(REGI_A0+1));
			WL(REG_SP+8,REG_(REGI_A0+2));
			WL(REG_SP+12,REG_(REGI_A0+3));

			a=REG_(REGI_A0);
			for(i=u=0,n=1;i<1024;){
				u8 v;

				RB(a,v);
				a++;
				if(!v)
					break;
				switch(v){
					case '%':
						u=0;
						cc[u++]=v;
A:
						RB(a,v);
						a++;
						switch(v){
							case 'l':
							case '.':
							case '-':
								cc[u++]=v;
								goto A;
							default:
								if(v >= '0' && v <= '9'){
									cc[u++]=v;
									goto A;
								}
								break;
						}
						cc[u++]=v;
						cc[u]=0;

						switch(tolower(v)){
							case 'f':{
								u32 vv;
								float f;

								RL(REG_SP+(n*4),vv);
								*((u32 *)&f)=vv;
								i += sprintf(&c[i], cc, f);
								n++;
							}
							break;
							case 'a':
							case 'e':
							case 'g':
							break;
							case 'p':
							case 'i':
							case 'u':
							case 'd':
							case 'o':
							case 'x':{
								u32 vv;

								RL(REG_SP+(n*4),vv);
								i += sprintf(&c[i], cc, vv);
								n++;
							}
							break;
							case 'c':{
								u32 vv;

								RL(REG_SP+(n*4),vv);
								i += sprintf(&c[i], cc, (u8)vv);
								n++;
							}
							break;
							case 's':{
								u32 vv;
								u8 *p;

								RL(REG_SP+(n*4),vv);
								n++;
								RMAP_(vv,p,NOARG);
								i += sprintf(&c[i], cc, (char *)p);
							}
							break;
							case '%':
								c[i++]=v;
							break;
						}
					break;
					default:
						c[i++]=v;
					break;
				}
			}
			c[i]=0;
			printf("%s",c);
			for(i=0;i<4;i++)
				WL(REG_SP+i*4,sp[i]);
		}
		break;
		case 0x44://flush cache
		break;
		case 0x49://GPU_cw
			WL(0x1F801810,REG_(REGI_A0));
		break;
		case 0x70:{
			//DeliverEvent(0x11, 0x2); // 0xf0000011, 0x0004
			//DeliverEvent(0x81, 0x2); // 0xf4000001, 0x0004
		}
		break;
		case 0x71://cd int
		case 0x72://cd remove
		break;
		case 0xab://card info
			PS1_DELIVERY_EVENT(0x81,2);
			REG_(REGI_V0)=1;
			printf("bios call %X %x %x\n",0xA0,a,_regs[REGI_A0]);
		break;
		case 0xac://card load
			PS1_DELIVERY_EVENT(0x81,2);
			REG_(REGI_V0)=1;
		//	EnterDebugMode();
			printf("bios call %X %x %x\n",0xA0,a,_regs[REGI_A0]);
		break;
		default:
			printf("bios call %X %x %x\n",0xA0,a,_regs[REGI_A0]);
			EnterDebugMode();
		break;
	}
	return 0;
}

int PS1BIOS::ExecB0(u32 a){
#ifdef _DEVELOP
	if(a != 0x17)
		DLOG("BIOS call %X %x %x",0xB0,a,REG_(REGI_A0));
#endif
	switch(a){
		case 0x7:{//psxBios_DeliverEvent
			int ev, spec,i;

			GetEv();
			GetSpec();
#ifdef _DEVELOPa
			DLOG("BIOS B0 7 ev;%d spec:%d %x",ev,spec,Event[ev][spec].status);
#endif
			if (Event[ev][spec].status == EvStACTIVE){
				if (Event[ev][spec].mode == EvMdINTR) {
				//softCall2(Event[ev][spec].fhandler);
				}
				else
					Event[ev][spec].status = EvStALREADY;
			}
		}
		break;
		case 0x8:{
			int ev, spec,i;

			GetEv();
			GetSpec();
#ifdef _DEVELOP
			DLOG("BIOS B0 8 ev;%d spec:%d",ev,spec);
#endif
			Event[ev][spec].status = EvStWAIT;
			Event[ev][spec].mode = REG_(REGI_A0+2);
			Event[ev][spec].fhandler = REG_(REGI_A0+3);

			REG_(REGI_V0) = ev | (spec << 8);
		}
		break;
		case 9:
			REG_(REGI_V0)=0;
		break;
		case 0xb:{//psxBios_TestEvent
			int ev, spec;

			ev   = (int)(u8)REG_(REGI_A0);
			spec = (int)(u8)(REG_(REGI_A0) >> 8);
			//printf("bios call %X %x %u %u\n",0xB0,a,ev,spec);
			if (Event[ev][spec].status == EvStALREADY) {
				Event[ev][spec].status = EvStACTIVE;
				REG_(REGI_V0) = 1;
			}
			else
				REG_(REGI_V0) = 0;
		}
		break;
		case 0xc:{
			int ev, spec;

			ev   = (int)(u8)REG_(REGI_A0);
			spec = (int)(u8)(REG_(REGI_A0) >> 8);
#ifdef _DEVELOP
			DLOG("BIOS %X %x %u %u %x  %x",0xB0,a,ev,spec,((u64)&Event[ev][spec].desc- (u64)Event),((u64)&RcEV[3] - (u64)Event));
#endif
			Event[ev][spec].status = EvStACTIVE;
			REG_(REGI_V0) = 1;
		}
		break;
		case 0xd:{
			int ev, spec;

			ev   = (int)(u8)REG_(REGI_A0);
			spec = (int)(u8)(REG_(REGI_A0) >> 8);
			Event[ev][spec].status = EvStWAIT;
			REG_(REGI_V0) = 1;
		}
		break;
		case 0x12://initpad
			RMAP_(REG_(REGI_A0),_pads._pad[0].buf,NOARG);
			_pads._pad[0].len=REG_(REGI_A0+1);
			RMAP_(REG_(REGI_A0+2),_pads._pad[1].buf,NOARG);
			_pads._pad[1].len=REG_(REGI_A0+3);
			REG_(REGI_V0) = 1;
		break;
		case 0x13://startpad
			PS1IOREG(0x1f801074) |=1;
			//CP0._regs[CP0.SR] |= 0x101;
		break;
		case 0x14:
			_pads._pad[0].buf=0;
			_pads._pad[1].buf=0;
		break;
		case 0x15://pad_init
			RMAP_(REG_(REGI_A0+1),_pads._buf,NOARG);
			PS1IOREG(0x1f801074) |=1;
			*((u32 *)_pads._buf)=-1;
		break;
		case 0x16:
			if(_pads._buf)
				*(u32*)_pads._buf=MAKELONG(_joy._pads[0]._buttons,_joy._pads[1]._buttons);
		break;
		case 0x17://ReturnFromException
			memcpy(CCore::_regs,_regs_copy,sizeof(_regs_copy));
			//printf("b017 %x%x\n",_pc,CP0._regs[CP0.EPC]);
			DLOG("IRQ Leave");
			CP0._rfe(&_pc);
			EnterDebugMode(DEBUG_BREAK_IRQ);
			return 1;
			//EnterDebugMode();//8001037c111
		break;
		case 0x18:
			_hook_irq=NULL;
		break;
		case 0x19://HookEntryInt
			RMAP_(REG_(REGI_A0),_hook_irq,NOARG);
		break;
		case 0x32:{//fopen
			u8 *p;
			u64 d[10];
			u32 i;

			i=(u32)-1;
			RMAP_(REG_(REGI_A0),p,NOARG);
		//	printf("%x %s\n",*(u32*)p,p);
			if(*(u32 *)p<=0x30307562 && *(u32 *)p<=0x31307562){
				if(!_joy._cards[0]._open((char *)p+5,REG_(REGI_A0+1)) && !_files._open((char*)p,p[3],0,&i)){
				//_items[idx]._start=p;
				//_items[idx]._type=1;
				}
			}
			else{
				d[0]=(u64)p;
				if(!_cdrom.Query(IGAME_GET_ISO_FILEI,d)){
					_files._open((char *)"cdrom:",d[2],d[1],&i);
				}
			}
			REG_(REGI_V0)=i;
		}
		break;
		case 0x33:{
			_files._seek(REG_(REGI_A0),REG_(REGI_A0+1),REG_(REGI_A0+2));
			REG_(REGI_V0)=_files._items[REG_(REGI_A0)]._pos;
		}
		break;
		case 0x34:{//fread
			u8 *p;
			u32 r;

			RMAP_(REG_(REGI_A0+1),p,NOARG);
			_files._read(_cdrom._game,REG_(REGI_A0),p,REG_(REGI_A0+2),&r);
			REG_(REGI_V0)=r;
			//printf("B0 34 %x %x %x %x\n",REG_(REGI_A0),REG_(REGI_A0+1),REG_(REGI_A0+2),r);
//			EnterDebugMode();
		}
		break;
		case 0x35:{//fwrite
			if(REG_(REGI_A0)==1){
				u8 *p;

				RMAP_(REG_(REGI_A0+1),p,NOARG);
				for(;REG_(REGI_A0+2);REG_(REGI_A0+2)--)
					printf("%c",*(char*)p++);
			}
			else
				printf("B0 35 %x\n",REG_(REGI_A0));
		}
		break;
		case 0x36:
			_files._close(REG_(REGI_A0));
		break;
		case 0x38:
			EnterDebugMode();
		break;
		case 0x3f:{
			u8 *p;

			RMAP_(REG_(REGI_A0),p,NOARG);
			printf("%s",p);
			fflush(stdout);
		}
		break;
		case 0x42:{//frstfile
			u8 *p;
			u64 d[10];
			struct DIRENTRY *dir;

			RMAP_(REG_(REGI_A0+1),p,NOARG);
			dir= (struct DIRENTRY *)p;
			RMAP_(REG_(REGI_A0),p,NOARG);
		//	printf("ff %s\n",p);
			if(!strncmp((char *)p,"bu00",4)){
			//	EnterDebugMode();
				_joy._cards[0]._buf=0;
				dir->size = 8192;
				REG_(REGI_V0)=0;//REG_(REGI_A0+1);
			}
			else if(!strncmp((char *)p,"bu01",4)){
			//	EnterDebugMode();
			//	dir->size = 8192;
				REG_(REGI_V0)=0;///REG_(REGI_A0+1);
			}
			else{
				d[0]=(u64)p;
				REG_(REGI_V0)=0;
				if(!_cdrom.Query(IGAME_GET_ISO_FILEI,d)){
					dir->size=d[1];
					REG_(REGI_V0)=REG_(REGI_A0+1);
				}
			}
		}
		break;
		case 0x43:{//nextfile
			u8 *p;
			struct DIRENTRY *dir;

			RMAP_(REG_(REGI_A0),p,NOARG);
			dir= (struct DIRENTRY *)p;
			//dir->size = 8192;
			REG_(REGI_V0)=0;;//REG_(REGI_A0);
		}
		break;
		case 0x4a://InitCard
		case 0x4b://startcard
			printf("bios call %X %x\n",0xB0,a);
		break;
		case 0x56:
			REG_(REGI_V0)=0x674;
		break;
		case 0x57:
			REG_(REGI_V0)=0x874;
		break;
		case 0x5b://ChangeClearPad
			//printf("bios call %X %x\n",0xB0,a);
		break;
		default:
			printf("bios call %X %x\n",0xB0,a);
			EnterDebugMode();
		break;
	}
	return 0;
}

int PS1BIOS::ExecC0(u32 a){
	switch(a){
		case 0xa://ChangeClearRC
			//printf("bios call %X %x %x %x\n",0xC0,a,REG_(REGI_A0),REG_(REGI_A0+1));
			RL(SL(REG_(REGI_A0),2)+0x8600,REG_(REGI_V0));
			WL(SL(REG_(REGI_A0),2)+0x8600,REG_(REGI_A0+1));
			//EnterDebugMode();
		break;
		case 2://SysEnqIntRP
		break;
		default:

	printf("bios call %X %x %x\n",0xC0,a,REG_(REGI_A0));
			EnterDebugMode();
		break;
	}
	return 0;
}

int PS1BIOS::_enterIRQ(int n, u32 pc){
	void *sp;

	if(PS1DEV::_enterIRQ(n))
		return 2;
	PS1IOREG(0x1f801070) |= n;
	EnterDebugMode(DEBUG_BREAK_IRQ);
	DLOG("IRQ Enter %x",n);
	//_irq &= ~n;
	//RMAP_(0xa000e1fc,sp,NOARG);

	mempcpy(_regs_copy,CCore::_regs,sizeof(_regs_copy));
	CP0._regs[CP0.EPC]=_pc;
	CP0._regs[CP0.SR]=  (CP0._regs[CP0.SR] & ~0x3f)|SL(CP0._regs[CP0.SR]&0xf,2);

	REG_SP=0xa000e1fc;
	if(n&1){
		if(_pads._buf)
			*(u32*)_pads._buf=MAKELONG(_joy._pads[0]._buttons,_joy._pads[1]._buttons);
		for(int i=0;i<2;i++){
			if(_pads._pad[i].buf){
				((u8 *)_pads._pad[i].buf)[0]=_joy._pads[i]._state;
				((u8 *)_pads._pad[i].buf)[1]=_joy._pads[i]._id;
				memcpy((u8 *)_pads._pad[i].buf + 2,&_joy._pads[i]._buttons,_pads._pad[i].len-2);
			}
		}
		if(RcEV[3][1].status == EvStACTIVE)
			_cb._addCall(RcEV[3][1].fhandler,_pc,(u32 *)CCore::_regs);
	}

	if(_hook_irq){
	//	printf("hook\n");
		u32 v=((u32 *)_hook_irq)[0];
		if(!v) goto A;
		_pc=REG_RA=v;
		REG_SP=((u32 *)_hook_irq)[1];
		REG_(REGI_FP)=((u32 *)_hook_irq)[2];
		for(int i=0;i<8;i++)
			REG_(REGI_S0+i)=((u32 *)_hook_irq)[3+i];
		REG_GP=((u32 *)_hook_irq)[11];
		REG_(REGI_V0)=1;
		//if(!_cb._isEmpty()) _cb._addCall(_pc,_pc,(u32 *)CCore::_regs);
		goto Z;
	}

	if(!_cb._doCall(&_pc,(u32 *)CCore::_regs)) goto Z;
A:
	memcpy(CCore::_regs,_regs_copy,sizeof(_regs_copy));
	CP0._rfe(&_pc);
Z:
	return 0;
}

int PS1BIOS::ReturnFromCall(){
	_cb._returnFromCall(&_pc,(u32 *)_regs);
	if(_cb._doCall(&_pc,(u32 *)_regs)){
		memcpy(CCore::_regs,_regs_copy,sizeof(_regs_copy));
		CP0._rfe(&_pc);
		//EnterDebugMode();
		return 1;
	}
	return 0;
}

int PS1BIOS::__calls::_addCall(u32 a,u32 pc,u32 *r){
	cb.push_back({a,pc,r});
	printf("addcall\n");
	return 0;
}

int PS1BIOS::__calls::_returnFromCall(u32 *pc,u32 *r){
	__handler &h=cb.front();
	cb.pop_back();
	*pc=h._regs[0];
	memcpy(r,&h._regs[1],sizeof(h._regs)-4);
	return 0;
}

int PS1BIOS::__calls::_doCall(u32 *pc,u32 *r){
	if(cb.size()==0) return 1;
	__handler &h=cb.front();
	*pc=h._adr;
	//memcpy(r,&h._regs[1],sizeof(h._regs)-4);
	r[REGI_RA]=0x1000;
	//EnterDebugMode();
	return 0;
}

int PS1BIOS::__calls::_reset(){
	cb.clear();
	return 0;
}

PS1BIOS::__calls::__handler::__handler(u32 a,u32 pc,u32 *r,u32 attr){
	_adr=a;
	_regs[0]=pc;
	_attr=attr;
	if(r)
		memcpy(&_regs[1],r,sizeof(_regs)-4);
}

int PS1BIOS::__pads::_reset(){
	memset(&_pad,0,sizeof(_pad));
	_buf=NULL;
	return 0;
}

int PS1BIOS::__files::_find_free_slot(u32 *r){
	u32 idx;

	for(idx=0;idx<sizeof(_items)/sizeof(struct __file);idx++){
		if(_items[idx]._start==0){
			if(r) *r=idx;
			return 0;
		}
	}
	return -1;
}

int PS1BIOS::__files::_reset(){
	memset(_items,0,sizeof(_items));
	_last=0;
	return 0;
}

int PS1BIOS::__files::_open(char *dev,u32 p,u32 s,u32 *r){
	u32 idx;

	if(!dev || _find_free_slot(&idx)) return -1;
	_items[idx]._type = dev[0]=='b' && dev[1]=='u'?1 :0;
	_items[idx]._pos=0;
	_items[idx]._size=s;
	_items[idx]._start=p;
//	idx=_last;
	*r=idx+3;
	return 0;
}

int PS1BIOS::__files::_write(u32 idx,void *p,u32,u32 *r){

	return -1;
}

int PS1BIOS::__files:: _seek(u32 idx,u32 d,u32 w){
	idx-=3;
	switch(w){
		case SEEK_SET:
			_items[idx]._pos=d;
		//	printf("SEEK SET %llu %x %x\n",_items[idx]._pos,w,d);
			return 0;
		case SEEK_CUR:
			_items[idx]._pos+=(s32)d;
			//printf("SEEK CUR %llu %x %d\n",_items[idx]._pos,w,d);
			return 0;
		case SEEK_END:
			_items[idx]._pos=_items[idx]._size -(s32)d;
			printf("SEEK END %llu %x %d\n",_items[idx]._pos,w,d);
			return 0;
	}
	return -1;
}

int PS1BIOS::__files::_close(u32 idx){
	//printf("close %d\n",idx);
	if(idx<3)
		return 1;
	idx-=3;
	_items[idx]._pos=0;
	_items[idx]._start=0;
	_items[idx]._size=0;
	if(idx==_last)
		_last--;
	return 0;
}

int PS1BIOS::__files::_read(IStreamer *game,u32 idx,void *buf,u32 sz,u32 *r){
	idx-=3;
	if(_items[idx]._type==1){
		*r=0;
		return 0;
	}
	if(!game || game->Seek(_items[idx]._pos+_items[idx]._start,SEEK_SET))
		return  -1;
	if(game->Read(buf,sz,r))
		return -2;
	game->Tell(&_items[idx]._pos);
	_items[idx]._pos-= _items[idx]._start;
	return 0;
}

};