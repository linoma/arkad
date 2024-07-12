#define R_(a,b,c,d,e,f){\
	void *__tmp;register u32 __a=(a);\
	RMAP_##d((a),__tmp,c);\
	if(__tmp) __data=*((e *)__tmp);	else __data=0xffffffff;\
	if(ISM68IO((a))){\
		register u32 __a=M68IO(a);\
		if(_portfnc_read[__a])\
			(((CCore *)this)->*_portfnc_read[__a])(a,__tmp,&__data,AM_READ|f);\
	}\
	(b)=(e)__data;\
	ONMEMORYUPDATE(a,AM_READ|f,0);\
}

#define W_(a,b,c,d,e,f){\
	register void *__tmp;\
	RMAP_##d((a),__tmp,c);\
	__data=b;\
	if(ISM68IO((a))){\
		register u32 __a=M68IO(a);\
		if(_portfnc_write[__a]){\
			if(!(((CCore *)this)->*_portfnc_write[__a])(a,__tmp,&__data,AM_WRITE|f)){\
				__tmp=0;\
			}\
		}\
	}\
	if(__tmp) *((e *)__tmp)=(e)__data;\
	ONMEMORYUPDATE(a,f|AM_WRITE,0);\
}

#define RB_(a,b,c,d) R_(a,b,c,d,u8,AM_BYTE)
#define RW_(a,b,c,d) {u16 __v;R_(a,__v,c,d,u16,AM_WORD);(b)=(u16)SWAP16(__v);}
#define RL_(a,b,c,d) {u32 __v;R_(a,__v,c,d,u32,AM_DWORD);(b)=BELE32(__v);}

#define WB_(a,b,c,d) W_(a,b,c,d,u8,AM_BYTE)
#define WW_(a,b,c,d) {u16 __v=(b);__v=SWAP16(__v);W_(a,__v,c,d,u16,AM_WORD);}
#define WL_(a,b,c,d) {u32 __v=(b);__v=BELE32(__v);W_(a,__v,c,d,u32,AM_DWORD);}

#define RB(a,b) RB_(a,b,R, )
#define RW(a,b) RW_(a,b,R, )
#define RL(a,b) RL_(a,b,R, )
#define WB(a,b) WB_(a,b,W, )
#define WW(a,b)	WW_(a,b,W, )
#define WL(a,b) WL_(a,b,W, )

#define RWPC(a,b) {\
register void *tmp__;\
RMAP_PC((a),tmp__,R);\
__data=tmp__ ? *((u16 *)tmp__) : 0;\
__data=SWAP16(__data);\
(b)=__data;}

#define RLPC(a,b) {\
register void *tmp__;\
RMAP_PC((a),tmp__,R);\
__data=tmp__ ? *((u32 *)tmp__) : 0;\
__data=BELE32(__data);\
(b)=__data;}

#define WLPC(a,b) {\
register void *tmp__;\
__data=(b);\
RMAP_PC((a),tmp__,W);\
__data=BELE32(__data);\
if(tmp__) *((u32  *)tmp__)=__data;}

#define WwPC(a,b) {\
register void *tmp__;\
__data=(b);\
RMAP_PC((a),tmp__,W);\
__data=SWAP16(__data);\
if(tmp__) *((u16  *)tmp__)=__data;}

#define RWOP(a,b) RWPC(a,b)

#define C_BIT	BV(0)
#define V_BIT	BV(1)
#define Z_BIT	BV(2)
#define N_BIT	BV(3)
#define X_BIT	BV(4)
#define S_BIT	BV(13)
#define M_BIT	BV(12)

#define REG_(a) 	((u32 *)_regs)[(a)]
#define REG_D(a) 	REG_(a)
#define REG_A(a) 	REG_((a)+8)
#define REG_SP 		REG_A(7)
#define REG_USP		REG_(17)
#define REG_ISP		REG_(18)
#define REG_VBR		REG_(19)

#define PRE_SF(a) {\
u8 __c,__n,__z,__v;\
asm volatile ("pushq %%rcx\n" "xor %%rcx,%%rcx\n" "movl %0,%%ecx\n" : : "m" (a)  : "rcx","memory");

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
	virtual int _enterIRQ(int n,u32 pc=0);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
	virtual int Disassemble(char *dest,u32 *padr);
	virtual int _dumpRegisters(char *p);
	virtual int _dumpMemory(char *p,u8 *mem,LPDEBUGGERDUMPINFO pdi=NULL,u32 sz=0);
	virtual int OnException(u32,u32);
protected:
	int _exec(u32);

	int _empty_op();
	int _empty_op_dis(char *);
	int _lea();

	int _movemr();
	int _movemw();
	int _ext();
	int _ext_dis(char *);
	int _movemr_dis(char *);
	int _andi();
	int _andi_dis(char *);
	int _ori();
	int _ori_dis(char *);
	int _cmpi();
	int _cmpi_dis(char *);
	int _subi();
	int _subi_dis(char *);
	int _addi();
	int _addi_dis(char *);
	int _bset();
	int _bset_dis(char *);
	int _bseti();
	int _bseti_dis(char *);
	int _btst();
	int _btst_dis(char *);
	int _btsti();
	int _btsti_dis(char *);
	int _bclr();
	int _bclr_dis(char *);
	int _bclri();
	int _bclri_dis(char *);
	int _and();
	int _and_dis(char *);
	int _andisr();
	int _andisr_dis(char *);
	int _orisr();
	int _orisr_dis(char *);
	int _exg();
	int _exg_dis(char *);
	int _mulu();
	int _mulu_dis(char *);
	int _muls();
	int _muls_dis(char *);

	int _lsl();
	int _lsl_dis(char *);
	int _lsr();
	int _lsr_dis(char *);
	int _asl();
	int _asl_dis(char *);
	int _asr();
	int _asr_dis(char *);
	int _rol();
	int _rol_dis(char *);
	int _ror();
	int _ror_dis(char *);
	int _roxl();
	int _roxl_dis(char *);
	int _roxr();
	int _roxr_dis(char *);

	int _lsli();
	int _lsli_dis(char *);
	int _lsri();
	int _lsri_dis(char *);
	int _asli();
	int _asli_dis(char *);
	int _asri();
	int _asri_dis(char *);
	int _roli();
	int _roli_dis(char *);
	int _rori();
	int _rori_dis(char *);
	int _roxli();
	int _roxli_dis(char *);
	int _roxri();
	int _roxri_dis(char *);

	u32 lsl_(u32,u32,u32);
	u32 lsr_(u32,u32,u32);
	u32 asr_(u32,u32,u32);
	u32 asl_(u32,u32,u32);
	u32 rol_(u32,u32,u32);
	u32 ror_(u32,u32,u32);
	u32 roxr_(u32,u32,u32);
	u32 roxl_(u32,u32,u32);

	int _change_sr(u16);

	u16 _opcode,_ccr,*_io_regs;
	u32 _irq_pending;
private:
	u32 ipc,dipc;
};