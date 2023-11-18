#include "cpu.h.inc"


#define REG_ ((u32 *)_regs)
#define RT(OP)         ((OP >> 16) & 0x1F)
#define RS(OP)         ((OP >> 21) & 0x1F)
#define RD(OP)         ((OP >> 11) & 0x1F)
#define FT(OP)         ((OP >> 16) & 0x1F)
#define FS(OP)         ((OP >> 11) & 0x1F)
#define FD(OP)         ((OP >> 6)  & 0x1F)
#define SA(OP)         ((OP >> 6)  & 0x1F)
#define IMM(OP)        (int)(short)OP
#define IMMU(OP)       (unsigned long)(unsigned short)OP
#define JUMP(PC,OP)    ((PC & 0xF0000000) | ((OP & 0x3FFFFFF) << 2))
#define CODE(OP)       ((OP >> 6) & 0xFFFFF)
#define SIZE(OP)       ((OP >> 11) & 0x1F)
#define POS(OP)        ((OP >> 6) & 0x1F)
#define MASK(a)        ((1 << a) - 1)
#define VO(OP)         (((OP & 3) << 5) | ((OP >> 16) & 0x1F))
#define VCC(OP)        ((OP >> 18) & 7)
#define VD(OP)         (OP & 0x7F)
#define VS(OP)         ((OP >> 8) & 0x7F)
#define VT(OP)         ((OP >> 16) & 0x7F)
#define VED(OP)        (OP & 0xFF)
#define VES(OP)        ((OP >> 8) & 0xFF)
#define VCN(OP)        (OP & 0x0F)
#define VI3(OP)        ((OP >> 16) & 0x07)
#define VI5(OP)        ((OP >> 16) & 0x1F)
#define VI8(OP)        ((OP >> 16) & 0xFF)

class  R3000Cpu : public CCore{
public:
	R3000Cpu();
	virtual ~R3000Cpu();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(void *m=NULL);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);
protected:
	int _exec(u32);
	u32 _opcode;
};

