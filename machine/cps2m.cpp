#include "cps2m.h"


namespace cps2{

CPS2M::CPS2M() : Machine(80),M68000Cpu() {
}

CPS2M::~CPS2M(){
}

int CPS2M::Load(IGame *g,char *fn){
	FILE *fp;

	if(!g && !fn)
		return -1;
	Reset();
	if(!(fp=fopen(fn,"rb")))
		return -2;
	fseek(fp,0,SEEK_END);
	u32 sz=ftell(fp);
	fseek(fp,0,SEEK_SET);
	fread(_memory,sz,1,fp);
	printf("o: %s %u\n",fn,sz);
	fclose(fp);
	return 0;
}

int CPS2M::Destroy(){
	Machine::Destroy();
	M68000Cpu::Destroy();
	return 0;
}

int CPS2M::Reset(){
	Machine::Reset();
	M68000Cpu::Reset();
	return 0;
}
int CPS2M::Init(){
	Machine::Init();
	_mem=_memory;
	M68000Cpu::Init();
	return 0;
}

int CPS2M::Exec(u32 status){
	if(M68000Cpu::Exec(status))
		return 1;
	if(BVT(Machine::_status,1) || BT(status,S_DEBUG)){
		BVC(Machine::_status,1);
		return 3;
	}
	return 0;
}

int CPS2M::Query(u32 what,void *pv){
	switch(what){
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
			}
			return 0;
		default:
			return M68000Cpu::Query(what,pv);
	}
}

int CPS2M::Dump(char **pr){
	int res,i;
	char *c,*cc,*p,*pp;
	u8 *mem;
	u32 adr;

	if(pr)
		*pr=0;
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
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	strcpy(p,"3102");
	p+=5;
	*((u32 *)p)=0;
	p+=4;
	res+=9;

	*((u64 *)p)=0;
	adr=_dumpAddress;
	pp=&cc[900];

	mem=_mem+adr;
	_dumpMemory(p,mem,adr);
	res += strlen(p)+1;

	p= &c[res];
	*((u64 *)p)=0;
	*pr = c;
	return res;
}

M68000Cpu::M68000Cpu() : CCore(){
	_regs=NULL;
}

M68000Cpu::~M68000Cpu(){
	Destroy();
}

int M68000Cpu::Destroy(){
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
	CCore::Destroy();
	return 0;
}

int M68000Cpu::Reset(){
	_pc=0;
	_ccr=0;
	return 0;
}

int M68000Cpu::Init(){
	if(!(_regs = new u8[16*sizeof(u32)]))
		return -1;
	return 0;
}

int M68000Cpu::SetIO_cb(u32,CoreMACallback,CoreMACallback b){
	return -1;
}

int M68000Cpu::_exec(u32){
	int ret;

	RWPC(_pc,_opcode);
	switch(SR(_opcode,12)){
		case 0x4:
			switch((u8)SR(_opcode,4))
			{
				case 0x2a:
					switch(SR(_opcode,3)&7){
						case 5:
							u16 d;

						_pc+=2;
						RWPC(_pc,d);
						break;
					}
					__F(%x %x %x,"CLR",SR(_opcode,6)&3,SR(_opcode,3)&7,_opcode&7);
				break;
				case 0xe5:

				u16 d;
				RWPC(_pc+2,d);

				__F(%x %d,"LINK",_opcode&7,(s16)d);
				_pc+=2;
				break;
			}
		break;
		case 6:
			{
				s32 d =(u8)_opcode;
				if(d==0)
				d=0;
				else if(d==0xff)
d=0xFF;
				__F(%d %x,"B",SR(_opcode,8)&0xf,(u8)_opcode);
				switch(SR(_opcode,8)&0xf){
					case 0:
						_pc+=d;
					break;
					default:
					break;
				}
			}
		break;
		case 0x7:
			REG_(SR(_opcode,8)&7)=(u32)(s8)_opcode;
			__F(#%x\x2c D%d,"MOVEQ",(s8)_opcode,SR(_opcode,8)&7);
		break;
		default:
			__F(NOARG,"unk");
		break;
	}
	_pc+=2;
	return ret;
}

int M68000Cpu::Disassemble(char *dest,u32 *padr){
	return -1;
}

};