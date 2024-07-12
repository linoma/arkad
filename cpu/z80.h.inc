
#define RMAP_(a,b,c)

#define W_(a,b,c,d){\
	register void *tmp__= &_mem[(u16)(a)];\
	__data=(b);\
	if(!_portfnc_write_mem[(u16)a])\
		*((c *)tmp__)=__data;\
	else if((((CCore *)this)->*_portfnc_write_mem[(u16)a])(a,tmp__,&__data,AM_WRITE|d))\
		*((c *)tmp__)=__data;\
	ONMEMORYUPDATE(a,d|AM_WRITE,this);\
}

#define R_(a,b,c,d){\
	register void *tmp__= &_mem[(u16)(a)];\
	__data=*((c *)tmp__);\
	if(_portfnc_read_mem[(u16)a])\
		(((CCore *)this)->*_portfnc_read_mem[(u16)a])(a,tmp__,&__data,AM_READ|d);\
	(b)=__data;\
	ONMEMORYUPDATE(a,d|AM_READ,this);\
}

#define WB(a,b)	W_(a,b,u8,AM_BYTE)
#define WW(a,b)	W_(a,b,u16,AM_WORD)
#define WL(a,b)

#define RW(a,b) R_(a,b,u16,AM_WORD)
#define RB(a,b) R_(a,b,u8,AM_BYTE)
#define RL(a,b) (b)=*((u32 *)&_mem[(a)])

#define RLPC(a,b) (b)=*((u32 *)&_mem[(a)])
#define RBPC(a,b) (b)=_mem[(a)]

#define C_BIT		0x1
#define N_BIT		0x2
#define V_BIT		0x4
#define A_BIT		0x10
#define Z_BIT		0x40
#define S_BIT		0x80
#define H_BIT		A_BIT

#define IFF1_BIT	1
#define IFF2_BIT	2
#define NMI_BIT		4

#define REGI_B		0
#define REGI_C		1
#define REGI_D		2
#define REGI_E		3
#define REGI_H		4
#define REGI_L		5
#define REGI_F		6
#define REGI_A		7

#define REGI_SP		8
#define REGI_IX	 	10
#define REGI_IY		12

#define REGI_I		14
#define REG_I2		(REGI_I+2)
#define REGI_R		26

#define REGI_BC		0
#define REGI_DE		2
#define REGI_HL		4
#define REGI_AF		6

#define REG_IFF		REG_((REGI_I+1))

#define REG_R		REG_(REGI_R)

#define REGN_(a) 	_regs[(a)]
#define REG_(a) 	REGN_((a)^1)
#define REG16_(a) 	(*((u16 *)&_regs[a]))
#define REG_SP 		REG16_(REGI_SP)
#define REG_F  		REG_(REGI_F)
#define REG_A  		REG_(REGI_A)
#define REG_IX	 	REG16_(REGI_IX)
#define REG_IY	 	REG16_(REGI_IY)
#define REG_I		REG_(REGI_I)

#define EXIT_SF \
if(c__) REG_F |= C_BIT; else REG_F &=~C_BIT;\
if(z__) REG_F |= Z_BIT; else REG_F &=~Z_BIT;\
if(v__) REG_F |= V_BIT; else REG_F &=~V_BIT;\
if(s__) REG_F |= S_BIT; else REG_F &=~S_BIT;\
if(h__) REG_F |= A_BIT; else REG_F &=~A_BIT;

#define EXIT_SF16 EXIT_SF

#define PRE_SF16(a) { u8 c__,z__,v__,s__,h__;\
asm volatile ("pushq %%rcx\n"\
"movzw %0,%%rcx\n"\
: : "m" (a)  : "rcx","memory");

#define AFT_SF16(r)\
asm volatile  (\
"setzb %2\n"\
"setcb %1\n"\
"setob %3\n"\
"setsb %4\n"\
"pushf\n"\
"movw %%cx,%0\n"\
"pop %%rcx\n"\
"btw $4,%%cx\n"\
"setcb %5\n"\
"popq %%rcx\n"\
: "=m" (r) ,"=m"(c__),"=m"(z__),"=m"(v__),"=m"(s__),"=m"(h__) : : "rcx","memory","cc");\
EXIT_SF16\
}

#define STATUSFLAGS16(i,r,a,b)\
PRE_SF16(a)\
asm volatile (i : : "m" (b) :  "rcx","memory");\
AFT_SF16(r)

#define PRE_SF(a){ u8 c__,z__,v__,s__,h__;\
asm volatile ("pushq %%rcx\n" "movzb %0,%%rcx\n" : : "m" (a)  : "rcx","memory","cc");

#define AFT_SF(r)\
asm volatile  (\
"setzb %2\n"\
"setcb %1\n"\
"setob %3\n"\
"setsb %4\n"\
"pushf\n"\
"movb %%cl,%0\n"\
"pop %%rcx\n"\
"btw $4,%%cx\n"\
"setcb %5\n"\
"popq %%rcx\n" : "=m" (r) ,"=m"(c__),"=m"(z__),"=m"(v__),"=m"(s__),"=m"(h__) : : "rcx","memory","cc");\
EXIT_SF\
}

#define STATUSFLAGS(i,r,a,b)\
PRE_SF(a)\
asm volatile (i : : "m" (b) :  "rcx","memory","cc");\
AFT_SF(r)

#define PARITYFLAG(a){\
u8 __p;\
asm volatile("movzb %0,%%rax\n"	"test %%al,%%al\n" "setpeb %1\n" : "=m"(a) : "m"(__p) : "rax","cc");\
if(__p) REG_F |= V_BIT; else REG_F &= ~V_BIT;\
}

#define Z80ZSP(a){\
u8 __p,__z,__s;\
asm volatile("movzb %0,%%rax\n"\
"test %%al,%%al\n"\
"setpeb %1\n"\
"setzb %2\n"\
"setsb %3\n"\
 : : "m"(a),"m"(__p), "m"(__z),"m"(__s) : "rax","cc","memory");\
if(__p) REG_F |= V_BIT; else REG_F &= ~V_BIT;\
if(__z) REG_F |= Z_BIT; else REG_F &=~Z_BIT;\
if(__s) REG_F |= S_BIT; else REG_F &=~S_BIT;\
}

#ifndef __Z80CPUHINC__
//#define __Z80CPUHINC__

class Z80Cpu : public CCore{
public:
	Z80Cpu(void *p=NULL);
	virtual ~Z80Cpu();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(void *m=NULL);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
	virtual int SetMemIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
	virtual int Query(u32,void *);
	virtual int Dump(char **p){if(_machine) return ((CCore *)_machine)->Dump(p);return -1;};
	virtual int _dumpRegisters(char *p);
	virtual int _enterIRQ(int n,int v,u32 pc=0);
	virtual int _enterNMI();
	virtual int Restart();
protected:
	virtual int Disassemble(char *dest,u32 *padr);
	virtual int OnChangeIRQ(u32,u32 *)=0;
	int _exec(u32);
	u32 _opcode,_irq_pending;
	u8 *_ioreg;
	CoreMACallback *_portfnc_write_mem,*_portfnc_read_mem;
};

class Z80CpuShared : public Z80Cpu{
public:
	Z80CpuShared(void *m=NULL);
	virtual ~Z80CpuShared();
	virtual s32 fn_shared_mem_w(u32,pvoid ,pvoid ,u32 );
	virtual int OnChangeIRQ(u32,u32 *){return -1;};
	u8 *_ipc;
};

#endif