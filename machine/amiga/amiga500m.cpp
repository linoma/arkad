#include "amiga500m.h"
#include "gui.h"

extern GUI gui;

namespace amiga{
//fcc946 fcc6e2 fe8772 fe8560
amiga500m::amiga500m() : Machine(MB(50)),amiga500dev(){
	M68000Cpu::_freq=MHZ(28.37516)/4;
	Machine::_mouse=(int *)mo;
	memset(keys,0xff,sizeof(keys));
	_keys=(u8 *)keys;
}

amiga500m::~amiga500m(){
}

int amiga500m::_loadBios(char *){
	FILE *fp;

	if(!(fp=fopen("roms/amiga/kick.rom","rb")))
		return -1;
	fread(M_BIOS,KB(512),1,fp);
	fclose(fp);
	return 0;
}

int amiga500m::Load(IGame *pg,char *fn){
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
	_loadBios(0);
	_fdc._add(fn,0);
	//pg->Read(M_BIOS,KB(512),&sz);
	_pc=0xfc00d2;
	_icache.invalidate();
	delete (FileGame *)pg;
	Query(ICORE_QUERY_SET_FILENAME,fn);
	for(int i=1;i<4;i++){
		fn += strlen(fn) + 1;
		if(*fn==0) break;
		_fdc._add(fn,i);
	}
	res=0;
	return res;
}

int amiga500m::Destroy(){
	Machine::Destroy();
	return amiga500dev::Destroy();
}

int amiga500m::Reset(){
	Machine::Reset();
	return amiga500dev::Reset();
}

int amiga500m::Init(){
	if(Machine::Init())
		return -1;
	if(amiga500dev::Init(&_memory[MB(10)],&_memory[MB(10)] + 0xa00000))
		return -2;
	_gpu_mem=M68000Cpu::_mem;
	for(int i=0;i<0x200;i++)
		SetIO_cb(0xdff000|i,(CoreMACallback)&amiga500m::fn_write_io,0);
	for(int i=0;i<0x2;i++){
		SetIO_cb(0xdff000|((REG_POT0DAT|i)*2),0,(CoreMACallback)&amiga500m::fn_read_io);
		SetIO_cb(0xdff000|((REG_POT1DAT|i)*2),0,(CoreMACallback)&amiga500m::fn_read_io);
		SetIO_cb(0xdff000|((REG_POTGOR|i)*2),0,(CoreMACallback)&amiga500m::fn_read_io);
		SetIO_cb(0xdff000|((REG_DSKBYTR|i)*2),0,(CoreMACallback)&amiga500m::fn_read_io);
		SetIO_cb(0xdff000|((REG_CLXDAT|i)*2),0,(CoreMACallback)&amiga500m::fn_read_io);
		SetIO_cb(0xdff000|((REG_JOY0DAT|i)*2),0,(CoreMACallback)&amiga500m::fn_read_io);
		SetIO_cb(0xdff000|((REG_JOY1DAT|i)*2),0,(CoreMACallback)&amiga500m::fn_read_io);
	}
	for(int i=0;i<0x2;i++){
		SetIO_cb(0xdff000|((REG_DMACON+i)*2),(CoreMACallback)&amiga500m::fn_write_io,(CoreMACallback)&amiga500m::fn_read_io);
	}
	for(int i=0;i<0x3000;i++)
		SetIO_cb(0xbfe000+i,(CoreMACallback)&amiga500m::fn_write_cia,(CoreMACallback)&amiga500m::fn_read_cia);
	//printf("%d\n",(int)(M68000Cpu::_freq / (59.94f * 312.0f)));
	AddTimerObj(this,_scanline_cycles,this);//
	return 0;
}

int amiga500m::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case ME_ENDFRAME:
			Paula::Update();
			Denise::Update(!OnFrame());
			return 0;
		case ME_MOVEWINDOW:{
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
			Denise::Update();
			Draw();
			CALLEE(Machine::OnEventI,ev,return,arg);
			return 0;
		case ME_KEYUP:{
				va_start(arg, ev);
				Machine::OnEventI(ev,arg);
				int v=va_arg(arg,int);
				v=va_arg(arg,int);
				_keyboard.push_back({2,1,v});
				va_end(arg);
				return 0;
			}
			return 0;
		case ME_KEYDOWN:{
				va_start(arg, ev);
				Machine::OnEventI(ev,arg);
				int v=va_arg(arg,int);
				v=va_arg(arg,int);
				_keyboard.push_back({2,0,v});
				va_end(arg);
				return 0;
			}
			return 0;
		case ME_MOUSEBUTTONDOWN:
		case ME_MOUSEMOVE:
		case ME_MOUSEBUTTONUP:{
			va_start(arg, ev);
			Machine::OnEventI(ev,arg);
			_joy[0].push_back({4,_mouse[0],_mouse[1],_mouse[2],_mouse[3]});
			//_joy[1].push_back({__frame,4,_mouse[0],_mouse[1],_mouse[2],_mouse[3]});
			va_end(arg);
			return 0;
		}
		case ME_JOYBUTTONDOWN:
		case ME_JOYBUTTONUP:
		case ME_JOYMOVE:{
			va_start(arg, ev);
			Machine::OnEventI(ev,arg);
			_joy[1].push_back({4,_mouse[5],_mouse[6],_mouse[7],_mouse[8]});
			va_end(arg);
			return 0;
		}
		case 0:{//pending irq bit
			va_start(arg, ev);
			int i=va_arg(arg,int);
			va_end(arg);
			switch(i){
				case 0:
					ev=1;
					printf("irq 1\n");
				break;
				case 1:
				break;
				default:{
					u32 n;
					u16 v;

					if(!(n=_irq_pending))
						return 0;
					for(ev=0,v=SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTENA));n;ev++){
						if(!(BV(ev) & _irq_pending))
							continue;
						if(!(v & BV(ev))){
							BVC(n,ev);
							continue;
						}
						break;
					}
					if(!n) return 0;
				}
				break;
			}
		}
		break;
		default:
			if(BVT(ev,31)) return -1;
			{
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				if(i == 1 || i==3){
					BS(_irq_pending,BV(ev));
					u16 r__= SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ));
					r__|= BV(ev);
					ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ)=SWAP16(r__);
					ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQR)=SWAP16(r__);
					if(i==3)
						_setStatus(S_IRQ_CHECK);
					return 1;
				}
				else if(i == 2){
					BC(_irq_pending,BV(ev));
					return 1;
				}
			}
		break;
	}
	BVC(_irq_pending,ev);
	switch(ev){
		case 0://nmi
		case 1:
		case 3:
		case 2:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
			if(_enterIRQ(ev))
				BVS(_irq_pending,ev);
			break;
		default:
			printf("irq %x\n",ev);
	}
	return 0;
}

static u32 lino=0;
int amiga500m::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_MEMORY_WRITE:{
			u32 *p=(u32 *)pv;
			if(p[2] & AM_WORD)
				WW(p[0],p[1]);
			//printf("ICORE_QUERY_MEMORY_WRITE %x\n",p[2]);
		}
			return 0;
		case IMACHINE_QUERY_MEMORY_ACCESS:{
			void *p;
			LPMEMORYACCESS d=(LPMEMORYACCESS)pv;
			RMAP_(d->addr,p,R);
			d->mem=p;
		}
			return 0;
		case ICORE_QUERY_DBG_MENU_SELECTED:{
				u32 id =*((u32 *)pv);
				printf("id %x\n",id);
				switch((u16)id){
					case 15:{
						u16 a=_fdc._floppies[0]._status;
						_fdc._floppies[0]._eject=1;
						//_cia[0]._regs[__cia::PRA] = (_cia[0]._regs[__cia::PRA] & ~4)|0x20;
						switch(++lino){
							case 1:
								_fdc._floppies[0]._add("roms/amiga/4.adf");
								//_fdc._floppies[0]._add("roms/amiga/barbarian2-2.adf");
							break;
							case 2:
								_fdc._floppies[0]._add("roms/amiga/3.adf");
								//_fdc._floppies[0]._add("roms/amiga/barbarian2-3.adf");
							break;
							case 3:
								//_fdc._floppies[0]._add("roms/amiga/barbarian2-1.adf");
								lino=0;
							break;
						}
						//_fdc._floppies[0]._on=1;
						//_fdc._floppies[0]._selected=1;
					}
						return 0;
					case 10:
						_display._status._sv=id&0xFFFF0000?1:0;
						Denise::write(REG_DMACON*2,SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DMACON)));
						return 0;
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
						_display._status._lv=id&0xFFFF0000?1:0;
						Denise::write(REG_DMACON*2,SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DMACON)));
						return 0;
					case 11:{
						char *p=new char[160000];
						if(p){
							p[0]=1;
							p[1]=0;
							_agnus._copper._dumpRegisters(p);
							printf("%s",p);
							delete []p;
						}
					}
					break;
				}
			}
			return -1;
		case ICORE_QUERY_DBG_MENU:{
				char *p = new char[1000];
				((void **)pv)[0]=p;
				//memset(p,0,1000);
				sprintf(p,"Copper dump");
				p+=strlen(p)+1;
				*((u32 *)p)=11;
				*((u32 *)&p[4])=0x101;
				p+=sizeof(u32)*2;
				for(int i=0;i<7;i++){
					sprintf(p,"Bitplanes %d",i);
					p+=strlen(p)+1;
					*((u32 *)p)=1+i;
					*((u32 *)&p[4])=0x102;
					p+=sizeof(u32)*2;
				}
				sprintf(p,"Sprites");
				p+=strlen(p)+1;
				*((u32 *)p)=10;
				*((u32 *)&p[4])=0x102;
				p+=sizeof(u32)*2;
				sprintf(p,"Disk Remove");
				p+=strlen(p)+1;
				*((u32 *)p)=15;
				*((u32 *)&p[4])=0x0001;
				p+=sizeof(u32)*2;
				*((u64 *)p)=0;
			}
			return 0;
		case ICORE_QUERY_IO_DEVICE:
			switch(*((u32 *)pv)){
				case CIAA:
					((void **)pv)[0] =&_cia[0];
					return 0;
				case CIAB:
					((void **)pv)[0] = &_cia[1];
					return 0;
				case FLOPPY:
					((void **)pv)[0] = &_fdc;
					return 0;
			}
			return -1;
		case ICORE_QUERY_IO_PORT:
			switch(*((u32 *)pv)){
				case 0:
					((void **)pv)[0] = M68000Cpu::_ioreg;
					return 0;
				case CIAA:
					((void **)pv)[0] =_cia[0]._regs;
					return 0;
				case CIAB:
					((void **)pv)[0] = _cia[1]._regs;
					return 0;
			}
			return -1;
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
			switch(SR(adr,20)){
				case 0:
					d->addr=adr&~0x7ffff;
					d->size=MB(.5);
					break;
				case 0xf:
					d->addr=adr&~0xfffff;
					d->size=MB(1);
					break;
				case 0xc:
					d->addr=adr&~0x7ffff;
					d->size=KB(512);
					break;
				case 0xd:
					d->addr=adr&~0x1ff;
					d->size=0x200;
					break;
				default:
					return -2;
			}
		}
		return 0;
		default:
			return M68000Cpu::Query(what,pv);
	}
	return -1;
}

int amiga500m::Exec(u32 status){
	int ret;

	ret=M68000Cpu::Exec(status);
	__cycles=M68000Cpu::_cycles;
	//if(*((u16 *)&CCore::_mem[0x6afd]) ) EnterDebugMode();
	switch(ret){
		case -5:
			machine->OnEvent(0,(LPVOID)-1);
			ret=0;
		break;
		case -101:{
/*			u32 a,b;

			RLPC(0x41c,a);
			RLPC(0xc004de,b);
			static FILE *lino=NULL;
			if(!lino)
				lino=fopen("lino.log","wb");
			if(lino)
				fprintf(lino,"%x %x %x\n",a,b,REG_D(0));
			fflush(lino);*/
		//	printf("%x %x\n",a,b);
		}
			ret=1;
		break;
		case -2:
		case -1:
			return -ret;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int amiga500m::Dump(char **pr){
	int res;
	char *c,*cc,*p;
	u8 *mem;
	u32 adr;
	DEBUGGERDUMPINFO di;

	CCore::_dump(pr,&di);
	if((c = new char[400000])==NULL)
		return -1;
	memset(c,0,40000);
	*((u64 *)c)=0;
	cc = &c[390000];
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

	RMAP_(adr,mem,R);
	((CCore *)cpu)->_dumpMemory(p,mem,&di);

	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3106");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	((CCore *)cpu)->_dumpCallstack(p);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3105");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3103");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;
	for(int i=0;i<sizeof(_cia)/sizeof(__cia);i++){
		_cia[i].dump();
		sprintf(cc,"CIA%d\n T:%c%02X%02X%02X-%02X%02X%02X IC:%02X-%02X SD:%02X AT:%02X%02X BT:%02X%02X CA:%02X CB:%02X A:%02X:%02X B:%02X:%02X\n",i,
		_cia[i]._tod._enabled?'e':0x20,_cia[i]._regs[__cia::TOD_MIN],_cia[i]._regs[__cia::TOD_SEC],_cia[0]._regs[__cia::TOD_10THS],
		_cia[i]._regs[__cia::TOD_AMIN],_cia[i]._regs[__cia::TOD_ASEC],_cia[i]._regs[__cia::TOD_A10THS],
		_cia[i]._regs[__cia::ICR],_cia[i]._regs[__cia::IMR],_cia[i]._regs[__cia::SDR],
		_cia[i]._regs[__cia::TA_HI],_cia[i]._regs[__cia::TA_LO],_cia[i]._regs[__cia::TB_HI],_cia[i]._regs[__cia::TB_LO],
		_cia[i]._regs[__cia::CRA],_cia[i]._regs[__cia::CRB],
		_cia[i]._regs[__cia::PRA],_cia[i]._regs[__cia::DDRA],
		_cia[i]._regs[__cia::PRB],_cia[i]._regs[__cia::DDRB]);
		strcat(p,cc);
	}
	adr=SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DMACON));

	sprintf(cc,"\nINTENA:%04X INTREQ:%04X DMACON:%04X:%c\n",SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTENA)),
		SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_INTREQ)),
		adr,adr & DMACON_DMAEN ? 49 : 48);
	strcat(p,cc);
	Denise::_dumpRegisters(p);
	sprintf(cc,"ADKCON:%04X DSKLEN:%04X DSKPTH:%04X DSKPTL:%04X DSKSYNC:%04X %02X:%02X\n",
		SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_ADKCON)),
		SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DSKLEN)),
		SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DSKPTH)),
		SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DSKPTL)),SWAP16(ACHIPREG_(M68000Cpu::_ioreg,REG_DSKSYNC)),
		_cia[0]._regs[__cia::PRA],0
		);
	strcat(p,cc);
	for(int n=0;n<4;n++){
		sprintf(cc,"DSK:%d\tM:%1d%1d C:%d/%d %u %u %u\n",n,_fdc._floppies[n]._on,_fdc._floppies[n]._selected,
			_fdc._floppies[n]._cyl,_fdc._floppies[n]._side,_fdc._floppies[n]._mfmpos,
			_fdc._floppies[n]._cycles[1],_fdc._floppies[n]._cycles[2]);
		strcat(p,cc);
	}
	strcat(p,"\n");
	Paula::_dumpRegisters(p);

	_agnus._dumpRegisters(p);
	adr=0xDFF000;
	RMAP_(adr,mem,R);
	strcat(p,"\nREG\n");
	for(int n=0,i=0;i<0x200;i+=2,mem+=2){
		*cc=0;
		sprintf(cc,"%3X:%04x ",i,SWAP16(*((u16 *)mem)));
		strcat(p,cc);
		if(++n == 8){
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

int amiga500m::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	m["keyboard_mode"]="1";
	return 0;
}

s32 amiga500m::fn_read_cia(u32 a,void *pmem,void *pdata,u32 attr){
	u16 v;

	//if((attr & AM_WORD) || (a&0x3000) == 0x3000) EnterDebugMode();
	if((a&0x1000)==0)
		_cia[0].read(a,&v);
	if((a&0x2000)==0)//fc0b76fixme
		_cia[1].read(a,&v);
	*(u16 *)pdata=v;
	return 0;
}

s32 amiga500m::fn_write_cia(u32 a,void *pmem,void *pdata,u32 attr){
	if((attr & AM_WORD) || (a&0x3000) == 0x3000) printf("cia W wod PC:%x %x %x\n",_pc,a,attr);
	if((a&0x2000)==0)
		_cia[1].write(a,*(u16 *)pdata);
	if((a&0x1000)==0)
		_cia[0].write(a,*(u16 *)pdata);
	return 0;
}

int amiga500m::Run(u8 *m,int cyc,void *obj){
	int i,n;

	for(n=0;n<sizeof(_cia)/sizeof(__cia);n++)
		_cia[n].update(cyc);
	_fdc.update(cyc);
	_potgo.update(cyc);
	_keyboard.update(cyc);
	_rs232.update(cyc);
	for(n=0;n<sizeof(_joy)/sizeof(__mouse);n++)
		_joy[n].update(cyc);
	Paula::update(cyc);

	while(cyc){
		i=Denise::Run(m,cyc,obj);
		_agnus._blitter.update(cyc);
		_agnus._copper.update(cyc);
		if(i >= 0) break;
		cyc = -i;
	}
	return i;
}

#define OPCODE_IDX_(a) (SR((a)&0xfffc,2))
#define M68OPCODE(a,b) {OPCODE(a,b);OPCODE(((a)|1),b);}

#define SOURCEM(a) 			(SR(a,3)&7)
#define SOURCER(a) 			(a&7)
#define SOURCEINC(sz,pa) 	(pa += BV(sz))
#define SOURCEDEC(sz,pa) 	(pa -= BV(sz))
#define SOURCEINCS(sz,pa)
#define SOURCEDECS(sz,pa)

#define DESTM(a) 		SOURCER(a)
#define DESTR(a)  		SOURCEM(a)
#define DESTINC(sz,pa) 	SOURCEINC(sz,pa)
#define DESTDEC(sz,pa) 	SOURCEDEC(sz,pa)
//get data for source
#define LPC(a,b)
#define M(sz,src,data) data=(src)
#define MS M

#define RLOP(a,b) RLPC(a,b) //warning  do not touch me!!!!!!

#define WM(sz,dst,data) switch(sz){case 0:WB(dst,data);break;case 3:case 1:WW(dst,data);break;case 2:WL(dst,data);break;}

#define RM(sz,src,data)\
switch(sz){case 0:RB(src,data);break;case 3:case 1:RW(src,data);break;case 2:RL(src,data);break;}

#define RMPC(sz,src,data) {if(src){u32 v___;\
switch(sz){case 0:v___=*(u8 *)src;break;case 1:v___=(u32)*(u16 *)src;break;case 2:v___=*(u32 *)src;break;} data=v___;}}

#define WMPC(sz,src,data) {__data=(data);if(src){\
switch(sz){case 0:*(u8 *)src=__data;break;case 1:*(u16 *)src=__data;break;case 2:*(u32 *)src=__data;break;}}}

#define RWCPC(a,b) (b)=(*(u16 *)&_icache.p[(a)-_pc])
#define RWCPCS(a,b) RWPC(a,b)

#define RMP(sz,a,src,data,op){\
	void *__tmp=(src);\
	RMPC(sz,__tmp,__data);\
	if(((op) & 0x38) > 8){\
		if(sz==2) __data=BELE32(((u32)__data));\
		else if(sz==1) __data=SWAP16(((u16)__data));\
		if(ISM68IO((a))){\
			u32 __a=M68IO(a);\
			if(_portfnc_read[__a])\
				(((CCore *)this)->*_portfnc_read[__a])(a,__tmp,&__data,AM_READ);\
		}\
	}\
	(data)=__data;\
}

#define WMP(sz,a,src,data,op){\
	void *__tmp=(src);\
	__data=(data);\
	if(((op) & 0x38) > 8){\
		if(sz==2) __data=BELE32(((u32)__data));\
		else if(sz==1) __data=SWAP16(((u16)__data));\
		if(ISM68IO((a))){\
			u32 __a=M68IO(a);\
			if(_portfnc_write[__a]){\
				if(!(((CCore *)this)->*_portfnc_write[__a])(a,__tmp,&__data,AM_WRITE)){\
					__tmp=0;\
				}\
			}\
		}\
	}\
	WMPC(sz,__tmp,__data);\
}

#define RMS(sz,src,data) {void *p___;RMAP_PC((src),p___,R);RMPC(sz,p___,data);}

#define AEA(dst,val,mode,sz,m,iop,mm,am)\
void  *d__,*c__=0;u32 v__;__address=0;\
switch(mm##M(mode)){\
	case 0:c__=d__=&REG_D(mm##R(mode));v__=*(u32 *)d__;goto CONCAT(A,__LINE__);break;\
	case 1:c__=d__=&REG_A(mm##R(mode));v__=*(u32 *)d__;goto CONCAT(A,__LINE__);break;\
	case 2:__address=REG_A(mm##R(mode));RMAP_PC(__address,d__,R);m##M##am(sz,__address,v__);goto CONCAT(A,__LINE__);break;\
	case 3:{c__=&REG_A(v__=mm##R(mode));RMAP_PC(*(u32 *)c__,d__,R);__address=*(u32 *)c__;mm##INC##am(sz,*(u32 *)c__);\
if(v__ == 7 && sz < 1) REG_A(7)=(REG_A(7)+1) & ~1;m##M##am(sz,__address,v__);} goto CONCAT(A,__LINE__);break;\
	case 4:{c__=&REG_A(v__=mm##R(mode));mm##DEC##am(sz,*(u32 *)c__);__address=*(u32 *)c__;RMAP_PC(__address,d__,R);\
if(v__ == 7 && sz < 1) REG_A(7)=(REG_A(7)-1) & ~1;m##M##am(sz,__address,v__);} goto CONCAT(A,__LINE__);break;\
	case 5:{RWCPC##am(_pc+iop,v__);iop+=2;v__=REG_A(mm##R(mode))+(s16)v__;RMAP_PC(v__,d__,R);__address=v__;m##M##am((sz),__address,v__);} goto CONCAT(A,__LINE__);break;\
	case 6:{RWCPC##am(_pc+iop,v__);iop+=2;u32 v0__=REG_D(SR((u16)v__,12));v__=REG_A(mm##R(mode)) + (s8)v__ + (s32)((v__&0x800) ? v0__ : (s16)v0__);RMAP_PC(v__,d__,R);__address=v__;m##M##am(sz,__address,v__);goto CONCAT(A,__LINE__);}break;\
	case 7:\
		switch(mm##R(mode)){\
			case 0:RWCPC##am(_pc+iop,v__);iop += 2;__address=v__;m##M##am(sz,v__,v__);RMAP_PC(__address,d__,R);goto CONCAT(A,__LINE__);break;\
			case 1:RLOP(_pc+iop,v__);iop += 4;__address=v__;m##M##am(sz,v__,v__);RMAP_PC(__address,d__,R);goto CONCAT(A,__LINE__);break;\
			case 2:RWCPC##am(_pc+iop,v__);v__=_pc+ipc + (s16)v__;__address=v__;iop += 2;m##M##am(sz,v__,v__); RMAP_PC(__address,d__,R);goto CONCAT(A,__LINE__);break;\
			case 3:{RWCPC##am(_pc+iop,v__);u32 v0__=REG_D(SR((u16)v__,12));v__=_pc+ipc + (s8)v__+ (s32)((v__&0x800) ? v0__ : (s16)v0__);iop += 2;RMAP_PC(v__,d__,R);__address=v__;m##M##am((sz),__address,v__); goto CONCAT(A,__LINE__);}break;\
			case 4:__address=_pc+iop;switch(sz){case 0:RWCPC##am(_pc+iop,v__);iop+=2;break;case 1:RWCPC##am(_pc+iop,v__);iop += 2;break;case 2:RLOP((_pc+iop),v__);iop+=4;break;}break;\
			default:v__=0;printf("AEA 7 %x PC:%x\n",mm##R(mode),_pc);break;\
		}\
	break;\
	default:printf("AEA %d\n",mm##M(mode));break;} RMAP_PC(v__,d__,R);\
CONCAT(A,__LINE__): (dst)=d__;(val)=v__;

#define AEA_(dst,src,val,mode,sz,m,iop,mm,am){AEA(dst,val,mode,sz,m,iop,mm,am) (src)=c__;}

#define SMODE(dst,mode,sz,m,iop){\
u32 __d;AEA(d__,__d,mode,(sz),m,iop,SOURCE, );\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}

#define AEAS(dst,val,mode,sz,m,iop,mm){ AEA(dst,val,mode,sz,m,iop,mm,S)}

#define SMODES(dst,mode,sz,m,iop){\
u32 __d;AEAS(d__,__d,mode,(sz),m,iop,SOURCE);\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}
#define DMODE(dst,mode,sz,m,iop){\
u32 __d;AEA(d__,__d,mode,(sz),m,iop,DEST,);\
switch(sz){case 0:(dst)=(u8)__d;break;case 1:(dst)=(u16)__d;break;case 2:(dst)=(u32)__d;break;}\
}

#define __68O(O,ofs,size,mode,a,b,...){\
	char size_[][10]={"\x2e\x62","\x2ew","\x2el","\x2ew",""};\
	switch(SR(mode,3)&7){\
		case 0:__##O(%s D%d a,b,size_[(size)],(mode &7),## __VA_ARGS__);break;\
		case 1:__##O(%s A%d a,b,size_[(size)],(mode &7),## __VA_ARGS__);break;\
		case 2:__##O(%s [A%d] a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 3:__##O(%s [A%d]+ a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 4:__##O(%s -[A%d] a,b,size_[size],(mode &7),## __VA_ARGS__);break;\
		case 5:{s16 __d;RWPC(_pc+ofs,__d);ofs+=2;__##O(%s [\x23\x24%x\x2c A%d] a,b,size_[size],__d,(mode &7),## __VA_ARGS__);} break;\
		case 6:{u16 __d;RWPC(_pc+ofs,__d);__##O(%s $%x[A%d\x2c %c%d] a,b,size_[size],(s8)__d,mode&7,(__d&0xf000)>0x7000?65:68,SR(__d,12)&7,## __VA_ARGS__);} break;\
		case 7:\
			switch(mode&7){\
				case 0:{u16 __d;RWPC(_pc+ofs,__d);__##O(%s [\x24%x]\x2ew a,b,size_[size],__d,## __VA_ARGS__);} break;\
				case 1:{u32 __d;RLOP(_pc+ofs,__d);__##O(%s [\x24%x]\x2el a,b,size_[size],__d,## __VA_ARGS__);} break;\
				case 2:{s16 __d;RWPC(_pc+ofs,__d);__##O(%s [PC\x2c\x24\x23%x] a,b,size_[size],__d,## __VA_ARGS__);}	break;\
				case 3:{s16 __d;RWPC(_pc+ofs,__d);__##O(%s [PC\x2c %c%d \x2c\x23\x24%x] a,b,size_[size],(__d&0xf000)>0x7000?65:68,SR(__d,12)&7,(s8)__d,## __VA_ARGS__);}	break;\
				case 4:{\
					u32 __d;\
					switch(size){\
						case 0:RWPC(_pc+ofs,__d);__d=(u8)__d;break;\
						case 1:RWPC(_pc+ofs,__d);ofs+=2;break;\
						case 2:RLOP(_pc+ofs,__d);break;\
					} __##O(%s \x23\x24%x a,b,size_[size],__d,## __VA_ARGS__);\
				}\
				break;\
				case 7:__##O(%s a,b,size_[size],## __VA_ARGS__);break;\
			}\
		break;\
		default:\
			__##O(M:%d %s a,b,SR(mode,3)&7,size_[size],## __VA_ARGS__);\
		break;\
	}\
}

#define __68S(ofs,size,mode,a,b,...) __68O(S,ofs,size,mode,a,b,## __VA_ARGS__)
#define _68S(a,b,...) __S(a,b,## __VA_ARGS__)

#define __68F(ofs,size,mode,a,b,...) if((gui._getStatus() & (S_PAUSE/*|S_DEBUG_NEXT*/)) == S_PAUSE){u32 o__=ofs;__68O(F,o__,size,mode,a,b,## __VA_ARGS__);}
#define _68F(a,...) if((gui._getStatus() & (S_PAUSE/*|S_DEBUG_NEXT*/)) == S_PAUSE) __F(a, ## __VA_ARGS__);

//#define __68F(ofs,size,mode,a,b,...)
//#define _68F(a,...)

#define _BCHG(a,b) a^=b
#define _BSET(a,b) a|=b
#define _BTST(a,b)
#define _BCLR(a,b) a&=~b

#define BIT_INS_(ins,val)\
void *p;\
u32 a,b__,b,sz;\
val;\
b__=b;sz=SOURCEM(_opcode);if(sz>0){sz=0;b__&=7;}else{sz=2;b__&=31;} b=BV(b__);\
AEA(p,a,_opcode,sz,R,ipc,SOURCE, );\
if(a & b) _ccr &= ~Z_BIT; else _ccr |= Z_BIT;\
_##ins(a,b);\
WMP(sz,__address,p,a,_opcode);

#define BIT_INS(ins) BIT_INS_(ins,b=REG_D(SR(_opcode,9)&7);sz=2;);\
__68F(2,sz,_opcode&0x3f,\x2c D%d,STR(ins),SR(_opcode,9)&7);

#define BIT_INSI(ins){BIT_INS_(ins,RWCPC(_pc+ipc,b);ipc+=2;sz=-1;);\
__68F(4,sz,_opcode&0x3f,\x2c\x23\x24%x,STR(ins),b__);}

static char _cc[][4]={"BRA","BF","BHI","BLS","BCC","BCS","BNE","BEQ","BVC","BVS","BPL","BMI","BGE","BLT","BGT","BLE"};
static u8 _szmove[]={0,0,2,1,0,0,2,2};

M68000Cpu::M68000Cpu() : CCore(){
	_regs=NULL;
	_ioreg=NULL;
	_opcodescb=NULL;
}

M68000Cpu::~M68000Cpu(){
	Destroy();
}

int M68000Cpu::Destroy(){
	if(_regs)
		delete []_regs;
	_regs=NULL;
	//return 0;
	CCore::Destroy();
	return 0;
}

int M68000Cpu::Reset(){
	CCore::Reset();
	_irq_pending=0;
	_pc=0;
	_ccr=0x2700;
	memset(_regs,0,0x20*sizeof(u32));
	REG_USP = 0;
	REG_ISP = 0;
	REG_SP = REG_ISP;
	return 0;
}

int M68000Cpu::Init(void *m,u32 ss,u32 f){
	u32 sz;

	_mem=(u8 *)m;
	sz=0x20*sizeof(u32) + (ss * sizeof(CoreMACallback) * 2) +0x4000*sizeof(CoreDecode) + 0x4000*sizeof(CoreDisassebler);
	if(!_regs && !(_regs = new u8[sz]))
		return -1;
	memset(_regs,0,sz);
	_portfnc_write = (CoreMACallback *)((u32 *)_regs + 0x20);
	_portfnc_read=&_portfnc_write[ss];
	_opcodescb=(CoreDecode *)&_portfnc_read[ss];
	_disopcodescb=(CoreDisassebler *)&_opcodescb[0x4000];

	for(int i=0;i<0x4000;i++)
		OPCODE(i,&M68000Cpu::_empty_op);

	for(int i=0;i<8;i++){
		M68OPCODE(OPCODE_IDX_(0x4880|(i*8)),&M68000Cpu::_movemw);
		M68OPCODE(OPCODE_IDX_(0x48C0|(i*8)),&M68000Cpu::_movemw);

		_disopcodescb[OPCODE_IDX_((0x800|0x33*8+i)*8)]=(CoreDisassebler)&M68000Cpu::_movemr_dis;
		_disopcodescb[OPCODE_IDX_((0x800|0x33*8+i)*8)|1]=(CoreDisassebler)&M68000Cpu::_movemr_dis;
	}
	M68OPCODE(OPCODE_IDX_(0x4880),&M68000Cpu::_ext);
	M68OPCODE(OPCODE_IDX_(0x48C0),&M68000Cpu::_ext);

	for(int i=0;i<0x20;i++){
		M68OPCODE(OPCODE_IDX_(i*8),&M68000Cpu::_ori);
		M68OPCODE(OPCODE_IDX_(i*8 | 0x200),&M68000Cpu::_andi);
		M68OPCODE(OPCODE_IDX_(i*8 | 0x400),&M68000Cpu::_subi);
		M68OPCODE(OPCODE_IDX_(i*8 | 0x600),&M68000Cpu::_addi);
		M68OPCODE(OPCODE_IDX_(i*8 | 0xa00),&M68000Cpu::_eori);
		M68OPCODE(OPCODE_IDX_(i*8 | 0xc00),&M68000Cpu::_cmpi);
	}

	for(int i=0;i<0x8;i++){
		for(int n=0;n<0x8;n++){
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|SL(n,9)),&M68000Cpu::_add_reg);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0x40|SL(n,9)),&M68000Cpu::_add_reg);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0x80|SL(n,9)),&M68000Cpu::_add_reg);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0x100|SL(n,9)),&M68000Cpu::_add_mem);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0x140|SL(n,9)),&M68000Cpu::_add_mem);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0x180|SL(n,9)),&M68000Cpu::_add_mem);
			//M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0xc0|SL(n,9)),&M68000Cpu::_adda_reg);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0xc0|SL(n,9)),&M68000Cpu::_adda_reg);
			//M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0x1c0|SL(n,9)),&M68000Cpu::_adda_reg);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*8)|0x1c0|SL(n,9)),&M68000Cpu::_adda_reg);
		}
	}
	for(int i=0;i<0x3;i++){
		for(int n=0;n<0x8;n++){
			M68OPCODE(OPCODE_IDX_(0xd000|(i*64)|0x100|SL(n,9)),&M68000Cpu::_addx_reg);
			M68OPCODE(OPCODE_IDX_(0xd000|(i*64)|0x100|8|SL(n,9)),&M68000Cpu::_addx_mem);
		}
	}
	for(int n=0;n<0x8;n++){
		for(int i=0;i<0x8;i++){
			M68OPCODE(OPCODE_IDX_(SL(n,9)|0x100|i*8),&M68000Cpu::_btst);
			M68OPCODE(OPCODE_IDX_(SL(n,9)|0x140|i*8),&M68000Cpu::_bchg);
			M68OPCODE(OPCODE_IDX_(SL(n,9)|0x180|i*8),&M68000Cpu::_bclr);
			M68OPCODE(OPCODE_IDX_(SL(n,9)|0x1c0|i*8),&M68000Cpu::_bset);

			M68OPCODE(OPCODE_IDX_(0x8000|SL(n,9)|(i*8)|0),&M68000Cpu::_or_reg);
			M68OPCODE(OPCODE_IDX_(0x8000|SL(n,9)|(i*8)|0x40),&M68000Cpu::_or_reg);
			M68OPCODE(OPCODE_IDX_(0x8000|SL(n,9)|(i*8)|0x80),&M68000Cpu::_or_reg);
			M68OPCODE(OPCODE_IDX_(0x8000|SL(n,9)|(i*8)|0x100),&M68000Cpu::_or_mem);
			M68OPCODE(OPCODE_IDX_(0x8000|SL(n,9)|(i*8)|0x140),&M68000Cpu::_or_mem);
			M68OPCODE(OPCODE_IDX_(0x8000|SL(n,9)|(i*8)|0x180),&M68000Cpu::_or_mem);

			M68OPCODE(OPCODE_IDX_(0xb000|SL(n,9)|(i*8)|0x100),&M68000Cpu::_eor);
			M68OPCODE(OPCODE_IDX_(0xb000|SL(n,9)|(i*8)|0x140),&M68000Cpu::_eor);
			M68OPCODE(OPCODE_IDX_(0xb000|SL(n,9)|(i*8)|0x180),&M68000Cpu::_eor);

			M68OPCODE(OPCODE_IDX_(0xc000|SL(n,9)|(i*8)|0),&M68000Cpu::_and_reg);
			M68OPCODE(OPCODE_IDX_(0xc000|SL(n,9)|(i*8)|0x40),&M68000Cpu::_and_reg);
			M68OPCODE(OPCODE_IDX_(0xc000|SL(n,9)|(i*8)|0x80),&M68000Cpu::_and_reg);
			M68OPCODE(OPCODE_IDX_(0xc000|SL(n,9)|(i*8)|0x100),&M68000Cpu::_and_mem);
			M68OPCODE(OPCODE_IDX_(0xc000|SL(n,9)|(i*8)|0x140),&M68000Cpu::_and_mem);
			M68OPCODE(OPCODE_IDX_(0xc000|SL(n,9)|(i*8)|0x180),&M68000Cpu::_and_mem);

			M68OPCODE(((0x1800|SL(n,6)|0x38|i)*2),&M68000Cpu::_muls);
			M68OPCODE(((0x1800|SL(n,6)|0x18|i)*2),&M68000Cpu::_mulu);
		}

		M68OPCODE(((0x1800|SL(n,6)|0x20|0x8)*2),&M68000Cpu::_exg);
		M68OPCODE(((0x1800|SL(n,6)|0x20|0x8|1)*2),&M68000Cpu::_exg);
		M68OPCODE(((0x1800|SL(n,6)|0x20|0x10|1)*2),&M68000Cpu::_exg);

		M68OPCODE(OPCODE_IDX_(0xb000|SL(n,9)|0x108),&M68000Cpu::_cmpm);
		M68OPCODE(OPCODE_IDX_(0xb000|SL(n,9)|0x148),&M68000Cpu::_cmpm);
		M68OPCODE(OPCODE_IDX_(0xb000|SL(n,9)|0x188),&M68000Cpu::_cmpm);
	}

	for(int i=0;i<0x8;i++){
		M68OPCODE(OPCODE_IDX_(0x800|(i*8)),&M68000Cpu::_btsti);
		M68OPCODE(OPCODE_IDX_(0x880|(i*8)),&M68000Cpu::_bclri);
		M68OPCODE(OPCODE_IDX_(0x8c0|(i*8)),&M68000Cpu::_bseti);
		M68OPCODE(OPCODE_IDX_(0x840|(i*8)),&M68000Cpu::_bchgi);

		M68OPCODE(((0x1C00|SL(i,6)|0x0)*2),&M68000Cpu::_asri);
		M68OPCODE(((0x1C00|SL(i,6)|0x4)*2),&M68000Cpu::_asri);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|0)*2),&M68000Cpu::_asri);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|0|4)*2),&M68000Cpu::_asri);
		M68OPCODE(((0x1C00|SL(i,6)|0x10)*2),&M68000Cpu::_asri);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|4|0)*2),&M68000Cpu::_asri);
		M68OPCODE(((0x1C00|SL(i,6)|0x0|0x20)*2),&M68000Cpu::_asli);
		M68OPCODE(((0x1C00|SL(i,6)|0x4|0x20)*2),&M68000Cpu::_asli);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|0|0x20)*2),&M68000Cpu::_asli);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|0|4|0x20)*2),&M68000Cpu::_asli);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|0x20)*2),&M68000Cpu::_asli);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|4|0|0x20)*2),&M68000Cpu::_asli);
		M68OPCODE(((0x1C00|SL(i,6)|0x1)*2),&M68000Cpu::_lsri);
		M68OPCODE(((0x1C00|SL(i,6)|0x5)*2),&M68000Cpu::_lsri);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|1)*2),&M68000Cpu::_lsri);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|1|4)*2),&M68000Cpu::_lsri);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|1)*2),&M68000Cpu::_lsri);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|4|1)*2),&M68000Cpu::_lsri);
		M68OPCODE(((0x1C00|SL(i,6)|0x1|0x20)*2),&M68000Cpu::_lsli);
		M68OPCODE(((0x1C00|SL(i,6)|0x5|0x20)*2),&M68000Cpu::_lsli);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|1|0x20)*2),&M68000Cpu::_lsli);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|1|4|0x20)*2),&M68000Cpu::_lsli);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|1|0x20)*2),&M68000Cpu::_lsli);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|4|1|0x20)*2),&M68000Cpu::_lsli);
		M68OPCODE(((0x1C00|SL(i,6)|0x3)*2),&M68000Cpu::_rori);
		M68OPCODE(((0x1C00|SL(i,6)|0x4|3)*2),&M68000Cpu::_rori);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|3)*2),&M68000Cpu::_rori);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|3|4)*2),&M68000Cpu::_rori);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|3)*2),&M68000Cpu::_rori);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|4|3)*2),&M68000Cpu::_rori);
		M68OPCODE(((0x1C00|SL(i,6)|0x3|0x20)*2),&M68000Cpu::_roli);
		M68OPCODE(((0x1C00|SL(i,6)|0x4|3|0x20)*2),&M68000Cpu::_roli);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|3|0x20)*2),&M68000Cpu::_roli);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|3|4|0x20)*2),&M68000Cpu::_roli);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|3|0x20)*2),&M68000Cpu::_roli);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|4|3|0x20)*2),&M68000Cpu::_roli);
		M68OPCODE(((0x1C00|0x40|0x10|0x20|0x8|i)*2),&M68000Cpu::_lsl);
		M68OPCODE(((0x1C00|0x40|0x10|0x8|i)*2),&M68000Cpu::_lsr);
		M68OPCODE(((0x1C00|0x10|0x20|8|i)*2),&M68000Cpu::_asl);
		M68OPCODE(((0x1C00|0x10|0x0|i|8)*2),&M68000Cpu::_asr);
		M68OPCODE(((0x1C00|0x80|0x40|0x10|0x20|0x8|i)*2),&M68000Cpu::_rol);
		M68OPCODE(((0x1C00|0x80|0x40|0x10|0x8|i)*2),&M68000Cpu::_ror);
		M68OPCODE(((0x1C00|0x80|0x10|0x20|0x8|i)*2),&M68000Cpu::_roxl);
		M68OPCODE(((0x1C00|0x80|0x10|0x8|i)*2),&M68000Cpu::_roxr);
		M68OPCODE(((0x1C00|SL(i,6)|0x2)*2),&M68000Cpu::_roxri);
		M68OPCODE(((0x1C00|SL(i,6)|0x4|2)*2),&M68000Cpu::_roxri);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|2)*2),&M68000Cpu::_roxri);
		M68OPCODE(((0x1C00|SL(i,6)|0x8|2|4)*2),&M68000Cpu::_roxri);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|2)*2),&M68000Cpu::_roxri);
		M68OPCODE(((0x1C00|SL(i,6)|0x10|4|2)*2),&M68000Cpu::_roxri);
		M68OPCODE(((0x1C00|SL(i,6)|0x20|0x2)*2),&M68000Cpu::_roxli);
		M68OPCODE(((0x1C00|SL(i,6)|0x20|0x4|2)*2),&M68000Cpu::_roxli);
		M68OPCODE(((0x1C00|SL(i,6)|0x20|0x8|2)*2),&M68000Cpu::_roxli);
		M68OPCODE(((0x1C00|SL(i,6)|0x20|0x8|2|4)*2),&M68000Cpu::_roxli);
		M68OPCODE(((0x1C00|SL(i,6)|0x20|0x10|2)*2),&M68000Cpu::_roxli);
		M68OPCODE(((0x1C00|SL(i,6)|0x20|0x10|4|2)*2),&M68000Cpu::_roxli);

		//M68OPCODE(OPCODE_IDX_(0x81c0|(i*0x200)),&M68000Cpu::_divs);
	//M68OPCODE(OPCODE_IDX_(0x80C0|(i*0x200)),&M68000Cpu::_divu);
	}

	OPCODE(OPCODE_IDX_(0x7c),&M68000Cpu::_orisr);
	OPCODE(OPCODE_IDX_(0x27c),&M68000Cpu::_andisr);
	OPCODE(OPCODE_IDX_(0x23c),&M68000Cpu::_andiccr);
	OPCODE(OPCODE_IDX_(0xa3c),&M68000Cpu::_eoriccr);
	OPCODE(OPCODE_IDX_(0x3c),&M68000Cpu::_oriccr);
	return 0;
}

int M68000Cpu::SetIO_cb(u32 adr,CoreMACallback a,CoreMACallback b){
	//printf("port %x %x\n",adr,M68IO(adr));
	if(M68IO(adr) >= 0x10000 || !_portfnc_write || !_portfnc_read)
		return -1;
	_portfnc_write[M68IO(adr)]=a;
	_portfnc_read[M68IO(adr)]=b;
	return 0;
}

int M68000Cpu::_exec(u32 status){
	int ret;

	ipc=2;
	ret=1;
#ifdef _DEVELOPa
	printf("%x ",_pc);
#endif
	/*RWOP(_pc,_opcode);
	FPI(_pc);
	FP(_pc);
	*/
//	_icache.p=NULL;
	if(_icache.p==NULL){
		if(_icache._remap){
			RMAP_PC(_pc,_icache._mem,R);
			_icache._remap=0;
			_icache._pc=_pc;
		}
		if(_icache._mem){
			__data=((u64 *)_icache._mem)[0];
			((u64 *)_icache._data)[0]= (__data & 0x00FF00FF00FF00FFULL) <<8;
			((u64 *)_icache._data)[0]|= (__data & 0xFF00FF00FF00FF00ULL) >>8;
			((u64 *)_icache._data)[2]= (__data & 0x00F000F000F000F0ULL)>>4;
			__data=((u64 *)_icache._mem)[1];
			((u64 *)_icache._data)[1]= (__data & 0x00FF00FF00FF00FFULL) <<8;
			((u64 *)_icache._data)[1]|= (__data & 0xFF00FF00FF00FF00ULL) >>8;
			((u64 *)_icache._data)[3]= (__data & 0x00F000F000F000F0ULL)>>4;
		}
		_icache._idx=0;
		_icache.p=_icache._data;
		_icache._opcode=&_icache._data[16];
	}
	_opcode=(*(u16 *)_icache.p);
	/*{
		u16 op;
		RWOP(_pc,op);
		if(_opcode != op) {
			printf("%u %u %u %08X %04x %04x\n",_icache._idx,_icache._idx+ipc,ipc,_pc,_opcode,op);EnterDebugMode();
		}
	}*/
//	printf("%x %x\n",*(u16 *)_icache._opcode,_opcode);
	switch(*(u16 *)_icache._opcode){
		case 1:
		case 2:
		case 3:{//MOVE
			u32 sz,res__;

			ret++;
			sz=_szmove[SR(_opcode,12)&3];
			res__=0;
			switch(SR(_opcode,6)&7){//dest mode
				case 0:{//Dx
					void *p;

					AEA(p,res__,_opcode,sz,R,ipc,SOURCE, );
					p=&REG_D(SR(_opcode,9)&7);
					WMPC(sz,p,res__);
					__68F(2,sz,_opcode,\x2c D%d,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 1:{//Ax
					void *p;

					SMODE(res__,_opcode,sz,R,ipc);
					p=&REG_A(SR(_opcode,9)&7);
					if(sz==1) res__= (u32)(s16)res__;
					WMPC(2,p,res__);
					__68F(2,sz,_opcode,\x2c A%d,"MOVEA",SR(_opcode,9)&7);
					goto V;
				}
				break;
				case 2:{//(Ax)
					SMODE(res__,_opcode,sz,R,ipc);
					WM(sz,REG_A(SR(_opcode,9)&7),res__);
					__68F(2,sz,_opcode,\x2c [A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 3:{//(Ax)+
					u16 d;

					SMODE(res__,_opcode,sz,R,ipc);
					d=SR(_opcode,9)&7;
					WM(sz,REG_A(d),res__);
					REG_A(d)+=BV(sz);
					if(d==7 && sz<1){REG_A(7)+=1;EnterDebugMode();}//fixme stack pointer
					__68F(2,sz,_opcode,\x2c [A%d]+,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 4:{//-(Ax) destmode
					void *p;
					u16 d;

					d=SR(_opcode,9)&7;
					AEA(p,res__,_opcode,sz,R,ipc,SOURCE,);
					REG_A(d)-=BV(sz);
					if(d==7 && sz < 1) {
						/*void *tmp__;
						u16 v__;

						sz = 1;
						REG_A(7)--;
						RMAP_PC(REG_A(7),tmp__,R);
						v__=*(u16 *)tmp__;
						res__|=SL((u8)v__,8);*/
						REG_A(7)--;
					}
					WM(sz,REG_A(d),res__);
					__68F(2,sz,_opcode,\x2c-[A%d],"MOVE",d);
				}
				break;
				case 5:{//(d16,Ax)
					s16 d;

					SMODE(res__,_opcode,sz,R,ipc);
					RWCPC(_pc+ipc,d);
					ipc +=2;
					WM(sz,REG_A(SR(_opcode,9) & 7)+d,res__);
					__68F(2,sz,_opcode,\x2c[\x24\x23%x\x2c A%d],"MOVE",d,SR(_opcode,9)&7);
				}
				break;
				case 6:{//(d16(Ax,Dx)
					s16 d;
					u32 a;

					SMODE(res__,_opcode,sz,R,ipc);
					RWCPC(_pc+ipc,d);
					ipc += 2;

					a=REG_D(SR(d,12));
					if((d&0x800) ==0)
						a=(u32)(s16)a;
					a=REG_A(SR(_opcode,9) & 7) + (s8)d + (s32)a;
					WM(sz,a,res__);//fixme Dx*size
					__68F(2,sz,_opcode,$%x[A%d\x2c %c%d],"MOVE",(u8)d,SR(_opcode,9)&7,'D',SR(d,12));
				}
				break;
				case 7:{//dest mode
					u32 d;
					void *p;

					SMODE(res__,_opcode,sz,R,ipc);
					switch(SR(_opcode,9)&7){//dst reg
						case 0:
							//EnterDebugMode()
							RWCPC(_pc+ipc,d);
							ipc += 2;
							WM(sz,d,res__);
							__68F(2,sz,_opcode&63,\x2c\x24%x.l,"MOVE",d);
						break;
						case 1:
							//EnterDebugMode()
							RLOP(_pc+ipc,d);
							ipc += 4;
							WM(sz,d,res__);
							__68F(2,sz,_opcode&63,\x2c\x24%x.l,"MOVE",d);
						break;
						default:
							EnterDebugMode();
							__F(7 %x %x,"MOVE",DESTM(SR(_opcode,6)),DESTR(SR(_opcode,6)));
						break;
					}
				}
				break;
				default:
					EnterDebugMode();
					__F(%x,"MOVE UNK",SR(_opcode,6)&7);
				break;
			}
			_ccr &= ~(C_BIT|V_BIT|Z_BIT|N_BIT);
			res__ &= SR(0xffffffff,32-SL(BV(sz),3));
			if(!res__)
				_ccr |= Z_BIT;
			else if(res__ & BV(SL(BV(sz),3)-1))
				_ccr  |= N_BIT;
		}
		break;
		case 0x4:{
			switch(SR(_opcode,6)&0x3f){
				default:
					EnterDebugMode();
					__F(%x,"4 UNK",SR(_opcode,6)&0x3f);
				break;
				case 3:{
					void *p;
					u32 a;

					AEA(p,a,_opcode,1,R,ipc,SOURCE,);
					a=(u32)_ccr;
					WMP(1,__address,p,a,_opcode);
					__68F(2,1,_opcode,SR ,"MOVE");
					goto W;
				}
				break;
//				case 5:
				case 0x10:
				case 0x11:
				case 0x12:{
					u32 a,b;
					u8 sz;
					void *p;

					sz=SR(_opcode,6) & 3;
					//ipc += ((sz+2) & 6);
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
					b=0;
					switch(sz){
						case 0:
							a=(u32)(s8)a;
							STATUSFLAGS("subb %0,%%cl\n",a,b,a);
						break;
						case 1:
							STATUSFLAGS("subw %0,%%cx\n",a,b,a);
						break;
						case 2:
							STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
						break;
					}
					_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
					WMP(sz,__address,p,a,_opcode);
					__68F(2,sz,_opcode, ,"NEG");
				}
				break;
				case 0x18:
				case 0x19:
				case 0x1a:{
					u32 a,res__;
					void *p;

					u8 sz = SR(_opcode,6)&3;
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
					a=~a;
					_ccr &= ~(C_BIT|V_BIT|Z_BIT|N_BIT);
					res__= a & SR(0xffffffff,32-SL(BV(sz),3));
					if(!res__)
						_ccr |= Z_BIT;
					else if(res__ & BV(SL(BV(sz),3)-1))
						_ccr |= N_BIT;

					WMP(sz,__address,p,a,_opcode);
					__68F(2,sz,_opcode, ,"NOT");
				}
				break;
				case 0x13:{
					void *p;
					u32 a;

					AEA(p,a,_opcode,1,R,ipc,SOURCE,);
					_change_sr( (_ccr & 0xff00)|(u8)a);

					//_setStatus(S_IRQ_CHECK);
					__68F(2,1,_opcode, CCR,"MOVE");
					//goto W;
				}
				break;
				case 0x1b:{
					void *p;
					u32 a;

					AEA(p,a,_opcode,1,R,ipc,SOURCE,);
					if(_ccr & S_BIT)
						_change_sr(a);
					else{
						//EnterDebugMode();
						_ccr |= S_BIT;
						//_pc += ipc;
						OnException(8,0);
					}
					//_setStatus(S_IRQ_CHECK);
					__68F(2,1,_opcode, SR,"MOVE");
					goto W;
				}
				break;
				case 0x8:
				case 0x9:
				case 0xa:{
					u32 d,sz;
					void *p;

					_ccr = (_ccr & ~(C_BIT|N_BIT|V_BIT)) | Z_BIT;
					sz=SR(_opcode,6)&3;
					AEA(p,d,_opcode,sz,R,ipc,SOURCE,);
					WMP(sz,__address,p,0,_opcode);
					__68F(2,sz,_opcode,NOARG,"CLR");
				}
				break;
				case 0x7:
				case 0xf:
				case 0x17:
				case 0x1f:
				case 0x27:
				case 0x2f:
				case 0x37:
				case 0x3f:
					_lea();
				break;
				case 0x21:{
					u32 v;

					switch(SR(_opcode,3)&7){
						case  0:
							v=REG_D(_opcode&7);
							v=SL(v,16)|SR(v,16);
							REG_D(_opcode&7) = v;
							_ccr &= ~(Z_BIT|N_BIT|C_BIT|V_BIT);
							if(v == 0)
								_ccr |= Z_BIT;
							else if(v&0x80000000)
								_ccr |= N_BIT;
							_68F(D%d,"SWAP",_opcode&7);
						break;
						case 2:
						case 5:
						case 6:
						case 7:{
							u32 d;
							void *p;

							AEA(p,d,_opcode,2, ,ipc,SOURCE, );
							REG_SP -=4;
							if((_opcode & 0x3f)==0x38) d = (u32)(s32)(s16)d;
							WLPC(REG_SP,d);
							__68F(2,2,_opcode,NOARG,"PEA");
						}
						break;
						default:
							EnterDebugMode();
							__F(%x,"4 21 UNK",SR(_opcode,3)&0x7);
						break;
					}
				}
				break;
				case 0x22:
				case 0x23:
					(((CCore *)this)->*_opcodescb[OPCODE_IDX_(_opcode)])();
				break;
				case 0x28:
				case 0x29:
				case 0x2a:{
					u32 a;
					u8 sz;

					SMODE(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					//printf("tst %x %x\n",a,BV(BV(sz)*8 -1));
					_ccr &= ~(N_BIT|Z_BIT|C_BIT|V_BIT);
					if(!a)
						_ccr |= Z_BIT;
					else if((a & BV(SL(BV(sz),3)-1)))
						_ccr |= N_BIT;
					__68F(2,sz,_opcode,NOARG,"TST");
				}
				break;
				case 0x32:
				case 0x33:{//movem w l regs to mem
					u16 l,sz;
					u32 a,v;
					void *p,*ps;
					int i,n;

					//RWPC(_pc+ipc,l);
					RWCPC(_pc+ipc,l);
					ipc+= 2;
					sz=1+(SR(_opcode,6)&1);
					AEA_(p,ps,a,_opcode,sz,R,ipc,SOURCE, );
					//printf("mm %x %p ",__address,ps);
					//fixme !!! RM instead RMP
					if(SOURCEM(_opcode)==4){//post decrement
						for(n=0,i=15;i>=0;i--){
							if(!(l & BV(15-i)))
								continue;
							RMP(sz,__address,p,v,_opcode);
							//RM(sz,__address-n,v);
							REG_D(i)=v;
							p = (u8 *)p - SL(sz,1);
							n+=SL(sz,1);
						}
						*(u32 *)ps -= (n-SL(sz,1));
					}
					else{
						for(n=i=0;i<16;i++){
							if(!(l & BV(i)))
								continue;
							RMP(sz,__address,p,v,_opcode);
							//RM(sz,__address+n,v);
							switch(sz){
								case 1:
									v|=REG_D(i)&0xffff0000;
								break;
								case 0:
									v|=REG_D(i)&0xffffff00;
								break;
							}
							REG_D(i)=v;
							p = (u8 *)p  + SL(sz,1);
							n+=SL(sz,1);
						}
						if(ps)
							*(u32 *)ps += n-SL(sz,1);
					}
				//	printf(" %x\n",n);
					__68F(4,sz,_opcode,%x,"MOVEM",l);
				}
				break;
				case 0x39:
					switch(_opcode & 0x3f){
						case 0x12:
						case 0x14:
						case 0x15:
						case 0x16:{
							s16 d;

							//RWPC(_pc+2,d);
							RWCPC(_pc+2,d);
							ipc+=2;
							REG_SP-=4;
							WLPC(REG_SP,REG_A(_opcode&7));
							REG_A(_opcode&7)=REG_SP;
							REG_SP += d;
							_68F(A%d\x2c#%x,"LINK",_opcode&7,(s32)(s16)d);
						}
						break;
						case 0x31:
							_68F(NOARG,"NOP");
						break;
						case 0x30:
							_68F(NOARG,"RESET");
						break;
						case 0x33:{
							u16 v;

							_icache.invalidate();
							_68F(NOARG,"RTE");
							if(!(_ccr & S_BIT))
								printf("rte\n");
							else{
								RWPC(REG_SP,v);
								REG_SP += 2;
								RLPC(REG_SP,_pc);
								REG_SP += 4;
								//printf("CPU RTE %x\n",_pc);
								LOADCALLSTACK(_pc);
								//DLOG("CPU RTE %x",_irq_pending);
								_change_sr(v);
								ipc=0;
								goto W;
							}
						}
						break;
						case 0x35:
							_68F(NOARG,"RTS");
							RLPC(REG_SP,_pc);
							REG_SP += 4;
							LOADCALLSTACK(_pc);
							ipc=0;
							_icache.invalidate();
						break;
						case 0x2b:
						case 0x3b:
							_68F(NOARG,"ILLEGAL");
							_pc+=ipc;
							OnException(4,0);
						break;
						case 0x32:{
							u16 v;

							RWCPC(_pc+2,v);
							ipc+=2;
							if(_ccr & S_BIT){
								Sleep();
								_change_sr(v);
							}
							else printf("stop \n");
							_68F(\x3\x24%x,"STOP",v);
							goto W;
						}
						break;
						default:
							switch(SR(_opcode,4)&3){
								case 0:
									EnterDebugMode(DEBUG_BREAK_OPCODE);
									_change_sr(_ccr|S_BIT);
									_pc+=ipc;
									OnException(32+(_opcode&15),0);
									_ccr |= S_BIT;
									_68F(%x,"TRAP",_opcode&15);
								break;
								case 1:
									REG_SP=REG_A(_opcode&7);
									RLPC(REG_SP,REG_A(_opcode&7));
									REG_SP += 4;
									_68F(A%d,"UNLK",_opcode&7);
								break;
								case 2:
								if(!(_ccr & S_BIT)) printf("linos\n");
									if(_opcode &8){
										REG_A(_opcode&7)=REG_USP;
										_68F(USP\x2c A%d,"MOVE",_opcode&7);
									}
									else{
										REG_USP=REG_A(_opcode&7);
										_68F(A%d\x2cUSP,"MOVE",_opcode&7);
									}
								break;
								default:
									printf("4 39 %x\n",SR(_opcode,4)&3);
									EnterDebugMode();
								break;
							}
						break;
					}
				break;
				case 0x3a:{
					u32 d;

					SMODE(d,_opcode&63,2,,ipc);
					REG_SP -= 4;
					WLPC(REG_SP,_pc+ipc);
					STORECALLLSTACK(_pc+ipc);
					_icache.invalidate();
					__68F(2,2,_opcode&63,NOARG,"JSR");
					_pc=d;
					ipc=0;
					ret += 2;
				}
				break;
				case 0x3b:{
					u32 d;

					SMODE(d,_opcode,2,,ipc);
					__68F(2,2,_opcode,NOARG,"JMP");
					_icache.invalidate();
					_pc=d;
					ipc=0;
					ret++;
				}
				break;
			}
		}
		break;
		case 5:
			switch(SR(_opcode,3) & 0x1f){
				case 0:
				case 2:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 0xa:
				case 0xd:
				case 0xe:
				case 0xf:
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x15:
				case 0x17:{
					u32 a,sz,b;
					void *p;

					sz=SR(_opcode,6)&3;
					AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
					if(!(b=SR(_opcode,9)&7))
						b=8;
					//printf("%x %x %x %x\n",a,b,__address,_pc);
					if(!(_opcode & 0x100)){
						if((_opcode & 0x38) == 8){
							a+=b;
							sz=2;
						}
						else{
							switch(sz){
								case 0:
									STATUSFLAGS("addb %0,%%cl\n",a,a,b);
								break;
								case 1:
									STATUSFLAGS("addw %0,%%cx\n",a,a,b);
								break;
								default:
									STATUSFLAGS("addl %0,%%ecx\n",a,a,b);
								break;
							}
							_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
						}
						WMP(sz,__address,p,a,_opcode);
						__68F(2,sz,_opcode,\x2c #%x,"ADDQ",b);
					}
					else{
						if((_opcode & 0x38) == 8){
							sz=2;
							a-=b;
						}
						else{
							switch(sz){
								case 0:
									STATUSFLAGS("subb %0,%%cl\n",a,a,b);
								break;
								case 1:
									STATUSFLAGS("subw %0,%%cx\n",a,a,b);
								break;
								default:
									STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
								break;
							}
							_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
						}
						WMP(sz,__address,p,a,_opcode);
						//printf(" %x\n",a);
						__68F(2,sz,_opcode,\x2c #%x,"SUBQ",b);
					}
				}
				break;
				case 0x18:
				case 0x1d:
				case 0x1e:
				case 0x1f:{
					char i,s[10];

					i=0;
					switch(SR(_opcode,8)&15){
						case 0:
							i=0xff;
						break;
						case 1:
						break;
						case 4:
							if(!(_ccr&C_BIT))//bcc
								i=0xff;
						break;
						case 5:
							if((_ccr&C_BIT))//bcc
								i=0xff;
						break;
						case 6:
							if(!(_ccr&Z_BIT))
								i=0xff;
						break;
						case 7:
							if((_ccr&Z_BIT))
								i=0xff;
						break;
						case 0xc:
							if((SR(_ccr,N_SHIFT)&1) == (SR(_ccr,V_SHIFT)&1))
								i=0xff;
						break;
						case 0xd://blt
							if((SR(_ccr,N_SHIFT)&1) != (SR(_ccr,V_SHIFT)&1))
								i=0xff;
						break;
						case 0xe:
					//		if( ((SR(_ccr,Z_SHIFT)&1) | ((SR(_ccr,N_SHIFT)&1) ^ (SR(_ccr,V_SHIFT)&1)))== 0)
							if( ((_ccr&Z_BIT)==0 && (SR(_ccr,N_SHIFT)&1) == (SR(_ccr,V_SHIFT)&1)) )
								i=0xff;
						break;
						case 0xf://ble
				//	if( ((SR(_ccr,Z_SHIFT)&1) | ((SR(_ccr,N_SHIFT)&1) ^ (SR(_ccr,V_SHIFT)&1))))
							if( ((_ccr&Z_BIT) || (SR(_ccr,N_SHIFT)&1) != (SR(_ccr,V_SHIFT)&1)) )
								i=0xff;
						break;
						default:
							EnterDebugMode();
							printf("Scc %x\n",SR(_opcode,8)&15);
						break;
					}
					{
						u32 d;
						void *p;
						AEA(p,d,_opcode,2, ,ipc,SOURCE,);
						WMPC(0,p,i);
					}
					sprintf(s,"S%s",&_cc[SR(_opcode,8) & 15][1]);
					__68F(2,0,_opcode,%s,s,"");
				}
				break;
				case 0x19:{
					s16 d;
					char i,s[10];

					//if(_pc==0xc0a78c){
					//if(_icache.p){
						RWCPC(_pc+2,d);
						//d=SWAP16(*(u16 *)&_icache.p[2]);
					//}
						/*if(d!=pd){//fc0610
							printf("%llx %llx\n",*(u64 *)_icache._data,*(u64 *)&_icache._data[8]);
							EnterDebugMode();
						}*/
					i=0;
					switch(SR(_opcode,8)&15){
						case 0:
							i=1;
							EnterDebugMode();
						break;
						case 1:
							i=0;
						break;
						case 5:
							if((_ccr&C_BIT))//bcs
								i=1;
						break;
						case 6:
							if(!(_ccr & Z_BIT)) i=1;
						break;
						case 7:
							if((_ccr & Z_BIT)) i=1;
						break;
						case 0xb://bmi
							if((_ccr & N_BIT)) i=1;
						break;
						case 0xe:
							if(( ((_ccr&Z_BIT)==0 && (SR(_ccr,N_SHIFT)&1) == (SR(_ccr,V_SHIFT)&1)) ))
								i=1;
						break;
						case 0xf://ble
							if( ((_ccr&Z_BIT) || (SR(_ccr,N_SHIFT)&1) != (SR(_ccr,V_SHIFT)&1)) )
								i=1;
						break;
						default:
							printf("Dcc %d %x\n",SR(_opcode,8)&15,_pc);
							EnterDebugMode();
						break;
					}
					//sprintf(s,"D%s",_cc[SR(_opcode,8) & 15]);
					//_68F(D%d\x2c #%x,s,_opcode&7,_pc+d+2);
					_68F(D%s D%d\x2c #%x,"\x08",_cc[SR(_opcode,8) & 15],_opcode&7,_pc+d+2);
					if(!i){
						u16 a=(u16)REG_D(_opcode&7);
						REG_D(_opcode&7)=(REG_D(_opcode&7)&0xffff0000)|--a;
						if(i==0 && a != (u16)-1){
							_pc += d-2;
							ret += 6;
							_icache.invalidate();
						}
						else
							i=1;
					}
					//if(i)
					ipc+=2;
				}
				break;
				default:
					__F(%x,"ADDQ",SR(_opcode,3)&0x1f);
					EnterDebugMode();
				break;
			}
		break;
		case 6:{
			s32 d = (s8)_opcode;
			u32 a,i;
			char s[10];

			a=i=0;
			switch((u8)d){
				case 0:
					//RWPC(_pc+2,d);
					RWCPC(_pc+2,d);
					d=(s32)(s16)d;
					a+=2;
					//printf("0 %d\n",d);
				break;
				case 0xff:
					RLOP(_pc+2,d);
					//printf("1 %d\n",d);
					a+=2;
				break;
			}
			strcpy(s,_cc[SR(_opcode,8)&0xf]);
			switch(SR(_opcode,8)&0xf){
				case 0:
					i=1;
				break;
				case 1:
					//EnterDebugMode();
					strcpy(s,"BSR");
					REG_SP -= 4;
					WLPC(REG_SP,_pc+ipc+a);
					i=1;
					STORECALLLSTACK(_pc+ipc+a);
					//ipc=0;
				break;
				case 2://bhi
					if(!(_ccr & (C_BIT|Z_BIT)))
						i=1;
				break;
				case 3:
					if((_ccr & (C_BIT|Z_BIT)))//bls
						i=1;
				break;
				case 4:
					if(!(_ccr&C_BIT))//bcc
						i=1;
				break;
				case 5:
					if((_ccr&C_BIT))//bcs
						i=1;
				break;
				case 6://bne
					if(!(_ccr&Z_BIT)){
						i=1;
						//ipc=0;
					}
				break;
				case 7://beq
					if(_ccr & Z_BIT){
						i=1;
					//	ipc=0;
					}
				break;
				case 8://bvc
					if(!(_ccr & V_BIT)){
						i=1;
					//	ipc=0;
					}
				break;
				case 9://bvs
					if(_ccr & V_BIT){
						i=1;
					//	ipc=0;
					}
				break;
				case 0xa://bpl
					if(!(_ccr & N_BIT)){
						i=1;
					//	ipc=0;
					}
				break;
				case 0xb://bmi
					if(_ccr & N_BIT){
						i=1;
					//	ipc=0;
					}
				break;
				case 0xc://bge
					if((SR(_ccr,N_SHIFT)&1) == (SR(_ccr,V_SHIFT)&1))
						i=1;
					//	ipc=0;
				break;
				case 0xd://blt
					if((SR(_ccr,N_SHIFT)&1) != (SR(_ccr,V_SHIFT)&1))
						i=1;
				break;
				case 0xe://bgt
					//if( ((SR(_ccr,Z_SHIFT) & 1) | ((SR(_ccr,N_SHIFT)&1) ^ (SR(_ccr,V_SHIFT)&1))) == 0)
					if( ((_ccr&Z_BIT)==0 && (SR(_ccr,N_SHIFT)&1) == (SR(_ccr,V_SHIFT)&1)) )
						i=1;
				break;
				case 0xf://ble
				//	if( ((SR(_ccr,Z_SHIFT)&1) | ((SR(_ccr,N_SHIFT)&1) ^ (SR(_ccr,V_SHIFT)&1))))
					if( ((_ccr&Z_BIT) || (SR(_ccr,N_SHIFT)&1) != (SR(_ccr,V_SHIFT)&1)) )
						i=1;
				break;
				default:
					__F(6 %x,"unk",SR(_opcode,8)&0xf);
					EnterDebugMode();
				break;
			}
			_68F($%x,s,_pc+d+ipc);
			if(i){
				_pc+=d-a;
				ret+=3;
				_icache.invalidate();
			}
			ipc+=a;
		}
		break;
		case 0x7:
			REG_D(SR(_opcode,9)&7)=(u32)(s8)_opcode;
			_ccr &= ~(C_BIT|V_BIT|N_BIT|Z_BIT);
			if((u8)_opcode == 0)
				_ccr |= Z_BIT;
			else if(_opcode & 0x80)
				_ccr |= N_BIT;
			_68F(#%x\x2c D%d,"MOVEQ",(s8)_opcode,SR(_opcode,9)&7);
		break;
		case 8:
			switch(SR(_opcode,6)&7){
				case 3:{
					void *p;
					u32 a,i,b,r;

					AEA(p,a,_opcode,1,R,ipc,SOURCE, );
					i=SR(_opcode,9)&7;
					if(!(u16)a){
						printf("divu 0 %08x\n\n",_pc);
						//_change_sr(_ccr|S_BIT);
						_pc+=ipc;
						OnException(5,0);
						_ccr |= S_BIT;
					//	EnterDebugMode();
					}
					else{
						b=REG_D(i);
						r=(b / (u16)a);
						if(r>0xffff){
							_ccr &=~(C_BIT);
							_ccr |=(V_BIT);
						}
						else{
							_ccr &=~(N_BIT|V_BIT|C_BIT|Z_BIT);
							b-=(r*(u16)a);
							REG_D(i) = ((u16)r)|SL(b,16);
							if(!r)
								_ccr |= Z_BIT;
							else if(r&0x8000)
								_ccr |= N_BIT;
						}
					}
					__68F(2,1,_opcode,\x2c D%d,"DIVU",i);
				}
				break;
				case 7:{
					void *p;
					u32 a,i,b,r;

					AEA(p,a,_opcode,1,R,ipc,SOURCE, );
					i=SR(_opcode,9)&7;
					if(!(u16)a){
						printf("divs 0 %08x\n\n",_pc);
						//_change_sr(_ccr|S_BIT);
						_pc+=ipc;
						OnException(5,0);
						_ccr |= S_BIT;
						//EnterDebugMode();
					}
					else{
						b=REG_D(i);
						r=(u32)(s16)((s32)b / (s16)a);
						if ((r & 0xffff8000) != 0 && (r & 0xffff8000) != 0xffff8000) {
							_ccr |=V_BIT|N_BIT;
						}
						else {
							_ccr &=~(N_BIT|V_BIT|C_BIT|Z_BIT);
							a=b-(r*(s16)a);
							if (((s16)a < 0) != ((s32)b < 0)) a = -a;

							REG_D(i) = ((u16)r)|SL(a,16);
							if(!r)
								_ccr |= Z_BIT;
							else if(r&0x8000)
								_ccr |= N_BIT;
						}
					}
					__68F(2,1,_opcode,\x2c D%d,"DIVS",i);
				}
					break;
				default:
					(((CCore *)this)->*_opcodescb[OPCODE_IDX_(_opcode)])();
				break;
			}
		break;
		case 9:{
			u32 a,b;
			void *p;
			u8 sz,m,d;
			char s[20]={0};

			m=SR(_opcode,6) & 7;
			if((sz = m & 3) > 2) sz=SR(m,2)+1;
			AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
			d=SR(_opcode,9)&7;
			switch(m){
				case 2:
					sprintf(s,"D%d",d);
					b=(u32)(s32)REG_D(d);
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
					REG_D(d)=a;
				break;
				case 1:
					sprintf(s,"D%d",d);
					b=(u32)(s16)REG_D(d);
					STATUSFLAGS("subw %0,%%cx\n",a,b,a);
					REG_D(d)=(REG_D(d) & 0xffff0000) | (u16)a;
					_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
				break;
				case 0:
					sprintf(s,"D%d",d);
					b=(u32)(s8)REG_D(d);
					STATUSFLAGS("subb %0,%%cl\n",a,b,a);
					//REG_D(d)=a;
					REG_D(d)=(REG_D(d) & 0xffffff00) | (u8)a;
					_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
				break;
				case 3:
					sprintf(s,"A%d",d);
					b=REG_A(d);
					//STATUSFLAGS("subw %0,%%cx\n",a,b,a);
					REG_A(d)=b-(s32)(s16)a;
				break;
				case 7:
					sprintf(s,"A%d",d);
					b=REG_A(d);
					//STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					REG_A(d)=b-a;
				break;
				default:
					switch(_opcode & 0x30){
						case 0:
							__F(%x,"unk",SR(_opcode,12));
							EnterDebugMode();
						break;
						default:
							switch(m){
								case 4:
									sprintf(s,"D%d",d);
									b=a;
									a=(u32)(s32)REG_D(d);
									STATUSFLAGS("subb %0,%%cl\n",a,b,a);
								break;
								case 5:
									sprintf(s,"D%d",d);
									b=a;
									a=(u32)(s32)REG_D(d);
									STATUSFLAGS("subw %0,%%cx\n",a,b,a);
								break;
								case 6:
									sprintf(s,"D%d",d);
									b=a;
									a=(u32)(s32)REG_D(d);
									STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
								break;
							}
							WMP(sz,__address,p,a,_opcode);
							_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
						break;
					}
				break;
			}
			__68F(2,sz,_opcode,\x2c%s,"SUB",s);
		}
		break;
		case 0xb:{
			switch(SR(_opcode,6)&7){
				case 0:
				case 1:
				case 2:{
					u32 a,b;
					u8 sz;

					SMODE(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					b=REG_D(SR(_opcode,9)&7);
					switch(sz){
						case 2:
							//if ((SR(_opcode,3)&7) > 1)  a=SWAP32(a);
						break;
						case 1:
							b=(u32)(s16)b;
							a=(u32)(s16)a;
						break;
						case 0:
							b=(u32)(s8)b;
							a=(u32)(s8)a;
						break;
						default:
							EnterDebugMode();
						break;
					}
					//printf("cmp  %x %x %x\n",a,b,__address);
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					__68F(2,sz,_opcode&63,\x2c D%d,"CMP",SR(_opcode,9)&7);
				}
				break;
				case 7:
				case 3:{
					u32 a,b;
					u8 sz;

				//	EnterDebugMode();
					SMODE(a,_opcode,(sz=1+(SR(_opcode,8) & 1)),R,ipc);
					b=REG_A(SR(_opcode,9)&7);
					switch(sz){
						case 1:
							b=(s16)b;
							a=(s16)a;
						break;
					}
					STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
					__68F(2,sz,_opcode&63,\x2c A%d,"CMPA",(SR(_opcode,9)&7));
				}
				break;
				case 4:
				case 5:
				case 6:
					(((CCore *)this)->*_opcodescb[OPCODE_IDX_(_opcode)])();
				break;
				default:
					EnterDebugMode();
					__F(%d,"CMP",SR(_opcode,6)&7);
				break;
			}
		}
		break;
		case 0:
		case 0xc:
		case 0xd:
		case 0xe:
			(((CCore *)this)->*_opcodescb[OPCODE_IDX_(_opcode)])();
		break;
		default:
			__F(%x,"unk",SR(_opcode,12));
			EnterDebugMode();
		break;
	}
V:
#ifdef _DEVELOPa
	printf("%x:%x\n ",_pc,ipc);
#endif
	_pc += ipc;
	goto Z;
W:
	_pc += ipc;
	if(_irq_pending) machine->OnEvent(0,(LPVOID)-1);
Z:
	if((_icache._idx+=ipc) > 9 || _icache._invalidate/* || ipc > 5*/){
		if(_icache._pc > _pc)
			_icache._remap=1;
		_icache._mem += _icache._idx;
		_icache.p=NULL;
		_icache._invalidate=0;
	}
	else{
		_icache.p += ipc;
		_icache._opcode += ipc;
	}
	return ret;
}

int M68000Cpu::Disassemble(char *dest,u32 *padr){
	u32 op,pc,oipc;
	char c[400],*cc;

	*((u64 *)c)=0;
	cc=&c[200];
	*((u64 *)cc)=0;

	pc=_pc;
	op=_opcode;
	oipc=ipc;
	_pc = *padr;
	RWOP(_pc,_opcode);
	dipc=ipc=2;
	sprintf(c,"%08X ",_pc);
	switch(SR(_opcode,12)){
		case 0:
			(((CCore *)this)->*_disopcodescb[OPCODE_IDX_(_opcode)])(cc);
		break;
		case 1:
		case 2:
		case 3://MOVE
			switch(SR(_opcode,6)&7){//dest mode
				case 0:{//Dx
					u32  s;
					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode,\x2c D%d,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 1:{//Ax
					u32 s;
					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode,\x2c A%d,"MOVEA",SR(_opcode,9)&7);
				}
				break;
				case 2:{//(Ax)
					u32 s;

					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 3:{//(Ax)+
					u32 s;

					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode & 0x3f,\x2c [A%d]+,"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 4:{//-(Ax) destmode
					u32 s;

					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c -[A%d],"MOVE",SR(_opcode,9)&7);
				}
				break;
				case 5:{
					s16 d;
					u32 s,sz;

					sz=_szmove[SR(_opcode,12)&3];
					SMODES(s,_opcode&63,sz,R,ipc);
					RWPC(_pc+ipc,d);
					ipc+=2;
					__68S(dipc,sz,_opcode,\x2c[\x24\x23%x\x2c A%d],"MOVE",d,SR(_opcode,9)&7);

				}
				break;
				case 6:{
					s16 d;
					u32 s,sz;

					sz=_szmove[SR(_opcode,12)&3];
					SMODES(s,_opcode&63,sz,R,ipc);
					RWPC(_pc+ipc,d);
					ipc+=2;
					__68S(dipc,sz,_opcode,$%x[A%d\x2c %c%d],"MOVE",(u8)d,SR(_opcode,9)&7,'D',SR(d,12));
				}
				break;
				case 7:{//dest mode
					u32 s,d;

					SMODES(s,_opcode&63,_szmove[SR(_opcode,12)&3],R,ipc);
					switch(SR(_opcode,9)&7){//dst reg
						case 0:{
							RWPC(_pc+ipc,d);
							ipc += 2;
							__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c #%x,"MOVE",d);
						}
						break;
						case 1:
							RLOP(_pc+ipc,d);
							ipc += 4;
							__68S(dipc,_szmove[SR(_opcode,12)&3],_opcode&63,\x2c $%x.l,"MOVE",d);
						break;
						default:
							_68S(7 %x %x,"MOVE",SR(_opcode,9)&7,SR(_opcode,3)&7);
						break;
					}
				}
				break;
				default:
					_68S(%x,"MOVE UNK",SR(_opcode,6)&7);
				break;
			}
		break;
		case 0x4:
			switch(SR(_opcode,6)&0x3f){
				default:
					_68S(%x,"4 UNK",SR(_opcode,6)&0x3f);
				break;
				case 3:{
					u32  d;

					SMODES(d,_opcode,1, ,ipc);
					__68S(dipc,1,_opcode,SR,"MOVE");
				}
				break;
				case 0x8:
				case 0x9:
				case 0xa:
				case 0xb:{
					u32 d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(dipc,SR(_opcode,6)&3,_opcode,NOARG,"CLR");
				}
				break;
				case 0x5:
				case 0x10:
				case 0x11:
				case 0x12:{
					u32  d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(dipc,SR(_opcode,6)&3,_opcode, ,"NEG");
				}
				break;
				case 0x18:
				case 0x19:
				case 0x1a:{
					u32  d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(dipc,SR(_opcode,6)&3,_opcode, ,"NOT");
				}
				break;
				case 0x21:{
					u32 v;

					switch(SR(_opcode,3)&7){
						case  0:
							_68S(D%d,"SWAP",_opcode&7);
						break;
						case 2:
						case 5:
						case 6:
						case 7:{
							u32  d;

							SMODES(d,_opcode,2, ,ipc);
							__68S(dipc,2,_opcode, ,"PEA");

							/*switch(_opcode&7){
								case 1:
									ipc+=4;
									RLPC(_pc+2,v);
									_68S(#%x,"PEA",v);
								break;
								case 0:
									ipc+=2;
									RWPC(_pc+2,v);
									_68S(#%x,"PEA",v);
								break;
							}*/
						}
						break;
						default:
							__S(%x,"4 21 UNK",SR(_opcode,3)&0x7);
						break;
					}
				}
				break;
				case 0x13:{
					u32  d;

					SMODES(d,_opcode,1, ,ipc);
					__68S(dipc,1,_opcode,\x2c CCR,"MOVE");
				}
				break;
				case 0x1b:{
					u32  d;

					SMODES(d,_opcode,1, ,ipc);
					__68S(dipc,1,_opcode,SR,"MOVE");
				}
				break;
				case 0x7:
				case 0xf:
				case 0x17:
				case 0x1f:
				case 0x27:
				case 0x2f:
				case 0x37:
				case 0x3f:{
					u32 d;

					SMODES(d,_opcode,2, ,ipc);
					__68S(dipc,2,_opcode,\x2c A%d,"LEA",SR(_opcode,9)&7);
				}
				break;
				case 0x22:
				case 0x23:
				case 0x32:
				case 0x33:
					(((CCore *)this)->*_disopcodescb[OPCODE_IDX_(_opcode)])(cc);
				break;
				case 0x28:
				case 0x29:
				case 0x2a:{
					u32 a;
					u8 sz;

					SMODES(a,_opcode,(sz=SR(_opcode,6)&3),R,ipc);
					__68S(dipc,sz,_opcode,NOARG,"TST");
				}
				break;
				case 0x39:
					switch(_opcode & 0x3f){
						case 0x12:
						case 0x14:
						case 0x15:
						case 0x16:{
							s16 d;

							RWPC(_pc+2,d);
							ipc+=2;
							_68S(A%d\x2c#%d,"LINK",_opcode&7,(s32)(s16)d);
						}
						break;
						case 0x31:
							_68S(NOARG,"NOP");
						break;
						case 0x32:{
							u16 d;

							RWPC(_pc+2,d);
							ipc+=2;
							_68S(\x23\x24%x,"STOP",d);
						}
						break;
						case 0x33:
							_68S(NOARG,"RTE");
						break;
						case 0x35:
							_68S(NOARG,"RTS");
						break;
						case 0x30:
							_68S(NOARG,"RESET");
						break;
						case 0x2b:
						case 0x3b:
							_68S(NOARG,"ILLEGAL");
						break;
						default:
							switch(SR(_opcode,4)&3){
								case 0:
									_68S(#%x,"TRAP",_opcode&15);
								break;
								case 1:
									_68S(A%d,"UNLK",_opcode&7);
								break;
								case 2:
									if(_opcode & 8){
										_68S(USP\x2c A%d,"MOVE",_opcode&7);
									}
									else{
										_68S(A%d\x2cUSP,"MOVE",_opcode&7);
									}
								break;
							}
						break;
					}
				break;
				case 0x3a:{
					u32 d;

					SMODES(d,_opcode&63,2,R,ipc);
					__68S(dipc,2,_opcode&63, ,"JSR");
				}
				break;
				case 0x3b:{
					u32 d;

					SMODES(d,_opcode&63,2,R,ipc);
					__68S(dipc,2,_opcode&63, ,"JMP");
				}
				break;
			}
		break;
		case 5:
			switch(SR(_opcode,3)&0x1f){
				case 0:
				case 2:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 0xa:
				case 0xd:
				case 0xe:
				case 0xf:
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x15:
				case 0x17:{
					char c[][5]={"ADDQ","SUBQ"};
					u8 v;

					if(!(v=SR(_opcode,9)&7)) v=8;
					__68S(ipc,SR(_opcode,6)&3,_opcode,\x2c #%x,c[SR(_opcode,8)&1],v);
				}
				break;
				case 0x18:
				case 0x1d:
				case 0x1e:
				case 0x1f:{
					u32 d;
					char s[10]="S\0";

					strcat(s,&_cc[SR(_opcode,8) & 15][1]);
					SMODES(d,_opcode,2, ,ipc);
					__68S(ipc,2,_opcode,NOARG,s);
				}
				break;
				case 0x19:{
					s16 d;
					char s[10]="D\0";

					RWPC(_pc+2,d);
					ipc+=2;
					strcat(s,_cc[SR(_opcode,8) & 15]);
					_68S(D%d\x2c #%x,s,_opcode&7,_pc+d+2);
				}
				break;
				default:
					__S(%x,"ADDQ",SR(_opcode,3)&0x1f);
				break;
			}
		break;
		case 6:{
			s32 d = (s8)_opcode;
			switch((u8)d){
				case 0:
					RWPC(_pc+2,d);
					d=(s32)(s16)d;
					ipc+=2;
				break;
				case 0xff:
					RLOP(_pc+2,d);
					ipc+=2;
				break;
			}
			switch(SR(_opcode,8)&0xf){
				case 1:
					_68S(\x24%x,"BSR",_pc+d+ipc);
				break;
				default:
					_68S(\x24%x,_cc[SR(_opcode,8)&0xf],_pc+d+ipc);
				break;
			}
		}
		break;
		case 0x7:
			_68S(#%x\x2c D%d,"MOVEQ",(s8)_opcode,SR(_opcode,9)&7);
		break;
		case 8:
			switch(SR(_opcode,6)&7){
				case 3:{
					u32 a;

					SMODES(a,_opcode,1,R,ipc);
					__68S(dipc,1,_opcode,\x2c D%d,"DIVU",SR(_opcode,9)&7);
				}
				break;
				case 7:{
					u32 a;

					SMODES(a,_opcode,1,R,ipc);
					__68S(dipc,1,_opcode,\x2c D%d,"DIVS",SR(_opcode,9)&7);
				}
				break;
				default:
					(((CCore *)this)->*_disopcodescb[OPCODE_IDX_(_opcode)])(cc);
				break;
			}
		break;
		case 9:{
			u32 a;
			u8 sz,m;
			char s[20]={0};

			m=SR(_opcode,6) & 7;
			if((sz = m & 3) > 2) sz=SR(m,2)+1;
			SMODES(a,_opcode,sz,R,ipc);
			switch(m){
				case 0:
				case 1:
				case 2:
					sprintf(s,"D%d",SR(_opcode,9)&7);
				break;
				case 4:
				case 6:
				case 5:
					sprintf(s,"D%d",SR(_opcode,9)&7);
				break;
				case 3:
				case 7:
					sprintf(s,"A%d",SR(_opcode,9)&7);
				break;

			}
			__68S(dipc,sz,_opcode,\x2c%s,"SUB",s);
		}
		break;
		case 0xb:{
			switch(SR(_opcode,6)&7){
				case 0:
				case 1:
				case 2:{
					u32 a;
					u8 sz;

					SMODES(a,_opcode&0x3f,(sz=SR(_opcode,6)&3),R,ipc);
					__68S(dipc,sz,_opcode&63,\x2c D%d,"CMP",(SR(_opcode,9)&7));
				}
				break;
				case 3:
				case 7:{
					u32 a;
					u8 sz;

					SMODES(a,_opcode&0x3f,(sz=1+(SR(_opcode,8) & 1)),R,ipc);
					__68S(dipc,sz,_opcode&63,\x2c A%d,"CMPA",(SR(_opcode,9)&7));
				}
				break;
				case 4:
				case 5:
				case 6:
					(((CCore *)this)->*_disopcodescb[OPCODE_IDX_(_opcode)])(cc);
				break;
				default:
					_68S(%d,"CMP",SR(_opcode,6)&7);
				break;
			}
		}
		break;
		case 0xc:
			(((CCore *)this)->*_disopcodescb[OPCODE_IDX_(_opcode)])(cc);
		break;
		case 0xd:
			switch(SR(_opcode,6)&7){
				case 0://Dx+=ea
				case 1:
				case 2:{
					u32 a;
					void *p;
					u8 sz,b;

					sz=SR(_opcode,6)&3;
					AEAS(p,a,_opcode&63,sz,R,ipc,SOURCE);
					__68S(dipc,sz,_opcode&63,\x2c D%d,"ADD",(SR(_opcode,9)&7));
				}
				break;
				case 3:
				case 7:{
					u32 a, sz=SR(_opcode&0x100,8)+1;
					SMODES(a,_opcode,sz,R,ipc);
					__68S(dipc,sz,_opcode,\x2c A%d,"ADDA",(SR(_opcode,9)&7));
				}
				break;
				case 4://ea+=Dx
				case 5:
				case 6:
					switch(SR(_opcode,3)&7){
						case 2:
							_68S(D%d\x2c[A%d],"ADD",SR(_opcode,9)&7,_opcode&7);
						break;
						case 5:{
							s16 d;

							_pc+=2;
							RWPC(_pc,d);
							_68S(D%d\x2c[\x23\x24%x\x2c A%d],"ADD",SR(_opcode,9)&7,d,_opcode&7);
						}
						break;
						default:
							_68S(%x,"ADD",SR(_opcode,3)&7);
						break;
					}
				break;
				default:
					_68S(%x,"unk D",SR(_opcode,6)&7);
				break;
			}
		break;
		case 0xe:
		/*	switch(SR(_opcode&0x100,5)|SR(_opcode&0x38,3)){
				case 0x5://lsl
				case 0x1:{
					u8 sz,v;

					sz=SR(_opcode,6)&3;
					v = SR(_opcode,9)&7;
					__68S(ipc,sz,_opcode&7,\x2c #%x,"LSL",v);
				}
				break;
				case 9:
				case 0xd:{//lsr
					u8 sz,v;

					sz=SR(_opcode,6)&3;
					v = SR(_opcode,9)&7;
					__68S(ipc,sz,_opcode&7,\x2c #%x,"LSR",v);
				}
				break;
				default:
					(((CCore *)this)->*_disopcodescb[OPCODE_IDX_(_opcode)])(cc);
					//_68S(%x,"unk E",SR(_opcode&0xe00,7)|SR(_opcode & 0x38,3));
				break;
			}*/
			(((CCore *)this)->*_disopcodescb[OPCODE_IDX_(_opcode)])(cc);
		break;
		default:
			_68S(NOARG,"NOP");
		break;
	}
	sprintf(&c[8]," %8x ",_opcode);
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	_pc += ipc;
	*padr=_pc;
	ipc=oipc;
	_opcode=op;
	_pc=pc;
	return 0;
}

int M68000Cpu::_dumpRegisters(char *p){
	char cc[200];

	sprintf(p,"PC: %08X CCR: %04X Z:%c C:%c V:%c N:%c X:%c IM:%x M:%x S:%x IP:%x %u-%u %u\n\n",_pc,_ccr,
		_ccr&Z_BIT ? 49 : 48,_ccr&C_BIT ? 49 : 48,_ccr&V_BIT ? 49 : 48,_ccr&N_BIT ? 49 : 48,_ccr&X_BIT ? 49 : 48,SR(_ccr,8)&7
		,SR(_ccr,12)&1,SR(_ccr,13)&1,_irq_pending,__line,__frame,__cycles);
	for(int i=0;i<8;i++){
		sprintf(cc,"D%d: %08X ",i,REG_D(i));
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	//strcat(p,"\n");
	for(int i=0;i<8;i++){
		sprintf(cc,"A%d: %08X ",i,REG_A(i));
		strcat(p,cc);
		if((i&3) == 3)
			strcat(p,"\n");
	}
	sprintf(cc,"\nUSP:%08X ISP:%08X MSP:%08X VBB:%08X",REG_USP,REG_ISP,REG_MSP,REG_VBR);
	strcat(p,cc);
	return 0;
}

int M68000Cpu::_dumpMemory(char *p,u8 *mem,LPDEBUGGERDUMPINFO pdi,u32 sz){
	RMAP_(pdi->_dumpAddress,mem,R);
	pdi->_dumpMode|=0x80;
	return CCore::_dumpMemory(p,mem,pdi,sz);
}

int M68000Cpu::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_NEXT_STEP:{
			switch(*((u32 *)pv)){
				case 1:{
					u16 op;

					RWOP(_pc,op);
					printf("qn %x %x %x %x\n",op,SR(op,12),SOURCEM(op),SOURCER(op));
					//EnterDebugMode();
					switch(SR(op,12)){
						case 6://BSR Bcc
							if((u8)op == 0)
								*((u32 *)pv) = 4;
							else
								*((u32 *)pv) = 2;
						break;
						case 4:
							switch(SOURCEM(op)){
								default:
									*((u32 *)pv) = 6;//jsr jmp
								break;
								case 7:
									switch(SOURCER(op)){
										case 1:
											*((u32 *)pv) = 6;//jsr jmp
										break;
										default:
											*((u32 *)pv) = 4;//jsr jmp
										break;
									}
								break;
								case 5:
									*((u32 *)pv) = 4;//jsr jmp
								break;
								case 2:
									*((u32 *)pv) = 2;//jsr jmp
								break;
							}
						break;
					}
				}
					return 0;
				default:
					*((u32 *)pv)=2;
					return 0;
			}
		}
			return -1;
		case ICORE_QUERY_REGISTER:{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					if(strcmp(c,"CCR") == 0)
						v=_ccr;
					else{
						sscanf(&c[1],"%d",&v);
						if(*c=='A') v+=8;
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
				if(strcmp(c,"CCR") == 0)
					_ccr=p[1];
				else{
					sscanf(&c[1],"%d",&i);
					if(*c=='A') i+=8;
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

int M68000Cpu::_change_sr(u16 v){
	u16 vv;

	vv=_ccr;
	if(_ccr & S_BIT)
		_ccr=v;
	else
		_ccr= (_ccr & 0xff00)|(u8)v;
	if((vv&S_BIT) != (v&S_BIT)){
		if(vv&S_BIT){
			REG_ISP=REG_SP;
			REG_SP=REG_USP;
		}
		else{
			REG_USP=REG_SP;
			REG_SP=REG_ISP;
		}
	}
	/*if((vv&0x700) != (_ccr&0x700)){
		if(_irq_pending)
			machine->OnEvent(0,(LPVOID)-1);
	}*/
	return 0;
}

int M68000Cpu::_empty_op(){
	EnterDebugMode();
	printf("unk %x %x %x\n",_opcode,SR(_opcode,3),SR(_opcode&0xfff,3));
	return 0;
}

int M68000Cpu::_empty_op_dis(char *cc){
	_68S(%x,"UNK",SR(_opcode,12));
	return 0;
}

int M68000Cpu::_add_reg(){
	u32 a,b,*r;
	void *p;

	u8 sz=SR(_opcode,6) & 3;

	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	b=a;
	r=&REG_D(SR(_opcode,9)&7);
	switch(sz){
		case 0:
			STATUSFLAGS("addb %0,%%cl\n",a,*r,b);//fixme
		break;
		case 1:
			STATUSFLAGS("addw %0,%%cx\n",a,*r,b);//fixme
		break;
		case 2:
			STATUSFLAGS("addl %0,%%ecx\n",a,*r,b);//fixme
		break;
		default:
			printf("aDD %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
	//STATUSFLAGS("addl %0,%%ecx\n",a,*r,b);//fixme
	WMPC(sz,r,a);
	__68F(2,sz,_opcode,\x2c D%d,"ADD",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_add_reg_dis(char *cc){
	u32 a;
	void *p;
	u8 sz,b;

	sz=SR(_opcode,6)&3;
	AEAS(p,a,_opcode&63,sz,R,ipc,SOURCE);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"ADD",(SR(_opcode,9)&7));
	return 0;
}

int M68000Cpu::_add_mem(){
	u32 a,b,*r;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	switch(sz){
		case 0:
			b=(u32)(u8)REG_D(SR(_opcode,9)&7);
			r=(u32 *)p;
			STATUSFLAGS("addb %0,%%cl\n",a,a,b);//fixme
			//fixme *r only 16bits but use all bits
		break;
		case 1:
			b=(u32)(u16)REG_D(SR(_opcode,9)&7);
			r=(u32 *)p;
			STATUSFLAGS("addw %0,%%cx\n",a,a,b);//fixme
			//fixme *r only 16bits but use all bits
		break;
		case 2:
			b=(u32)REG_D(SR(_opcode,9)&7);
			r=(u32 *)p;
			//*r=a;//a is swapped fixme!!!
			STATUSFLAGS("addl %0,%%ecx\n",a,a,b);//fixme
		break;
		default:
			printf("aDD %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
	//STATUSFLAGS("addl %0,%%ecx\n",a,*r,b);//fixme
	WMP(sz,__address,r,a,_opcode);
	__68F(2,sz,_opcode,\x2c D%d,"ADD",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_add_mem_dis(char *cc){
	switch(SR(_opcode,3)&7){
		case 2:
			_68S(D%d\x2c[A%d],"ADD",SR(_opcode,9)&7,_opcode&7);
		break;
		case 5:{
			s16 d;

			_pc+=2;
			RWPC(_pc,d);
			_68S(D%d\x2c[\x23\x24%x\x2c A%d],"ADD",SR(_opcode,9)&7,d,_opcode&7);
		}
		break;
		default:
			_68S(%x,"ADD",SR(_opcode,3)&7);
		break;
	}
	return 0;
}

int M68000Cpu::_addx_reg(){
	u32 a,b,*r;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	switch(sz){
		case 0:
			b=(u8)a + (_ccr & X_BIT ? 1 :0);
			r=&REG_D(SR(_opcode,9)&7);
			STATUSFLAGS("addb %0,%%cl\n",a,*r,b);//fixme
		break;
		case 1:
			b=(u16)a + (_ccr & X_BIT ? 1 :0);;
			r=&REG_D(SR(_opcode,9)&7);
			STATUSFLAGS("addw %0,%%cx\n",a,*r,b);//fixme
		break;
		case 2:
			b=a + (_ccr & X_BIT ? 1 :0);;
			r=&REG_D(SR(_opcode,9)&7);
			STATUSFLAGS("addl %0,%%ecx\n",a,*r,b);//fixme
		break;
		default:
			printf("aDD %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
	//STATUSFLAGS("addl %0,%%ecx\n",a,*r,b);//fixme
	WMP(sz,__address,r,a,_opcode);
	__68F(2,sz,_opcode,\x2c D%d,"ADDX",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_addx_reg_dis(char *cc){
	u32 a;
	void *p;
	u8 sz;

	sz=SR(_opcode,6)&3;
	AEAS(p,a,_opcode&63,sz,R,ipc,SOURCE);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"ADDX",(SR(_opcode,9)&7));
	return 0;
}

int M68000Cpu::_addx_mem(){
	EnterDebugMode();
	return 1;
}

int M68000Cpu::_addx_mem_dis(char *cc){
	return 0;
}

int M68000Cpu::_adda_reg(){
	u32 a,sz=SR(_opcode&0x100,8)+1;
	SMODE(a,_opcode,sz,R,ipc);
	switch(sz){
		case 1:
			REG_A(SR(_opcode,9)&7) += (s16)a;//fe6788
		break;
		case 2:
			REG_A(SR(_opcode,9)&7) += a;//fe6788
		break;
	}
	__68F(2,sz,_opcode,\x2c A%d,"ADDA",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_adda_reg_dis(char *cc){
	u32 a,sz;

	sz=SR(_opcode&0x100,8)+1;
	SMODES(a,_opcode,sz,R,ipc);
	__68S(dipc,sz,_opcode,\x2c A%d,"ADDA",(SR(_opcode,9)&7));
	return 0;
}

int M68000Cpu::_eoriccr(){//147 a38 eori to ccr
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68F(#%x\x2c CCR,"EOR",d);
	_ccr ^= (u8)d;
	return 1;
}

int M68000Cpu::_eoriccr_dis(char *cc){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68S(#%x\x2c CCR,"EOR",d);
	return 0;
}

int M68000Cpu::_andisr(){//278 andi to sr
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68F(#%x\x2c SR,"AND",d);
	if(!(_ccr&S_BIT)){
		printf("ansr\n");
		d=(u8)_ccr | S_BIT;
		_change_sr(d);
		OnException(8,0);
		_ccr |= S_BIT;
	}
	else{
		_change_sr(_ccr&d);
		_setStatus(S_IRQ_CHECK);
	}
	return 1;
}

int M68000Cpu::_andisr_dis(char *cc){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68S(#%x\x2c SR,"AND",d);
	return 0;
}

int M68000Cpu::_andiccr(){//147 a38 eori to ccr
	u16 d;

	RWCPC(_pc+2,d);
	ipc+=2;
	_68F(#%x\x2c CCR,"ANDI",d);
	_ccr = (_ccr & 0xff00)|(_ccr & (u8)d);
	return 1;
}

int M68000Cpu::_andiccr_dis(char *cc){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68S(#%x\x2c CCR,"ANDI",d);
	return 0;
}

int M68000Cpu::_orisr(){//78 ori to sr
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68F(#%x\x2c SR,"OR",d);
	if(!(_ccr&S_BIT)){
		d=(u8)_ccr | S_BIT;
		_change_sr(d);
		//_pc += 2;
		//_pc+=ipc;
		OnException(8,0);
		_ccr |= S_BIT;
	}
	else{
		d|=_ccr;
		_change_sr(d);
		_setStatus(S_IRQ_CHECK);
	}
	return 1;
}

int M68000Cpu::_orisr_dis(char *cc){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68S(#%x\x2c SR,"OR",d);
	return 0;
}

int M68000Cpu::_oriccr(){//147 a38 eori to ccr
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68F(#%x\x2c CCR,"ORI",d);
	_ccr |= (u8)d;
	return 1;
}

int M68000Cpu::_oriccr_dis(char *cc){
	u16 d;

	RWPC(_pc+2,d);
	ipc+=2;
	_68S(#%x\x2c CCR,"ORI",d);
	return 0;
}

int M68000Cpu::_cmpm(){
	u32 a,b,sz;

	sz=SR(_opcode,6)&3;
	a=REG_A(_opcode&7);
	REG_A(_opcode&7) += BV(sz);
	b=REG_A(SR(_opcode,9)&7);
	REG_A(SR(_opcode,9)&7) += BV(sz);
	switch(sz){
		case 0:
			RB(a,a);
			a=(u32)(s8)a;
			RB(b,b);
			b=(u32)(s8)b;
			//EnterDebugMode();
		break;
		case 1:
			RW(a,a);
			a=(u32)(s16)a;
			RW(b,b);
			b=(u32)(s16)b;
			//EnterDebugMode();
		break;
		case 2:
			RL(a,a);
			RL(b,b);
		break;
		default:
			EnterDebugMode();
		break;
	}
	STATUSFLAGS("subl %0,%%ecx\n",a,b,a);
	_68F(%d [A%d]+\x2c [A%d]+,"CMPM",sz,(SR(_opcode,9)&7),_opcode&7);
	return 1;
}

int M68000Cpu::_cmpm_dis(char *cc){
	_68S(%d [A%d]+\x2c [A%d]+,"CMPM",SR(_opcode,6)&3,SR(_opcode,9)&7,_opcode&7);
	return 1;
}

int M68000Cpu::_and_reg(){
	u32 a,b,*r;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	b=a;
	r=&REG_D(SR(_opcode,9)&7);
	switch(sz){
		case 0:
			STATUSFLAGS("andb %0,%%cl\n",a,*r,b);
		break;
		case 1:
			STATUSFLAGS("andw %0,%%cx\n",a,*r,b);
		break;
		case 2:
			STATUSFLAGS("andl %0,%%ecx\n",a,*r,b);
		break;
		default:
			printf("AND %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	//WMP(sz,__address,r,a,_opcode);
	WMPC(sz,r,a);
	__68F(2,sz,_opcode,\x2c D%d,"AND",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_and_reg_dis(char *cc){
	u32 a,sz;

	sz=SR(_opcode,6) & 3;
	SMODES(a,_opcode & 0x3f,sz,R,ipc);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"AND",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_and_mem(){
	u32 a,b;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	b=REG_D(SR(_opcode,9)&7);
	switch(sz){
		case 0:
			STATUSFLAGS("andb %0,%%cl\n",a,a,b);
		break;
		case 1:
			STATUSFLAGS("andw %0,%%cx\n",a,a,b);
		break;
		case 2:
			STATUSFLAGS("andl %0,%%ecx\n",a,a,b);
		break;
		default:
			printf("AND %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	WMP(sz,__address,p,a,_opcode);
	__68F(2,sz,_opcode,\x2c D%d,"AND",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_and_mem_dis(char *cc){
	u32 a,sz;

	sz=SR(_opcode,6) & 3;
	SMODES(a,_opcode & 0x3f,sz,R,ipc);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"AND",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_andi(){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	switch(sz){
		case 0:
			RWCPC(_pc+2,b);
			b=(u8)b;
		break;
		case 1:
			RWCPC(_pc+2,b);
			b=(u16)b;
		break;
		case 2:
			RLPC(_pc+2,b);
		break;
	}
	//STATUSFLAGS("andl %0,%%ecx\n",a,a,b);
	a &= b;
	WMP(sz,__address,p,a,_opcode);
	a &= SR(0xffffffff,32-SL(BV(sz),3));
	_ccr &= ~(C_BIT|V_BIT|N_BIT|Z_BIT);
	if(!a)
		_ccr |= Z_BIT;
	else if(a & SL(1,SL(8,sz)-1))
		_ccr |= N_BIT;

	__68F(2+((sz+2) & 6),sz,_opcode,\x2c #%x,"ANDI",b);
	return 1;
}

int M68000Cpu::_andi_dis(char *cc){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
		break;
		case 1:
			RWPC(_pc+2,b);
		break;
		case 2:
			RLOP(_pc+2,b);
		break;
	}
	__68S(dipc,sz,_opcode,\x2c #%x,"ANDI",b);
	return 0;
}

int M68000Cpu::_or_reg(){
	u32 a,b,*r;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	b=a;
	r=&REG_D(SR(_opcode,9)&7);
	//_ccr &= ~(C_BIT|V_BIT|Z_BIT|N_BIT);
	switch(sz){
		case 0:
			STATUSFLAGS("orb %0,%%cl\n",a,*r,b);
		break;
		case 1:
			STATUSFLAGS("orw %0,%%cx\n",a,*r,b);
		break;
		case 2:
			STATUSFLAGS("orl %0,%%ecx\n",a,*r,b);
		break;
		default:
			printf("OR %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	//STATUSFLAGS("orl %0,%%ecx\n",a,*r,b);
	//WMP(sz,__address,r,a,_opcode);
	WMPC(sz,r,a);
	__68F(2,sz,_opcode,\x2c D%d [%x %x],"OR",(SR(_opcode,9)&7),a,b);
	return 1;
}

int M68000Cpu::_or_reg_dis(char *cc){
	u32 a,sz;

	sz=SR(_opcode,6) & 3;
	SMODES(a,_opcode & 0x3f,sz,R,ipc);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"OR",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_or_mem(){
	u32 a,b;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	b=REG_D(SR(_opcode,9)&7);
	//_ccr &= ~(C_BIT|V_BIT|Z_BIT|N_BIT);
	switch(sz){
		case 0:
			STATUSFLAGS("orb %0,%%cl\n",a,a,b);
		break;
		case 1:
			STATUSFLAGS("orw %0,%%cx\n",a,a,b);
		break;
		case 2:
			STATUSFLAGS("orl %0,%%ecx\n",a,a,b);
			//if(a || b) EnterDebugMode();
		break;
		default:
			printf("OR %x PC:%x\n",SR(_opcode,6)&7,_pc);
			EnterDebugMode();
		break;
	}
	//STATUSFLAGS("orl %0,%%ecx\n",a,*r,b);
	WMP(sz,__address,p,a,_opcode);
	__68F(2,sz,_opcode,\x2c D%d [%x %x],"OR",(SR(_opcode,9)&7),a,b);
	return 1;
}

int M68000Cpu::_or_mem_dis(char *cc){
	u32 a,sz;

	sz=SR(_opcode,6) & 3;
	SMODES(a,_opcode & 0x3f,sz,R,ipc);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"OR",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_ori(){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
		break;
		case 1:
			RWPC(_pc+2,b);
			b=(u16)b;
		break;
		case 2:
			RLPC(_pc+2,b);
		break;
	}
	//STATUSFLAGS("orl %0,%%ecx\n",a,a,b);
	a|=b;
	b = a & SR(0xffffffff,32-SL(BV(sz),3));
	_ccr &= ~(Z_BIT|N_BIT|V_BIT|C_BIT);
	if(!b)
		_ccr |= Z_BIT;
	else if(b & SL(1,SL(8,sz)-1))
		_ccr |= N_BIT;
	WMP(sz,__address,p,a,_opcode);
	__68F(2+((sz+2) & 6),sz,_opcode,\x2c\x24\x23%x,"ORI",b);
	return 1;
}

int M68000Cpu::_ori_dis(char *cc){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
			dipc+=2;
		break;
		case 1:
			RWPC(_pc+2,b);
			b=(u16)b;
			dipc+=2;
		break;
		case 2:
			RLOP(_pc+2,b);
			dipc+=4;
		break;
	}
	__68S(dipc,sz,_opcode,\x2c\x23%x,"ORI",b);
	return 0;
}

int M68000Cpu::_eor(){
	u32 a,b;
	void *p;

	u8 sz=SR(_opcode,6) & 3;
	AEA(p,a,_opcode,sz,R,ipc,SOURCE, );
	b=REG_D(SR(_opcode,9)&7);

	//printf("eor %x %x %x",sz,a,b);
	switch(sz){
		case 1:
			b = (u16)a ^ (u16)b;
			a&=0xffff0000;
		break;
		case 0:
			b = (u8)a ^ (u8)b;
			a&=0xffffff00;
		break;
		case 2:
			b=a^b;
			a=0;
		break;
	}
	a |= b;
	b &= SR(0xffffffff,32-SL(BV(sz),3));
	_ccr &= ~(Z_BIT|N_BIT|V_BIT|C_BIT);
	if(!b)
		_ccr |= Z_BIT;
	else if(b & SL(1,SL(8,sz)-1))
		_ccr |= N_BIT;
	WMP(sz,__address,p,a,_opcode);
	__68F(2,sz,_opcode,\x2c D%d,"EOR",SR(_opcode,9)&7);
	return 1;
}

int M68000Cpu::_eor_dis(char *cc){
	u32 a,sz;

	sz=SR(_opcode,6) & 3;
	SMODES(a,_opcode & 0x3f,sz,R,ipc);
	__68S(dipc,sz,_opcode&63,\x2c D%d,"EOR",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_eori(){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	_ccr &= ~(Z_BIT|N_BIT|V_BIT|C_BIT);
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	switch(sz){
		case 1:
			RWPC(_pc+2,b);
			b = (u16)a ^ (u16)b;
			a&=0xffff0000;
		break;
		case 0:
			RWPC(_pc+2,b);
			b = (u8)a ^ (u8)b;
			a&=0xffffff00;
		break;
		case 2:
			RLPC(_pc+2,b);
			b=a^b;
			a=0;
		break;
	}
	if(!b)
		_ccr |= Z_BIT;
	else if(b & SL(1,SL(8,sz)-1))
		_ccr |= N_BIT;
	a |= b;
	WMP(sz,__address,p,a,_opcode);
	__68F(2+((sz+2) & 6),sz,_opcode,\x2c #%x,"EORI",b);
	return 1;
}

int M68000Cpu::_eori_dis(char *cc){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
		break;
		case 1:
			RWPC(_pc+2,b);
		break;
		case 2:
			RLOP(_pc+2,b);
		break;
	}
	__68S(dipc,sz,_opcode,\x2c #%x,"EORI",b);
	return 0;
}

int M68000Cpu::_exg(){
	u32 a;

	switch(_opcode&0x88){
		case 8:
			a=REG_A(_opcode&7);
			REG_A(_opcode&7)=REG_A(SR(_opcode,9)&7);
			REG_A(SR(_opcode,9)&7)=a;
			_68F(A%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x0:
			a=REG_D(_opcode&7);
			REG_D(_opcode&7)=REG_D(SR(_opcode,9)&7);
			REG_D(SR(_opcode,9)&7)=a;
			_68F(D%d\x2c D%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x88:
			a = REG_A(_opcode&7);
			REG_A(_opcode&7)=REG_D(SR(_opcode,9)&7);
			REG_D(SR(_opcode,9)&7)=a;
			_68F(D%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
	}
	return 1;
}

int M68000Cpu::_exg_dis(char *cc){
	u32 a;

	switch(_opcode&0x88){
		case 8:
			_68S(A%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x0:
			_68S(D%d\x2c D%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
		case 0x88:
			_68S(D%d\x2c A%d,"EXG",SR(_opcode,9)&7,_opcode&7);
		break;
	}
	return 1;
}

int M68000Cpu::_mulu(){
	u32 a,b;

	SMODE(a,_opcode,1,R,ipc);
	b=(u16)REG_D(SR(_opcode,9)&7)*(u16)a;
	REG_D(SR(_opcode,9)&7)=b;
	_ccr &= ~(N_BIT|Z_BIT|V_BIT|C_BIT);
	if(!b)
		_ccr |= Z_BIT;
	else if(b & 0x80000000)
		_ccr |= N_BIT;
	__68F(2,1,_opcode,\x2c D%d,"MULU",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_mulu_dis(char *cc){
	u32 a;

	SMODES(a,_opcode,1,R,ipc);
	__68S(dipc,1,_opcode,\x2c D%d,"MULU",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_muls(){
	s32 a,b;

	SMODE(a,_opcode,1,R,ipc);
	b=(s16)REG_D(SR(_opcode,9)&7)*(s16)a;
	REG_D(SR(_opcode,9)&7)=b;
	_ccr &= ~(N_BIT|Z_BIT|V_BIT|C_BIT);
	if(!b)
		_ccr |= Z_BIT;
	else if(b&0x80000000)
		_ccr |= N_BIT;
	__68F(2,1,_opcode,\x2c D%d,"MULS",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_muls_dis(char *cc){
	u32 a;

	SMODES(a,_opcode,1,R,ipc);
	__68S(dipc,1,_opcode,\x2c D%d,"MULS",(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_cmpi(){
	u32 a,b;
	u8 sz;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	SMODE(a,_opcode,sz,R,ipc);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u32)(s8)b;
			a=(u32)(s8)a;
		break;
		case 1:
		//	EnterDebugMode();
			RWPC(_pc+2,b);
			b=(u32)(s16)b;
			a=(u32)(s16)a;
		break;
		case 2:
			RLPC(_pc+2,b);
		break;
	}
	//printf("cmpi %x %x %x",a,b,sz);
	STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
	//printf(" %x\n",a);
	__68F(2+((sz+2) & 6),sz,_opcode,\x2c #$%x,"CMPI",b);
	return 1;
}

int M68000Cpu::_cmpi_dis(char *cc){
	u32 a,b;
	u8 sz;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	SMODES(a,_opcode & 0x3f,sz,R,ipc);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u32)(u8)b;
			dipc+=2;
		break;
		case 1:
			RWPC(_pc+2,b);
			dipc+=2;
		break;
		case 2:
			RLOP(_pc+2,b);
			dipc+=4;
		break;
	}
	__68S(dipc,sz,_opcode&0x3f,\x2c #$%x,"CMPI",b);
	return 0;
}

int M68000Cpu::_subi(){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	switch(sz){
		case 0:
			RWCPC(_pc+2,b);
			b=(u32)(s8)b;
			a=(u32)(s8)a;
			STATUSFLAGS("sub %0,%%cl\n",a,a,b);
		break;
		case 1:
			RWCPC(_pc+2,b);
			b=(u32)(s16)b;
			a=(u32)(s16)a;
			STATUSFLAGS("sub %0,%%cx\n",a,a,b);
		break;
		case 2:
			RLPC(_pc+2,b);
			STATUSFLAGS("sub %0,%%ecx\n",a,a,b);
		break;
	}
	//STATUSFLAGS("subl %0,%%ecx\n",a,a,b);
	WMP(sz,__address,p,a,_opcode);
	_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
	__68F(2+((sz+2) & 6),sz,_opcode&0x3f,\x2c\x24\x23%x,"SUBI",b);
	return 1;
}

int M68000Cpu::_subi_dis(char *cc){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEAS(p,a,_opcode,2,R,ipc,SOURCE);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
		break;
		case 1:
			RWPC(_pc+2,b);
		break;
		case 2:
			RLOP(_pc+2,b);
		break;
	}
	__68S(ipc,sz,_opcode,\x2c\x24\x23%x,"SUBI",b);
	return 0;
}

int M68000Cpu::_addi(){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += ((sz+2) & 6);
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	switch(sz){
		case 0:
			RWCPC(_pc+2,b);
			b=(u8)b;
			STATUSFLAGS("add %0,%%cl\n",a,a,b);
		break;
		case 1:
			RWCPC(_pc+2,b);
			STATUSFLAGS("add %0,%%cx\n",a,a,b);
		break;
		case 2:
			RLPC(_pc+2,b);
			STATUSFLAGS("add %0,%%ecx\n",a,a,b);
		break;
	}
	WMP(sz,__address,p,a,_opcode);
	_ccr = (_ccr & ~X_BIT) | SL(_ccr & C_BIT,X_SHIFT);
	__68F(2+((sz+2) & 6),sz,_opcode,\x2c #$%x,"ADDI",b);
	return 1;
}

int M68000Cpu::_addi_dis(char *cc){
	u32 a,b;
	u8 sz;
	void *p;

	sz=SR(_opcode,6) & 3;
	ipc += (sz+2) & 6;
	AEAS(p,a,_opcode,2,R,ipc,SOURCE);
	switch(sz){
		case 0:
			RWPC(_pc+2,b);
			b=(u8)b;
		break;
		case 1:
			RWPC(_pc+2,b);
		break;
		case 2:
			RLOP(_pc+2,b);
		break;
	}
	__68S(dipc,sz,_opcode,\x2c #$%x,"ADDI",b);
	return 0;
}

int M68000Cpu::_bchg(){
	BIT_INS(BCHG);
	return 1;
}

int M68000Cpu::_bchg_dis(char *cc){
	void *p;
	u32 a,sz;

	//fixme
	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(ipc,sz,_opcode,\x2c D%d,"BCHG",SR(_opcode,9)&7);
	return 0;
}

int M68000Cpu::_bchgi(){
	BIT_INSI(BCHG);
	return 1;
}

int M68000Cpu::_bchgi_dis(char *cc){
	void *p;
	u32 a,b,sz;

	//fixme
	RWPC(_pc+ipc,b);
	ipc+=2;
	dipc+=2;
	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(dipc,sz,_opcode,\x2c#%x,"BCHG",b);
	return 0;
}

int M68000Cpu::_bset(){
	BIT_INS(BSET);
	return 1;
}

int M68000Cpu::_bset_dis(char *cc){
	void *p;
	u32 a,sz;

	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(ipc,sz,_opcode,\x2c D%d,"BSET",SR(_opcode,9)&7);
	return 0;
}

int M68000Cpu::_bseti(){
	BIT_INSI(BSET);
	return 1;
}

int M68000Cpu::_bseti_dis(char *cc){
	void *p;
	u32 a,b,sz;

	//fixme
	RWPC(_pc+ipc,b);
	ipc+=2;
	dipc+=2;
	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(dipc,sz,_opcode,\x2c#%x,"BSET",b);
	return 0;
}

int M68000Cpu::_btst(){
	BIT_INS(BTST);
	return 1;
}

int M68000Cpu::_btst_dis(char *cc){
	void *p;
	u32 a,sz;

	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(dipc,sz,_opcode,\x2c D%d,"BTST",SR(_opcode,9)&7);
	return 0;
}

int M68000Cpu::_btsti(){
	BIT_INSI(BTST);
	return 1;
}

int M68000Cpu::_btsti_dis(char *cc){
	void *p;
	u32 a,f,sz;
	u16 d;

	RWPC(_pc+ipc,d);
	ipc+=2;
	dipc +=2;
	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(dipc,sz,_opcode,\x2c #%x,"BTST",d);
	return 0;
}

int M68000Cpu::_bclr(){
	BIT_INS(BCLR);
	return 1;
}

int M68000Cpu::_bclr_dis(char *cc){
	void *p;
	u32 a,sz;

	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(ipc,sz,_opcode,\x2c D%d,"BCLR",SR(_opcode,9)&7);
	return 0;
}

int M68000Cpu::_bclri(){
	BIT_INSI(BCLR);
	return 1;
}

int M68000Cpu::_bclri_dis(char *cc){
	void *p;
	u32 a,b,sz;

	RWPC(_pc+ipc,b);
	ipc+=2;
	dipc +=2;
	sz=SOURCEM(_opcode) > 1 ? 0 : 2;
	AEAS(p,a,_opcode,sz,R,ipc,SOURCE);
	__68S(dipc,sz,_opcode,\x2c #%x,"BCLR",b);
	return 0;
}

int M68000Cpu::_lea(){
	u32 d;

	SMODE(d,_opcode,2, ,ipc);
	REG_A(SR(_opcode,9)&7)=d;
	__68F(2,2,_opcode,\x2c A%d,"LEA",SR(_opcode,9)&7);
	return 1;
}

int M68000Cpu::_movemr(){
	u16 l,sz;
	u32 a,v;
	void *p,*ps;
	int i,n;

	RWCPC(_pc+ipc,l);
	ipc+= 2;
	sz=1+(SR(_opcode,6)&1);
	AEA_(p,ps,a,_opcode,sz,R,ipc,SOURCE, );
	if(SOURCEM(_opcode)==4){
		for(n=0,i=15;i>=0;i--){
			if(!(l & BV(15-i)))
				continue;
		//	RMP(sz,__address,p,v,_opcode);
			RM(sz,__address+n,v);
			REG_D(i)=v;
		//	p = (u8 *)p  - SL(sz,1);
			n += SL(sz,1);
		}
		*(u32 *)ps -= (n-SL(sz,1));
	}
	else{
		for(n=i=0;i<16;i++){
			if(!(l & BV(i)))
				continue;
			RMP(sz,__address,p,v,_opcode);
			REG_D(i)=v;
			p = (u8 *)p  + SL(sz,1);
			n += SL(sz,1);
		}
		if(ps)
			*(u32 *)ps += n-SL(sz,1);
	}
	__68F(4,sz,_opcode,%x,"MOVEM",l);
	return 1;
}

int M68000Cpu::_movemw(){
	u16 l,sz;
	u32 a,v;
	void *p,*ps;
	int i,n;

	RWCPC(_pc+ipc,l);
	ipc+= 2;
	sz=1+(SR(_opcode,6)&1);
	AEA_(p,ps,a,_opcode,sz,R,ipc,SOURCE,);
	if(SOURCEM(_opcode)==4){
		for(i=15,n=0;i>=0;i--){
			if(!(l & BV(15-i)))
				continue;
			v=REG_D(i);
			WMP(sz,__address,p,v,_opcode);
			p = (u8 *)p - SL(sz,1);
			n += SL(sz,1);
		}
		*(u32 *)ps -= (n-SL(sz,1));
	}
	else{
		for(i=n=0;i<16;i++){
			if(!(l & BV(i)))
				continue;
			v=REG_D(i);
			WMP(sz,__address,p,v,_opcode);
			p = (u8 *)p + SL(sz,1);
			n+=SL(sz,1);
		}
		if(ps)
			*(u32 *)ps += n;
	}
	__68F(4,sz,_opcode,%x,"MOVEM",l);
	return 1;
}

int M68000Cpu::_movemw_dis(char *cc){
	return _movemr_dis(cc);
}

int M68000Cpu::_movemr_dis(char *cc){
	u16 l,sz;
	u32 a;
	char s[50];

	RWPC(_pc+ipc,l);
	sz=1+(SR(_opcode,6)&1);
	SMODES(a,_opcode,sz,R,ipc);
	ipc+= 2;
	dipc+=2;
	*((u64 *)s)=0;
	if(SOURCEM(_opcode)==4){
		for(int i=15;i>=0;i--){
			char c[10];

			if(!(l&BV(15-i)))
				continue;
			sprintf(c,"%c%d",i<8 ? 'D':'A',i&7);
			if(*s) strcat(s,",");
			strcat(s,c);
		}
	}
	else{
		for(int i=0;i<16;i++){
			char c[10];

			if(!(l&BV(i)))
				continue;
			sprintf(c,"%c%d",i<8 ? 'D':'A',i&7);
			if(*s) strcat(s,",");
			strcat(s,c);
		}
	}
	__68S(dipc,sz,_opcode,%s,"MOVEM",s);
	return 0;
}

int M68000Cpu::_ext(){
	u8 sz;
	void  *p;
	u32 a,b;

	sz=SR((_opcode & 0x1c0)+0x40,8);
	AEA(p,a,_opcode,sz,R,ipc,SOURCE,);
	_ccr &=  ~(C_BIT|N_BIT|V_BIT|Z_BIT);
	switch(sz){
		case 0:
			a=(a&0xffff0000)|((u16)(s16)(s8)a);//fd678a
			b=0x8000;
		break;
		case 1:
			a=(u32)(s16)a;
		//break;
		case 2:
			b=0x80000000;
		break;
		default:
			goto A;
	}
	WMP(sz+1,__address,p,a,_opcode);
	if(!a)
		_ccr |=Z_BIT;
	else if(a&b)
		_ccr |= N_BIT;
A:
	__68F(2,sz,_opcode,NOARG,"EXT");
	return 1;
}

int M68000Cpu::_ext_dis(char *cc){
	__68S(ipc,SR((_opcode & 0x1c0)+0x40,8),_opcode,NOARG,"EXT");
	return 0;
}

int M68000Cpu::_lsl(){
	void *p;
	u32 a;

	AEA(p,a,_opcode,1,R,ipc,SOURCE,);
	a=lsl_(a,1,1);
	WMP(1,__address,p,a,_opcode);
	__68F(2,1,_opcode,\x2c #$1,"LSL");
	return 1;
}

int M68000Cpu::_lsl_dis(char *cc){
	void *p;
	u32 a;

	__68S(ipc,1,_opcode,\x2c #$1,"LSL");
	return 1;
}

int M68000Cpu::_lsr(){
	void *p;
	u32 a;

	AEA(p,a,_opcode,1,R,ipc,SOURCE,);
	a=lsr_(a,1,1);
	WMP(1,__address,p,a,_opcode);
	__68F(2,1,_opcode,\x2c #$1,"LSR");
	return 1;
}

int M68000Cpu::_lsr_dis(char *cc){
	void *p;
	u32 a;

	__68S(ipc,1,_opcode,\x2c #$1,"LSR");
	return 1;
}

int M68000Cpu::_asl(){
	void *p;
	u32 a;

	AEA(p,a,_opcode,1,R,ipc,SOURCE,);
	a=asl_(a,1,1);
	WMP(1,__address,p,a,_opcode);
	__68F(2,1,_opcode,\x2c #$1,"ASL");
	return 1;
}

int M68000Cpu::_asl_dis(char *cc){
	__68S(ipc,1,_opcode,\x2c #$1,"ASL");
	return 1;
}

int M68000Cpu::_asr(){
	void *p;
	u32 a;

	AEA(p,a,_opcode,1,R,ipc,SOURCE,);
	a=asr_(a,1,1);
	WMP(1,__address,p,a,_opcode);
	__68F(2,1,_opcode,\x2c #$1,"ASR");
	return 1;
}

int M68000Cpu::_asr_dis(char *cc){
	__68S(ipc,1,_opcode,\x2c #$1,"ASR");
	return 1;
}

int M68000Cpu::_rol(){
	printf("%s %x P:%x\n",__FUNCTION__,_opcode,_pc);
	EnterDebugMode();
	return 1;
}

int M68000Cpu::_rol_dis(char *cc){
	return 1;
}

int M68000Cpu::_ror(){
	printf("%s %x P:%x\n",__FUNCTION__,_opcode,_pc);
	EnterDebugMode();
	return 1;
}

int M68000Cpu::_ror_dis(char *cc){
	return 1;
}

int M68000Cpu::_roxl(){
	void *p;
	u32 a;

	AEA(p,a,_opcode,1,R,ipc,SOURCE,);
	a=roxl_(a,1,1);
	WMP(1,__address,p,a,_opcode);
	__68F(2,1,_opcode,\x2c #$1,"ROXL");
	return 1;
}

int M68000Cpu::_roxl_dis(char *cc){
	__68S(ipc,1,_opcode,\x2c #$1,"ROXL");
	return 1;
}

int M68000Cpu::_roxr(){
	void *p;
	u32 a;

	AEA(p,a,_opcode,1,R,ipc,SOURCE,);
	a=roxr_(a,1,1);
	WMP(1,__address,p,a,_opcode);
	__68F(2,1,_opcode,\x2c #$1,"ROXR");
	return 1;
}

int M68000Cpu::_roxr_dis(char *cc){
	__68S(ipc,1,_opcode,\x2c #$1,"ROXR");
	return 1;
}

int M68000Cpu::_lsli(){
	u32 a,b;
	u8 sz,v,vv;

	v=vv=SR(_opcode,9)&7;
	if(_opcode&0x20){
		vv=REG_D(v) & 0x3f;
		if(!vv) goto Z;
	}
	else if(!vv)
		vv=8;
	REG_D(_opcode&7)=lsl_(REG_D(_opcode&7),vv,(sz=SR(_opcode,6)&3));
Z:
	__68F(2,sz,_opcode&7,\x2c %c%x,"LSL",_opcode&0x20?'D':'#',vv);
	return 1;
}

int M68000Cpu::_lsli_dis(char *cc){
	u32 sz;

	sz=SR(_opcode,6)&3;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"LSL",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

u32 M68000Cpu::lsr_(u32 a,u32 v,u32 sz){
	u32 b;

	b = a;
	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	switch(sz&3){
		case 2:
			b = SR(a,v-1);
			if(b&1)
				_ccr |= C_BIT|X_BIT;
			b=SR(b,1);
			a=0;
		break;
		case 1:
			b = SR((u16)a,v-1);
			if(b&1)
				_ccr |= C_BIT|X_BIT;
			b=SR(b,1);
			a &= 0xffff0000;
		break;
		case 0:
			b = SR((u8)a,v-1);
			if(b&1)
				_ccr |= C_BIT|X_BIT;
			b=SR(b,1);
			a &= 0xffffff00;
		break;
	}
	if(!b)
		_ccr |= Z_BIT;
	else if(b & BV(SL(BV(sz),3)-1))
		_ccr |= N_BIT;
	return a|b;
}

u32 M68000Cpu::lsl_(u32 a,u32 v,u32 sz){
	u32 b;

	b=a;
	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	switch(sz&3){
		case 2:
			b = (u32)SL((u32)a,v-1);
			if(b & 0x80000000)
				_ccr |= C_BIT|X_BIT;
			b=(u32)SL(b,1);
			a=0;
		break;
		case 1:
			b = (u16)SL((u16)a,v-1);
			if(b & 0x8000)
				_ccr |= C_BIT|X_BIT;
			b=(u16)SL(b,1);
			a &= 0xffff0000;
		break;
		case 0:
			b = (u8)SL((u8)a,v-1);
			if(b & 0x80)
				_ccr |= C_BIT|X_BIT;
			b=(u8)SL(b,1);
			a &= 0xffffff00;
		break;
		default:
			EnterDebugMode();
		break;
	}
	if(!b)
		_ccr |= Z_BIT;
	else if(b & BV(SL(BV(sz),3)-1))
		_ccr |= N_BIT;
	return a|b;
}

u32 M68000Cpu::asr_(u32 a,u32 v,u32 sz){
	u32 b;

	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	if(!(b = a) || !v) goto Z;
	switch(sz){
		case 1:
			b = (s16)SR((s16)a,v-1);
			if(b & 1)
				_ccr |= C_BIT|X_BIT;
			b=(u32)(u16)SR((s16)b,1);
			a &= 0xffff0000;
		break;
		case 2:
			b = (u32)SR((s32)a,v-1);
			if(b & 1)
				_ccr |= C_BIT|X_BIT;
			b=(u32)SR((s32)b,1);
			a =0;
		break;
		default:
			b = (s8)SR((s8)a,v-1);
			if(b & 1)
				_ccr |= C_BIT|X_BIT;
			b=(u32)(u8)SR((s8)b,1);
			a &= 0xffffff00;
		break;
	}
Z:
	if(!b)
		_ccr |= Z_BIT;
	else if(b & BV(SL(BV(sz),3)-1))
		_ccr |= N_BIT;
	return a|b;
}

u32 M68000Cpu::asl_(u32 a,u32 v,u32 sz){
	u32 b;

	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	if(!(b = a) || !v) goto Z;
	switch(sz&3){
		case 2:
			b = (u32)SL((s32)a,v-1);
			if(b & 0x80000000)
				_ccr |= C_BIT|X_BIT;
			b=(u32)SL(b,1);
			a=0;
		break;
		case 1:
			b = (u32)(u16)SL((s16)a,v-1);
			if(b & 0x8000)
				_ccr |= C_BIT|X_BIT;
			b=(u32)(u16)SL(b,1);
			a &= 0xffff0000;
		break;
		case 0:
			b = (u32)(u8)SL((u8)a,v-1);
			if(b & 0x80)
				_ccr |= C_BIT|X_BIT;
			b=(u32)(u32)(u8)SL(b,1);
			a &= 0xffffff00;
		break;
	}
Z:
	if(!b)
		_ccr |= Z_BIT;
	else if(b & BV(SL(BV(sz),3)-1))
		_ccr |= N_BIT;
	return a|b;
}

u32 M68000Cpu::rol_(u32 a,u32 v,u32 sz){
	u32 b;

	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT);
	if(!(b = a) || !v) goto Z;
	switch(sz&3){
		case 2:
			b = SL(a,v-1);
			if(b & 0x80000000){
				_ccr |= C_BIT;
				//b |= BV(0);
			}
			b=SL(b,1)|SR(a,32-v);
			if(b&0x80000000) _ccr |= N_BIT;
			a=0;
		break;
		case 1:
			b = SL((u16)a,v-1);
			if(b&0x8000){
				_ccr |= C_BIT;
				//b|=BV(0);
			}
			b=((u16)SL((u16)b,1))|((u16)SR((u16)a,16-v));
			if(b&0x8000) _ccr |= N_BIT;
			a &= 0xffff0000;
		break;
		case 0:
			b = SL((u8)a,v-1);
			if(b & 0x80){
				_ccr |= C_BIT;
				//b |= BV(0);
			}
			b=((u8)SR((u8)b,1))|((u8)SL((u8)a,8-v));
			if(b & 0x80) _ccr |= N_BIT;
			a &= 0xffffff00;
		break;
	}
Z:
	if(!b)
		_ccr |= Z_BIT;
	return a|b;
}

u32 M68000Cpu::ror_(u32 a,u32 v,u32 sz){
	u32 b;

	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT);
	if(!(b = a) || !v) goto Z;
	switch(sz&3){
		case 2:
			b = SR(a,v-1);
			if(b & 1){
				_ccr |= C_BIT|N_BIT;
			//	b |= BV(31);
			}
			b=SR(b,1)|SL(a,32-v);
			a=0;
		break;
		case 1:
			b=SR((u16)b,v-1);
			if(b&1){
				_ccr |= C_BIT|N_BIT;
			//	b|=BV(15);
			}
			b=SR((u16)b,1)|((u16)SL((u16)a,16-v));
			a &= 0xffff0000;
		break;
		case 0:
			b = SR((u8)a,v-1);
			if(b&1){
				_ccr |= C_BIT|N_BIT;
			//	b |= BV(7);
			}
			b=SR((u8)b,1)|((u8)SL((u8)a,8-v));
			a &= 0xffffff00;
		break;
	}
Z:
	if(!b)
		_ccr |= Z_BIT;
	return a|b;
}

u32 M68000Cpu::roxr_(u32 a,u32 v,u32 sz){
	u32 b;
	u8 c__;

	b=a;
	c__=_ccr;
	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	switch(sz){
		case 0:
			if(v && a){
				b = ((u8)SL((u8)a,8-v)) | SR((u8)a,v);
				if(b & 0x80)
					_ccr |= C_BIT|X_BIT;
			}
			b = (b & ~0x80)|SL(SR(c__ & X_BIT,X_SHIFT),7);
			a &= 0xFFFFFF00;
		break;
		case 1:
			if(v && a){
				b = ((u16)SL((u16)a,16-v)) | SR((u16)a,v);
				if(b & 0x8000)
					_ccr |= C_BIT|X_BIT;
			}
			b = (b & ~0x8000)|SL(SR(c__ & X_BIT,X_SHIFT),15);
			a &= 0xFFFF0000;
		break;
		case 2:
			if(v && a){
				b = SL(a,32-v) | SR(a,v);
				if(b & 0x80000000)
					_ccr |= C_BIT|X_BIT;
			}
			b = (b & ~0x80000000)|SL(SR(c__ & X_BIT,X_SHIFT),31);
			a = 0;
		break;
		default:
			EnterDebugMode();
		break;
	}
	if(!b)
		_ccr |= Z_BIT;
	else if(b & BV(SL(BV(sz),3)-1))
		_ccr |= N_BIT;
	return a|b;
}

u32 M68000Cpu::roxl_(u32 a,u32 v,u32 sz){
	u32 b;
	u8 c__;

	c__=_ccr;
	_ccr &= ~(N_BIT|V_BIT|Z_BIT|C_BIT|X_BIT);
	b=a;
	//if(!(b = a) || !v) goto Z;
	switch(sz){
		case 0:
			if(v && a){
				b=SL((u8)b,v)|SR((u8)b,8-v);
				if(b&1) _ccr |= C_BIT|X_BIT;
			}
			b=(b&0xfe)|(SR(c__ & X_BIT,X_SHIFT));
			a&=0xFFFFFF00;
		break;
		case 2:
			if(v && a){
				b=SL(b,v)|SR(b,32-v);
				if(b&1) _ccr |= C_BIT|X_BIT;
			}
			b=(b&0xfffffffe)|(SR(c__ & X_BIT,X_SHIFT));
			a=0;
		break;
		case 1:
			if(v && a){
				b=SL((u16)b,v)|SR((u16)b,16-v);
				if(b&1) _ccr |= C_BIT|X_BIT;
			}
			b=(b&0xfffe)|(SR(c__ & X_BIT,X_SHIFT));
			a&=0xffff0000;
		break;
		default:
			EnterDebugMode();
		break;
	}
Z:
	if(!b)
		_ccr |= Z_BIT;
	else if(b & BV(SL(BV(sz),3)-1))
		_ccr |= N_BIT;
	return a|b;
}

int M68000Cpu::_lsri(){
	u8 v,sz,vv;

	v=vv=SR(_opcode,9)&7;
	if(_opcode&0x20){
		v=REG_D(v) & 0x3f;
		if(!v) goto Z;
	}
	else if(!v)
		vv=v=8;
	REG_D(_opcode&7)=lsr_(REG_D(_opcode&7),v,(sz=SR(_opcode,6)&3));
Z:
	__68F(2,sz,_opcode&7,\x2c %c%x,"LSR",_opcode&0x20?'D':'#',vv);
	return 1;
}

int M68000Cpu::_lsri_dis(char *cc){
	u32 sz;

	sz=SR(_opcode,6)&3;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"LSR",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_asli(){
	u8 v,sz;

	v=SR(_opcode,9)&7;
	if(_opcode&0x20)
		v=REG_D(v) & 0x3f;
	else if(!v)
		v=8;
	REG_D(_opcode&7)=asl_(REG_D(_opcode&7),v,(sz=SR(_opcode,6)&3));
	_68F(D%d \x2c %c%x,"ASL",_opcode&7,_opcode&0x20?'D':'#',v);//fixme
	return 1;
}

int M68000Cpu::_asli_dis(char *cc){
	u32 sz;

	sz=SR(_opcode,6)&3;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"ASL",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_asri(){
	u8 v,sz;

	v=SR(_opcode,9)&7;
	if(_opcode&0x20)
		v=REG_D(v) & 0x3f;
	else if(!v)
		v=8;
	REG_D(_opcode&7) = asr_(REG_D(_opcode & 7),v,(sz=SR(_opcode,6)&3));
	__68F(2,sz,_opcode&7,\x2c %c%x,"ASR",_opcode&0x20?'D':'#',v);
	return 1;
}

int M68000Cpu::_asri_dis(char *cc){
	void *p;
	u32 a,sz;

	sz=SR(_opcode,6)&3;
	a=SR(_opcode,9)&7;
	if(!a && !(_opcode&0x20))
		a=8;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"ASR",_opcode&0x20?'D':'#',a);
	return 1;
}

int M68000Cpu::_roli(){
	u8 v,sz,vv;

	v=vv=SR(_opcode,9)&7;
	if(_opcode&0x20){
		v=REG_D(v) & 0x3f;
		if(!v) goto Z;
	}
	else if(!v)
		v=vv=8;
	REG_D(_opcode&7)=rol_(REG_D(_opcode&7),v,(sz=SR(_opcode,6)&3));
Z:
	__68F(2,sz,_opcode&7,\x2c %c%x,"ROL",_opcode&0x20?'D':'#',vv);
	return 1;
}

int M68000Cpu::_roli_dis(char *cc){
	u32 sz;

	sz=SR(_opcode,6)&3;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"ROL",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_rori(){
	u8 v,sz;

	v=SR(_opcode,9)&7;
	if(_opcode&0x20){
		v=REG_D(v) & 0x3f;
		if(!v) goto Z;
	}
	else if(!v)
		v=8;
	REG_D(_opcode&7)=ror_(REG_D(_opcode&7),v,(sz=SR(_opcode,6)&3));
Z:
	__68F(2,sz,_opcode&7,\x2c %c%x,"ROR",_opcode&0x20?'D':'#',v);
	return 1;
}

int M68000Cpu::_rori_dis(char *cc){
	void *p;
	u32 a,sz;

	sz=SR(_opcode,6)&3;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"ROR",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_roxli(){
	u32 a;
	u8 sz,v;

	a = REG_D(_opcode&7);
	v = SR(_opcode,9)&7;
	if(_opcode&0x20)
		v=REG_D(v) & 0x3f;
	else if(!v)
		v=8;
	REG_D(_opcode&7) = roxl_(a,v,(sz=SR(_opcode,6)&3));
	__68F(2,sz,_opcode&7,\x2c %c%x,"ROXL",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_roxli_dis(char *cc){
	void *p;
	u32 a,sz;

	sz=SR(_opcode,6)&3;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"ROXL",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_roxri(){
	u32 a;
	u8 sz,v;

	a = REG_D(_opcode&7);
	v = SR(_opcode,9)&7;
	if(_opcode&0x20)
		v=REG_D(v) & 0x3f;
	else if(!v)
		v=8;
	REG_D(_opcode&7) = roxr_(a,v,(sz=SR(_opcode,6)&3));
	__68F(2,sz,_opcode&7,\x2c %c%x,"ROXR",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_roxri_dis(char *cc){
	void *p;
	u32 a,sz;

	sz=SR(_opcode,6)&3;
	__68S(dipc,sz,_opcode&7,\x2c %c%x,"ROXR",_opcode&0x20?'D':'#',(SR(_opcode,9)&7));
	return 1;
}

int M68000Cpu::_enterIRQ(int n,u32 pc){
	if(n <= (SR(_ccr,8) & 7))
		return 1;
	//_status &= ~S_IRQ_PENDING;
	//printf("zzo %x %x;%x %x\n",pc,_pc,_pc+ipc,n);
	_change_sr(_ccr|S_BIT);
	//_pc+=ipc;
	//ipc=0;
	OnException(24+n,0);
	_ccr = (_ccr & ~0x700)|SL(n,8)|S_BIT;
	EnterDebugMode(DEBUG_BREAK_IRQ);
	return  0;
}

int M68000Cpu::OnException(u32 code,u32 b){
	REG_SP -= 4;
	WLPC(REG_SP,_pc);
	REG_SP -= 2;
	WwPC(REG_SP,_ccr);
	RLPC(code*4,_pc);
	DLOG("CPU Exception %u %x:%x",code,_pc,REG_SP);
	ipc=0;
	_icache.invalidate();
	return 0;
}

void M68000Cpu::__icache::invalidate(u32 f){
	_invalidate=1;
	_remap=f&1 ? 1 : 0;
	p=NULL;
}

void M68000Cpu::__icache::reset(){
	invalidate();
	_idx=0;
	_pc=0;
}

};
