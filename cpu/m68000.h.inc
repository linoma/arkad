#include "cpu.h.inc"

#define MI_ROM 		MB(20)
#define MI_RAM 		(MI_ROM+MB(4))
#define MI_VRAM 	(MI_RAM+MB(4))

#define M_ROM 		(&_mem[MI_ROM])
#define M_RAM		(&_mem[MI_RAM])
#define M_VRAM		(&_mem[MI_VRAM])

#define ROM_(a) MA_(a,0,0x3fffff)
#define ROM_R(a,b) ROM_(a){b=&M_ROM[a&0x3fffff];}
#define ROM_W(a,b) ROM_(a){b=0;}

#define RAM_(a) MA_(a,0xff0000,0xffffff)
#define RAM_R(a,b) RAM_(a){b=M_RAM[(u16)a];}
#define RAM_W(a,b) RAM_(a){b=M_RAM[(u16)a];}

#define VRAM_(a) MA_(a,0x900000,0x92ffff)
#define VRAM_R(a,b) VRAM_(a){b=M_VRAM[(u16)a];}
#define VRAM_W(a,b) VRAM_(a){b=M_VRAM[(u16)a];}

#define RMAP_(a,b,c)\
ROM_##c(a,b)\
RAM_##c(a,b)\
VRAM_##c(a,b)

#define RB(a,b)
#define RW(a,b) (b)=SL(_mem[a],8)|_mem[a+1];
#define RL(a,b) {u32 __v=*((u32 *)&_mem[(a)]);asm("bswap %0\n":"=r"(__v):"r"(__v));(b)=__v;}
#define WB(a,b)
#define WW(a,b)
#define WL(a,b)  {u32 __v=(b);asm("bswap %0\n":"=r"(__v):"r"(__v));*((u32 *)&_mem[(a)])=__v;}
#define RWPC(a,b) RW(a,b)

#define C_BIT	BV(0)
#define V_BIT	BV(1)
#define Z_BIT	BV(2)
#define N_BIT	BV(4)
#define X_BIT	BV(5)

#define REG_(a) 	(*((u32 *)_regs + (a)))
#define REG_D(a) 	REG_(a)
#define REG_A(a) 	REG_((a)+8)
#define REG_SP 		REG_A(8)
#define REG_USP		REG_A(7)
#define REG_VBR		REG_(17)

#define PRE_SF(a)\
{\
u8 __c,__n,__z,__v;\
asm volatile ("pushq	%%rcx\n" "xor %%rcx,%%rcx\n" "movl %0,%%ecx\n" : : "m" (a)  : "rcx","memory");

#define AFT_SF(r)\
asm volatile  (\
"setsb %2\n"\
"setzb %3\n"\
"setcb %1\n"\
"setob %4\n"\
"movl %%ecx,%0\n"\
"popq %%rcx\n"\
: "=m" (r) ,"=m"(__c),"=m"(__n),"=m"(__z),"=m"(__v) : : "rcx","memory");\
if(__c) _ccr |= C_BIT; else _ccr &=~C_BIT;\
if(__z) _ccr |= Z_BIT; else _ccr &=~Z_BIT;\
if(__n) _ccr |= N_BIT; else _ccr &=~N_BIT;\
if(__v) _ccr |= V_BIT; else _ccr &=~V_BIT;\
}

#define STATUSFLAGS(i,r,a,b)\
PRE_SF(a)\
asm volatile  (i : : "m" (b) :  "rcx","memory");\
AFT_SF(r)

class  M68000Cpu : public CCore{
public:
	M68000Cpu();
	virtual ~M68000Cpu();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(void *);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
	int Disassemble(char *dest,u32 *padr);
protected:
	int _exec(u32);
	u16 _opcode,_ccr;
};