#include "arkad.h"
#include "ccore.h"
#include "cpu.h.inc"
#include "gbagpu.h"
#include "gbaspu.h"

#ifndef __GBADEVH__
#define __GBADEVH__

namespace gba{

#undef IOREG__
#undef IOREG_
#undef IOREG

#define IOREG__(a,b) 		((u16 *)(a) + (SR((b),1) & 0x1ff))
#define IOREG_(a,b) 		*IOREG__(a,b)
#define IOREG(b) 			IOREG_(_ioreg,b)
#define IOREG32_(b,a) 		(*(u32 *)IOREG__(b,(a)))

#define MS_BIOS		KB(16)
#define MS_RAM		KB(256)
#define MS_WRAM		KB(32)
#define MS_IO		KB(8)
#define MS_CRAM		KB(1)
#define MS_VRAM		KB(96)
#define MS_ORAM		KB(1)
#define MS_PRAM		KB(1)
#define MS_SRAM		KB(128)
#define MS_ROM		MB(32)

#define MI_BIOS 	0
#define MI_RAM 		(MI_BIOS+MS_BIOS)
#define MI_WRAM 	(MI_RAM+MS_RAM)
#define MI_IO 		(MI_WRAM+MS_WRAM)
#define MI_CRAM 	(MI_IO+MS_IO)
#define MI_VRAM 	(MI_CRAM+MS_CRAM)
#define MI_ORAM 	(MI_VRAM+MS_VRAM)
#define MI_PRAM 	(MI_ORAM+MS_ORAM)
#define MI_SRAM 	(MI_PRAM+MS_PRAM)
#define MI_ROM 		(MI_SRAM+MS_SRAM)

#define M_RAM 		(&_mem[MI_RAM])
#define M_BIOS 		(&_mem[MI_BIOS])
#define M_ROM 		(&_mem[MI_ROM])

#define BIOSR(a)	a
#define BIOSW(a)	0

#define RMAP_(a,b,c) switch(a>>24){case 0:b=BIOS##c(&_mem[MI_BIOS|(a&(MS_BIOS-1))]);__bus|=MAROMASK;break;case 2:b=&_mem[MI_RAM + (a&(MS_RAM-1))];break;\
	case 3:b=&_mem[MI_WRAM + (a&(MS_WRAM-1))];break;\
	case 4:b=&_mem[MI_IO + (a&(MS_IO-1))];__bus |= MAIOMASK;break;case 5:b=&_mem[MI_PRAM + (a&(MS_PRAM-1))];__bus |= MAIOMASK;break;\
	case 6:b=&_mem[MI_VRAM + (a & ((a&0x10000) | (~(((u32)a << 15) & 0x80000000)) >> 16))];__bus |= MAIOMASK|MAAMASK;break;\
	case 7:b=&_mem[MI_ORAM + (a&(MS_ORAM-1))];__bus |= MAIOMASK;break;\
	case 8:case 9:b=&_mem[MI_ROM + (a&(MS_ROM-1))];;break;\
	case 0xa:case 0xb:case 0xc:case 0xd:b=NULL;__bus|=MAIOMASK|MA16ASK;break;\
	case 0xE:b=&_mem[MI_SRAM + (a&(MS_SRAM-1))];__bus |= MAIOMASK|MA8MASK;break;default:b=NULL;break;}

#define RMAP_PC(a,b,c) RMAP_(a,b,c)

#define ISIO(a)		(__bus & (MAIOMASK))
#define ISIOR(a)	((__bus & (MAIOMASK|AM_READ)) == (MAIOMASK|AM_READ))
#define ISIOW(a)	((__bus & (MAIOMASK|AM_WRITE)) ==  (MAIOMASK|AM_WRITE))
#define ISMAS(a)	(__bus & MAAMASK)

#define RMAPIO(a)	(SL(SR(a&0x0F000000,24)-4,10)|(a&0x3ff))

#define R_(a,b,c,d,e,f){\
	void *__tmp;\
	__address=a;__bus=f|AM_READ;\
	RMAP_##d((a),__tmp,c);\
	if(__tmp) __data=*((e *)__tmp);	else __data=0xffffffff;\
	if(ISIOR((a))){\
		if(ISMAS(a)){\
		} else{\
			u32 __a=RMAPIO(a);\
			if(_portfnc_read[__a])\
				(((CCore *)this)->*_portfnc_read[__a])(a,__tmp,&__data,AM_READ|f);\
		}\
	}\
	(b)=(e)__data;\
}

#define W_(a,b,c,d,e,f){\
	void *__tmp;__address=a;__bus=f|AM_WRITE;\
	__data=(b);\
	RMAP_##d((a),__tmp,c);\
	if(ISIOW((a))){\
		if(ISMAS(a)){\
			if(!(a & 1) && (__bus & AM_BYTE) ){\
               *((u16 *)(__tmp)) = (u16)(((u8)__data << 8) | (u8)__data);\
				__tmp=0;\
            }\
		} else{\
			u32 __a=RMAPIO(a);\
			if(_portfnc_write[__a]){\
				if(!(((CCore *)this)->*_portfnc_write[__a])(a,__tmp,&__data,AM_WRITE|f)){\
					__tmp=0;\
				}\
			}\
		}\
	}\
	if(__tmp) *((e *)__tmp)=(e)__data;\
}

#define RB_(a,b,c,d) R_(a,b,c,d,u8,AM_BYTE)
#define RW_(a,b,c,d) R_(((a)),b,c,d,u16,AM_WORD)
#define RL_(a,b,c,d) R_(((a)),b,c,d,u32,AM_DWORD)

#define WB_(a,b,c,d) W_(a,b,c,d,u8,AM_BYTE)
#define WW_(a,b,c,d) W_(((a)&~1),b,c,d,u16,AM_WORD)
#define WL_(a,b,c,d) W_(((a)&~3),b,c,d,u32,AM_DWORD)

#define RB(a,b) {RB_(a,b,R, );ONMEMORYUPDATE(a,AM_BYTE|AM_READ,0);}
#define RW(a,b) {RW_(a,b,R, );ONMEMORYUPDATE(a,AM_WORD|AM_READ,0);}
#define RL(a,b) {RL_(a,b,R, );ONMEMORYUPDATE(a,AM_DWORD|AM_READ,0);}
#define WB(a,b) {WB_(a,b,W, );ONMEMORYUPDATE(a,AM_BYTE|AM_WRITE,0);}
#define WW(a,b)	{WW_(a,b,W, );ONMEMORYUPDATE(a,AM_WORD|AM_WRITE,0);}
#define WL(a,b) {WL_(a,b,W, );ONMEMORYUPDATE(a,AM_DWORD|AM_WRITE,0);}

#define RLPC(a,b) {__bus=AM_DWORD|AM_READ;\
void *tmp__;\
RMAP_PC(((a)),tmp__,R);\
__data=tmp__ ? *((u32 *)tmp__) : 0;\
(b)=__data;}

#define RWPC(a,b) {__bus=AM_WORD|AM_READ;\
void *tmp__;\
RMAP_PC(((a)),tmp__,R);\
__data=tmp__ ? *((u16 *)tmp__) : 0;\
(b)=__data;}

#define RBPC(a,b) {__bus=AM_BYTE|AM_READ;\
void *tmp__;\
RMAP_PC((a),tmp__,R);\
__data=tmp__ ? *((u8 *)tmp__) : 0;\
(b)=__data;}

#define WLPC(a,b) {__bus=AM_DWORD|AM_WRITE;\
void *tmp__;\
__data=(b);\
RMAP_PC(((a)&~3),tmp__,W);\
if(tmp__) *((u32  *)tmp__)=__data;}

#define WWPC(a,b) {__bus=AM_WORD|AM_WRITE;\
void *tmp__;\
__data=(b);\
RMAP_PC(((a)&~1),tmp__,W);\
if(tmp__) *((u16  *)tmp__)=__data;}

#define WBPC(a,b) {__bus=AM_BYTE|AM_WRITE;\
void *tmp__;\
__data=(b);\
RMAP_PC((a),tmp__,W);\
if(tmp__) *((u8  *)tmp__)=__data;}

#define RLOP(a,b) RLPC(a,b)
#define RWOP(a,b) RWPC(a,b)

#define REG_DISPCNT		0
#define REG_DISPCNT1	(2*1)
#define REG_VCOUNT		(2*0x3)
#define REG_DISPSTAT   	(2*0x2)
#define REG_BG0CNT		(2*0x4)
#define REG_BG1CNT		(2*0x5)
#define REG_BG2CNT		(2*0x6)
#define REG_BG3CNT		(2*0x7)
#define REG_WININ      	(2*0x24)
#define REG_WINOUT   	(2*0x25)
#define REG_MOSAIC     	(0x4c)
#define REG_BLENDCNT 	(0x50)
#define REG_BLENDV     	(0x52)
#define REG_BLENDY     	(0x54)

#define REG_IF			(0x202)
#define REG_IE			(0x200)
#define REG_IME			(0x208)
#define REG_DMACNT(a)	(0xb8+(a*12))
#define REG_DMASRC(a)	(0xb0+(a*12))
#define REG_DMADST(a)	(0xb4+(a*12))
#define REG_TIMERD(a) 	(0x100+(a*4))
#define REG_TIMERCNT(a) (0x102+(a*4))
#define REG_KEYINPUT	0x130
#define REG_KEYCNT		0x132
#define REG_BGCNT(a)	((4+a)*2)
#define REG_BGXOFS(a)	((8+(a*2))*2)
#define REG_BGYOFS(a)	((9+(a*2))*2)

#define REG_BGRXOFS(a)	(((a*8) + 4)*2)
#define REG_BGRYOFS(a)	(((a*8)+ 4 + 2)*2)

#define REG_BGPA(a)		((a*8)*2)
#define REG_BGPB(a)		((1+a*8)*2)
#define REG_BGPC(a)		((2+a*8)*2)
#define REG_BGPD(a)		((3+a*8)*2)

#define REG_NR10     	0x60
#define REG_NR11     	0x62
#define REG_NR12     	0x63
#define REG_NR13     	0x64
#define REG_NR14     	0x65
#define REG_NR21     	0x68
#define REG_NR22     	0x69
#define REG_NR23     	0x6c
#define REG_NR24     	0x6d
#define REG_NR30     	0x70
#define REG_NR31     	0x72
#define REG_NR32     	0x73
#define REG_NR33     	0x74
#define REG_NR34     	0x75
#define REG_NR41     	0x78
#define REG_NR42     	0x79
#define REG_NR43     	0x7c
#define REG_NR44     	0x7d
#define REG_NR50     	0x80
#define REG_NR51     	0x81
#define REG_NR52     	0x84
#define REG_SOUNDCNT_H	0x82
#define REG_SOUNDBIAS	0x88

#define USER_MODE		    	0x10
#define FIQ_MODE		    	0x11
#define IRQ_MODE		    	0x12
#define SUPERVISOR_MODE			0x13
#define ABORT_MODE		    	0x17
#define UNDEFINED_MODE	    	0x1B
#define SYSTEM_MODE				0x1F
#define V_SHIFT            		28
#define C_SHIFT            		29
#define Z_SHIFT            		30
#define N_SHIFT            		31
#define Q_SHIFT            		27
#define T_SHIFT            		5
#define FIQ_SHIFT            	6
#define IRQ_SHIFT            	7
#define FIQ_BIT					BV(FIQ_SHIFT)
#define IRQ_BIT					BV(IRQ_SHIFT)
#define V_BIT		        	BV(V_SHIFT)
#define C_BIT		        	BV(C_SHIFT)
#define Z_BIT		        	BV(Z_SHIFT)
#define N_BIT		        	BV(N_SHIFT)
#define Q_BIT					BV(Q_SHIFT)
#define T_BIT		        	BV(T_SHIFT)

#define REG_(a) 				((u32 *)_pregs)[(a)]
#define REG_SPSR				REG_(16)

#define DEST_REG_INDEX         	((u16)_opcode >> 12)
#define BASE_REG_INDEX         	((_opcode >> 16) & 0xF)
#define DEST_REG		       	REG_(DEST_REG_INDEX)
#define BASE_REG		       	REG_(BASE_REG_INDEX)
#define LO_REG                 	DEST_REG
#define HI_REG                 	BASE_REG
#define OP_REG_INDEX			(_opcode & 0xF)
#define OP_REG			        REG_(OP_REG_INDEX)
#define SHFT_AMO_REG_INDEX 		((_opcode >> 8) & 0xF)
#define SHFT_AMO_REG	        REG_(SHFT_AMO_REG_INDEX)
#define IMM_SHIFT	            ((_opcode >> 7) & 0x1F)

class arm7 : public CCore{
public:
	arm7();
	virtual ~arm7();
	virtual int Destroy();
	virtual int Reset();
	virtual int Init(void *m=NULL,u32 ss=0,u32 f=0);
	virtual int SetIO_cb(u32,CoreMACallback,CoreMACallback b=NULL);

	virtual int Query(u32,void *);
	virtual int _enterIRQ(int n,int v,u32 pc=0);
	virtual int OnException(u32,u32);
	virtual int OnReturnFromException(u32);
	virtual int Dump(char **p){if(_machine) return ((CCore *)_machine)->Dump(p);return -1;};
	virtual int _dumpRegisters(char *p);
	virtual int Disassemble(char *dest,u32 *padr);

	int cpu_mode(u8 m,char *p,u32 **pr);
protected:
	virtual int _exec(u32);

	void switchmode(u8 mode,u8 to);

	int _empty_op();
	int _empty_op_dis(char *);

	int _bmi();
	int _bmi_dis(char *);
	int _blmi();
	int _blmi_dis(char *);
	int _swi();
	int _swi_dis(char *);

	int _bx();
	int _bx_dis(char *);

	int _mul();
	int _mul_dis(char *);
	int _muls();
	int _muls_dis(char *);
	int _mla();
	int _mla_dis(char *);
	int _mlas();
	int _mlas_dis(char *);
	int _mull();
	int _mull_dis(char *);
	int _mulls();
	int _mulls_dis(char *);
	int _mlal();
	int _mlal_dis(char *);
	int _mlals();
	int _mlals_dis(char *);
	int _mullu();
	int _mullu_dis(char *);
	int _mullus();
	int _mullus_dis(char *);
	int _mlalu();
	int _mlalu_dis(char *);
	int _mlalus();
	int _mlalus_dis(char *);

	int _and();
	int _and_imm();
	int _and_reg();
	int _and_dis(char *);
	int _ands();
	int _ands_imm();
	int _ands_reg();
	int _ands_dis(char *);

	int _orr();
	int _orr_imm();
	int _orr_reg();
	int _orr_dis(char *);
	int _orrs();
	int _orrs_imm();
	int _orrs_reg();
	int _orrs_dis(char *);

	int _eor();
	int _eor_imm();
	int _eor_reg();
	int _eor_dis(char *);
	int _eors();
	int _eors_imm();
	int _eors_reg();
	int _eors_dis(char *);

	int _bic();
	int _bic_imm();
	int _bic_reg();
	int _bic_dis(char *);
	int _bics();
	int _bics_imm();
	int _bics_reg();
	int _bics_dis(char *);

	int _sub();
	int _sub_imm();
	int _sub_reg();
	int _sub_dis(char *);
	int _subs();
	int _subs_imm();
	int _subs_reg();
	int _subs_dis(char *);

	int _subc();
	int _subc_imm();
	int _subc_reg();
	int _subc_dis(char *);
	int _subcs();
	int _subcs_imm();
	int _subcs_reg();
	int _subcs_dis(char *);

	int _rsb();
	int _rsb_imm();
	int _rsb_reg();
	int _rsb_dis(char *);
	int _rsbs();
	int _rsbs_imm();
	int _rsbs_reg();
	int _rsbs_dis(char *);

	int _rsbc();
	int _rsbc_imm();
	int _rsbc_reg();
	int _rsbc_dis(char *);
	int _rsbcs();
	int _rsbcs_imm();
	int _rsbcs_reg();
	int _rsbcs_dis(char *);

	int _add();
	int _add_imm();
	int _add_reg();
	int _add_dis(char *);
	int _adds();
	int _adds_imm();
	int _adds_reg();
	int _adds_dis(char *);

	int _adc();
	int _adc_imm();
	int _adc_reg();
	int _adc_dis(char *);
	int _adcs();
	int _adcs_imm();
	int _adcs_reg();
	int _adcs_dis(char *);

	int _tst();
	int _tst_imm();
	int _tst_reg();
	int _tst_dis(char *);
	int _tsts();
	int _tsts_imm();
	int _tsts_reg();
	int _tsts_dis(char *);

	int _teq();
	int _teq_imm();
	int _teq_reg();
	int _teq_dis(char *);
	int _teqs();
	int _teqs_imm();
	int _teqs_reg();
	int _teqs_dis(char *);

	int _cmp();
	int _cmp_imm();
	int _cmp_reg();
	int _cmp_dis(char *);

	int _cmn();
	int _cmn_imm();
	int _cmn_reg();
	int _cmn_dis(char *);

	int _mov();
	int _mov_imm();
	int _mov_reg();
	int _mov_dis(char *);
	int _movs();
	int _movs_imm();
	int _movs_reg();
	int _movs_dis(char *);

	int _mvn();
	int _mvn_imm();
	int _mvn_reg();
	int _mvn_dis(char *);
	int _mvs();
	int _mvns_imm();
	int _mvns_reg();
	int _mvns_dis(char *);

	int _msr_cpsr();
	int _msr_cpsr_dis(char *);
	int _msr_spsr();
	int _msr_spsr_dis(char *);
	int _mrs_cpsr();
	int _mrs_cpsr_dis(char *);
	int _mrs_spsr();
	int _mrs_spsr_dis(char *);
	int _mvs_dis(char *);

	int _ldr_postdownimm();
	int _ldr_postupimm();
	int _ldr_predownimm();
	int _ldr_preupimm();
	int _ldr_predownimmwb();
	int _ldr_preupimmwb();

	int _ldr_postdown();
	int _ldr_postup();
	int _ldr_predown();
	int _ldr_preup();
	int _ldr_predownwb();
	int _ldr_preupwb();

	int _ldr_dis(char *);
	int _sdt_dis(char *);

	int _str_postdownimm();
	int _str_postupimm();
	int _str_predownimm();
	int _str_preupimm();
	int _str_predownimmwb();
	int _str_preupimmwb();
	int _str_postdown();
	int _str_postup();
	int _str_predown();
	int _str_preup();
	int _str_predownwb();
	int _str_preupwb();

	int _str_dis(char *);

	int _ldrb_postdown();
	int _ldrb_postup();
	int _ldrb_predown();
	int _ldrb_preup();
	int _ldrb_predownwb();
	int _ldrb_preupwb();

	int _strb_postdown();
	int _strb_postup();
	int _strb_predown();
	int _strb_preup();
	int _strb_predownwb();
	int _strb_preupwb();

	int _ldrb_postdownimm();
	int _ldrb_postupimm();
	int _ldrb_predownimm();
	int _ldrb_preupimm();
	int _ldrb_predownimmwb();
	int _ldrb_preupimmwb();
	int _ldrb_dis(char *);

	int _strb_postdownimm();
	int _strb_postupimm();
	int _strb_predownimm();
	int _strb_preupimm();
	int _strb_predownimmwb();
	int _strb_preupimmwb();
	int _strb_dis(char *);

	int _ldrh_postdown();
	int _ldrh_postup();
	int _ldrh_predown();
	int _ldrh_preup();
	int _ldrh_predownwb();
	int _ldrh_preupwb();

	int _strh_postdown();
	int _strh_postup();
	int _strh_predown();
	int _strh_preup();
	int _strh_predownwb();
	int _strh_preupwb();

	int _ldrh_postdownimm();
	int _ldrh_postupimm();
	int _ldrh_predownimm();
	int _ldrh_preupimm();
	int _ldrh_predownimmwb();
	int _ldrh_preupimmwb();
	int _ldrh_dis(char *);

	int _strh_postdownimm();
	int _strh_postupimm();
	int _strh_predownimm();
	int _strh_preupimm();
	int _strh_predownimmwb();
	int _strh_preupimmwb();
	int _strh_dis(char *);

	int _ldrsb_postdownimm();
	int _ldrsb_postupimm();
	int _ldrsb_predownimm();
	int _ldrsb_preupimm();
	int _ldrsb_predownimmwb();
	int _ldrsb_preupimmwb();

	int _ldrsb_postdown();
	int _ldrsb_postup();
	int _ldrsb_predown();
	int _ldrsb_preup();
	int _ldrsb_predownwb();
	int _ldrsb_preupwb();

	int _ldrsb_dis(char *);

	int _ldrsh_postdownimm();
	int _ldrsh_postupimm();
	int _ldrsh_predownimm();
	int _ldrsh_preupimm();
	int _ldrsh_predownimmwb();
	int _ldrsh_preupimmwb();

	int _ldrsh_postdown();
	int _ldrsh_postup();
	int _ldrsh_predown();
	int _ldrsh_preup();
	int _ldrsh_predownwb();
	int _ldrsh_preupwb();

	int _ldrsh_dis(char *);

	int _stm();
	int _stm_dis(char *);
	int _ldm();
	int _ldm_dis(char *);

	int _tbl();
	int _tbl_dis(char *);
	int _tbu();
	int _tbu_dis(char *);
	int _tb();
	int _tb_dis(char *);
	int _tbx();
	int _tbx_dis(char *);

	int _tldm_ia();
	int _tldm_ia_dis(char *);
	int _tstm_ia();
	int _tstm_ia_dis(char *);

	int _tldrb_imm();
	int _tldrb_imm_dis(char *);
	int _tldrh_imm();
	int _tldrh_imm_dis(char *);
	int _tldr_imm();
	int _tldr_imm_dis(char *);
	int _tldr_reg();
	int _tldr_reg_dis(char *);

	int _tstrb_imm();
	int _tstrb_imm_dis(char *);
	int _tstrh_imm();
	int _tstrh_imm_dis(char *);
	int _tstr_imm();
	int _tstr_imm_dis(char *);


	int _tldrb_reg();
	int _tldrb_reg_dis(char *);
	int _tldrsb_reg();
	int _tldrsb_reg_dis(char *);
	int _tldrh_reg();
	int _tldrh_reg_dis(char *);
	int _tldrsh_reg();
	int _tldrsh_reg_dis(char *);

	int _tstrb_reg();
	int _tstrb_reg_dis(char *);
	int _tstrh_reg();
	int _tstrh_reg_dis(char *);
	int _tstr_reg();
	int _tstr_reg_dis(char *);

	int _tpush();
	int _tpush_dis(char *);
	int _tpop();
	int _tpop_dis(char *);
	int _tstr_sp();
	int _tstr_sp_dis(char *);
	int _tldr_sp();
	int _tldr_sp_dis(char *);

	int _tadd_sp();
	int _tadd_sp_dis(char *);
	int _tadd_sp_reg();
	int _tadd_sp_reg_dis(char *);
	int _tsub_sp();
	int _tsub_sp_dis(char *);

	int _tldr_pc();
	int _tldr_pc_dis(char *);
	int _tadd_pc_reg();
	int _tadd_pc_reg_dis(char *);

	int _tmov_imm();
	int _tmov_imm_dis(char *);
	int _tcmp_imm();
	int _tcmp_imm_dis(char *);
	int _tcmp_reg();
	int _tcmp_reg_dis(char *);

	int _tadd_reg();
	int _tadd_reg_dis(char *);
	int _tadd_imm();
	int _tadd_imm_dis(char *);
	int _tadd_short_imm();
	int _tadd_short_imm_dis(char *);
	int _tadd_hi();
	int _tadd_hi_dis(char *);

	int _tsub_reg();
	int _tsub_reg_dis(char *);
	int _tsub_imm();
	int _tsub_imm_dis(char *);
	int _tsub_short_imm();
	int _tsub_short_imm_dis(char *);
	int _tsub_hi();
	int _tsub_hi_dis(char *);

	int _tand_reg();
	int _tand_reg_dis(char *);
	int _tor_reg();
	int _tor_reg_dis(char *);
	int _tneg_reg();
	int _tneg_reg_dis(char *);
	int _teor_reg();
	int _teor_reg_dis(char *);
	int _tror_reg();
	int _tror_reg_dis(char *);
	int _tmul_reg();
	int _tmul_reg_dis(char *);
	int _tbic_reg();
	int _tbic_reg_dis(char *);
	int _tmvn_reg();
	int _tmvn_reg_dis(char *);
	int _tcmn_reg();
	int _tcmn_reg_dis(char *);
	int _ttst_reg();
	int _ttst_reg_dis(char *);
	int _tsbc_reg();
	int _tsbc_reg_dis(char *);
	int _tadc_reg();
	int _tadc_reg_dis(char *);
	int _tswi_imm();
	int _tswi_imm_dis(char *);
	int _tcmp_hi();
	int _tcmp_hi_dis(char *);
	int _tlsl_imm();
	int _tlsl_imm_dis(char *);
	int _tlsr_imm();
	int _tlsr_imm_dis(char *);
	int _tasr_imm();
	int _tasr_imm_dis(char *);

	int _tlsl_reg();
	int _tlsl_reg_dis(char *);
	int _tlsr_reg();
	int _tlsr_reg_dis(char *);
	int _tasr_reg();
	int _tasr_reg_dis(char *);

	int _tmov_hi();
	int _tmov_hi_dis(char *);

	int _treg_dis(char *);
	void _fillMultipleRegisterString(u8 *,char *);

	u32 _opcode,_irq_pending,*_ioreg,_cpsr;
	u8 *_pregs;
};

class gbadev : public arm7,public gbagpu,public gbaspu{
public:
	struct __dma;

	gbadev();
	virtual ~gbadev();
	virtual int Reset();
	virtual int Destroy();
	int Init(void *,void *);
	virtual int _enterIRQ(int n,u32 pc=0);
	virtual int Update(u32);
	int EnableBackupMemory(u32,u32);
	int dma_do(struct __dma *);
	int dma_do(u32);

	using CCore::_mem;
	using arm7::_ioreg;
protected:
	struct __timer{
		union{
			u16 _status;
			struct{
				unsigned int _enabled:1;
				unsigned int _irq:1;
				unsigned int _cascade:1;
				unsigned int _freq:4;
				unsigned int _changed:1;
				unsigned int _ovr:1;
				unsigned int _a:8;
				unsigned int _idx:2;
			};
		};
		int Init(int,void *,void *,u32);
		int reset();
		int update(int);
		int write(u32,u32);
		int read(u32,u32 *);
		u32 freq();
		protected:
		void *_ioreg;
		u8 *_mem;
		u16 _count,_reset,_remainder,_diff;
		s32 _value;
		u32 _cycles,_pllHz;
	} _timers[4];

	struct __dma{
		u32 _dst,_src,_count,_load;
		s8 _incD,_incS;
		union{
			u16 _status;
			struct{
				unsigned int _enabled:1;
				unsigned int _irq:1;
				unsigned int _repeat:1;
				unsigned int _reload:1;
				unsigned int _mode:1;
				unsigned int _start:2;
				unsigned int _changed:1;
				unsigned int _a:8;
				unsigned int _idx:2;
			};
		};
		int Init(int,void *,void *,u32);
		int reset();
		int update(int);
		int write(u32,u32);
		int read(u32,u32 *);
		protected:
		void *_ioreg;
		u8 *_mem;
	} _dmas[4];

	static struct __sramid{
		u16 ID;
		u16 addrTest;
		u8 mode;
	} SRAMID[8];

	struct __sram{
		u8 *_buffer;
		u32 size;
		u32 com;
		u16 block;
		u8 mode;
		u32 mask;

		int _idxId;
		__sramid *_id;

		int reset();
		int read(u32,u8 *);
		int write(u32,u8);
	} _sram;

	struct __packrom{
		u8 *_buffer;
		u32 size;
		void _free();
		__packrom(){_buffer=NULL;size=0;};
	};

	struct __eeprom{
		enum :u8 {eecNull,eecWrite,eecErase,eecSeek,eecNull2,eecUnlock,eecNull4,eecLock} eeCommand;

		u32 com,byteIndex;
		u16 blocco;
		u8 bitIndex,bytesWrite,sizeCommand,isUsed,bitCommand,mode;

		__packrom rom_pack[0x200];
		__eeprom(){reset();};

		int write(u32,u16);
		int read(u32,u16 *);
		int reset();

	} _eeprom[2];
};

};

#endif
