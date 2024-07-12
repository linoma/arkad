#include "ps1m.h"
#include "gui.h"

extern GUI gui;

namespace ps1{

#define __R3000F(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE) __F(a,## __VA_ARGS__);
#define __R3000D(a,...)	sprintf(cc,STR(%s) STR(\x20) STR(a),## __VA_ARGS__);

static 	u8 _key_config[]={4,6,7,5,0,3,10,11,12,14,8,9,1,2,15};
	//u16 a[]={GDK_KEY_Up,GDK_KEY_Down,GDK_KEY_Left,GDK_KEY_Right,'a','s','q','w','z','x','p','o','l','k','m','n','1','2','3','4',0};

PS1M::PS1M() : Machine(MB(50)),PS1BIOS(){
	_freq=MHZ(33.868);
	_keys=_key_config;
}

PS1M::~PS1M(){
}

int PS1M::Load(IGame *pg,char *fn){
	int  res;
	u32 sz,d[10];

	if(!pg && !fn)
		return -1;
	if(!pg)
		pg=new PS1ROM();

	if(!pg || pg->Open(fn,0))
		return -1;
	Reset();
	if(pg->Query(IGAME_GET_INFO,d))
		return -2;

//		fread(PSXM(SWAP32(tmpHead.t_addr)), SWAP32(tmpHead.t_size), 1, (FILE *)_data);

	//_key1=d[0];
	//_key2=d[1];

	printf("adr:%x size;%u PC:%x %5x %u\n",d[IGAME_GET_INFO_SLOT],d[IGAME_GET_INFO_SLOT+1],d[IGAME_GET_INFO_SLOT+2],d[IGAME_GET_INFO_SLOT+4],(u32)sizeof(PS1EXE_HEADER));

	pg->Read(_memory,d[IGAME_GET_INFO_SLOT+1],NULL);

	memcpy(&_mem[d[IGAME_GET_INFO_SLOT] & 0x1fffff],_memory,d[IGAME_GET_INFO_SLOT+1]);

//0x5ff74 8005ff70 8006046c cc9a42 CC9A42241E8003

	_pc=d[IGAME_GET_INFO_SLOT+2];
	REG_SP=d[IGAME_GET_INFO_SLOT+4];
	REG_GP=d[IGAME_GET_INFO_SLOT+3];
	_cdrom._load(pg);
	Query(ICORE_QUERY_SET_FILENAME,fn);
	res=0;
A:
	return res;
}

int PS1M::Destroy(){
	PS1SPU::Destroy();
	PS1GPU::Destroy();
	Machine::Destroy();
	R3000Cpu::Destroy();
	if(_portfnc_write)
		delete []_portfnc_write;
	_portfnc_write = _portfnc_read=NULL;
	return 0;
}
static int lino=0;
int PS1M::Reset(){
	Machine::Reset();
	PS1BIOS::Reset();
	PS1GPU::Reset();
	PS1SPU::Reset();
	return 0;
}

int PS1M::Init(){
	if(Machine::Init())
		return -1;
	if(R3000Cpu::Init(&_memory[MB(2)]))
		return -2;
	_io_regs=(u32 *)&_mem[MB(4)];
	_gpu_mem=&_mem[MB(5)];
	_gpu_regs=_io_regs;
	if(PS1GPU::Init())
		return -3;
	if(PS1BIOS::Init(*this))
		return -5;
	_spu_regs=_io_regs;
	if(PS1SPU::Init(*this,_io_regs,&_memory[MB(20)]))
		return -6;
	_portfnc_write = (CoreMACallback *)new CoreMACallback[0x40000];
	if(_portfnc_write==NULL)
		return -10;
	_portfnc_read = &_portfnc_write[0x20000];
	memset(_portfnc_write,0,sizeof(CoreMACallback)*0x40000);

	for(u32 i=0x1F801100;i<0x1F80112f;i++)
		SetIO_cb(i,(CoreMACallback)&PS1M::fn_write_io_timer,(CoreMACallback)&PS1M::fn_read_io_timer);
	for(u32 i=0x1F801080;i<0x1F801100;i++)
		SetIO_cb(i,(CoreMACallback)&PS1M::fn_write_io_dma,0);

	for(int i=0;i<0x24;i++){
		SetIO_cb(0x1f801000|i,(CoreMACallback)&PS1M::fn_write_io,0);
		SetIO_cb(0x1f801040|i,(CoreMACallback)&PS1M::fn_write_io,0);
	}
	SetIO_cb(0x1f801040,(CoreMACallback)&PS1M::fn_write_io,(CoreMACallback)&PS1M::fn_read_io);
	SetIO_cb(0x1f801070,(CoreMACallback)&PS1M::fn_write_io,0);
	for(int i=0;i<0x4;i++)
		SetIO_cb(0x1F801800|i,(CoreMACallback)&PS1M::fn_write_io,(CoreMACallback)&PS1M::fn_read_io);
	//SetIO_cb(0x1f801074,(CoreMACallback)&PS1M::fn_write_io,0);
	SetIO_cb(0x1f801810,(CoreMACallback)&PS1M::fn_write_io_gp,0);
	SetIO_cb(0x1f801814,(CoreMACallback)&PS1M::fn_write_io_gp,(CoreMACallback)&PS1M::fn_read_io_gp);
	SetIO_cb(0x1f801820,(CoreMACallback)&PS1M::fn_write_io_mdec,(CoreMACallback)&PS1M::fn_read_io_mdec);
	SetIO_cb(0x1f801824,(CoreMACallback)&PS1M::fn_write_io_mdec,0);

	for(u32 i=0x1F801c00;i<0x1F801e60;i++)
		SetIO_cb(i,(CoreMACallback)&PS1M::fn_write_io_spu);
	AddTimerObj((ICpuTimerObj *)this,2128);
	return 0;
}

int PS1M::Exec(u32 status){
	int ret;

	ret=R3000Cpu::Exec(status);
	__cycles=R3000Cpu::_cycles;
	//if(!lino &&_pc==0x80059090){lino=1; EnterDebugMode();}
	//if(*(u32 *)&_mem[0x85758] != 0x03ff0005) EnterDebugMode();
	switch(ret){
		case -1:
		case -2:
			return ret*-1;
	}
	EXECTIMEROBJLOOP(ret,OnEvent(i__,0),0);
	MACHINE_ONEXITEXEC(status,0);
}

int PS1M::OnEvent(u32 ev,...){
	va_list arg;

	switch(ev){
		case PS1ME_PLAY_XA_SECTOR:{
			void *p;
			u32 a;

			va_start(arg, ev);
			p=va_arg(arg,void *);
			int res=_xa_decode_sector(0,p,a);

			/*FILE *fp=fopen("xa.bin","ab");
			fwrite(p,1,0x908,fp);
			fclose(fp);
			*/
			va_end(arg);
			return  res;
		}
		case PS1ME_PLAYTRACK:{
			va_start(arg, ev);
			int m=va_arg(arg,int);
			if(m){
				char *p=va_arg(arg,char *);
				u32 pos=va_arg(arg,u32);
				_playAudioFile(p,pos);
			}
			else
				_closeAudioFile();
			va_end(arg);
			return 0;
		}
		case ME_MOVEWINDOW:{
			int x,y,w,h;

			va_start(arg, ev);
			x=va_arg(arg,int);
			y=va_arg(arg,int);
			w=va_arg(arg,int);
			h=va_arg(arg,int);

			CreateBitmap(w,h);
			PS1GPU::MoveResize(x,y,w,h);

			Draw();
			va_end(arg);
		}
		return 0;
		case ME_REDRAW:
			//SaveBitmap("cazzo.bmp",1024,512,16,16,_gpu_mem);
			_gpu_status |= GRE_STATUS_SWAP_BUFFER;
	//		GRE::_clearTextures();
			PS1GPU::Update();
			Draw();
			return 0;
		case ME_ENDFRAME:
			PS1SPU::Update();
			PS1GPU::Update(OnFrame()==0);
	//		else
	//			GRE::Update(0);
			return 0;
		case (u32)-2:
			return 0;
		case ME_KEYUP:
			{
				int key;

				va_start(arg, ev);
				key=va_arg(arg,int);
				_joy._key_event(_keys[key],0);
				va_end(arg);
			}
			return 0;
		case ME_KEYDOWN:
			{
				int key;

				va_start(arg, ev);
				key=va_arg(arg,int);
				_joy._key_event(_keys[key],1);
				va_end(arg);
			}
			return 0;
		case 0://pending irq bit
			if(!_irq_pending)
				return 0;
			ev=BV(_log2(_irq_pending));
		//	printf("irq %x %x %x\n",_irq_pending,ev,_log2(_irq_pending));
		break;
		default:
			if(BVT(ev,31))
				return -1;
			{
				va_start(arg, ev);
				int i=va_arg(arg,int);
				va_end(arg);
				if(i == 1){
					BS(_irq_pending,BV(ev));
					return 1;
				}
			}
		break;
	}
	switch(ev){
		default:
			BC(_irq_pending,ev);
			OnException(0,ev);
			break;
	}
	return 0;
}

int PS1M::Query(u32 what,void *pv){
	switch(what){
		case  ICORE_QUERY_DBG_OAM:{
			GRETEXTURE tex;
			u32 b,*params;
			u64 id;
			int res;

			res=-3;
			params= *((u32 **)pv);
			int idx = params[0];
	//		printf("oam %u %u\n",idx,_textures.size());
			if(idx < 0 || idx >=_textures.size())
				return -4;
			tex=_textures[idx];
			id=tex.id;
			u32 *p=(u32 *)calloc(1,tex.w*tex.h*sizeof(u32)+10*sizeof(u32));
			if(!p) return -5;

			*((void **)pv)=p;
			p[0]= MAKELONG(tex.w,tex.h);
			p[1]=10*sizeof(u32);
			p[2]=32;
			int ty=SR(id,19)&0x1ff;
			int tx=SR(id,28)&0x3ff;
			b=SR(id,58) & 0x1f;
			b=SL(b&0xf,6)|SL(b&0x10,14);
			ty=SL(ty,10);
			switch(tex._mode){
				case 1:
					tx=SR(tx,1);
				break;
				case 0:
					tx=SR(tx,2);
					//tex._stride*=2;
				break;
			}
			//printf("%u %u %u %u\n",ttx,tty,tx,ty);
			b+= ty+tx;
			//printf("ct %x %u %u %x %u\n",b*2,tex.w,tex.h,tex._mode,tex._stride);
				_createTexture((u16 *)_gpu_mem + b,&tex,&p[10]);
				return  0;
			}
			return -1;
		case ICORE_QUERY_DBG_LAYER:{
			int res;
			u32 *params;

			res=-3;
			params=*((u32 **)pv);
			if(!params  || (int)params[0] < 0) return -1;

			if(params[1]==0 && params[2]==0){
				params[1]= MAKELONG(1024,512);
				params[2]=512;
				res=0;
			}
			else{
				u32 *p=(u32 *)calloc(1,1024*512*sizeof(u16)+10*sizeof(u32));
				if(p){
					*((void **)pv)=p;
					memcpy(p,params,10*sizeof(u32));
					//p[1]= MAKELONG(1024,512);
					p[3]=10*sizeof(u32);
				//	_layers[params[0]]->_drawToMemory((u16 *)&p[10],p,*this);
					memcpy(&p[10],_gpu_mem,MB(1));
					res=0;
				}
			}
			printf("L %u  %u %u %u\n",params[0],params[1],params[2],params[3]);
			return res;
		}
			return  -1;
		case ICORE_QUERY_DBG_MENU_SELECTED:{
				u32 id =*((u32 *)pv);
				//printf("id %x\n",id);
				switch((u16)id){
					case 1:
						PS1GPU::_enable(GL_TEXTURE_2D,SR(id,16));
						return 0;
				}
			}
			return -1;
		case ICORE_QUERY_DBG_MENU:{
				char *p = new char[1000];
				((void **)pv)[0]=p;

				strcpy(p,"VRAM");
				p+=strlen(p)+1;
				*((u32 *)p)=0;
				*((u32 *)&p[4])=0x10000;
				p+=sizeof(u32)*2;
				*p=0;
				strcpy(p,"Textures");
				p+=strlen(p)+1;
				*((u32 *)p)=1;
				*((u32 *)&p[4])=0x0102;
				p+=sizeof(u32)*2;
				*p=0;
				return 0;
		}
		case ICORE_QUERY_MEMORY_WRITE:{
			u32 *p=(u32 *)pv;
			WL(p[0],p[1]);
			printf("ICORE_QUERY_MEMORY_WRITE %x\n",p[2]);
		}
			return 0;
		case ICORE_QUERY_ADDRESS_INFO:
			{
				u32 adr,*pp,*p = (u32 *)pv;
				adr =*p++;
				pp=(u32 *)*((u64 *)p);
				switch(SR(adr,20)){
					case 0x800:
					case 0x801:
						pp[0]=0x80000000;
						pp[1]=MB(2);
						break;
					case 0x1f8:
						pp[0]=0x1F800000;
						pp[1]=KB(64);
					break;
#ifdef _DEVELOP
					case 0x1f9:
						pp[0]=0x1F900000;
						pp[1]=MB(1);
					break;
#endif
					default:
						return -2;
				}
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
				memset(p,0,9*sizeof(DEBUGGERPAGE));
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Registers");
				strcpy(p->name,"3100");
				p->type=1;
				p->popup=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Memory");
				strcpy(p->name,"3102");
				p->type=2;
				p->editable=1;
				p->popup=1;
				p->clickable=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"GTE");
				strcpy(p->name,"3103");
				p->type=1;
				p->popup=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Devces");
				strcpy(p->name,"3104");
				p->type=1;
				p->popup=1;

				p++;
				p->size=sizeof(DEBUGGERPAGE);
				strcpy(p->title,"Call Stack");
				strcpy(p->name,"3108");
				p->type=1;
				p->popup=1;

			}
			return 0;
		default:
			return R3000Cpu::Query(what,pv);
	}
}

int PS1M::Dump(char **pr){
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
	*((u64 *)cc)=0;

	res = 0;
	p=c;
	strcpy(p,"3100");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	((CCore *)cpu)->_dumpRegisters(p);
#ifdef _DEVELOP
	sprintf(cc,"C: %u L:%u F:%u SR:%8X IP:%08X",__cycles,__line,__frame,CP0._regs[CP0.SR],_irq_pending);
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
	pp=&cc[900];

	RMAP_(adr,mem,R);
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
	strcat(p,"Data\n");
	for(int i=0;i<sizeof(GTE._regs)/sizeof(u32);i++){
		sprintf(pp,"R%02d %08X ",i,GTE._regs[i]);
		strcat(p,pp);
		if((i&3)==3) strcat(p,"\n");
	}
	strcat(p,"\nControl\n");
	for(int i=0;i<sizeof(GTE.ctrl._regs)/sizeof(u32);i++){
		sprintf(pp,"C%02d %08X ",i,GTE.ctrl._regs[i]);
		strcat(p,pp);
		if((i&3)==3) strcat(p,"\n");
	}
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3104");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;
	*((u64 *)p)=0;

	RMAP_(0x1f801000,mem,R);
	strcpy(p,"CDROM\n");
	sprintf(pp,"00:%02X 01:%02X 02:%02X 03:%02X S:%02X-%u-%x %u-%u\n",mem[0x800],mem[0x801],mem[0x802],mem[0x803],
		_cdrom._status,_cdrom._state,_cdrom._mode,_cdrom._cw,_cdrom._cr);
	strcat(p,pp);
	for(int i=0;i<4;i++){
		sprintf(pp,"%02X %02X %02X %02X\n",_cdrom._regs[i*4],_cdrom._regs[i*4+1],_cdrom._regs[i*4+2],_cdrom._regs[i*4+3]);
		strcat(p,pp);
	}

	sprintf(pp,"\nIRQ\nISTAT:%04X IMASK:%04X\n",*(u16 *)&mem[0x70],*(u16 *)&mem[0x74]);
	strcat(p,pp);

	strcat(p,"\nTimers\n");
	for(int i=0;i<3;i++){
		sprintf(pp,"%02d  %04X %04X %04X M:%x S:%x\n",i,_timers[i]._count(),_timers[i]._status,*(u16 *)&mem[0x100+i*16+8],_timers[i]._mode,_timers[i]._source);
		strcat(p,pp);
	}
	strcat(p,"\nDMA\n");
	sprintf(pp,"DPCR:%08X DICR:%08X\n",*(u32 *)&mem[0xf0],*(u32 *)&mem[0xf4]);
	strcat(p,pp);
	for(int i=0;i<7;i++){
		sprintf(pp,"%02d %08X %08X %08X\n",i,*(u32 *)&mem[0x80+i*16],*(u32 *)&mem[0x80+i*16+4],*(u32 *)&mem[0x80+i*16+8]);
		strcat(p,pp);
	}

	strcat(p,"\nMDEC\n");
	sprintf(pp,"MDEC0:%08X MDEC1:%08X\n",*((u32 *)&mem[0x820]),*((u32 *)&mem[0x824]));
	strcat(p,pp);

	strcat(p,"\nGPU\n");
	sprintf(pp,"STAT:%08X M:%08x %dx%d %dx%d\n",*((u32 *)&mem[0x814]),_env._display._mode,_env._display._w,_env._display._h,
		_env._display._x,_env._display._y);
	strcat(p,pp);
	strcat(p,"\nJOY\n");
	sprintf(pp,"STAT:%08X DATA:%0X\t%04x\n",*((u32 *)&mem[0x44]),mem[0x40],_joy._pads[0]._buttons);
	strcat(p,pp);

	strcat(p,"\nSPU\n");
	sprintf(pp,"CNT:%04X\n",*((u16 *)&mem[0xdaa]));
	strcat(p,pp);

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

	*pr = c;
	return res;
}

int PS1M::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	Machine::LoadSettings(v);
	m["width"]=to_string(_width);
	m["height"]=to_string(_height);
	PS1SPU::LoadSettings(v);
	PS1GPU::LoadSettings(v);
	return 0;
}

int PS1M::OnCop(u32 cop,u32 op,u32 s,u32 *d){
	//if(!cop &&(op !=0 && op!=4)) {printf("cop0 %x %x %x\n",s,*d,op);EnterDebugMode();}
	switch(op){
		case 0://mfc
			switch(cop){
				case 2:
					return GTE._mv_from(s,d);
				case 0:
					if(s!=12) printf("read cop0 %x \n",s);
					*d=CP0._regs[s];
					return 0;
			}
		break;
		case 1://cop
			switch(cop){
				case 2:
					return GTE._op(s,*this);
				case 0:
					switch(s&0x3f){
						default:
						EnterDebugMode();
					}
					return 0;
			}
		break;
		case 2://cfc
			switch(cop){
			case 2:
				//printf("GTE cfc %x\n",s);
				return GTE._ctrl_mv_from(s,d);
				//EnterDebugMode();
			case 0:
				return 0;
			}
		break;
		case 4://mtc
			switch(cop){
				case 2:
				//	printf("GTE %d %08X %x %08x\n",s,*d,RT(_opcode),REG_(RT(_opcode)));
					return GTE._mv_to(s,d);
				case 0:
					CP0._regs[s]=*d;
					return 0;
			}
		break;
		case 6://ctc
			switch(cop){
				case 2:
					//	printf("GTE TL;%d %08X %x %08x\n",s,*d,RT(_opcode),REG_(RT(_opcode)));
					return GTE._ctrl_mv_to(s,d);
				case 0:
					return 0;
			}
		break;
		case 8://lwc
			switch(cop){
				case 2:{
					u32 v_;

					RL(s,v_);
					return GTE._mv_to((u32)(u64)d,&v_);
				}
			}
		break;
		case 9://swc
			switch(cop){
				case 2:{
					u32 v_;

					GTE._mv_from((u32)(u64)d,&v_);
					WL(s,v_);
				}
			}
		break;
	}
	return -1;
}

int PS1M::OnJump(u32 pc){
	switch(pc&0x1fffffff){
		case 0xa0:
			return ExecA0(REG_(9));
		case 0xb0:
			return ExecB0(REG_(9));
		case 0xc0:
			return ExecC0(REG_(9));
		case 0x1000:
			return ReturnFromCall();
	}
	return -1;
}

int PS1M::OnException(u32 code,u32 b){
	switch(CP0._regs[CP0.CAUSE]=code){
		case 0:
			if(_enterIRQ(b))
				_irq_pending |= b;
		break;
		case 0x20:
			DLOG("SYSCALL %x",REG_(REGI_A0));
			//	EnterDebugMode();
			switch(REG_(REGI_A0)){
				case 1:
					CP0._regs[CP0.SR]=  (CP0._regs[CP0.SR] & ~0x3f)|SL(CP0._regs[CP0.SR]&0xf,2);
				break;
				case 2:
					CP0._regs[CP0.SR]= (CP0._regs[CP0.SR] & ~0xf)|SR(CP0._regs[CP0.SR]&0x3f,2);//rfee();
				break;
#ifdef _DEVELOP
				default:
				printf("syscall  %x\n",REG_(REGI_A0));
				break;
#endif
			}
		break;
	}
	return 0;
}

s32 PS1M::fn_write_io_timer(u32 a,void *,void *pdata,u32){
	//printf("fn_write_io_timer %x %x\n",a,*(u32 *)pdata);
	_timers[SR(a&0x3f,4)].write(a&0xf,*(u32 *)pdata);
	return 1;
}

s32 PS1M::fn_read_io_timer(u32 a,void *,void *pdata,u32){
	//printf("fn_write_io_timer %x %x\n",a,*(u32 *)pdata);
	_timers[SR(a&0x3f,4)].read(a&0xf,(u32 *)pdata);
//	EnterDebugMode();
	return 0;
}

s32 PS1M::fn_write_io_dma(u32 a,void *mem,void *pdata,u32 attr){
	//printf("fn_write_io_dma %x %x\n",a,*(u32 *)pdata);
	int res=1;
	switch((u8)a){
		case 0xf0:
		break;
		case 0xf4:{
			u32 v,vv;

			DLOG("DMA CNT %x %x %x",*(u32 *)pdata,*(u32*)mem,attr);
			v=*(u32 *)pdata;
			vv=*(u32*)mem;
			vv = (v&0xffffff)| ((vv & ~(v & 0x7f000000)) & 0x7f000000);
			if((vv&BV(15)) || ((vv & BV(23)) && (vv & 0x7f000000)) )
				vv |= BV(31);
			*(u32 *)pdata=vv;
		}
		break;
		case 0xf6:
			if(attr &AM_WORD) EnterDebugMode();
		case 0xf8:
		case 0xfc:
		break;
		default:
			switch((u8)(res=_dmas[SR((u8)a,4)-8]->write(a&0xf,*(u32 *)pdata,*this))){
				case 0:
				case 1:
					return res;
				case 2:
					AddTimerObj(_dmas[SR((u8)a,4)-8],SR((u32)res,8),this);
					return 0;
			}
		break;
	}
	//EnterDebugMode();
	return res;
}

s32 PS1M::fn_write_io_gp(u32 a,void *,void *pdata,u32){
	return PS1GPU::write(a,*(u32 *)pdata);
}

s32 PS1M::fn_read_io_gp(u32 a,void *,void *pdata,u32){
	return PS1GPU::read(a,(u32 *)pdata);
}

s32 PS1M::fn_write_io_mdec(u32 a,void *,void *pdata,u32){
	int res;

	switch((u8)(res=_mdec.write(a,*(u32 *)pdata,*this))){
		case 2:
			AddTimerObj((ICpuTimerObj *)&_mdec,SR(res,8),this);
			res=1;
		break;
		default:
		break;
	}
	return res;
	//EnterDebugMode();
}

s32 PS1M::fn_read_io_mdec(u32 a,void *,void *pdata,u32){
	//printf("io mdec %x %x\n",a,*(u32 *)pdata);
	return _mdec.read(a,(u32 *)pdata,*this);
}

s32 PS1M::fn_write_io_spu(u32 a,void *,void *pdata,u32){
	PS1SPU::write_reg(a,*((u32 *)pdata));
	//EnterDebugMode();
	return 1;
}

s32 PS1M::fn_read_io(u32 a,void *pmem,void *pdata,u32){
	switch(a){
		case 0x1f801040:
			return _joy._read(a,(u32 *)pdata);
		default:
			return _cdrom.read(a,(u32 *)pdata);
	}
	return 0;
}

s32 PS1M::fn_write_io(u32 a,void *pmem,void *pdata,u32){
	int r;
//	printf("io %x %x\n",a,*(u32 *)pdata);
	//EnterDebugMode();
	switch(a){
		case 0x1F801800:
		case 0x1F801801:
		case 0x1F801802:
		case 0x1F801803:
			switch((u8)(r=_cdrom.write(a,(u8)*(u32 *)pdata,*this))){
				case 1:
					OnEvent(2,1);
				//	EnterDebugMode();
				break;
				case 3:
					OnEvent(2,1);
				case 2:
					AddTimerObj((ICpuTimerObj *)&_cdrom,SR(r,8));
					//printf("cdrrom %x %u\n",r,SR(r,8));
				break;
			}
			//printf("io %x %x\n",a,*(u32 *)pdata);
		break;
		case 0x1f801040:
		case 0x1f801048:
		case 0x1f80104a:
		case 0x1f80104e:
			return  _joy._write(a,*((u32*)pdata));
		case 0x1f801070:
		//	printf("io %x %x\n",a,*(u32 *)pdata);
		//	_irq_pending &= *((u32 *)pdata);
			PS1IOREG(0x1f801070) &= *((u32 *)pdata);
			return 0;
	}
	return 1;
}

static char prs[][3]={"zr","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7",
	"s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"};

R3000Cpu::R3000Cpu() : CCore(){
	_regs=NULL;
	_io_regs=NULL;
	_freq=MHZ(33);
}

R3000Cpu::~R3000Cpu(){
	Destroy();
}

int R3000Cpu::Destroy(){
	CCore::Destroy();
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int R3000Cpu::Reset(){
	CCore::Reset();
	if(_regs)
		memset(_regs,0,sizeof(RSZU)*40);
	_jump=0;
	_irq_pending=0;
	return 0;
}

int R3000Cpu::Init(void *m){
	u32 n;

	if(!(_regs = new u8[n=40*sizeof(RSZU)]))
		return -1;
	memset(_regs,0,n);
	_mem=(u8 *)m;
	//_bk.push_back(0x80000000800b13c0);
	return 0;
}

int R3000Cpu::SetIO_cb(u32 a,CoreMACallback w,CoreMACallback r){
	if(!_portfnc_write && !_portfnc_read)
		return -1;
	if(w  && _portfnc_write)
		_portfnc_write[RMAP_IO(a)]=w;
	if(r && _portfnc_read)
		_portfnc_read[RMAP_IO(a)]=r;
	return 0;
}

int R3000Cpu::_dumpRegisters(char *p){
	char cc[1024];

	sprintf(p,"PC: %08X HI: %08X LO: %08X\n\n",_pc,REG_(REGI_HI),REG_(REGI_LO));
	for(int i=1,n=0;i<sizeof(prs)/sizeof(prs[0]);i++){
		sprintf(cc,"%s:%08x ",prs[i],REG_(i));
		strcat(p,cc);
		if(++n == 6){
			strcat(p,"\n");
			n=0;
		}
		cc[0]=0;
	}
	//sprintf(cc,"\nVBR: %08X SR: %08X T:%c IL:%d Q:%c M:%c",_vbr,_sr,_sr&SH_T ? 49:48,SR(_sr&0xf0,4),_sr&SH_Q ? 49:48,_sr&SH_M ? 49:48);
	strcat(p,cc);
	return 0;
}

int R3000Cpu::_exec(u32 status){
	int ret;

	ret=1;
	RLPC(_pc,_opcode);
	//if(_pc==0x80058fdc) EnterDebugMode();
	switch((_opcode >> 24) & 0xFC){
		default:
			printf("PC %08x %08x\n",_pc,(_opcode&0xfc000000));
			EnterDebugMode();
		break;
		case 0:
			switch(_opcode & 0x3f){
				case 0:
					switch(_opcode & 0x7c0){
						case 0:
							__R3000F(NOARG,"NOP");
						break;
						default:
							REG_(RD(_opcode)) = SL(REG_(RT(_opcode)),POS(_opcode));
							__R3000F(%s\x2c%s\x2c$%x,"SLL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
						break;
					}
				break;
				case 2:
					REG_(RD(_opcode)) = SR(REG_(RT(_opcode)),POS(_opcode));
					__R3000F(%s\x2c%s\x2c$%x,"SRL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 3:
					REG_(RD(_opcode)) = SR((s32)REG_(RT(_opcode)),POS(_opcode));//fiixme
					__R3000F(%s\x2c%s\x2c$%x,"SRA",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 4:
					REG_(RD(_opcode)) = SL(REG_(RT(_opcode)),REG_(RS(_opcode))&0x1f);
					__R3000F(%s\x2c%s\x2c%s,"SLLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 6:
					REG_(RD(_opcode)) = SR(REG_(RT(_opcode)),REG_(RS(_opcode))&0x1f);
					__R3000F(%s\x2c%s\x2c%s,"SRLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 7:
					REG_(RD(_opcode)) = SR((s32)REG_(RT(_opcode)),REG_(RS(_opcode))&0x1f);
					__R3000F(%s\x2c%s\x2c%s,"SRAV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 0x8:
					ret+=3;
					_next_pc=REG_(RS(_opcode));
					_jump=2;
					LOADCALLSTACK(_next_pc);
					__R3000F(%s,"JR",prs[RS(_opcode)]);
				break;
				case 0x9:
					REG_RA=_pc+8;
					_next_pc=REG_(RS(_opcode));
					_jump=2;
					STORECALLLSTACK(REG_RA);
					__R3000F(%s,"JALR",prs[RS(_opcode)]);
				break;
				case 0xc:
					OnException(0x20,SR(_opcode,6)&0xFFFFF);
					__R3000F(%x,"SYSCALL",SR(_opcode,6)&0xFFFFF);
				break;
				case 0xd:
					EnterDebugMode();
					__R3000F(NOARG,"BREAK");
				break;
				case 0x10:
					REG_(RD(_opcode)) = REG_HI;
					__R3000F(%s,"MFHI",prs[RD(_opcode)]);
				break;
				case 0x11:
					REG_LO=REG_(RD(_opcode));
					__R3000F(%s,"MTLO",prs[RD(_opcode)]);
				break;
				case 0x12:
					REG_(RD(_opcode)) = REG_LO;
					__R3000F(%s,"MFLO",prs[RD(_opcode)]);
				break;
				case 0x13:
					REG_HI=REG_(RD(_opcode));
					__R3000F(%s,"MTHI",prs[RD(_opcode)]);
				break;
				case 0x18:{
					s64 value;

					value = (s64)(s32)REG_(RS(_opcode))*(s64)(s32)REG_(RT(_opcode));
					REG_LO=(u32)value;
					REG_HI=(u32)(value >> 32);
					ret += 2;
					__R3000F(%s\x2c%s,"MULT",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x19:{
					u64 value;

					value = (u64)REG_(RS(_opcode))*(u64)REG_(RT(_opcode));
					REG_LO=(u32)value;
					REG_HI=(u32)(value >> 32);
					__R3000F(%s\x2c%s,"MULTU",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x1a:{
					s32 v0,v1,v2;

					v1=REG_(RT(_opcode));
					if(v1){
						v2 = (v0=(s32)REG_(RS(_opcode))) / v1;
						REG_LO=(u32)v2;
						REG_HI=(u32)(v0-(v2*v1));
					}
					__R3000F(%s\x2c%s,"DIV",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x1b:{
					u32 v0,v1,v2;

					v1=REG_(RT(_opcode));
					if(v1){
						v2 = (v0=REG_(RS(_opcode))) / v1;
						REG_LO=(u32)v2;
						REG_HI=(u32)(v0-(v2*v1));
					}
					__R3000F(%s\x2c%s,"DIVU",prs[RS(_opcode)],prs[RT(_opcode)]);
				}
				break;
				case 0x20://'fixme add oveflow xcepion
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"ADD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x21:
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"ADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x22:
					REG_(RD(_opcode)) = REG_(RS(_opcode)) - REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"SUB",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x23:
					REG_(RD(_opcode)) = REG_(RS(_opcode)) - REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"SUBU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x24:
					REG_(RD(_opcode)) = REG_(RS(_opcode))&REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"AND",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x25:
					REG_(RD(_opcode)) = REG_(RS(_opcode))|REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"OR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x26:
					REG_(RD(_opcode)) = REG_(RS(_opcode))^REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"XOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x27:
					REG_(RD(_opcode)) = ~(REG_(RS(_opcode))|REG_(RT(_opcode)));
					__R3000F(%s\x2c%s\x2c%s,"NOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2a:
					REG_(RD(_opcode)) = (s32)REG_(RS(_opcode)) < (s32)REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"SLT",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2b:
					REG_(RD(_opcode)) = REG_(RS(_opcode)) < REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"SLTU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				default:
					printf("0 %x\n",_opcode & 0x3f);
					EnterDebugMode();
				break;
			}
		break;
		case 0x4:
			switch(FT(_opcode)){
				case 0:
					if((s32)REG_(RS(_opcode))<0){
						_next_pc=_pc+SL(IMM(_opcode),2)+4;
						_jump=2;
					}
					__R3000F(%s\x2c$%x,"BLTZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
				break;
				case 1:
					if((s32)REG_(RS(_opcode)) >=0){
						_next_pc=_pc+SL(IMM(_opcode),2)+4;
						_jump=2;
					}
					__R3000F(%s\x2c$%x,"BGEZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
				break;
				default:
					printf("4 %x\n",_opcode & 0x3f);
					EnterDebugMode();
				break;
			}
		break;
		case 0x8:
			_next_pc=JUMP(_pc,_opcode);
			_jump=2;
			__R3000F(%X,"J",_next_pc);
		break;
		case 0xc:
			REG_RA=_pc+8;
			_next_pc=JUMP(_pc,_opcode);
			_jump=2;
			STORECALLLSTACK(REG_RA);
			ret+=3;
			__R3000F(%X,"JAL",_next_pc);
		break;
		case 0x10:
			if(REG_(RT(_opcode))==REG_(RS(_opcode))){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			__R3000F(%s\x2c%s\x2c$%x,"BEQ",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x14:
			if(REG_(RT(_opcode)) !=REG_(RS(_opcode))){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
				ret+=2;
			}
			__R3000F(%s\x2c%s\x2c$%x,"BNE",prs[RT(_opcode)],prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x18:
			if((s32)REG_(RS(_opcode))<=0){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
			}
			__R3000F(%s\x2c$%x,"BLEZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x1c:
			if((s32)REG_(RS(_opcode))>0){
				_next_pc=_pc+SL(IMM(_opcode),2)+4;
				_jump=2;
			}
			__R3000F(%s\x2c$%x,"BGTZ",prs[RS(_opcode)],_pc+(IMM(_opcode)<<2));
		break;
		case 0x20:
			REG_(RT(_opcode)) = REG_(RS(_opcode))+IMM(_opcode);
			__R3000F(%s\x2c%s\x2c$%x,"ADDI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x24:
			REG_(RT(_opcode)) = (REG_(RS(_opcode))+IMM(_opcode));
			__R3000F(%s\x2c%s\x2c$%x,"ADDUI",prs[RT(_opcode)],prs[RS(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x28:
			REG_(RT(_opcode)) = (s32)REG_(RS(_opcode)) < IMM(_opcode);
			__R3000F(%s\x2c%s\x2c$%x,"SLTI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x2C:
			REG_(RT(_opcode)) = REG_(RS(_opcode)) < (u32)IMMU(_opcode);
			__R3000F(%s\x2c%s\x2c$%x,"SLTIU",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x30:
			REG_(RT(_opcode)) = REG_(RS(_opcode)) & IMMU(_opcode);
			__R3000F(%s\x2c%s\x2c$%x,"ANDI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x34:
			REG_(RT(_opcode)) = REG_(RS(_opcode))|IMMU(_opcode);
			__R3000F(%s\x2c%s\x2c$%x,"ORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x38:
			REG_(RT(_opcode)) = REG_(RS(_opcode))^IMMU(_opcode);
			__R3000F(%s\x2c%s\x2c$%x,"XORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x3c:
			REG_(RT(_opcode)) = IMM(_opcode) << 16;
			__R3000F(%s\x2c$%X,"LUI",prs[RT(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x40:
		case 0x44:
		case 0x48:
		case 0x4c:
			switch(RS(_opcode)){
				case 0:
					OnCop(SR(_opcode,26)&3,0,RD(_opcode),&REG_(RT(_opcode)));
					__R3000F(%d %s\x2c%s,"MFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 2:
					OnCop(SR(_opcode,26)&3,2,RD(_opcode),&REG_(RT(_opcode)));
					__R3000F(%d %s\x2c%s,"CFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 4:
					OnCop(SR(_opcode,26)&3,4,RD(_opcode),&REG_(RT(_opcode)));
					__R3000F(%d %s\x2c%s,"MTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 6:
					OnCop(SR(_opcode,26)&3,6,RD(_opcode),&REG_(RT(_opcode)));
					__R3000F(%d %s\x2c%s,"CTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 1:
				case 3:
				case 5:
					printf("40 %x\n",RS(_opcode));
					EnterDebugMode();
				break;
				default:
				//	EnterDebugMode();
					OnCop(SR(_opcode,26)&3,1,_opcode & 0xFFFFFF,0);
					__R3000F(%x,"COP",_opcode & 0xFFFFFF);
				break;
			}
		break;
		case 0x80:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			u8 v;

			RB(a,v);
			REG_(RT(_opcode))=(s32)(s8)v;
			__R3000F(%s\x2c$%x[%s],"LB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x84:{
			u16 v;
			u32 a=REG_(RS(_opcode))+IMM(_opcode);

			RW(a,v);
			REG_(RT(_opcode))=(s32)(s16)v;
			__R3000F(%s\x2c$%x[%s],"LH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x88:{
			u32 c,b,a=REG_(RS(_opcode))+IMM(_opcode);
			RL(a&~3,b);
			c=REG_(RT(_opcode));
			switch(a & 3){
				case 0:
					c = (c & 0xFFFFFF) | (b << 24);
				break;
				case 1:
					c = (c & 0xFFFF) | (b << 16);
				break;
				case 2:
					c = (c & 0xFF) | (b << 8);
				break;
				case 3:
					c = b;
				break;
			}
			REG_(RT(_opcode))=c;
			__R3000F(%s\x2c$%x[%s],"LWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x8c:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			RL(a,REG_(RT(_opcode)));
			ret++;
			__R3000F(%s\x2c$%x[%s],"LW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x90:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			RB(a,REG_(RT(_opcode)) );
			__R3000F(%s\x2c$%x[%s],"LBU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x94:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			RW(a,REG_(RT(_opcode)));
			ret++;
			__R3000F(%s\x2c$%x[%s],"LHU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0x98:{
			u32 c,b,a=REG_(RS(_opcode))+IMM(_opcode);
			RL(a & ~3,b);
			c=REG_(RT(_opcode));
			switch(a & 3){
				case 3:
					c = (c & 0xFFFFFF00) | (b >> 24);
				break;
				case 2:
					c = (c & 0xFFFF0000) | (b >> 16);
				break;
				case 1:
					c = (c & 0xFF000000) | (b >> 8);
				break;
				case 0:
					c = b;
				break;
			}
			REG_(RT(_opcode))=c;
			__R3000F(%s\x2c$%x[%s],"LWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xa0:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			WB(a,REG_(RT(_opcode)));
			__R3000F(%s\x2c$%x[%s],"SB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xa4:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			WW(a,REG_(RT(_opcode)));
			__R3000F(%s\x2c$%x[%s],"SH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xa8:{
			 u32 adr,value,reg;

			adr = REG_(RS(_opcode)) + IMM(_opcode);
			reg = REG_(RT(_opcode));
			RL(adr & ~3,value);
			switch(adr & 3){
				case 2:
					reg = (reg >> 8) | (value & 0xFF000000);
				break;
				case 1:
					reg = (reg >> 16) | (value & 0xFFFF0000);
				break;
				case 0:
					reg = (reg >> 24) | (value & 0xFFFFFF00);
				break;
			}
			WL(adr & ~3,reg);
			__R3000F(%s\x2c$%x[%s],"SWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xac:{
			u32 a=REG_(RS(_opcode))+IMM(_opcode);
			//if(a==528482548){ printf("%x\n",_pc);EnterDebugMode();}
			WL(a,REG_(RT(_opcode)));
			ret+=2;
			__R3000F(%s\x2c$%x[%s],"SW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xb8:{
			 u32 adr,value,reg;

			adr = REG_(RS(_opcode)) + IMM(_opcode);
			reg = REG_(RT(_opcode));
			RL(adr & ~3,value);
			switch(adr & 3){
				case 1:
					reg = (reg << 8) | (value & 0xFF);
				break;
				case 2:
					reg = (reg << 16) | (value & 0xFFFF);
				break;
				case 3:
					reg = (reg << 24) | (value & 0xFFFFFF);
				break;
			}
			WL(adr & ~3,reg);
			__R3000F(%s\x2c$%x[%s],"SWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		}
		break;
		case 0xc8:
			OnCop(SR(_opcode,26)&3,8,REG_(RS(_opcode))+IMM(_opcode),(u32 *)(u64)RT(_opcode));
			__R3000F(%d %d\x2c$%x[%s],"LWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xe8:
			OnCop(SR(_opcode,26)&3,9,REG_(RS(_opcode))+IMM(_opcode),(u32 *)(u64)RT(_opcode));
			__R3000F(%d %d\x2c$%x[%s],"SWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
	}
Z:
	_pc+=4;
	if(_jump && --_jump==0){
		_pc=_next_pc;
		if(!(_pc & 0x0fff0000) && !OnJump(_pc))
			_pc = REG_RA;
		if(_irq_pending){//fixme
			//EnterDebugMode();
			machine->OnEvent(0,(LPVOID)-1);
			//_irq=0;
		}
	}
Z1:
	return ret;
}

int R3000Cpu::Disassemble(char *dest,u32 *padr){
	u32 op,adr;
	char c[200],cc[100];
	u8 *p;

	*((u64 *)c)=0;
	*((u64 *)cc)=0;

	adr = *padr;
	RMAP_(adr,p,R);

	sprintf(c,"%08X ",adr);

	*((u64 *)cc)=0;
	op=_opcode;
	RLPC(adr,_opcode);
	sprintf(&c[8]," %8x ",_opcode);
	adr +=4;
	switch((_opcode >> 24) & 0xFC){
		default:
		break;
		case 0:
			switch(_opcode & 0x3f){
				case 0:
					switch(_opcode & 0x7c0){
						case 0:
							__R3000D(NOARG,"NOP");
						break;
						default:
							__R3000D(%s\x2c%s\x2c$%x,"SLL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
						break;
					}
				break;
				case 2:
					__R3000D(%s\x2c%s\x2c$%x,"SRL",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 3:
					__R3000D(%s\x2c%s\x2c$%x,"SRA",prs[RD(_opcode)],prs[RT(_opcode)],POS(_opcode));
				break;
				case 4:
					__R3000D(%s\x2c%s\x2c%s,"SLLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 6:
					__R3000D(%s\x2c%s\x2c%s,"SRLV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 7:
					__R3000D(%s\x2c%s\x2c%s,"SRAV",prs[RD(_opcode)],prs[RT(_opcode)],prs[RS(_opcode)]);
				break;
				case 0x8:
					__R3000D(%s,"JR",prs[RS(_opcode)]);
				break;
				case 0x9:
					__R3000D(%s,"JALR",prs[RS(_opcode)]);
				break;
				case 0xc:
					__R3000D(%x,"SYSCALL",SR(_opcode,6)&0xFFFFF);
				break;
				case 0xd:
					__R3000D(NOARG,"BREAK");
				break;
				case 0x10:
					__R3000D(%s,"MFHI",prs[RD(_opcode)]);
				break;
				case 0x11:
					__R3000D(%s,"MTLO",prs[RD(_opcode)]);
				break;
				case 0x12:
					__R3000D(%s,"MFLO",prs[RD(_opcode)]);
				break;
				case 0x13:
					__R3000D(%s,"MTHI",prs[RD(_opcode)]);
				break;
				case 0x18:
					__R3000D(%s\x2c%s,"MULT",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x19:
					__R3000D(%s\x2c%s,"MULTU",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x1a:
					__R3000D(%s\x2c%s,"DIV",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x1b:
					__R3000D(%s\x2c%s,"DIVU",prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x20:
					__R3000D(%s\x2c%s\x2c%s,"ADD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x21:
					__R3000D(%s\x2c%s\x2c%s,"ADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x22:
					__R3000D(%s\x2c%s\x2c%s,"SUB",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x23:
					__R3000D(%s\x2c%s\x2c%s,"SUBU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x24:
					__R3000D(%s\x2c%s\x2c%s,"AND",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x25:
					__R3000D(%s\x2c%s\x2c%s,"OR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x26:
					__R3000D(%s\x2c%s\x2c%s,"XOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x27:
					__R3000D(%s\x2c%s\x2c%s,"NOR",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2a:
					__R3000D(%s\x2c%s\x2c%s,"SLT",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x2b:
					__R3000D(%s\x2c%s\x2c%s,"SLTU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
			}
		break;
		case 0x4:switch(FT(_opcode)){
				case 0:
					__R3000D(%s\x2c$%x,"BLTZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
				break;
				case 1:
					__R3000D(%s\x2c$%x,"BGEZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
				break;
			}
		break;
		case 0x8:
			__R3000D(%X,"J",JUMP(adr,_opcode));
		break;
		case 0xc:
			__R3000D(%X,"JAL",JUMP(adr,_opcode));
		break;
		case 0x10:
			__R3000D(%s\x2c%s\x2c$%x,"BEQ",prs[RT(_opcode)],prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x14:
			__R3000D(%s\x2c%s\x2c$%x,"BNE",prs[RT(_opcode)],prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x18:
			__R3000D(%s\x2c$%x,"BLEZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x1C:
			__R3000D(%s\x2c$%x,"BGTZ",prs[RS(_opcode)],adr+(IMM(_opcode)<<2));
		break;
		case 0x20:
			__R3000D(%s\x2c%s\x2c$%x,"ADDI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x24:
			__R3000D(%s\x2c%s\x2c$%x,"ADDUI",prs[RT(_opcode)],prs[RS(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x28:
			__R3000D(%s\x2c%s\x2c$%x,"SLTI",prs[RT(_opcode)],prs[RS(_opcode)],IMM(_opcode));
		break;
		case 0x2C:
			__R3000D(%s\x2c%s\x2c$%x,"SLTIU",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x30:
			__R3000D(%s\x2c%s\x2c$%x,"ANDI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x34:
			__R3000D(%s\x2c%s\x2c$%x,"ORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x38:
			__R3000D(%s\x2c%s\x2c$%x,"XORI",prs[RT(_opcode)],prs[RS(_opcode)],(u32)IMMU(_opcode));
		break;
		case 0x3c:
			__R3000D(%s\x2c$%X,"LUI",prs[RT(_opcode)],(u16)IMM(_opcode));
		break;
		case 0x40:
			if(RS(_opcode) == 0 && (_opcode &0x04000000)){
				__R3000D(NOARG,"RFE");
				goto A;
			}
		case 0x44:
		case 0x48:
		case 0x4c:
			switch(RS(_opcode)){
				case 0:
					__R3000D(%d %s\x2c%s,"MFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 2:
					__R3000D(%d %s\x2c%s,"CFC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 4:
					__R3000D(%d %s\x2c%s,"MTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 6:
					__R3000D(%d %s\x2c%s,"CTC",SR(_opcode,26)&3,prs[RT(_opcode)],prs[RD(_opcode)]);
				break;
				case 1:
				case 3:
				case 5:
				break;
				default:
					__R3000D(%x,"COP",_opcode & 0xFFFFFF);
				break;
			}
		break;
		case 0x80:
			__R3000D(%s\x2c$%x[%s],"LB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x84:
			__R3000D(%s\x2c$%x[%s],"LH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x88:
			__R3000D(%s\x2c$%x[%s],"LWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x8c:
			__R3000D(%s\x2c$%x[%s],"LW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x90:
			__R3000D(%s\x2c$%x[%s],"LBU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x94:
			__R3000D(%s\x2c$%x[%s],"LHU",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0x98:
			__R3000D(%s\x2c$%x[%s],"LWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xa0:
			__R3000D(%s\x2c$%x[%s],"SB",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xa4:
			__R3000D(%s\x2c$%x[%s],"SH",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xa8:
			__R3000D(%s\x2c$%x[%s],"SWL",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xac:
			__R3000D(%s\x2c$%x[%s],"SW",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xb8:
			__R3000D(%s\x2c$%x[%s],"SWR",prs[RT(_opcode)],(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xc8:
			__R3000D(%d %d\x2c$%x[%s],"LWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
		case 0xe8:
			__R3000D(%d %d\x2c$%x[%s],"SWC",SR(_opcode,26)&3,RT(_opcode),(u16)IMM(_opcode),prs[RS(_opcode)]);
		break;
	}
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	*padr=adr;
	_opcode=op;
	return 0;
}

int R3000Cpu::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_REGISTER:
		{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					for(int i =0;i<sizeof(prs)/sizeof(prs[0]);i++){
						if(!strcasecmp(c,prs[i])){
							v=REG_(i);
							goto A;
						}
					}
						return -1;
				}
				else
					v=REG_(v);
A:
				p[1]=v;
			}
			return 0;
		case ICORE_QUERY_SET_REGISTER:
		{
			u32 v,*p=(u32 *)pv;
			v=p[0];
			if(v == (u32)-1){
				char *c;

				c=*((char **)&p[2]);
				for(int i =0;i<sizeof(prs)/sizeof(prs[0]);i++){
					if(!strcasecmp(c,prs[i])){
						REG_(i)=p[1];
						return 0;
					}
				}
				return -1;
			}
			else
				REG_(v)=p[1];
		}
			return 0;
		case ICORE_QUERY_NEXT_STEP:{
			switch(*((u32 *)pv)){
				case 1:{
				//	u32 op;

					//RLPC(_pc,op);
					*((u32 *)pv)=8;
				}
					return 0;
				case 2://f4
					*((u32 *)pv)=2;
					return 0;
				default:
					*((u32 *)pv)=4;
					return 0;
			}
			return -1;
		}
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

int R3000Cpu:: _enterIRQ(int n,u32 pc){
	return _jump==0 ? 0 : 1;
}

};