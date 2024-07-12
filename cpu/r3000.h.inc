#include "cpu.h.inc"

#ifndef MIPSCORE
#define MIPSCORE R3000
#endif

#ifndef CORE_REG_SIZE
#define CORE_REG_SIZE	32
#endif

#ifndef MIPS
#define MIPS	1
#endif

#define MIPSCORECPU		CONCAT(MIPSCORE,Cpu)
#define RSZU			CONCAT(u,CORE_REG_SIZE)
#define RSZS			CONCAT(s,CORE_REG_SIZE)

#ifndef CORE_REG_STEP
#define CORE_REG_STEP	1
#endif

#define REG_(a) 		(((u32 *)MIPSCORECPU::_regs)[(a)*CORE_REG_STEP])

#define RT(OP)         	((OP >> 16) & 0x1F)
#define RS(OP)         	((OP >> 21) & 0x1F)
#define RD(OP)         	((OP >> 11) & 0x1F)
#define FT(OP)         	((OP >> 16) & 0x1F)
#define FS(OP)         	((OP >> 11) & 0x1F)
#define FD(OP)         	((OP >> 6)  & 0x1F)
#define SA(OP)         	((OP >> 6)  & 0x1F)
#define IMM(OP)        	(s32)(s16)OP
#define IMMU(OP)       	((u32)(u16)OP)
#define JUMP(PC,OP)    	((PC & 0xF0000000) | ((OP & 0x3FFFFFF) << 2))
//#define CODE(OP)       	((OP >> 6) & 0xFFFFF)
#define SIZE(OP)       	((OP >> 11) & 0x1F)
#define POS(OP)        	((OP >> 6) & 0x1F)
#define MASK(a)        	((1 << a) - 1)
#define VO(OP)         	(((OP & 3) << 5) | ((OP >> 16) & 0x1F))
#define VCC(OP)        	((OP >> 18) & 7)
#define VD(OP)         	(OP & 0x7F)
#define VS(OP)         	((OP >> 8) & 0x7F)
#define VT(OP)         	((OP >> 16) & 0x7F)
#define VED(OP)        	(OP & 0xFF)
#define VES(OP)        	((OP >> 8) & 0xFF)
#define VCN(OP)        	(OP & 0x0F)
#define VI3(OP)        	((OP >> 16) & 0x07)
#define VI5(OP)        	((OP >> 16) & 0x1F)
#define VI8(OP)        	((OP >> 16) & 0xFF)

#define REGI_GP		28
#define REGI_FP		30
#define REGI_SP		29
#define REGI_RA		31
#define REGI_A0 	4
#define REGI_V0 	2
#define REGI_S0 	16
#define REGI_T0 	8
#define REGI_LO 	32
#define REGI_HI 	33

#define REG_SP		REG_(REGI_SP)
#define REG_GP		REG_(REGI_GP)
#define REG_RA		REG_(REGI_RA)
#define REG_LO		REG_(REGI_LO)
#define REG_HI		REG_(REGI_HI)

#define W_(a,b,c,d){\
	register void *tmp__;\
	__data=(b);\
	RMAP_((a),tmp__,NOARG);\
	if(ISIO(a)){\
		register u32 a__ = RMAP_IO(a);\
		if(_portfnc_write[a__]){\
			if(!(((CCore *)this)->*_portfnc_write[a__])(a,tmp__,&__data,d))\
				tmp__=NULL;\
		}\
	}\
	if(tmp__) *((c *)tmp__)=(b);\
	ONMEMORYUPDATE(a,d|AM_WRITE,(CCore *)this);\
}

#define WB(a,b)	W_(a,b,u8,AM_BYTE)
#define WW(a,b)	W_(a,b,u16,AM_WORD)
#define WL(a,b) W_(a,b,u32,AM_DWORD)
#define WP(a,b) W_(a,b,u64,AM_LWORD)
#define WQ(a,b) W_(a,b,u128,AM_QWORD)

#define R_(a,b,c,d){\
	register void *tmp__;\
	RMAP_(a,tmp__,NOARG);\
	__data=tmp__ ? *((c *)tmp__) : 0;\
	RIO(a,tmp__,__data,d);\
	(b)=__data;\
	ONMEMORYUPDATE(a,d|AM_READ,(CCore *)this);\
}

#define RW(a,b) R_(a,b,u16,AM_WORD)
#define RB(a,b) R_(a,b,u8,AM_BYTE)
#define RL(a,b) R_(a,b,u32,AM_DWORD)
#define RP(a,b) R_(a,b,u64,AM_LWORD)
#define RQ(a,b) R_(a,b,u128,AM_QWORD)

#define RLPC(a,b){\
	register void *tmp__;\
	RMAP_(a,tmp__,PC);\
	(b)=*(u32 *)tmp__;\
}

#define RBPC(a,b) (b)=_mem[(a)]

class  MIPSCORECPU : public CCore{
public:
	MIPSCORECPU();
	virtual ~MIPSCORECPU();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(void *m=NULL);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
	virtual int _dumpRegisters(char *p);
	virtual int Disassemble(char *dest,u32 *padr);
	virtual int Query(u32 what,void *pv);
	virtual int _enterIRQ(int n,u32 pc=0);
protected:
	int _exec(u32);

	virtual int OnJump(u32) = 0;
	virtual int OnCop(RSZU,RSZU,RSZU,RSZU *) = 0;
	virtual int OnException(u32,u32) = 0;

	u32 _opcode,_jump,_next_pc,_irq_pending,*_io_regs;
};