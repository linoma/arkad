#include "c64m.h"
#include "gui.h"

extern GUI gui;

namespace c64{

struct __imagestreamer : public FileStream,vector<c64dev::c64_dir_entry>{
	__imagestreamer() : FileStream(),vector<c64dev::c64_dir_entry>(){
	}
	virtual int Add(char *c=NULL)=0;
//protected:
	u8 _header[20];
	u32 _size,_pos;
};

struct __t64reader : __imagestreamer{
	__t64reader() : __imagestreamer(){
	}

	virtual int Read(void *p,u32 sz,u32 *pr){
		if(_pos  < 2){
			*(u8 *)p = _adr[_pos++];
			return 0;
		}
		if(_pos >=_size + 2)
			return -3;
		_pos++;
		return FileStream::Read(p,sz,pr);
	}

	virtual int Add(char *c=NULL){
		c64dev::c64_dir_entry ce;
		int res;
		u8 buf[32],*b,*buf2;
		u32 max,num_files;

		res=-1;
		if(Open(c)) goto Z;
		res--;
		if (fread(_header, 1, sizeof(_header), fp) != sizeof(_header))
			goto Z;
		res--;
		if (!(memcmp(_header, "C64S tape file", 14) == 0
			|| memcmp(_header, "C64 tape image", 14) == 0
			|| memcmp(_header, "C64S tape image", 15) == 0))
				goto Z;
		fseek(fp,32,SEEK_SET);
		fread(buf,32,1,fp);

		max = (buf[3] << 8) | buf[2];
		if (max == 0) max = 1;

		//memcpy(dir_title, buf+8, 16);

		if(!(buf2 = new u8[max * 32]))
			goto Z;
		fread(buf2, 32, max, fp);
		num_files = 0;
		for (unsigned i = 0; i < max; ++i) {
			if (buf2[i*32] == 1)
				num_files++;
		}
		if (!num_files) {
			delete[] buf2;
			goto Z;
		}

		b = buf2;
		for (unsigned i = 0; i < max; ++i, b += 32) {
			if (b[0] == 1) {
				u32 size,offset;
				char *p,name_buf[17];

				memcpy(name_buf, b + 16, 16);
				name_buf[16] = 0x20;
				p = name_buf + 16;
				while (*p-- == 0x20);
				p[2] = 0;

				size = ((b[5] << 8) | b[4]) - ((b[3] << 8) | b[2]);
				offset = (b[11] << 24) | (b[10] << 16) | (b[9] << 8) | b[8];
				push_back({offset,size,name_buf,b[2],b[3]});

				//printf("%u %u %x %x %s\n",offset,size,b[2],b[3],name_buf);
			}
		}
		delete[] buf2;
		ce = front();
		fseek(fp,ce.offset,SEEK_SET);
		_pos=0;
		_lo=ce.sa_lo;
		_hi=ce.sa_hi;
		_size=ce.size;
	//	printf("reading %u %u %x %x\n",ce.offset,ce.size,ce.sa_lo,ce.sa_hi);
		res=0;
	Z:
		return res;
	}

	virtual int Query(u32 what,void *pv){
		switch(what){
			default:
				return FileStream::Query(what,pv);
		}
	}
	union{
		u8 _adr[2];
		struct{
			u8 _lo,_hi;
		};
	};
} _t64reader;

struct __tape : public ICpuTimerObj,public __imagestreamer{
	__tape();
	int reset();
	virtual int Init(int,void *,void *,u32);
	virtual int Query(u32 what,void *pv){return FileStream::Query(what,pv);}
	int write(u32,u8);
	int read(u32,u8 *);
	int update(int);
	virtual int Run(u8 *,int cyc,void *obj);
	int Add(char *c=NULL);
	int motor(int);
	union{
		struct{
			unsigned int _play:1;
			unsigned int _record:1;
			unsigned int _motor:1;
		};
		u32 _status;
	};
//private:
	u32 _pulse,_freq,_cycles;
	IDevice *_cia1;
	u8 _tap_version;
} _tape;

__imagestreamer *_streamer=NULL;

#define MI_BASIC 		0x10000
#define MI_CHAR			0x14000
#define MI_KERNEL 		0x16000
#define MI_IO			0x12000
#define MI_COLOR		0x18000
#define MI_PLA			(MI_COLOR+0x2000)

#define RMAP_R(a,b,c)\
	if((a) >= 0xa000 && (a) <= 0xbfff && (m6502::_mem[MI_PLA+4] & 3) == 3) b=&CCore::_mem[MI_BASIC + (a&0x1fff)];\
	else if((a) >= 0xd000 && (a) <= 0xdfff){\
		if((m6502::_mem[MI_PLA+4] & 7) > 4){\
			if((a) >= 0xd800 && (a) <= 0xdbff) b=&CCore::_mem[MI_COLOR + (a&0x3ff)];\
			else b=&CCore::_mem[MI_IO | (a&0xfff)];\
		} else if((m6502::_mem[MI_PLA+4] & 7) >0 && (m6502::_mem[MI_PLA+4] & 7) < 4) b=&CCore::_mem[MI_CHAR + (a&0xfff)];\
		else b=&CCore::_mem[(u16)a];\
	} else if((a) >= 0xe000 && (a) <= 0xffff && (m6502::_mem[MI_PLA+4] & 2)) b=&CCore::_mem[MI_KERNEL + (a&0x1fff)];\
	else b=&CCore::_mem[(u16)a];

#define RMAP_W(a,b,c)\
	if((a) >= 0xd000 && (a) <= 0xdfff){\
		if((m6502::_mem[MI_PLA+4] & 7) > 4) {\
			if((a) >= 0xd800 && (a) <= 0xdbff) b=&CCore::_mem[MI_COLOR + (a&0x3ff)];\
			else b=&CCore::_mem[MI_IO | (a & 0xfff)];\
		} else b=&CCore::_mem[(u16)a];\
	} else b=&CCore::_mem[(u16)a];

#define RMAP_PC(a,b,c)  RMAP_R(a,b,c);

#define SET_ALU_FLAGS(a,b,ret,ins){\
u8 v__,z__,s__,c__=REG_P&C_BIT;\
asm volatile ("movzb %0,%%ecx\n" : : "m" (a)  : "ecx","memory","cc");\
asm volatile (ins "setzb %2\n" "setob %3\n" "setsb %4\n" "movb %%cl,%0\n" \
 : "=m" (ret) ,"=m"(c__),"=m"(z__),"=m"(v__),"=m"(s__) : "m"(b) : "ecx","eax","edx","memory","cc");\
if(c__) REG_P |= C_BIT; else REG_P &= ~C_BIT;\
if(z__) REG_P |= Z_BIT; else REG_P &= ~Z_BIT;\
if(s__) REG_P |= N_BIT; else REG_P &= ~N_BIT;

#define SET_SBC_FLAGS(a,b,ret) {\
	SET_ALU_FLAGS(a,b,ret,"movzbl %1,%%edx\n xorl $1,%%edx\n btw $0,%%dx\n sbb %5,%%cl\n setncb %1\n" )\
	if(v__) REG_P |= V_BIT; else REG_P &= ~V_BIT;}}

#define SET_ADC_FLAGS(a,b,ret) {\
	SET_ALU_FLAGS(a,b,ret,"btw $0,%1\n adc %5,%%cl\n setcb %1\n" )\
	if(v__) REG_P |= V_BIT; else REG_P &= ~V_BIT;}}

#define SET_CMP_FLAGS(a,b) {u8 ret__;SET_ALU_FLAGS(a,b,ret__,"sub %5,%%cl\n setncb %1\n" )}}

#define STATUSZN(a){ u8 a__=(a);REG_P &= ~(N_BIT|Z_BIT);if(!a__) REG_P |= Z_BIT; else if(a__&0x80) REG_P |=N_BIT; }

#define DO_SBC(a,b,ret) if(REG_P & D_BIT){\
	u16 al,ah,tmp = a - (b) - ((REG_P & C_BIT) ? 0 : 1);\
	al = (a & 0x0f) - ((b) & 0x0f) - ((REG_P & C_BIT) ? 0 : 1);\
	ah = (a >> 4) - ((b) >> 4);\
	if (al & 0x10) {al -= 6;ah--;}\
	if (ah & 0x10) ah -= 6;\
	REG_P &= ~(C_BIT|N_BIT|Z_BIT|V_BIT);\
	if(tmp < 0x100) REG_P |= C_BIT;\
	if(((a ^ tmp) & 0x80) && ((a ^ (b)) & 0x80)) REG_P |= V_BIT;\
	if(!tmp) REG_P |= Z_BIT; else if(tmp & 0x80) REG_P |= N_BIT;\
	ret = (ah << 4) | (al & 0x0f);\
	} else {SET_SBC_FLAGS(a,b,ret);}

#define DO_ADC(a,b,ret) if(REG_P & D_BIT){\
	u16 al, ah;	al = (a & 0x0f) + ((b) & 0x0f) + (REG_P & C_BIT);\
	if (al > 9) al += 6;\
	ah = (a >> 4) + ((b) >> 4);\
	if (al > 0x0f) ah++;\
	REG_P &= ~(C_BIT|N_BIT|Z_BIT|V_BIT);\
	if(! (a + (b) + (REG_P & C_BIT)) ) REG_P |= N_BIT;\
	else if((ah << 4) & 0x80) REG_P |= N_BIT;\
	if((((ah << 4) ^ a) & 0x80) && !((a ^ (b)) & 0x80)) REG_P |= V_BIT;\
	if (ah > 9) ah += 6;\
	if(ah > 0x0f) REG_P |= C_BIT;\
	ret = (ah << 4) | (al & 0x0f);\
	} else {SET_ADC_FLAGS(a,b,ret);}

#define __C64F(a,...) if((gui._getStatus() & (S_PAUSE|S_DEBUG_NEXT)) == (S_PAUSE|S_DEBUG_NEXT)) __F(a, ## __VA_ARGS__)

static u8 mo[80];

c64m::c64m() : Machine(MB(15)),m6502(),VICII((ICpuTimerObj *)this),SID(),c64dev(){
	CCore::_freq=MHZ(14.318181)/14;
	Machine::_mouse=(int *)mo;
}

c64m::~c64m(){
}

int c64m::Load(IGame *,char *p){
	FILE *fp;
	int res;

	Reset();

	if(!(fp=fopen("roms/c64/basic.c64","rb")))
		return -1;
	fread(&CCore::_mem[MI_BASIC],KB(8),1,fp);
	fclose(fp);
	if(!(fp=fopen("roms/c64/chargen.c64","rb")))
		return -3;
	fread(&CCore::_mem[MI_CHAR],KB(8),1,fp);
	fclose(fp);
	if(!(fp=fopen("roms/c64/kernal.c64","rb")))
		return -2;
	fread(&CCore::_mem[MI_KERNEL],KB(8),1,fp);
	fclose(fp);
	res=-5;
	RW(0xfffc,_pc);
	if(_tape.Add(p)){
		res--;
		if(!_t64reader.Add(p)){
			res=0;
			_streamer=&_t64reader;
		}
		else printf("An error loading input file!!!\n");
	}
	else res=0;

	Query(ICORE_QUERY_SET_FILENAME,p);
//e430
	//printf("load %x %x\n",_pc,*((u16 *)&CCore::_mem[0xfffc]));
	if(!res){
		CCore::_mem[MI_BASIC+0x560]=0xf2;
		CCore::_mem[MI_BASIC+0x561]=0x10;
		if(_streamer){
			CCore::_mem[MI_KERNEL+0xd40]=0xf2;
			CCore::_mem[MI_KERNEL+0xd41]=0x0;//out
			CCore::_mem[MI_KERNEL+0xd23]=0xf2;
			CCore::_mem[MI_KERNEL+0xd24]=0x1;//outatn
			CCore::_mem[MI_KERNEL+0xd36]=0xf2;
			CCore::_mem[MI_KERNEL+0xd37]=0x2;//outsec
			CCore::_mem[MI_KERNEL+0xe13]=0xf2;
			CCore::_mem[MI_KERNEL+0xe14]=0x3;//In
			CCore::_mem[MI_KERNEL+0xdef]=0xf2;
			CCore::_mem[MI_KERNEL+0xdf0]=0x4;//setatn
			CCore::_mem[MI_KERNEL+0xdbe]=0xf2;
			CCore::_mem[MI_KERNEL+0xdbf]=0x5;//relatn
			CCore::_mem[MI_KERNEL+0xdcc]=0xf2;
			CCore::_mem[MI_KERNEL+0xdcd]=0x6;//turnaround
			CCore::_mem[MI_KERNEL+0xe03]=0xf2;
			CCore::_mem[MI_KERNEL+0xe04]=0x7;//Release
		}
	}
	return res;
}

int c64m::Destroy(){
	m6502::Destroy();
	VICII::GPU::Destroy();
	SID::Destroy();
	return Machine::Destroy();
}

int c64m::Reset(){
	Machine::Reset();
	c64dev::Reset();
	VICII::Reset();
	SID::Reset();
	_tape.reset();
	DelTimerObj(&_tape);
	//m6502::_ioreg[0xd00]=3;
	//CCore::_mem[0]=0x2f;
	memset(&CCore::_mem[MI_IO+0xe00],0xff,0x200);
	return m6502::Reset();
}

int c64m::Init(){
	if(Machine::Init())
		return -1;
	if(m6502::Init(&_memory[MB(5)],0x20))
		return -2;
	m6502::_ioreg=&m6502::_mem[MI_IO];
	_ports=&m6502::_mem[MI_PLA];
	if(c64dev::Init(0,m6502::_mem,m6502::_ioreg,CCore::_freq))
		return -1;
	if(VICII::Init(m6502::_mem,m6502::_ioreg,(ICpuTimerObj *)this))
		return -3;
	//_gpu_mem=&m6502::_mem[0];
	_palette=&m6502::_mem[MI_COLOR];
	if(SID::Init(m6502::_mem,m6502::_ioreg,CCore::_freq))
		return -4;
	if(_tape.Init(0,m6502::_mem,m6502::_ioreg,CCore::_freq))
		return -6;
	for(int i=0;i<0x2f;i++){
		SetIO_cb(0xd000|i,(CoreMACallback)&c64m::fn_write_vic,(CoreMACallback)&c64m::fn_read_vic);
		SetIO_cb(0xd400|i,(CoreMACallback)&c64m::fn_write_sid,0);
		SetIO_cb(0xdc00|i,(CoreMACallback)&c64m::fn_write_cia,(CoreMACallback)&c64m::fn_read_cia);
		SetIO_cb(0xdd00|i,(CoreMACallback)&c64m::fn_write_cia,(CoreMACallback)&c64m::fn_read_cia);
	}
	SetIO_cb(1,(CoreMACallback)&c64m::fn_write_io,0);
	SetIO_cb(0xd41b,(CoreMACallback)&c64m::fn_write_sid,(CoreMACallback)&c64m::fn_read_sid);
	SetIO_cb(0xd41c,(CoreMACallback)&c64m::fn_write_sid,(CoreMACallback)&c64m::fn_read_sid);
	//AddTimerObj(this,65);
	AddTimerObj((LPCPUTIMEROBJ)(VICII *)this);
	return 0;
}

int c64m::Run(u8 *,int cyc,void *obj){
	_cia[0].update(cyc);
	_cia[1].update(cyc);
	//_tape.Run(0,cyc,0);
	SID::update(cyc);
	return VICII::Run(0,cyc,obj);
}

int c64m::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case MACHINE_EVENT(0x201):{//
			if(_streamer)
				_write_to_screen("LOAD \"*\",8,1");
			else
				_write_to_screen("LOAD");
			CCore::_mem[MI_BASIC+0x560]=0xa2;
			CCore::_mem[MI_BASIC+0x561]=0x0;
		}
		return 0;
		case MACHINE_EVENT(0x101):{//In
				u32 r;

				va_start(arg, ev);
				u8 *p=va_arg(arg,u8 *);
				va_end(arg);
				if(_streamer->Read(p,1,&r))
					return 0x40;//eof
				//return 0x40;
			}
			return 0;
		case MACHINE_EVENT(0x102):{//outsec
				va_start(arg, ev);
				u8 *p=va_arg(arg,u8 *);
				//_streamer->Read(p,1);
				*p=0;
				va_end(arg);
			}
		case MACHINE_EVENT(0x103):
		case MACHINE_EVENT(0x104):
			return 0;
		case ME_ENDFRAME:
			_keyboard.update(0);
			_joy[0].update(0);
			SID::Update();
			VICII::Update(!OnFrame());
			return 0;
		case ME_MOUSEBUTTONDOWN:
		case ME_MOUSEMOVE:
		case ME_MOUSEBUTTONUP:{
			va_start(arg, ev);
			Machine::OnEventI(ev,arg);
			_joy[0].push_back({4,_mouse[0],_mouse[1],_mouse[2],_mouse[3]});
			va_end(arg);
			return 0;
		}
		case ME_KEYUP:{
			va_start(arg, ev);
			Machine::OnEventI(ev,arg);
			int w=va_arg(arg,int);
			int v=va_arg(arg,int);
			_keyboard.push_back({3,1,v,w});
			va_end(arg);
			return 0;
		}
		return 0;
		case ME_KEYDOWN:{
			va_start(arg, ev);
			Machine::OnEventI(ev,arg);
			int w=va_arg(arg,int);
			int v=va_arg(arg,int);
			_keyboard.push_back({3,0,v,w});
			va_end(arg);
			return 0;
		}
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
			VICII::Update();
			Draw();
			CALLEE(Machine::OnEventI,ev,return,arg);
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
				case 1:
				break;
				default:{
					u32 n;

					if(!(n=_irq_pending))
						return 0;
					for(ev=7;n;ev--){
						if(!(BV(ev) & _irq_pending))
							continue;
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
				switch(i){
					case 1:
						BS(_irq_pending,BV(ev));
						return 1;
					case 2:
						BC(_irq_pending,BV(ev));
						return 0;
				}
			}
		break;
	}
	BVC(_irq_pending,ev);
	//BVS(_irq_pending,ev);
	switch(ev){
		case 3://cia 1 nmi
			OnException(3,0);
			RW(0xfffa,_pc);
	//REG_P |= I_BIT;
	//EnterDebugMode();
		break;
		case 1://vic
			//EnterDebugMode();
		case 2:
			if(_enterIRQ(ev,0))
				BVS(_irq_pending,ev);
			//_enterIRQ(ev,0);
		break;
	}
	return 0;
}

int c64m::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_DBG_PALETTE:{
			u32 *data,*p= (u32 *)calloc(sizeof(u32),0x50+0x10+0x10);
			data=p;
			*((void **)pv)=data;
			*data++=0x10;
			*data++=0x10;
			extern u32 PALETTE_MOS[];
			for(int i=0;i<0x10;i++){
				u32 c = PALETTE_MOS[i];
				p[i+0x10]=c;
			}
			return 0;
		}
		return -1;
		case ICORE_QUERY_EMU_MENU_SELECT:
			switch(*((u32 *)pv)){
				case 1:
					int i=_tape.write(1,1);
					_ports[3] &= ~0x10;
			//if(!i){
					AddTimerObj(&_tape,65);
					return 0;
			}
			return -1;
		case ICORE_QUERY_EMU_MENU:{
			char *s,*p;

			if(!(s=new char[500])) return -2;
			p=s;
			strcpy(s,"Play");
			p+=5;
			*(u32 *)p=1;
			p+=sizeof(u32);
			strcpy(p,"Record");
			p+=7;
			p+=sizeof(u32);
			strcpy(p,"Stop");
			p+=5;
			p+=sizeof(u32);
			strcpy(p,"New TAP");
			p+=5;
			p+=sizeof(u32);
			*(u32 *)p=0;
			*(char **)pv=s;
			return 0;
		}
		case ICORE_QUERY_IO_PORT:
			switch(*((u32 *)pv)){
				case 0:
					((void **)pv)[0] = m6502::_ioreg;
					return 0;
				case 1:
					((void **)pv)[0] =_cia[0]._regs;
					return 0;
				case 2:
					((void **)pv)[0] = _cia[1]._regs;
					return 0;
			}
			return -1;
		case ICORE_QUERY_IO_DEVICE:
			switch(*((u32 *)pv)){
				case 1:
					((void **)pv)[0] =&_cia[0];
					return 0;
				case 2:
					((void **)pv)[0] = &_cia[1];
					return 0;
			}
			return -1;
		case IMACHINE_QUERY_MEMORY_ACCESS:{
			void *p;
			LPMEMORYACCESS d=(LPMEMORYACCESS)pv;

			p=&CCore::_mem[d->addr];
			d->mem=p;
		}
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
			memset(p,0,sizeof(DEBUGGERPAGE));
			p->size=sizeof(DEBUGGERPAGE);
			strcpy(p->title,"IO");
			strcpy(p->name,"3103");
			p->type=1;
			p->popup=1;

		}
		return 0;
		case ICORE_QUERY_ADDRESS_INFO:{
			LPMEMORYACCESS d =(LPMEMORYACCESS)pv;
			u32 adr=d->addr;

			d->addr=0;
			d->size=KB(128);
		}
		return 0;
		default:
			return m6502::Query(what,pv);
	}
}

int c64m::Exec(u32 status){
	int ret;

	ret=m6502::Exec(status);
	__cycles=m6502::_cycles;
	switch(ret){
		case -1:
		case -2:
			return -ret;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int c64m::Dump(char **pr){
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
	sprintf(cc,"\n\nPLA: %02X %02X %02X\n",_ports[0],_ports[1],_ports[4]);
	strcat(p,cc);

	res += strlen(p)+1;
	p= &c[res];
	strcpy(p,"3102");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;

	*((u64 *)p)=0;
	((CCore *)cpu)->_dumpMemory(p,&CCore::_mem[di._dumpAddress],&di);
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
		_cia[i].flush();
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

	sprintf(cc,"TAPE S:%02x P:%x T:%x %u %u\n",
		_tape._status,_tape._pulse,0xffff-_cia[0]._timers[1]._count,
		_tape._pos,D_CYCLES(_tape._cycles,CCore::_freq,__cycles));
	strcat(p,cc);
	VICII::_dumpRegisters(p);
	SID::_dumpRegisters(p);
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
	*pr = c;
	return res;
}

s32 c64m::fn_write_io(u32 a,void *,void *pdata,u32){
	switch(a){
		default:
			return 1;
		case 0:
			_ports[0]=*(u8 *)pdata;//ddr
		break;
		case 1:
			_ports[1]=*(u8 *)pdata;//pr
			//*(u8 *)pdata = (_ports[1] | ~_ports[0]) & (_ports[2] | _ports[3]);
		break;
	}
	_ports[2]= (_ports[2] & ~_ports[0]) | (_ports[1] & _ports[0]);//pr_out
	_ports[4]= (_ports[1] | ~_ports[0]);//port
	_ports[5]= _ports[4] & (_ports[2] | _ports[3]);
	CCore::_mem[0]=_ports[0];
	CCore::_mem[1]=_ports[5];
	//if((_ports[0] & 0x20)==0) CCore::_mem[1] &= 0xdf;
	_tape.motor(SR(_ports[5] ^ 0x20,5) & 1);
	return 0;
}

s32 c64m::fn_read_io(u32 a,void *,void *pdata,u32){
	return 1;
}

s32 c64m::fn_write_vic(u32 adr,void *,void *pdata,u32){
	if((_ports[4] & 7) > 4)
		return VICII::write(adr,*(u8 *)pdata);
	return 1;
}

s32 c64m::fn_read_vic(u32 a,void *mem,void *pdata,u32){
	if((_ports[4] & 7) > 4)
		return VICII::read(a,(u16 *)pdata);
	return 1;
}

s32 c64m::fn_write_sid(u32 adr,void *,void *pdata,u32){
	if((_ports[4] & 7) > 4)
		return SID::write(adr,*(u8 *)pdata);
	return 1;
}

s32 c64m::fn_read_sid(u32 adr,void *,void *pdata,u32){
	if((_ports[4] & 7) > 4)
		return SID::read(adr,(u8 *)pdata);
	return 1;
}

s32 c64m::fn_write_cia(u32 adr,void *,void *pdata,u32){
	s32 res=1;

	if((_ports[4] & 7) > 4){
		res = _cia[SR(adr,8)&1].write(adr,*(u8 *)pdata);
		VICII::write(0x40,~(_cia[1]._regs[__cia::PRA] | ~_cia[1]._regs[__cia::DDRA]));
	}
	return res;
}

s32 c64m::fn_read_cia(u32 adr,void *,void *pdata,u32){
	if((_ports[4] & 7) > 4){
		_cia[SR(adr,8)&1].read(adr,(u8 *)pdata);
		if((u8)adr > 15) printf("%x %x\n",adr,_ports[4]);
	}
	return 1;
}

int c64m::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	m["keyboard_mode"]="1";
	return 0;
}

void c64m::_write_to_screen(const char * str){
	u16 pnt = CCore::_mem[0xd1] | (CCore::_mem[0xd2] << 8);	// Pointer to current screen line

	while (true) {
		u8 c = tolower(*str++);
		if (c == '\0')
			break;
		if (c == '@')
			c = 0x00;
		else if ((c >= 'a') && (c <= 'z'))
			c ^= 0x60;
		CCore::_mem[pnt++] = c;
	}
}

void c64m::_set_keyboard_buffer(const char * str){
	u16 keyd = 0x277;	// Keyboard buffer

	int i = 0;
	while (true) {
		u8 c = str[i];
		if (c == '\0')
			break;
		CCore::_mem[keyd++] = c;
		++i;
	}
	CCore::_mem[0xc6] = i;	// Number of characters
}

int c64m::SaveState(IStreamer *p){
	if(Machine::SaveState(p))
		return -1;
	if(m6502::SaveState(p))
		return -2;
	return 0;
}

int c64m::LoadState(IStreamer *p){
	if(Machine::LoadState(p))
		return -1;
	/*m6502::_mem=&_memory[MB(5)];
	m6502::_ioreg=&m6502::_mem[0x11000];
	_ports=&m6502::_ioreg[0xff0];
	_gpu_mem=&m6502::_mem[0];
	_palette=&m6502::_mem[0xd800];
	VICII::_remap(m6502::_mem,m6502::_ioreg);
	SID::_remap(m6502::_mem,m6502::_ioreg);
	_cia[0]._remap(m6502::_mem,m6502::_ioreg);
	_cia[1]._remap(m6502::_mem,m6502::_ioreg);*/
	_keyboard.reset();
	if(m6502::LoadState(p))
		return -2;
	return 0;
}

m6502::m6502() : CCore(){
}

m6502::~m6502(){
}

int m6502::SaveState(IStreamer *p){
	if(CCore::SaveState(p))
		return -1;
	p->Write(_regs,10,0);
	return 0;
}

int m6502::LoadState(IStreamer *p){
	if(CCore::LoadState(p))
		return -1;
	p->Read(_regs,10,0);
	return 0;
}

int m6502::Destroy(){
	if(_regs)
		delete []_regs;
	_regs=NULL;
	//return 0;
	return CCore::Destroy();
}

int m6502::Reset(){
	if(_regs) memset(_regs,0,10);
	REG_SP=0x1FF;
	REG_P=0x84;
	_irq_pending=0;
	CCore::Reset();
	return CCore::Restart();
}

int m6502::Init(void *m,u32 ss,u32 f){
	u32 sz;

	_mem=(u8 *)m;
	sz=10*sizeof(u8) + (ss * sizeof(CoreMACallback) * 2)/* +0x4000*sizeof(CoreDecode) + 0x4000*sizeof(CoreDisassebler)*/;
	if(!_regs && !(_regs = new u8[sz]))
		return -1;
	memset(_regs,0,sz);
	_portfnc_write=(CoreMACallback *)&_regs[10];
	_portfnc_read=&_portfnc_write[ss];
	return 0;
}

int m6502::_enterIRQ(int n,u32 pc){
	if((REG_P & I_BIT)) return -1;
	LOGD("IRQ Enter %d %x\n",n,_pc);
	OnException(n,0);
	RW(0xfffe,_pc);
	//REG_P |= I_BIT;
	EnterDebugMode(DEBUG_BREAK_IRQ);
	return 0;
}

int m6502::SetIO_cb(u32 adr,CoreMACallback a,CoreMACallback b){
	if(!ISIO(adr) || !_portfnc_write || !_portfnc_read)
		return -1;
	_portfnc_write[RMAP_IO(adr)]=a;
	_portfnc_read[RMAP_IO(adr)]=b;
	return 0;
}

int m6502::Disassemble(char *dest,u32 *padr){
	u32 op,adr,ipc;
	char c[200],cc[100];
	u8 *mem;

	*((u64 *)c)=0;
	*((u64 *)cc)=0;
	adr = *padr;
	sprintf(c,"%08X ",adr);
	*((u64 *)cc)=0;
	op=_opcode;
	RMAP_R(adr,mem,NOARG);
	_opcode=*mem;
	dipc = 1;
	switch((u8)_opcode){
		case 0x5:
			__S($%x,"ORA",mem[dipc++]);
		break;
		case 0x6:
			__S($%x,"ASL",mem[dipc++]);
		break;
		case 0x8:
			__S(NOARG,"PHP");
		break;
		case 0x9:
			__S(#$%x,"ORA",mem[dipc++]);
		break;
		case 0xa:
			__S(NOARG,"ASL A");
		break;
		case 0xd:
			__S($%x,"ORA",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0xe:
			__S($%x,"ASL",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x10:{
			s8 d=mem[dipc++];
			__S($%04x,"BPL",adr+(s8)d+dipc);
		}
		break;
		case 0x11:
			__S([$%x]\x2cY,"ORA",mem[dipc++]);
		break;
		case 0x15:
			__S($%x\x2cX,"ORA",mem[dipc++]);
		break;
		case 0x16:
			__S($%x\x2cX,"ASL",mem[dipc++]);
		break;
		case 0x18:
			__S(NOARG,"CLC");
		break;
		case 0x19:
			__S($%x\x2cX,"ORA",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x1d:
			__S($%x\x2cY,"ORA",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x1e:
			__S($%x\x2cX,"ASL",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x20:
			__S($%x,"JSR",*((u16 *)&mem[dipc]));
			dipc +=2;
		break;
		case 0x24:
			__S($%x,"BIT",mem[dipc++]);
		break;
		case 0x25:
			__S($%x,"AND",mem[dipc++]);
		break;
		case 0x26:
			__S($%x,"ROL",mem[dipc++]);
		break;
		case 0x28:
			__S(NOARG,"PLP");
		break;
		case 0x29:
			__S(#$%x,"AND",mem[dipc++]);
		break;
		case 0x2a:
			__S(NOARG,"ROL A");
		break;
		case 0x2C:
			__S($%x,"BIT",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x2D:
			__S($%x,"AND",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x2e:
			__S($%x,"ROL",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x30:{
			s8 d=mem[dipc++];
			__S($%04x,"BMI",adr+(s8)d+dipc);
		}
		break;
		case 0x31:
			__S([$%x]\x2cY,"AND",mem[dipc++]);
		break;
		case 0x35:
			__S($%x\x2cX,"AND",mem[dipc++]);
		break;
		case 0x36:
			__S($%x\x2cX,"ROL",mem[dipc++]);
		break;
		case 0x38:
			__S(NOARG,"SEC");
		break;
		case 0x39:
			__S($%x\x2cY,"AND",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x3d:
			__S($%x\x2cX,"AND",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x3e:
			__S($%x\x2cX,"ROL",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0x40:
			__S(NOARG,"RTI");
		break;
		case 0x45:
			__S($%x,"EOR",mem[dipc++]);
		break;
		case 0x46:
			__S($%x,"LSR",mem[dipc++]);
		break;
		case 0x48:
			__S(NOARG,"PHA");
		break;
		case 0x49:
			__S(#$%x,"EOR",mem[dipc++]);
		break;
		case 0x4a:
			__S(NOARG,"LSR A");
		break;
		case 0x4c:
			__S($%x,"JMP",*((u16 *)&mem[dipc]));
			dipc +=2;
		break;
		case 0x4d:
			__S($%x,"EOR",*((u16 *)&mem[dipc]));
			dipc +=2;
		break;
		case 0x4e:
			__S($%x,"LSR",*((u16 *)&mem[dipc]));
			dipc += 2;
		break;
		case 0x50:{
			s8 d=mem[dipc++];
			__S($%04x,"BVC",adr+(s8)d+dipc);
		}
		break;
		case 0x56:
			__S($%x\x2cX,"LSR",mem[dipc++]);
		break;
		case 0x58:
			__S(NOARG,"CLI");
		break;
		case 0x59:
			__S($%x\x2cY,"EOR",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x5D:
			__S($%x\x2cX,"EOR",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x5E:
			__S($%x\x2cX,"LSR",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x60:
			__S(NOARG,"RTS");
		break;
		case 0x65:
			__S($%x,"ADC",mem[dipc++]);
		break;
		case 0x66:
			__S($%x,"ROR",mem[dipc++]);
		break;
		case 0x68:
			__S(NOARG,"PLA");
		break;
		case 0x69:
			__S(#$%x,"ADC",mem[dipc++]);
		break;
		case 0x6a:
			__S(NOARG,"ROR A");
		break;
		case 0x6c:
			__S([$%x],"JMP",*((u16 *)&mem[dipc]));
			dipc +=2;
		break;
		case 0x6d:
			__S($%x,"ADC",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x6e:
			__S($%x,"ROR",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x70:{
			s8 d=mem[dipc++];
			__S($%04x,"BVS",adr+(s8)d+dipc);
		}
		break;
		case 0x71:
			__S([$%x]\x2cY,"ADC",mem[dipc++]);
		break;
		case 0x75:
			__S($%x\x2cX,"ADC",mem[dipc++]);
		break;
		case 0x76:{
			u8 d=mem[dipc++];
			__S($%x\x2cX,"ROR",d);
		}
		break;
		case 0x78:
			__S(NOARG,"SEI");
		break;
		case 0x79:
			__S($%x\x2cY,"ADC",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x7d:
			__S($%x\x2cX,"ADC",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x7e:
			__S($%x\x2cX,"ROR",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0x81:
			__S([$%x\x2cX],"STA",mem[dipc++]);
		break;
		case 0x84:{
			u8 f=mem[dipc++];
			__S($%x,"STY",f);
		}
		break;
		case 0x85:{
			u8 f=mem[dipc++];
			__S($%x,"STA",f);
		}
		break;
		case 0x88:
			__S(NOARG,"DEY");
		break;
		case 0x86:{
			u8 f=mem[dipc++];
			__S($%x,"STX",f);
		}
		break;
		case 0x8a:
			__S(NOARG,"TXA");
		break;
		case 0x8c:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x,"STY",f);
		}
		break;
		case 0x8d:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x,"STA",f);
		}
		break;
		case 0x8e:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x,"STX",f);
		}
		break;
		case 0x90:{
			s8 d=mem[dipc++];
			__S($%04x,"BCC",adr+(s8)d+dipc);
		}
		break;
		case 0x91:{
			u8 f=mem[dipc++];
			__S([$%x]\x2cY,"STA",f);
		}
		break;
		case 0x94:{
			u8 d=mem[dipc++];
			__S($%x\x2cX,"STY",d);
		}
		break;
		case 0x95:{
			u8 d=mem[dipc++];
			__S($%x\x2cX,"STA",d);
		}
		break;
		case 0x98:
			__S(NOARG,"TYA");
		break;
		case 0x99:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc += 2;
			__S($%x\x2cY,"STA",f);
		}
		break;
		case 0x9d:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc += 2;
			__S($%x\x2cX,"STA",f);
		}
		break;
		case 0x9a:
			__S(NOARG,"TXS");
		break;
		case 0xa0:
			__S(#$%x,"LDY",*((u8 *)&mem[dipc++]));
		break;
		case 0xa1:
			__S([$%x\x2cX],"LDA",mem[dipc++]);
		break;
		case 0xa2:
			__S(#$%x,"LDX",*((u8 *)&mem[dipc++]));
		break;
		case 0xa4:
			__S($%x,"LDY",mem[dipc++]);
		break;
		case 0xa5:
			__S($%x,"LDA",mem[dipc++]);
		break;
		case 0xa6:
			__S($%x,"LDX",mem[dipc++]);
		break;
		case 0xa8:
			__S(NOARG,"TAY");
		break;
		case 0xa9:
			__S(#$%x,"LDA",mem[dipc++]);
		break;
		case 0xaa:
			__S(NOARG,"TAX");
		break;
		case 0xac:
			__S($%x,"LDY",*((u16 *)&mem[dipc]));
			dipc+=2;
		break;
		case 0xad:
			__S($%x,"LDA",*((u16 *)&mem[dipc]));
			dipc+=2;
		break;
		case 0xae:
			__S($%x,"LDX",*((u16 *)&mem[dipc]));
			dipc+=2;
		break;
		case 0xb0:{
			s8 d=mem[dipc];
			dipc+=1;
			__S($%04x,"BCS",adr+(s8)d+dipc);
		}
		break;
		case 0xb1:{
			u8 f=mem[dipc++];
			__S([$%x]\x2cY,"LDA",f);
		}
		break;
		case 0xb4:{
			u8 f=mem[dipc++];
			__S($%x\x2cX,"LDY",f);
		}
		break;
		case 0xb5:{
			u8 f=mem[dipc++];
			__S($%x\x2cX,"LDA",f);
		}
		break;
		case 0xb8:
			__S(NOARG,"CLV");
		break;
		case 0xb9:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x\x2cY,"LDA",f);
		}
		break;
		case 0xba:
			__S(NOARG,"TSX");
		break;
		case 0xbc:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x\x2cX,"LDY",f);
		}
		break;
		case 0xbd:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x\x2cX,"LDA",f);
		}
		break;
		case 0xbe:
			__S($%x\x2cY,"LDX",*((u16 *)&mem[dipc]));
			dipc+=2;
		break;
		case 0xc0:
			__S(#$%x,"CPY",mem[dipc++]);
		break;
		case 0xc1:
			__S([$%x\x2cX],"CMP",mem[dipc++]);
		break;
		case 0xc3:
			__S($%x\x2cX,"DCP",mem[dipc++]);
		break;
		case 0xc4:
			__S($%x,"CPY",mem[dipc++]);
		break;
		case 0xc5:
			__S($%x,"CMP",mem[dipc++]);
		break;
		case 0xc6:
			__S($%x,"DEC",mem[dipc++]);
		break;
		case 0xc7:
			__S($%x,"DCP",mem[dipc++]);
		break;
		case 0xc8:
			__S(NOARG,"INY");
		break;
		case 0xc9:
			__S(#$%x,"CMP",mem[dipc++]);
		break;
		case 0xca:
			__S(NOARG,"DEX");
		break;
		case 0xcc:
			__S($%x,"CPY",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0xcd:
			__S($%x,"CMP",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0xce:
			__S($%x,"DEC",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0xd0:{
			s8 d=mem[dipc++];
			__S($%04x,"BNE",adr+(s8)d+dipc);
		}
		break;
		case 0xd1:
			__S([$%x]\x2cY,"CMP",mem[dipc++]);
		break;
		case 0xd5:
		__S($%x\x2cX,"CMP",mem[dipc++]);
		break;
		case 0xd6:
			__S($%x\x2cX,"DEC",mem[dipc++]);
		break;
		case 0xd8:
			__S(NOARG,"CLD");
		break;
		case 0xd9:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x\x2cY,"CMP",f);
		}
		break;
		case 0xdd:{
			u16 f=*((u16 *)&mem[dipc]);
			dipc+=2;
			__S($%x\x2cX,"CMP",f);
		}
		break;
		case 0xde:
			__S($%x\x2cX,"DEC",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0xe0:
			__S(#$%x,"CPX",mem[dipc++]);
		break;
		case 0xe4:
			__S($%x,"CPX",mem[dipc++]);
		break;
		case 0xe5:
			__S($%x,"SBC",mem[dipc++]);
		break;
		case 0xe6:
			__S($%x,"INC",mem[dipc++]);
		break;
		case 0xe8:
			__S(NOARG,"INX");
		break;
		case 0xe9:
			__S(#$%x,"SBC",mem[dipc++]);
		break;
		case 0x1c:
		case 0xfc:
			dipc++;
		case 0x44:
		case 0x80:
			dipc++;
		case 0xea:
			__S(NOARG,"NOP");
		break;
		case 0xec:
			__S($%x,"CPX",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0xed:
			__S($%x,"SBC",*(u16 *)&mem[dipc]);
			dipc += 2;
		break;
		case 0xee:
			__S($%x,"INC",*(u16 *)&mem[dipc]);
			dipc+=2;
		break;
		case 0xF0:{
			s8 d=mem[dipc];
			dipc+=1;
			__S($%04x,"BEQ",adr+(s8)d+dipc);
		}
		break;
		case 0xf1:
			__S([$%x]\x2cY,"SBC",mem[dipc++]);
		break;
		case 0xf2:
			__S(#$%x,"F2",mem[dipc++]);
		break;
		case 0xf5:
			__S($%x\x2cX,"SBC",mem[dipc++]);
		break;
		case 0xf6:
			__S($%x\x2cX,"INC",mem[dipc++]);
		break;
		case 0xf8:
			__S(NOARG,"SED");
		break;
		case 0xf9:
			__S($%x\x2cY,"SBC",*(u16 *)&mem[dipc]);
			dipc +=2;
		break;
		case 0xfd:
			__S($%x\x2cX,"SBC",*(u16 *)&mem[dipc]);
			dipc +=2;
		break;
		case 0xfe:
			__S($%x\x2cX,"INC",*(u16 *)&mem[dipc]);
			dipc +=2;
		break;
		case 0xff:
			__S($%x\x2cX,"ISB",*(u16 *)&mem[dipc]);
			dipc +=2;
		break;
	}
	sprintf(&c[8]," %2x ",_opcode);
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	adr +=dipc;
	*padr=adr;
	_opcode=op;
	return 0;
}

int m6502::_dumpRegisters(char *p){
	char cc[1024];

	sprintf(p,"\\bPC\\n:%04X P:%02X Z:%d C:%d N:%d V:%d I:%d D:%d SP:%04X\n\n",_pc,REG_P,(REG_P&Z_BIT) ?1:0,(REG_P&C_BIT) ?1:0,
		(REG_P&N_BIT) ?1:0,(REG_P&V_BIT) ?1:0,(REG_P&I_BIT) ?1:0,(REG_P&D_BIT) ?1:0,REG_SP);
	sprintf(cc,"A:%02X\tX:%02X\tY:%02X\n",REG_A,REG_X,REG_Y);
	strcat(p,cc);
	sprintf(cc,"\nD:%02X C:%u F:%u L:%u IP:%x",(u32)__data,_cycles,__frame,__line,_irq_pending);
	strcat(p,cc);
	return 0;
}

int m6502::OnException(u32 iq,u32 pc){
	WBPC(REG_SP,SR(_pc,8));
	REG_SP--;
	WBPC(REG_SP,(u8)_pc);
	REG_SP--;
	WBPC(REG_SP,REG_P);
	REG_SP--;
	REG_P |= I_BIT;
	return 0;
}

int m6502::_exec(u32 status){
	u8 *mem;
	int res;

	res=1;
	RMAP_R(_pc,mem,NOARG);
	_opcode=*mem;
	ipc=1;
//printf("%x %x\n",_opcode,_pc);
	switch((u8)_opcode){
		case 0x5:{
			u8 v,f=mem[ipc];
			REG_A |= _mem[f];
			STATUSZN(REG_A);
			ipc++;
			res += 2;
			__C64F($%x,"ORA",f);
		}
		break;
		case 0x6:{
			u8 v,f=mem[ipc++];
			v=_mem[f];
			if(v&0x80) REG_P |= C_BIT; else REG_P &=~C_BIT;
			v = (u8)SL(v,1);
			WB(f,v);//fixme speedup
			STATUSZN(v);
			res+=4;
			__C64F($%x,"ASL",f);
		}
		break;
		case 0x8:
			WBPC(REG_SP,REG_P);
			REG_SP--;
			res += 2;
			__C64F(NOARG,"PHP");
		break;
		case 0x9:{
			u8 f=mem[ipc];
			REG_A |= f;
			STATUSZN(REG_A);
			ipc++;
			res++;
			__C64F(#$%x,"ORA",f);
		}
		break;
		case 0xa:
			if(REG_A&0x80) REG_P |= C_BIT; else REG_P &=~C_BIT;
			REG_A = SL(REG_A,1);
			STATUSZN(REG_A);
			res++;
			__C64F(NOARG,"ASL A");
		break;
		case 0xd:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];
			RB(f,v);
			REG_A |= v;
			STATUSZN(REG_A);
			ipc+=2;
			res += 3;
			__C64F($%x,"ORA",f);
		}
		break;
		case 0xe:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];
			RB(f,v);
			if(v&0x80) REG_P |= C_BIT; else REG_P &=~C_BIT;
			v = (u8)SL(v,1);
			STATUSZN(v);
			WB(f,v);
			ipc+=2;
			__C64F($%x,"ASL",f);
		}
		break;
		case 0x10:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BPL",d+ipc);
			if(!(REG_P & N_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0x11:{
			u8 v=mem[ipc];
			u16 f;

			RW(v,f);
			RB(f+REG_Y,v);
			REG_A |= v;
			STATUSZN(REG_A);
			ipc++;
			res += 4;
			__C64F([$%x]\x2cY,"ORA",f);
		}
		break;
		case 0x15:{
			u8 v,f=mem[ipc];
			v=f+REG_X;
			RB(v,v);
			REG_A |= v;
			STATUSZN(REG_A);
			ipc++;
			res += 3;
			__C64F($%x\x2cX,"ORA",f);
		}
		break;
		case 0x16:{
			u8 v,ff,f=mem[ipc++];
			ff=f+REG_X;
			RB(ff,v);
			if(v&0x80) REG_P |= C_BIT; else REG_P &= ~C_BIT;
			v=SL(v,1);
			STATUSZN(v);
			WB(ff,v);//fixme speedup
			res += 5;
			__C64F($%x\x2cX,"ASL",f);
		}
		break;
		case 0x18:
			REG_P &= ~C_BIT;
			res++;
			__C64F(NOARG,"CLC");
		break;
		case 0x19:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];

			RB(f+REG_Y,v);
			REG_A |= v;
			STATUSZN(REG_A);
			ipc+=2;
			res += 3;
			__C64F($%x\x2cY,"ORA",f);
		}
		break;
		case 0x1d:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];

			RB(f+REG_X,v);
			REG_A |= v;
			STATUSZN(REG_A);
			ipc+=2;
			res += 3;
			__C64F($%x\x2cX,"ORA",f);
		}
		break;
		case 0x1e:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];
			RB(f+REG_X,v);
			if(v&0x80) REG_P |= C_BIT; else REG_P &=~C_BIT;
			v = (u8)SL(v,1);
			STATUSZN(v);
			WB(f+REG_X,v);
			ipc+=2;
			__C64F($%x\x2cX,"ASL",f);
		}
		break;
		case 0x20:{
			u16 opc=_pc+ipc;

			__C64F($%x,"JSR",*((u16 *)&mem[ipc]));
			_pc=*((u16 *)&mem[ipc]);
			opc++;
			STORECALLLSTACK((u32)opc);
			WBPC(REG_SP,SR(opc,8));
			REG_SP--;
			WBPC(REG_SP,(u8)opc);
			REG_SP--;
			ipc = 0;
			res += 5;
		}
		break;
		case 0x24:{
			u8 v,f=mem[ipc];
			v=_mem[f];
			REG_P &= ~(N_BIT|V_BIT|Z_BIT);
			if(v&0x40) REG_P |= V_BIT;
			if(v&0x80) REG_P |= N_BIT;
			v &= REG_A;
			//STATUSZN(v);
			if(v==0) REG_P |= Z_BIT;
			ipc++;
			res += 2;
			__C64F($%x,"BIT",f);
		}
		break;
		case 0x25:{
			u8 v,f=mem[ipc];
			v=_mem[f];
			REG_A &= v;
			STATUSZN(REG_A);
			ipc++;
			res +=2;
			__C64F($%x,"AND",f);
		}
		break;
		case 0x28:
			REG_SP++;
			RB(REG_SP,REG_P);
			res+=3;
			__C64F(NOARG,"PLP");
		break;
		case 0x29:{
			u8 f=mem[ipc];
			REG_A &= f;
			STATUSZN(REG_A);
			ipc++;
			res++;
			__C64F(#$%x,"AND",f);
		}
		break;
		case 0x26:{
			u8 v,f,c =REG_P & C_BIT;
			f=mem[ipc++];
			v=_mem[f];
			if(v&0x80) REG_P |=C_BIT; else REG_P &= ~C_BIT;
			v=SL(v,1)|c;
			WB(f,v);//fixme speedup
			STATUSZN(v);
			__C64F($%x,"ROL",f);
		}
		break;
		case 0x2a:{
			u8 c =REG_P & C_BIT;
			if(REG_A&0x80) REG_P |= C_BIT; else REG_P &= ~C_BIT;
			REG_A=SL(REG_A,1)|c;
			STATUSZN(REG_A);
			res++;
			__C64F(NOARG,"ROL A");
		}
		break;
		case 0x2c:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];

			RB(f,v);
			REG_P &= ~(N_BIT|V_BIT|Z_BIT);
			if(v&0x40) REG_P |= V_BIT;
			if(v&0x80) REG_P |= N_BIT;
			v &= REG_A;
			if(v==0) REG_P |= Z_BIT;
			ipc+=2;
			__C64F($%x,"BIT",f);
		}
		break;
		case 0x2d:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];

			RB(f,v);
			REG_A &= v;
			STATUSZN(REG_A);
			ipc+=2;
			__C64F($%x,"AND",f);
		}
		break;
		case 0x2e:{
			u8 v,c =REG_P & C_BIT;
			u16 f=*(u16 *)&mem[ipc];

			RB(f,v);
			if(v&0x80) REG_P |=C_BIT; else REG_P &= ~C_BIT;
			v=SL(v,1)|c;
			WB(f,v);//fixme speedup
			STATUSZN(v);
			ipc+=2;
			__C64F($%x,"ROL",f);
		}
		break;
		case 0x30:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BMI",d+ipc);
			if((REG_P & N_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0x31:{
			u8 v=mem[ipc];
			u16 f;

			RW(v,f);
			RB(f+REG_Y,v);
			REG_A &= v;
			STATUSZN(REG_A);
			ipc++;
			res += 4;
			__C64F([$%x]\x2cY,"ORA",f);
		}
		break;
		case 0x35:{
			u8 v,ff,f=mem[ipc];

			ff=f+REG_X;
			RB(ff,v);
			REG_A &= v;
			STATUSZN(REG_A);
			ipc++;
			res++;
			__C64F($%x\x2cX,"AND",f);
		}
		break;
		case 0x36:{
			u8 v,f,c =REG_P & C_BIT;
			f=mem[ipc++];
			RB(f,v);
			if(v&0x80) REG_P |=C_BIT; else REG_P &= ~C_BIT;
			v=SL(v,1)|c;
			WB(f,v);//fixme speedup
			STATUSZN(v);
			__C64F($%x\x2cX,"ROL",f);
		}
		break;
		case 0x38:
			REG_P |= C_BIT;
			res++;
			__C64F(NOARG,"SEC");
		break;
		case 0x39:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];

			RB(f+REG_Y,v);
			REG_A &= v;
			STATUSZN(REG_A);
			ipc+=2;
			res+=3;
			__C64F($%x\x2cY,"AND",f);
		}
		break;
		case 0x3d:{
			u8 v;
			u16 f=*(u16 *)&mem[ipc];

			RB(f+REG_X,v);
			REG_A &= v;
			STATUSZN(REG_A);
			ipc+=2;
			res+=3;
			__C64F($%x\x2cX,"AND",f);
		}
		break;
		case 0x3e:{
			u8 v,c =REG_P & C_BIT;
			u16 f=*(u16 *)&mem[ipc];

			RB(f+REG_X,v);
			if(v&0x80) REG_P |=C_BIT; else REG_P &= ~C_BIT;
			v=SL(v,1)|c;
			WB(f+REG_X,v);//fixme speedup
			STATUSZN(v);
			ipc+=2;
			res += 6;
			__C64F($%x\x2c,"ROL",f);
		}
		break;
		case 0x40:{
			u8 a;

			__C64F(NOARG,"RTI");
			REG_SP++;
			RBPC(REG_SP,REG_P);
			REG_SP++;
			RBPC(REG_SP,_pc);
			REG_SP++;
			RBPC(REG_SP,a);
			_pc=SL((u8)a,8)|(u8)_pc;
			ipc=0;
			res += 5;
		}
		break;
		case 0x45:{
			u8 a,v=mem[ipc++];
			RB(v,a);
			REG_A ^= a;
			STATUSZN(REG_A);
			__C64F($%x,"EOR",v);
			res += 2;
		}
		break;
		case 0x46:{
			u8 a,v=mem[ipc++];
			a=_mem[v];
			REG_P &= ~(C_BIT|N_BIT|Z_BIT);
			if(a & 1) REG_P |= C_BIT;
			a = SR(a,1);
			WB(v,a);//fixme speedup
			if(!a) REG_P |= Z_BIT;
			__C64F($%x,"LSR",v);
			res += 4;
		}
		break;
		case 0x48:
			WBPC(REG_SP,REG_A);
			REG_SP--;
			res += 2;
			__C64F(NOARG,"PHA");
		break;
		case 0x49:{
			u8 v=mem[ipc++];
			REG_A ^= v;
			STATUSZN(REG_A);
			__C64F(#$%x,"EOR",v);
			res += 1;
		}
		break;
		case 0x4a:{
			u8 a=REG_A;
			REG_P &= ~(C_BIT|N_BIT|Z_BIT);
			if(a & 1) REG_P |= C_BIT;
			a = SR(a,1);
			REG_A=a;
			if(!a) REG_P |= Z_BIT;
			__C64F(NOARG,"LSR A");
			res++;
		}
		break;
		case 0x4c:{
			__C64F($%x,"JMP",*((u16 *)&mem[ipc]));
			_pc=*((u16 *)&mem[ipc]);
			//ipc+=2;
			ipc = 0;
			res += 2;
		}
		break;
		case 0x4d:{
			u8 a;
			u16 v=*(u16 *)&mem[ipc];

			RB(v,a)
			REG_A ^= a;
			STATUSZN(REG_A);
			ipc += 2;
			__C64F($%x,"EOR",v);
			res += 3;
		}
		break;
		case 0x4e:{
			u8 a;
			u16  v=*(u16 *)&mem[ipc];

			RB(v,a);
			REG_P &= ~(C_BIT|N_BIT|Z_BIT);
			if(a & 1) REG_P |= C_BIT;
			a = SR(a,1);
			WB(v,a);//fixme speedup
			if(!a) REG_P |= Z_BIT;
			__C64F($%x,"LSR",v);
			res += 4;
			ipc+=2;
		}
		break;
		case 0x50:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BVC",d+ipc);
			if(!(REG_P & V_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0x56:{
			u8 a,ff,v=mem[ipc++];

			ff=v+REG_X;
			RB(ff,a);
			REG_P &= ~(C_BIT|N_BIT|Z_BIT);
			if(a & 1) REG_P |= C_BIT;
			a = SR(a,1);
			WB(ff,a);//fixme speedup
			if(!a) REG_P |= Z_BIT;
			__C64F($%x\x2cX,"LSR",v);
			res += 4;
		}
		break;
		case 0x58:
			REG_(REGI_P) &= ~I_BIT;
			__C64F(NOARG,"CLI");
		break;
		case 0x59:{
			u8 a;
			u16 v=*(u16 *)&mem[ipc];

			RB(v+REG_Y,a)
			REG_A ^= a;
			STATUSZN(REG_A);
			ipc += 2;
			__C64F($%x\x2cY,"EOR",v);
			res += 3;
		}
		break;
		case 0x5D:{
			u8 a;
			u16 v=*(u16 *)&mem[ipc];

			RB(v+REG_X,a)
			REG_A ^= a;
			STATUSZN(REG_A);
			ipc += 2;
			__C64F($%x\x2cX,"EOR",v);
			res += 3;
		}
		break;
		case 0x5E:{
			u8 a;
			u16 v=*(u16 *)&mem[ipc];

			RB(v+REG_X,a)
			REG_P &= ~(C_BIT|N_BIT|Z_BIT);
			if(a & 1) REG_P |= C_BIT;
			a = SR(a,1);
			WB(v+REG_X,a);//fixme speedup
			if(!a) REG_P |= Z_BIT;
			ipc += 2;
			__C64F($%x\x2cX,"LSR",v);
			res += 6;
		}
		break;
		case 0x60:{
			u8 v;

			__C64F(NOARG,"RTS");
			REG_SP++;
			RBPC(REG_SP,v);
			REG_SP++;
			RBPC(REG_SP,_pc);
			_pc=SL((u8)_pc,8)|v;
			LOADCALLSTACK(_pc);
			res += 5;
		}
		break;
		case 0x65:{
			u8 a,v = mem[ipc++];
			a=_mem[v];
			DO_ADC(REG_A,a,a);
			REG_A=a;
			res += 2;
			__C64F($%x,"ADC",v);
		}
		break;
		case 0x66:{
			u8 c,v,a=mem[ipc++];
			c=REG_P & C_BIT;
			v=_mem[a];
			if(v&1) REG_P |= C_BIT; else REG_P &= ~C_BIT;
			v=SR(v,1)|SL(c,7);
			WB(a,v);//fixme speedup
			STATUSZN(v);
			res+=5;
			__C64F($%x,"ROR",a);
		}
		break;
		case 0x68:
			REG_SP++;
			RBPC(REG_SP,REG_A);
			STATUSZN(REG_A);
			res += 3;
			__C64F(NOARG,"PLA");
		break;
		case 0x69:{
			u8 a,v = mem[ipc++];
			DO_ADC(REG_A,v,a);
			REG_A=a;
			res++;
			__C64F(#$%x,"ADC",v);
		}
		break;
		case 0x6a:{
			u8 c =REG_P & C_BIT;
			if(REG_A&1) REG_P |=C_BIT; else REG_P &= ~C_BIT;
			REG_A=SR(REG_A,1)|SL(c,7);
			STATUSZN(REG_A);
			res++;
			__C64F(NOARG,"ROR A");
		}
		break;
		case 0x6c:{
			__C64F([$%x],"JMP",*((u16 *)&mem[ipc]));
			_pc=*((u16 *)&mem[ipc]);
			RW(_pc,_pc);
			ipc = 0;
			res += 4;
		}
		break;
		case 0x6d:{
			u8 a;
			u16 v = *(u16 *)&mem[ipc];
			RB(v,a);
			DO_ADC(REG_A,a,a);
			REG_A=a;
			ipc +=2;
			res += 3;
			__C64F($%x,"ADC",v);
		}
		break;
		case 0x6e:{
			u8 c,v;
			u16 a= *(u16 *)&mem[ipc];
			c=REG_P & C_BIT;
			RB(a,v);
			if(v&1) REG_P |= C_BIT; else REG_P &= ~C_BIT;
			v=SR(v,1)|SL(c,7);
			WB(a,v);//fixme speedup
			STATUSZN(v);
			res+=5;
			ipc+=2;
			__C64F($%x,"ROR",a);
		}
		break;
		case 0x70:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BVS",d+ipc);
			if((REG_P & V_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0x71:{
			u8 a,v = mem[ipc++];
			u16 f;

			RW(v,f);
			RB(f+REG_Y,a);
			DO_ADC(REG_A,a,a);
			REG_A=a;
			res += 5;
			__C64F([$%x]\x2cY,"ADC",v);
		}
		break;
		case 0x75:{
			u8 a,ff,v = mem[ipc++];

			ff=v+REG_X;
			RB(ff,a);
			DO_ADC(REG_A,a,a);
			REG_A=a;
			res += 3;
			__C64F($%x\x2cX,"ADC",v);
		}
		break;
		case 0x76:{
			u8 ff,c,v,a=mem[ipc++];
			c=REG_P&C_BIT;
			ff=a+REG_X;
			RB(ff,v);
			if(v&1) REG_P |= C_BIT; else REG_P &= ~C_BIT;
			v=SR(v,1)|SL(c,7);
			WB(ff,v);//fixme speedup
			STATUSZN(v);
			res+=5;
			__C64F($%x\x2cX,"ROR",a);
		}
		break;
		case 0x78:
			REG_(REGI_P) |= I_BIT;
			res++;
			__C64F(NOARG,"SEI");
		break;
		case 0x79:{
			u8 a;
			u16 v = *(u16 *)&mem[ipc];

			RB(v+REG_Y,a);
			DO_ADC(REG_A,a,a);
			REG_A=a;
			res += 4;
			ipc+=2;
			__C64F($%x\x2cY,"ADC",v);
		}
		break;
		case 0x7d:{
			u8 a;
			u16 v = *(u16 *)&mem[ipc];

			RB(v+REG_X,a);
			DO_ADC(REG_A,a,a);
			REG_A=a;
			res += 4;
			ipc+=2;
			__C64F($%x\x2cX,"ADC",v);
		}
		break;
		case 0x7e:{
			u8 c,v;
			u16 a= *(u16 *)&mem[ipc];

			c=REG_P & C_BIT;
			RB(a+REG_X,v);
			if(v&1) REG_P |= C_BIT; else REG_P &= ~C_BIT;
			v=SR(v,1)|SL(c,7);
			WB(a+REG_X,v);//fixme speedup
			STATUSZN(v);
			res+=5;
			ipc+=2;
			__C64F($%x\x2cX,"ROR",a);
		}
		break;
		case 0x81:{
			u8 d=mem[ipc++];
			u16 f;

			RW(d,f);
			WB(f+REG_X,REG_A);
			__C64F([$%x\x2cX],"STA",d);
		}
		break;
		case 0x84:{
			u8 f=mem[ipc];
			WB(f,REG_Y);
			ipc+=1;
			res += 2;
			__C64F($%x,"STY",f);
		}
		break;
		case 0x85:{
			u8 f=mem[ipc++];
			WB(f,REG_A);
			res += 2;
			__C64F($%x,"STA",f);
		}
		break;
		case 0x86:{
			u8 f=mem[ipc++];
			WB(f,REG_X);
			res += 2;
			__C64F($%x,"STX",f);
		}
		break;
		case 0x88:
			REG_Y--;
			STATUSZN(REG_Y);
			res++;
			__C64F(NOARG,"DEY");
		break;
		case 0x8a:
			REG_A=REG_X;
			STATUSZN(REG_A);
			res++;
			__C64F(NOARG,"TXA");
		break;
		case 0x8c:{
			u16 f=*((u16 *)&mem[ipc]);
			WB(f,REG_Y);
			ipc+=2;
			res += 3;
			__C64F($%x,"STY",f);
		}
		break;
		case 0x8d:{
			u16 f=*((u16 *)&mem[ipc]);
			WB(f,REG_A);
			ipc+=2;
			res += 3;
			__C64F($%x,"STA",f);
		}
		break;
		case 0x8e:{
			u16 f=*((u16 *)&mem[ipc]);
			WB(f,REG_X);
			ipc+=2;
			res += 3;
			__C64F($%x,"STX",f);
		}
		break;
		case 0x90:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BCC",d+ipc);
			if(!(REG_P & C_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0x91:{
			u8 d=mem[ipc++];
			u16 f;

			RW(d,f);
			WB(f+REG_Y,REG_A);
			res += 5;
			__C64F([$%x]\x2cY,"STA",d);//ea0c
		}
		break;
		case 0x94:{
			u8 ff,d=mem[ipc++];

			ff=d+REG_X;
			WB(ff,REG_Y);
			res += 3;
			__C64F($%x\x2cX,"STY",d);
		}
		break;
		case 0x95:{
			u8 ff,d=mem[ipc++];

			ff=d+REG_X;
			WB(ff,REG_A);
			res += 3;
			__C64F($%x\x2cX,"STA",d);
		}
		break;
		case 0x98:
			REG_A=REG_Y;
			STATUSZN(REG_A);
			res++;
			__C64F(NOARG,"TYA");
		break;
		case 0x99:{
			u16 f=*((u16 *)&mem[ipc]);

			WB(f+REG_Y,REG_A);
			ipc+=2;
			res += 4;
			__C64F($%x\x2cY,"STA",f);
		}
		break;
		case 0x9a:
			REG_SP=(REG_SP & 0xFF00)|REG_X;
			res++;
			__C64F(NOARG,"TXS");
		break;
		case 0x9d:{
			u16 f=*((u16 *)&mem[ipc]);
			WB(f+REG_X,REG_A);
			ipc+=2;
			res += 4;
			__C64F($%x\x2cX,"STA",f);
		}
		break;
		case 0xa0:
			REG_Y=mem[ipc++];
			STATUSZN(REG_Y);
			res++;
			__C64F(#$%x,"LDY",REG_Y);
		break;
		case 0xa1:{
			u8 d=mem[ipc++];
			u16 f;

			RW(d,f);
			RB(f+REG_X,REG_A);
			STATUSZN(REG_A);
			__C64F([$%x\x2cX],"LDA",d);
		}
		break;
		case 0xa2:
			REG_X=mem[ipc++];
			STATUSZN(REG_X);
			res++;
			__C64F(#$%x,"LDX",REG_X);
		break;
		case 0xa4:{
			u8 d = mem[ipc++];
			RB(d,REG_Y);
			STATUSZN(REG_Y);
			res+=2;
			__C64F($%x,"LDY",d);
		}
		break;
		case 0xa5:{
			u8 d=mem[ipc++];
			RB(d,REG_A);
			STATUSZN(REG_A);
			res+=2;
			__C64F($%x,"LDA",d);
		}
		break;
		case 0xa6:{
			u8 d = mem[ipc++];
			RB(d,REG_X);
			STATUSZN(REG_X);
			res+=2;
			__C64F($%x,"LDX",d);
		}
		break;
		case 0xa8:
			REG_Y=REG_A;
			STATUSZN(REG_A);
			res++;
			__C64F(NOARG,"TAY");
		break;
		case 0xa9:
			REG_A=mem[ipc];
			STATUSZN(REG_A);
			ipc++;
			res++;
			__C64F(#$%x,"LDA",REG_A);
		break;
		case 0xaa:
			REG_X=REG_A;
			STATUSZN(REG_A);
			res++;
			__C64F(NOARG,"TAX");
		break;
		case 0xac:{
			u16 f=*((u16 *)&mem[ipc]);
			RB(f,REG_Y);
			STATUSZN(REG_Y);
			ipc+=2;
			res +=3;
			__C64F($%x,"LDY",f);
		}
		break;
		case 0xad:{
			u16 f=*((u16 *)&mem[ipc]);
			RB(f,REG_A);
			STATUSZN(REG_A);
			ipc+=2;
			res +=3;
			__C64F($%x,"LDA",f);
		}
		break;
		case 0xae:{
			u16 f=*((u16 *)&mem[ipc]);
			RB(f,REG_X);
			STATUSZN(REG_X);
			ipc+=2;
			res +=3;
			__C64F($%x,"LDX",f);
		}
		break;
		case 0xb0:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BCS",d+ipc);
			if((REG_P & C_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0xb1:{
			u16 a;
			u8 f=mem[ipc++];

			RW(f,a);//fixme speedup
			RB(a+REG_Y,REG_A);
			STATUSZN(REG_A);
			res +=4;
			__C64F([$%x]\x2cY,"LDA",f);
		}
		break;
		case 0xb4:{
			u8 ff,f=mem[ipc++];

			ff=REG_X+f;
			RB(ff,REG_Y);
			STATUSZN(REG_Y);
			__C64F($%x\x2cX,"LDY",f);
		}
		break;
		case 0xb5:{
			u8 ff,f=mem[ipc++];

			ff=REG_X+f;
			RB(ff,REG_A);
			STATUSZN(REG_A);
			res += 4;
			__C64F($%x\x2cX,"LDA",f);
		}
		break;
		case 0xb9:{
			u16 f=*((u16 *)&mem[ipc]);

			RB(REG_Y+f,REG_A);
			STATUSZN(REG_A);
			ipc+=2;
			res += 3;
			__C64F($%x\x2cY,"LDA",f);
		}
		break;
		case 0xb8:
			REG_P &= ~V_BIT;
			res++;
			__C64F(NOARG,"CLV");
		break;
		case 0xba:
			REG_X=REG_SP;
			STATUSZN(REG_X);
			res++;
			__C64F(NOARG,"TSX");
		break;
		case 0xbc:{
			u16 f=*((u16 *)&mem[ipc]);

			RB(REG_X+f,REG_Y);
			STATUSZN(REG_Y);
			ipc+=2;
			__C64F($%x\x2cX,"LDY",f);
		}
		break;
		case 0xbd:{
			u16 f=*((u16 *)&mem[ipc]);

			RB(REG_X+f,REG_A);
			STATUSZN(REG_A);
			ipc+=2;
			res += 3;
			__C64F($%x\x2cX,"LDA",f);
		}
		break;
		case 0xbe:{
			u16 f=*((u16 *)&mem[ipc]);

			RB(f+REG_Y,REG_X);
			STATUSZN(REG_X);
			ipc+=2;
			res +=3;
			__C64F($%x\x2cY,"LDX",f);
		}
		break;
		case 0xc0:{
			u8 v,d=mem[ipc++];
			v=d;
			SET_CMP_FLAGS(REG_Y,v);
			__C64F(#$%x,"CPY",d);
		}
		break;
		case 0xc1:{
			u8 v,d=mem[ipc++];
			u16 f;

			RW(d,f);
			RB(f+REG_X,v);
			//v += REG_X;
			SET_CMP_FLAGS(REG_A,v);
		//	EnterDebugMode();
			__C64F([$%x\x2cX],"CMP",d);
		}
		break;
		case 0xc3:{
			u8 v,d=mem[ipc++];
			v=_mem[d+REG_X];
			v--;
			WB(d+REG_X,v);
			SET_CMP_FLAGS(REG_A,v);
			res += 4;
			__C64F($%x\x2cX,"DCP",d);
		}
		break;
		case 0xc4:{
			u8 v,d=mem[ipc++];

			RB(d,v);
			SET_CMP_FLAGS(REG_Y,v);
			__C64F($%x,"CPY",d);
		}
		break;
		case 0xc5:{
			u8 v,d=mem[ipc++];

			RB(d,v);
			res += 2;
			SET_CMP_FLAGS(REG_A,v);
			__C64F($%x,"CMP",d);
		}
		break;
		case 0xc6:{
			u8 v,d=mem[ipc++];
			v=_mem[d]-1;
			WB(d,v);
			STATUSZN(v);
			res += 4;
			__C64F($%x,"DEC",d);
		}
		break;
		case 0xc7:{
			u8 v,d=mem[ipc++];
			v=_mem[d];
			v--;
			WB(d,v);
			SET_CMP_FLAGS(REG_A,v);
			res += 4;
			__C64F($%x,"DCP",d);
		}
		break;
		case 0xc8:
			REG_Y++;
			STATUSZN(REG_Y);
			res++;
			__C64F(NOARG,"INY");
		break;
		case 0xc9:{
			u8 v=mem[ipc++];
			res++;
			SET_CMP_FLAGS(REG_A,v);
			__C64F(#$%x,"CMP",v);
		}
		break;
		case 0xca:
			REG_X--;
			STATUSZN(REG_X);
			res++;
			__C64F(NOARG,"DEX");
		break;
		case 0xcc:{
			u8 v;
			u16 d=*(u16 *)&mem[ipc];

			RB(d,v);
			ipc+=2;
			SET_CMP_FLAGS(REG_Y,v);
			__C64F($%x,"CPY",d);
		}
		break;
		case 0xcd:{
			u8 v;
			u16 d=*(u16 *)&mem[ipc];

			RB(d,v);
			ipc+=2;
			SET_CMP_FLAGS(REG_A,v);
			__C64F($%x,"CMP",d);
		}
		break;
		case 0xce:{
			u8 v;
			u16 d=*(u16 *)&mem[ipc];

			RB(d,v);//fixme speedup
			v--;
			WB(d,v);
			STATUSZN(v);
			ipc+=2;
			__C64F($%x,"DEC",d);
		}
		break;
		case 0xd0:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BNE",d+ipc);
			if(!(REG_P & Z_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0xd1:{
			u16 f;
			u8 v,d=mem[ipc++];

			RW(d,f);
			RB(REG_Y+f,v);
			SET_CMP_FLAGS(REG_A,v);
			res += 4;
			__C64F([$%x]\x2cY,"CMP",d);
		}
		break;
		case 0xd5:{
			u8 v,ff,d=mem[ipc++];

			ff=d+REG_X;
			RB(ff,v);
			SET_CMP_FLAGS(REG_A,v);
			res += 4;
			__C64F($%x\x2cX,"CMP",d);
		}
		break;
		case 0xd6:{// 0page
			u8 ff,v,d=mem[ipc++];

			ff=d+REG_X;
			RB(ff,v);
			v--;
			WB(ff,v);
			STATUSZN(v);
			__C64F($%x,"DEC",d);
		}
		break;
		case 0xd9:{
			u8 v;
			u16 f=*((u16 *)&mem[ipc]);

			RB(REG_Y+f,v);
			SET_CMP_FLAGS(REG_A,v);
			ipc+=2;
			__C64F($%x\x2cY,"CMP",f);
		}
		break;
		case 0xd8:
			REG_P &= ~D_BIT;
			__C64F(NOARG,"CLD");
		break;
		case 0xdd:{
			u8 v;
			u16 f=*((u16 *)&mem[ipc]);

			RB(REG_X+f,v);
			SET_CMP_FLAGS(REG_A,v);
			ipc+=2;
			__C64F($%x\x2cX,"CMP",f);
		}
		break;
		case 0xde:{
			u8 v;
			u16 d=*(u16 *)&mem[ipc];

			RB(d+REG_X,v);//fixme speedup
			v--;
			WB(d+REG_X,v);
			STATUSZN(v);
			ipc+=2;
			__C64F($%x,"DEC",d);
		}
		break;
		case 0xe0:{
			u8 v,d;

			v=d=mem[ipc++];
			SET_CMP_FLAGS(REG_X,v);
			res++;
			__C64F(#$%x,"CPX",d);
		}
		break;
		case 0xe4:{
			u8 v,d=mem[ipc++];

			RB(d,v);
			SET_CMP_FLAGS(REG_X,v);
			res += 2;
			__C64F($%x,"CPX",d);
		}
		break;
		case 0xe5:{
			u8 a,v = mem[ipc++];

			RB(v,a);
			DO_SBC(REG_A,a,a);
			REG_A=a;
			res += 2;
			__C64F($%x,"SBC",v);
		}
		break;
		case 0xe6:{
			u8 a,f = mem[ipc++];
			a=_mem[f]+1;
			WB(f,a);
			STATUSZN(a);
			res+=4;
			__C64F($%x,"INC",f);
		}
		break;
		case 0xe8:{
			REG_X++;
			STATUSZN(REG_X);
			res++;
			__C64F(NOARG,"INX");
		}
		break;
		case 0xe9:{
			u8 a,v = mem[ipc++];
			DO_SBC(REG_A,v,a);
			REG_A=a;
			res++;
			__C64F(#$%x,"SBC",v);
		}
		break;
		case 0x1c:
		case 0xfc:
			ipc++;
		case 0x44:
		case 0x80:
			ipc++;
		case 0xea:
			__C64F(NOARG,"NOP");
		break;
		case 0xec:{
			u8 v;
			u16 d =*(u16 *)&mem[ipc];
			RB(d,v);
			SET_CMP_FLAGS(REG_X,v);
			ipc += 2;
			res += 3;
			__C64F($%x,"CPX",d);
		}
		break;
		case 0xed:{
			u8 mm,a;
			u16 v = *(u16 *)&mem[ipc];

			RB(v,a);
			DO_SBC(REG_A,a,a);
			REG_A=a;
			ipc += 2;
			res+=3;
			__C64F($%x,"SBC",v);
		}
		break;
		case 0xee:{
			u16 f =*(u16 *)&mem[ipc];
			u8 a;

			RB(f,a);
			a++;
			WB(f,a);
			STATUSZN(a);
			ipc += 2;
			res+=5;
			__C64F($%x,"INC",f);
		}
		break;
		case 0xf0:{
			u16 d=_pc + (s8)mem[ipc++];
			res++;
			__C64F($%04X,"BEQ",d+ipc);
			if((REG_P & Z_BIT)){
				_pc = d;
				res++;
			}
		}
		break;
		case 0xf1:{
			u8 a,v;
			u16 f;

			a = mem[ipc++];
			RW(a,f);
			RB(REG_Y+f,v);
			DO_SBC(REG_A,v,v);
			REG_A=v;
			res += 4;
			__C64F([$%x]\x2cY,"SBC",a);
		}
		break;
		case 0xf2:{
			u8 op;

			switch((op=mem[ipc++])){
				case 0:{
					u8 a;

				//ram[0x90] |= the_iec->Out(ram[0x95], ram[0xa3] & 0x80);
					_mem[0x90] |= machine->OnEvent(MACHINE_EVENT(0x104),&a);
				//	printf("f200 %x %x %x\n",_mem[0x95],_mem[0xa3],_mem[0x90]);
					//_mem[0x90] = 0;
					REG_P &= ~C_BIT;
					ipc=0;
					_pc=0xedac;
				}
				break;
				case 1:{
					u8 a;

					_mem[0x90] |=machine->OnEvent(MACHINE_EVENT(0x103),&a);
					//ram[0x90] |= the_iec->OutATN(ram[0x95]);
				//	printf("f201 %x %x\n",_mem[0x95],_mem[0x90]);
					REG_P &= ~C_BIT;
					ipc=0;
					_pc=0xedac;
				}
				break;
				case 2:{
					u8 a;

					a=_mem[0x95];
					//ram[0x90] |= the_iec->OutSec(ram[0x95]);
					_mem[0x90] |= machine->OnEvent(MACHINE_EVENT(0x102),&a);
				//	printf("f202 %x %x %x\n",_mem[0x95],a,_mem[0x90]);
					REG_P &= ~C_BIT;
					ipc=0;
					_pc=0xedac;
				}
				break;
				case 3:{
					u8 a;

					_mem[0x90] |= machine->OnEvent(MACHINE_EVENT(0x101),&a);
					//printf("f203 %x %x %x\n",a,_mem[0x90],_pc);
					STATUSZN(a);
					REG_A=a;
					REG_P &= ~C_BIT;
					_pc = 0xedac;
					ipc=0;
				//	EnterDebugMode();
				}
				break;
				case 4:
					//printf("f204\n");
					//the_iec->SetATN();
					_pc = 0xedfb;
					ipc=0;
				break;
				case 5:
					//printf("f205\n");
					//the_iec->RelATN();
					_pc = 0xedac;
					ipc=0;
				break;
				case 6:
					//printf("f206\n");
					//the_iec->Turnaround();
					_pc = 0xedac;
					ipc=0;
				break;
				case 7:
					//the_iec->Release();
					_pc = 0xedac;
					ipc=0;
				break;
				case 0x10:
					machine->OnEvent(MACHINE_EVENT(0x201));
					REG_X=0;
				break;
				default:
					//printf("f2 %x\n",op);
					//EnterDebugMode();
				break;
			}
		}
		break;
		case 0xf5:{//0 page
			u8 ff,a,v = mem[ipc++];

			ff=v+REG_X;
			RB(ff,a);
			DO_SBC(REG_A,a,a);
			REG_A=a;
			res+=3;
			__C64F($%x\x2cX,"SBC",v);
		}
		break;
		case 0xf6:{
			u8 v,ff,f = mem[ipc++];

			ff=f+REG_X;
			RB(ff,v);
			v++;
			STATUSZN(v);
			WB(ff,v);
			res+=5;
			__C64F($%x\x2cX,"INC",f);
		}
		break;
		case 0xf8:
			REG_P |= D_BIT;
			__C64F(NOARG,"SED");
		break;
		case 0xf9:{
			u8 a;

			u16 v = *(u16 *)&mem[ipc];
			RB(v+REG_Y,a);
			DO_SBC(REG_A,a,a);
			REG_A=a;
			ipc += 2;
			res += 4;
			__C64F($%x\x2cY,"SBC",v);
		}
		break;
		case 0xfd:{
			u8 a;
			u16 v = *(u16 *)&mem[ipc];

			RB(v+REG_X,a);
			DO_SBC(REG_A,a,a);
			REG_A=a;
			ipc += 2;
			res += 4;
			__C64F($%x\x2cX,"SBC",v);
		}
		break;
		case 0xfe:{
			u16 f =*(u16 *)&mem[ipc];
			u8 a;

			RB(f+REG_X,a);
			a++;
			WB(f+REG_X,a);
			STATUSZN(a);
			ipc += 2;
			res+=6;
			__C64F($%x\x2cX,"INC",f);
		}
		break;
		case 0xff:{
			u8 a;
			u16 f = *(u16 *)&mem[ipc];

			RB(f+REG_X,a);
			a++;
			WB(f+REG_X,a);
			DO_SBC(REG_A,a,a);
			REG_A=a;
			ipc += 2;
			__C64F($%x\x2cX,"ISB",f);
		}
		break;
		default:
			EnterDebugMode();
			printf("unk %x P:%x\n",_opcode,_pc);
		break;
	}
Z:
	_pc+=ipc;
	if(_irq_pending)
		machine->OnEvent(0,-1);
	//if(REG_P & D_BIT) EnterDebugMode();
	return res;
}

int m6502::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_REGISTER:{
			u32 v,*p=(u32 *)pv;

			v=p[0];
			if(v == (u32)-1){
				char *c;

				c=*((char **)&p[2]);
				if(strcmp(c,"REG_P") == 0)
					v=REG_P;
				else if(strcmp(c,"A") == 0)
					v=REG_A;
				else if(strcmp(c,"X") == 0)
					v=REG_X;
				else if(strcmp(c,"Y") == 0)
					v=REG_Y;
			}
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
				if(strcmp(c,"REG_P") == 0)
					REG_P=p[1];
				else if(strcmp(c,"A") == 0)
					REG_A=p[1];
				else if(strcmp(c,"X") == 0)
					REG_X=p[1];
				else if(strcmp(c,"Y") == 0)
					REG_Y=p[1];
				return 0;
			}
		}
			return 0;
		case ICORE_QUERY_SET_LOCATION:{
			u32 *p=(u32 *)pv;
			void *__tmp;

			RMAP_W(p[0],__tmp,W);
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
		case ICORE_QUERY_NEXT_STEP:{
			switch(*((u32 *)pv)){
				case 1:
					*((u32 *)pv) = 3;

					return 0;
				default:
					*((u32 *)pv)=2;
					return 0;
			}
		}
	}
	return CCore::Query(what,pv);
}

__tape::__tape() : __imagestreamer(){
}

int __tape::reset(){
	if(fp) fclose(fp);
	fp=NULL;
	_status=0;
	_pos=0;
	return 0;
}

int __tape::Init(int,void *a,void *b,u32 f){
	_freq=f;
	//_ciabreg=(u8 *)CIAB;
	//cpu->Query(ICORE_QUERY_IO_PORT,&_ciabreg);
	_cia1=(IDevice *)1;
	if(cpu->Query(ICORE_QUERY_IO_DEVICE,&_cia1))
		return -1;
	return 0;
}

int __tape::Run(u8 *,int cyc,void *obj){
	if(!_pulse)
		return 0;
	u32 dd=0,d=D_CYCLES(_cycles,_freq,__cycles);
	//printf("%u\n",_pulse);
	if(d >_pulse){
		dd=d-_pulse;
		d = _pulse;
	}
	_pulse -= d;
	_cycles=__cycles;
	if(_pulse > 0) return 0;
	_cia1->Trigger(0,0,0,(void *)0x10);
	update(0);
	//printf("tape delta %u %u\n",dd,_pulse);
	//if(_pulse >= dd) _pulse-= dd;
	//if(_pos==45631) {printf ("tape pos\n");EnterDebugMode();}
	return 0;//i|0x80000000;
}

int __tape::write(u32 a,u8 v){
//	printf("%x %x\n",a,v);
	/*
	 * 	memset(header, 0, sizeof(header));
	 *	memcpy(header, "C64-TAPE-RAW", 12);
	 *	header[12] = 1;
	*/
	_play=1;
	_record=0;
	//_motor=1;
	_pulse=0;
	_cycles=__cycles;
	return 0;
}

int __tape::read(u32,u8 *){
	return 0;
}

int __tape::motor(int s){
	if(s != _motor){
		//if(_motor && !s) EnterDebugMode();
		_motor=s;
		update(0);
		DLOG("TAPE Motor %u %u %u",s,_pulse,_pos);
	}
	return 0;
}

int __tape::update(int){
	int pulse,res;
	u8 c;
	u32 d;

	if(!_motor || !fp || !_play)
		return 0;
	res=-1;
	pulse=0;
	if(!fread(&c,1,1,fp))
		goto Z;
	++_pos;
	if (c)
		pulse += c * 8;
	else if (_tap_version == 1) {
		if(!fread(&c,1,1,fp))
			goto Z;
		++_pos;
		pulse += c;
		if(!fread(&c,1,1,fp))
			goto Z;
		++_pos;
		pulse += SL(c,8);
		if(!fread(&c,1,1,fp))
			goto Z;
		++_pos;
		pulse += SL(c,16);
		//printf("lp %x\n",pulse);
	}
	else
		pulse += 1024 * 8;
	res=pulse;
Z:
	_pulse=pulse;
	//if(res==-1) printf("end tape %u %u\n",_motor,pulse);
	return res;
}

int __tape::Add(char *c){
	int res,cc;
	int header_size;

	res=-1;
	if(Open(c)) goto Z;
	res--;
	if (fread(_header, 1, sizeof(_header), fp) != sizeof(_header))
		goto Z;
	if(memcmp(_header, "C64-TAPE-RAW", 12)) goto Z;

	_tap_version =_header[12];
	res--;
	if (_tap_version != 0 && _tap_version != 1)
		goto Z;
	header_size = sizeof(_header);
	_size = (_header[19] << 24)
	          | (_header[18] << 16)
	          | (_header[17] <<  8)
	          | (_header[16] <<  0);
	res=0;
	_pos=ftell(fp);
Z:
	if(res)	Close();
	//printf("%d %x %x %u\n",res,_tap_version,header_size,_size);
	return res;
}

};