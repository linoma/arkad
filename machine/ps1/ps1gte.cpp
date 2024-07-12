#include "ps1m.h"

namespace ps1{

#define R    	_regs8[6][0]
#define G    	_regs8[6][1]
#define B    	_regs8[6][2]
#define R0    	_regs8[20][0]
#define G0    	_regs8[20][1]
#define B0    	_regs8[20][2]
#define R1    	_regs8[21][0]
#define G1    	_regs8[21][1]
#define B1    	_regs8[21][2]
#define R2    	_regs8[22][0]
#define G2    	_regs8[22][1]
#define B2    	_regs8[22][2]

#define CODE 	_regs8[6][3]
#define CD2 	_regs8[22][3]
#define OTZ  	_regs16[7][0]

#define IR0  	_sregs16[8][0]
#define IR1  	_sregs16[9][0]
#define IR2  	_sregs16[10][0]
#define IR3  	_sregs16[11][0]
#define SXY0 	_regs[12]
#define SX0  	_sregs16[12][0]
#define SY0  	_sregs16[12][1]
#define SXY1 	_regs[13]
#define SX1		_sregs16[13][0]
#define SY1  	_sregs16[13][1]
#define SXY2 	_regs[14]
#define SX2  	_sregs16[14][0]
#define SY2  	_sregs16[14][1]
#define SXYP 	_regs[15]
#define SXP  	_sregs16[15][0]
#define SYP  	_sregs16[15][1]
#define SZ0  	_regs16[16][0]
#define SZ1  	_regs16[17][0]
#define SZ2  	_regs16[18][0]
#define SZ3  	_regs16[19][0]
#define RGB0 	_regs[20]
#define RGB1 	_regs[21]
#define RGB2 	_regs[22]
#define LZCR 	_regs[31]
#define FLAG 	ctrl._regs[31]
#define MAC0	_regs[24]
#define MAC1	_sregs[25]
#define MAC2	_sregs[26]
#define MAC3	_sregs[27]

#define TRX		ctrl._sregs[5]
#define TRY		ctrl._sregs[6]
#define TRZ		ctrl._sregs[7]
#define RFC		ctrl._sregs[21]
#define GFC		ctrl._sregs[22]
#define BFC		ctrl._sregs[23]
#define OFX 	ctrl._sregs[24]
#define OFY 	ctrl._sregs[25]
#define H 		ctrl._regs16[26][0]
#define DQA 	ctrl._sregs16[27][0]
#define DQB 	ctrl._sregs[28]
#define ZSF3 	ctrl._sregs16[ 29 ][0]
#define ZSF4 	ctrl._sregs16[ 30 ][0]

#define VX(n) (n < 3 ? _sregs16[ n << 1 ][0] : IR1)
#define VY(n) (n < 3 ? _sregs16[ n << 1 ][1] : IR2)
#define VZ(n) (n < 3 ? _sregs16[ (n << 1) + 1 ][0] : IR3)
#define MX11(n) (n < 3 ? ctrl._sregs16[(n << 3) ][0] : -R << 4)
#define MX12(n) (n < 3 ? ctrl._sregs16[(n << 3) ][1] : R << 4)
#define MX13(n) (n < 3 ? ctrl._sregs16[(n << 3) + 1 ][0] : IR0)
#define MX21(n) (n < 3 ? ctrl._sregs16[(n << 3) + 1 ][1] : R13)
#define MX22(n) (n < 3 ? ctrl._sregs16[(n << 3) + 2 ][0] : R13)
#define MX23(n) (n < 3 ? ctrl._sregs16[(n << 3) + 2 ][1] : R13)
#define MX31(n) (n < 3 ? ctrl._sregs16[(n << 3) + 3 ][0] : R22)
#define MX32(n) (n < 3 ? ctrl._sregs16[(n << 3) + 3 ][1] : R22)
#define MX33(n) (n < 3 ? ctrl._sregs16[(n << 3) + 4 ][0] : R22)
#define CV1(n) (n < 3 ? ctrl._sregs[(n << 3) + 5] : 0)
#define CV2(n) (n < 3 ? ctrl._sregs[(n << 3) + 6] : 0)
#define CV3(n) (n < 3 ? ctrl._sregs[(n << 3) + 7] : 0)

#define R11 	MX11(0)
#define R12 	MX12(0)
#define R13 	MX13(0)
#define R21 	MX21(0)
#define R22 	MX22(0)
#define R23 	MX23(0)
#define R31 	MX31(0)
#define R32 	MX32(0)
#define R33 	MX33(0)

#define L11	MX11(1)
#define L12 MX12(1)
#define L13 MX13(1)
#define L21 MX21(1)
#define L22 MX22(1)
#define L23 MX23(1)
#define L31 MX31(1)
#define L32 MX32(1)
#define L33 MX33(1)

#define RBK ctrl._sregs[13]
#define GBK ctrl._sregs[14]
#define BBK ctrl._sregs[15]

#define LR1 MX11(2)
#define LR2 MX12(2)
#define LR3 MX13(2)
#define LG1 MX21(2)
#define LG2 MX22(2)
#define LG3 MX23(2)
#define LB1 MX31(2)
#define LB2 MX32(2)
#define LB3 MX33(2)

#define GTE_OVR31(a)	GTE_OVR<s32,31,0,1>(a,0,0)
#define GTE_OVRF		BV(31)
#define GTE_OVR44H(a) 	(GTE_OVRF|BV((31-a)))
#define GTE_OVR44L(a) 	(GTE_OVRF|BV((28-a)))
#define GTE_OVR32H 		(GTE_OVRF|BV(16))
#define GTE_OVR32L 		(GTE_OVRF|BV(15))

#define GTE_OVR16F(a)	BV((25-a))
#define GTE_OVR16H(a) 	(GTE_OVRF|GTE_OVR16F(a))
#define GTE_OVR16U		(GTE_OVRF|BV(18))
#define GTE_OVRDIV		(GTE_OVRF|BV(17))

#define GTE_OVR8U(a)	BV(22-a)
#define GTE_OVR12U		(BV(12))
#define GTE_OVR12S(a)	(GTE_OVRF|BV(15-a))
//0x80680000
__gte::__gte(){
	for(int i =0;i<256;i++){
		u8 n=(0x40000/(i+0x100)+1)/2-0x101;
		_div_table[i]=n;
	}
	_div_table[256]=0;
}

int __gte::_reset(){
	memset(ctrl._regs,0,sizeof(ctrl._regs));
	memset(_regs,0,sizeof(_regs));
	return 0;
}

int __gte::_op(u32 op,PS1M &g){
	s64  a,b,c;
	u8 sh;

	//EnterDebugMode();
	FLAG=0;
	_opcode=op;
	sh=SL(_sh,3)+SL(_sh,2);
	//if(_cmd==1)
	//printf("GTE sh:%x lm:%x mx:%x vx:%x tx:%x CMD:%x\n",_sh,_lm,_mx,_vx,_tx,_cmd);
	switch(_cmd){
		case 1:{//RTPS
			c=_rtp(0,sh);
			a=GTE_OVR<s64,GTE_MAX_32BIT,GTE_MIN_32BIT,0>((s64)DQB + ((s64)DQA * c),GTE_OVR32H,GTE_OVR32L);
			MAC0 = a;
			IR0=GTE_OVR<s32,0x1000,0,1>(SR(a,12),GTE_OVR12U,GTE_OVR12U);
			//SX2
			//SY2
			//(s64) OFX + (s64)(IR1 * c)
			//(s64) OFY + (s64)(IR2 * c)
			//SZ3
			//u32 p[]={0x1f801810,0,AM_WORD};
			//g._drawVertex(0);
		//	g.Query(ICORE_QUERY_MEMORY_WRITE,p);
		}
		break;
		case 6:
			MAC0 = GTE_OVR<s64,GTE_MAX_32BIT,GTE_MIN_32BIT,0>((s64) (SX0 * SY1) + (SX1 * SY2) + (SX2 * SY0) - (SX0 * SY2) - (SX1 * SY0) - (SX2 * SY1),(GTE_OVRF|BV(16)),(GTE_OVRF|BV(15)));
		break;
		case 12:
			a = GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (R22 * IR3) - (R33 * IR2),GTE_OVR44H(1),GTE_OVR44L(1));
			b = GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (R33 * IR1) - (R11 * IR3),GTE_OVR44H(2),GTE_OVR44L(2));
			c = GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (R11 * IR2) - (R22 * IR1),GTE_OVR44H(3),GTE_OVR44L(3));

			MAC1=(s32)SR(a,sh);
			MAC2=(s32)SR(b,sh);
			MAC3=(s32)SR(c,sh);//fixme

			_cmac_store();
		break;
		case 0x10:
			a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)RFC << 12) - (R << 16),GTE_OVR44H(1),GTE_OVR44L(1));
			a=SR(a,sh);
			a=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(a,GTE_OVR16H(1),GTE_OVR16H(1));
			a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)R << 16)+(IR0*a),GTE_OVR44H(1),GTE_OVR44L(1));

			b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)GFC << 12) - (G << 16),GTE_OVR44H(2),GTE_OVR44L(2));
			b=SR(b,sh);
			b=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(b,GTE_OVR16H(2),GTE_OVR16H(2));
			b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)G << 16)+(IR0*b),GTE_OVR44H(2),GTE_OVR44L(2));

			c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)BFC << 12) - (B << 16),GTE_OVR44H(3),GTE_OVR44L(3));
			c=SR(c,sh);
			c=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(c,GTE_OVR16F(3),GTE_OVR16F(3));
			c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)B << 16)+(IR0*c),GTE_OVR44H(3),GTE_OVR44L(3));

			MAC1=(s32)SR(a,sh);
			MAC2=(s32)SR(b,sh);
			MAC3=(s32)SR(c,sh);//fixme

			_cmac_store();
			_cfifo();
		break;
		case 0x11:{
			a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)RFC << 12) - (IR1 << 12),GTE_OVR44H(1),GTE_OVR44L(1));
			a=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(SR(a,sh),GTE_OVR16H(1),GTE_OVR16H(1));
			a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)IR1 << 12)+(IR0*a),GTE_OVR44H(1),GTE_OVR44L(1));

			b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)GFC << 12) - (IR2 << 12),GTE_OVR44H(2),GTE_OVR44L(2));
			b=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(SR(b,sh),GTE_OVR16H(2),GTE_OVR16H(2));
			b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)IR2 << 12)+(IR0*b),GTE_OVR44H(2),GTE_OVR44L(2));

			c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)BFC << 12) - (IR3 << 12),GTE_OVR44H(3),GTE_OVR44L(3));
			c=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(SR(c,sh),GTE_OVR16F(3),GTE_OVR16F(3));
			c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)IR3 << 12)+(IR0*c),GTE_OVR44H(3),GTE_OVR44L(3));

			MAC1=(s32)SR(a,sh);
			MAC2=(s32)SR(b,sh);
			MAC3=(s32)SR(c,sh);//fixme

			_cmac_store();
			_cfifo();
		}
		break;
		case 0x12:
			switch(_tx){
				case 2:
					a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (MX12(_mx) * VY(_vx)) + (MX13(_mx) * VZ(_vx)),GTE_OVR44H(1),GTE_OVR44L(1));
					MAC1 =SR(a,sh);
					b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (MX22(_mx) * VY(_vx)) + (MX23(_mx) * VZ(_vx)),GTE_OVR44H(2),GTE_OVR44L(2));
					MAC2 =SR(b,sh);
					c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (MX32(_mx) * VY(_vx)) + (MX33(_mx) * VZ(_vx)),GTE_OVR44H(3),GTE_OVR44L(3));
					MAC3 =SR(c,sh);

					a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64) CV1(2) << 12) + (MX11(_mx) * VX(_vx)),GTE_OVR44H(1),GTE_OVR44L(1));
					b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64) CV2(2) << 12) + (MX21(_mx) * VX(_vx)),GTE_OVR44H(2),GTE_OVR44L(2));
					c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64) CV3(2) << 12) + (MX31(_mx) * VX(_vx)),GTE_OVR44H(3),GTE_OVR44L(3));

					GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,0>(SR(a,sh),GTE_OVR16H(1),GTE_OVR16H(1));
					GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,0>(SR(b,sh),GTE_OVR16H(2),GTE_OVR16H(2));
					GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,0>(SR(c,sh),GTE_OVR16F(3),GTE_OVR16F(3));
				break;
				default:
					a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64)((s64) CV1(_tx) << 12) + (MX11(_mx) * VX(_vx)) + (MX12(_mx) * VY(_vx)) + (MX13(_mx) * VZ(_vx))
						,GTE_OVR44H(1),GTE_OVR44L(1));
					MAC1=SR(a,sh);
					b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64) CV2(_tx) << 12) + (MX21(_mx) * VX(_vx)) + (MX22(_mx) * VY(_vx)) + (MX23(_mx) * VZ(_vx))
						,GTE_OVR44H(2),GTE_OVR44L(2));
					MAC2=SR(b,sh);
					c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64)((s64) CV3(_tx) << 12) + (MX31(_mx) * VX(_vx)) + (MX32(_mx) * VY(_vx)) + (MX33(_mx) * VZ(_vx))
						,GTE_OVR44H(3),GTE_OVR44L(3));
					MAC3=SR(c,sh);
				break;
			}
			_cmac_store();
		break;
		case 0x13:
			_ncd(0,sh);
		break;
		case 0x14:
			_cdp(sh);
		break;
		case 0x16:
			for(int  i=0;i<3;i++) _ncd(i,sh);
		break;
		case 0x1b:
			_cv(0,sh);
			_bkcdp(sh);
			_cinterp(sh);
			_cmac_store();
			_cfifo();
		break;
		case 0x1c:
			_bkcdp(sh);
			_cinterp(sh);
			_cmac_store();
			_cfifo();
		break;
		case 0x1e:
			_cv(0,sh);
			_bkcdp(sh);
			_cfifo();
		break;
		case 0x20:
			for(int  i=0;i<3;i++){
				_cv(i,sh);
				_bkcdp(sh);
				_cfifo();
			}
		break;
		case 0x28:
			a = GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (IR1 * IR1),GTE_OVR44H(1),GTE_OVR44L(1));
			b = GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (IR2 * IR2),GTE_OVR44H(2),GTE_OVR44L(2));
			c = GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>((s64) (IR3 * IR3),GTE_OVR44H(3),GTE_OVR44L(3));

			MAC1=(s32)SR(a,sh);
			MAC2=(s32)SR(b,sh);
			MAC3=(s32)SR(c,sh);//fixme
			_cmac_store();
		break;
		case 0x29:
			_cinterp(0);
			_fccdp(sh);
		//	_cmac_store();
			_cfifo();
		break;
		case 0x2a:
			for(int i=0;i<3;i++){
			a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)RFC << 12) - (R0 << 16),GTE_OVR44H(1),GTE_OVR44L(1));
			a=SR(a,sh);
			a=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(a,GTE_OVR16H(1),GTE_OVR16H(1));
			a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)R0 << 16)+(IR0*a),GTE_OVR44H(1),GTE_OVR44L(1));

			b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)GFC << 12) - (G0 << 16),GTE_OVR44H(2),GTE_OVR44L(2));
			b=SR(b,sh);
			b=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(b,GTE_OVR16H(2),GTE_OVR16H(2));
			b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)G0 << 16)+(IR0*b),GTE_OVR44H(2),GTE_OVR44L(2));

			c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((s64)BFC << 12) - (B0 << 16),GTE_OVR44H(3),GTE_OVR44L(3));
			c=SR(c,sh);
			c=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(c,GTE_OVR16F(3),GTE_OVR16F(3));
			c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(((u32)B0 << 16)+(IR0*c),GTE_OVR44H(3),GTE_OVR44L(3));

			MAC1=(s32)SR(a,sh);
			MAC2=(s32)SR(b,sh);
			MAC3=(s32)SR(c,sh);//fixme

			_cmac_store();
			_cfifo();
		}
		break;
		case 0x2d:
			//a=((s64) (ZSF3 * SZ1) + (ZSF3 * SZ2) + (ZSF3 * SZ3));
			a=ZSF3;;
			a=GTE_OVR<s64,GTE_MAX_32BIT,GTE_MIN_32BIT,0>((a * SZ1) + (a * SZ2) + (a * SZ3),(GTE_OVRF|BV(16)),(GTE_OVRF|BV(15)));
			MAC0 = a;
			OTZ= GTE_OVR<s32,0xffff,0,1>((s32)(a>>12),GTE_OVR16U,GTE_OVR16U);
		break;
		case 0x2e:
			a=ZSF4;;
			a=GTE_OVR<s64,GTE_MAX_32BIT,GTE_MIN_32BIT,0>((a * SZ0) + (a * SZ1) + (a * SZ2) + (a * SZ3),(GTE_OVRF|BV(16)),(GTE_OVRF|BV(15)));
			MAC0 = a;
			OTZ= GTE_OVR<s32,0xffff,0,1>((s32)(a>>12),GTE_OVR16U,GTE_OVR16U);
		break;
		case 0x30:
			for(int i=0;i<3;i++)
				c=_rtp(i,sh);
			a=GTE_OVR<s64,GTE_MAX_32BIT,GTE_MIN_32BIT,0>((s64)DQB + ((s64)DQA * c),GTE_OVR32H,GTE_OVR32L);
			MAC0 = a;
			IR0=GTE_OVR<s32,0x1000,0,1>(SR(a,12),GTE_OVR12U,GTE_OVR12U);
			//printf("%llx %5x %x SZ3:%x H:%x %llx",c,DQB,DQA,(u16)SZ3,(u16)H,a);
		break;
		case 0x3d:
			a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(IR0*IR1,GTE_OVR44H(1),GTE_OVR44L(1));
			b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(IR0*IR2,GTE_OVR44H(2),GTE_OVR44L(2));
			c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(IR0*IR3,GTE_OVR44H(3),GTE_OVR44L(3));
			MAC1=(s32)SR(a,sh);
			MAC2=(s32)SR(b,sh);
			MAC3=(s32)SR(c,sh);//fixme
			_cmac_store();
			_cfifo();
		break;
		case 0x3e:
			a=GTE_ADD44_OVR(1,(s64)IR0*IR1,SL((s64)MAC1,sh));
			b=GTE_ADD44_OVR(2,(s64)IR0*IR2,SL((s64)MAC2,sh));
			c=GTE_ADD44_OVR(3,(s64)IR0*IR3,SL((s64)MAC3,sh));

			MAC1=(s32)SR(a,sh);
			MAC2=(s32)SR(b,sh);
			MAC3=(s32)SR(c,sh);

			_cmac_store();
			_cfifo();
		break;
		case 0x3f:
			for(int i=0;i<3;i++){
				_cv(i,sh);
			_bkcdp(sh);
			_cinterp(sh);
			_cmac_store();
			_cfifo();
		}
		break;
	}
	return 0;
}

void __gte::_ncd(int i,int sh){
	_cv(i,sh);
	_cdp(sh);
}

s64 __gte::_rtp(int i,int sh){
	s64 a,b,c;

	a=GTE_ADD44_OVR(1,(s64)SL((s64)TRX,12),(s64)R11 * VX(i),(s64)R12 * VY(i),(s64)R13 * VZ(i));
	b=GTE_ADD44_OVR(2,(s64)SL((s64)TRY,12),(s64)R21 * VX(i),(s64)R22 * VY(i),(s64)R23 * VZ(i));
	c=GTE_ADD44_OVR(3,(s64)SL((s64)TRZ,12),(s64)R31 * VX(i),(s64)R32 * VY(i),(s64)R33 * VZ(i));

	MAC1=(s32)SR(a,sh);
	MAC2=(s32)SR(b,sh);
	MAC3=(s32)SR(c,sh);

	if(_lm){
		IR1=GTE_OVR<s32,GTE_MAX_16BIT,0,1>(MAC1,GTE_OVR16H(1),GTE_OVR16H(1));
		IR2=GTE_OVR<s32,GTE_MAX_16BIT,0,1>(MAC2,GTE_OVR16H(2),GTE_OVR16H(2));
		IR3=GTE_OVR_<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,GTE_MAX_16BIT,0,1>(MAC3,(c>>12),GTE_OVR16H(3),GTE_OVR16H(3));//fixme
	}
	else{
		IR1=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(MAC1,GTE_OVR16H(1),GTE_OVR16H(1));
		IR2=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(MAC2,GTE_OVR16H(2),GTE_OVR16H(2));
		//IR3=GTE_OVR<s16,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(MAC3,GTE_OVR16H(3),GTE_OVR16H(3));
		IR3=GTE_OVR_<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(MAC3,(c>>12),GTE_OVR16H(3),GTE_OVR16H(3));//fixme
	}

	SZ0 = SZ1;
	SZ1 = SZ2;
	SZ2 = SZ3;
	SZ3 = GTE_OVR<s32,0xffff,0,1>((s32)SR(c,12),GTE_OVR16U,GTE_OVR16U);
	SXY0 = SXY1;
	SXY1 = SXY2;

	if(SZ3){
		c=(SL((u64)H,16) + SR(SZ3,1)) / (SZ3+1);
		c=_div(H,SZ3);
		///c=floor(( ((u16)H*65536.0 + (SZ3/2.0)) / (double)(SZ3) ) - 0.6);
		c=GTE_OVR_<u32,0xffffffff,0,0x1ffff,0,1>(c,c,GTE_OVRDIV,GTE_OVRDIV);
	}
	else{
		FLAG |= GTE_OVRDIV;
		c=(s64)0x1ffff;
	}

	a=GTE_OVR<s64,GTE_MAX_32BIT,GTE_MIN_32BIT,0>((s64) OFX + ((s64) IR1 * c * 1),GTE_OVR32H,GTE_OVR32L);
	SX2=GTE_OVR<s32,GTE_MAX_10BIT,GTE_MIN_10BIT,1>(SR(a,16),(GTE_OVRF|BV(14)),(GTE_OVRF|BV(14)));

	a=GTE_OVR<s64,GTE_MAX_32BIT,GTE_MIN_32BIT,0>((s64) OFY + ((s64) IR2 * c),GTE_OVR32H,GTE_OVR32L);
	SY2=GTE_OVR<s32,GTE_MAX_10BIT,GTE_MIN_10BIT,1>(SR(a,16),(GTE_OVRF|BV(13)),(GTE_OVRF|BV(13)));

	return c;
}

void __gte::_cv(int i,int sh){
	s64 a,b,c;

	a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>( ((s64)L11 * VX(i)) + ((s64)L12 * VY(i)) + ((s64)L13 * VZ(i)),GTE_OVR44H(1),GTE_OVR44L(1));
	b=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>( ((s64)L21 * VX(i)) + ((s64)L22 * VY(i)) + ((s64)L23 * VZ(i)),GTE_OVR44H(2),GTE_OVR44L(2));
	c=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>( ((s64)L31 * VX(i)) + ((s64)L32 * VY(i)) + ((s64)L33 * VZ(i)),GTE_OVR44H(3),GTE_OVR44L(3));

	MAC1 = SR(a,sh);
	MAC2 = SR(b,sh);
	MAC3 = SR(c,sh);
	_cmac_store();
}

void __gte::_bkcdp(int sh){
	s64  a,b,c;

	a=GTE_ADD44_OVR(1,(s64)SL((s64)RBK,12),(s64)(LR1 * IR1),(s64)(LR2 * IR2),(s64)(LR3 * IR3));
	b=GTE_ADD44_OVR(2,(s64)SL((s64)GBK,12),(s64)(LG1 * IR1),(s64)(LG2 * IR2),(s64)(LG3 * IR3));
	c=GTE_ADD44_OVR(3,(s64)SL((s64)BBK,12),(s64)(LB1 * IR1),(s64)(LB2 * IR2),(s64)(LB3 * IR3));

	MAC1 = SR(a,sh);
	MAC2 = SR(b,sh);
	MAC3 = SR(c,sh);

	_cmac_store();
}

void __gte::_cinterp(int sh){
	MAC1=SR(SL(R * IR1,4),sh);
	MAC2=SR(SL(G * IR2,4),sh);
	MAC3=SR(SL(B * IR3,4),sh);
}

void __gte::_fccdp(int sh){
	s64  a,b,c;

	a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(SL((s64)RFC,12) - MAC1,GTE_OVR44H(1),GTE_OVR44L(1));
	a=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(SR(a,sh),GTE_OVR16H(1),GTE_OVR16H(1));
	a=GTE_OVR<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(MAC1+(IR0*a),GTE_OVR44H(1),GTE_OVR44L(1));

	b=GTE_ADD44_OVR(2,(s64)SL((s64)GFC,12),(s64)-MAC2);
	b=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(SR(b,sh),GTE_OVR16H(2),GTE_OVR16H(2));
	b=GTE_ADD44_OVR(2,(s64)MAC2,(IR0 * b));

	c=GTE_ADD44_OVR(3,(s64)SL((s64)BFC,12),(s64)-MAC3);
	c=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(SR(c,sh),GTE_OVR16F(3),GTE_OVR16F(3));
	c=GTE_ADD44_OVR(3,(s64)MAC3,(IR0 * c));

	MAC1=SR(a,sh);
	MAC2=SR(b,sh);
	MAC3=SR(c,sh);

	_cmac_store();
}

void __gte::_cmac_store(){
	if(_lm){
		IR1=GTE_OVR<s32,GTE_MAX_16BIT,0,1>(MAC1,GTE_OVR16H(1),GTE_OVR16H(1));
		IR2=GTE_OVR<s32,GTE_MAX_16BIT,0,1>(MAC2,GTE_OVR16H(2),GTE_OVR16H(2));
		IR3=GTE_OVR<s32,GTE_MAX_16BIT,0,1>(MAC3,GTE_OVR16F(3),GTE_OVR16F(3));
	}
	else{
		IR1=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(MAC1,GTE_OVR16H(1),GTE_OVR16H(1));
		IR2=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(MAC2,GTE_OVR16H(2),GTE_OVR16H(2));
		IR3=GTE_OVR<s32,GTE_MAX_16BIT,GTE_MIN_16BIT,1>(MAC3,GTE_OVR16F(3),GTE_OVR16F(3));
	}
}

void __gte::_cfifo(){
	RGB0 = RGB1;
	RGB1 = RGB2;
	CD2 = CODE;
	R2=GTE_OVR<s32,0xff,0,1>(MAC1 >> 4,GTE_OVR8U(1),GTE_OVR8U(1));
	G2=GTE_OVR<s32,0xff,0,1>(MAC2 >> 4,GTE_OVR8U(2),GTE_OVR8U(2));
	B2=GTE_OVR<s32,0xff,0,1>(MAC3 >> 4,GTE_OVR8U(3),GTE_OVR8U(3));
}

void __gte::_cdp(int sh){
	_bkcdp(sh);
	_cinterp(0);
	_fccdp(sh);
	_cfifo();
}

int __gte::_ctrl_mv_from(u32 s,u32 *d){
	switch(s){
		default:
			*d=ctrl._regs[s];
			break;
	}
	return 0;
}

int __gte::_ctrl_mv_to(u32 s,u32 *d){
	switch(s){
		case 4:
		case 12:
		case 20:
		case 26:
		case 27:
		case 29:
		case 30:
			ctrl._sregs[s]=(s32)(s16)*d;
		break;
		case 31:{
			u32 value=*d & 0x7ffff000;
			if((value & 0x7f87e000) != 0)
				value |= 0x80000000;
			ctrl._regs[s]=value;
		}
		break;
		default:
			ctrl._regs[s]=*d;
			break;
	}
	return 0;
}

u32 __gte::_clz(u32 lzcs){
	u32 lzcr = 0;

	if((lzcs & 0x80000000) == 0)
		lzcs = ~lzcs;
	while((lzcs & 0x80000000)) {
		lzcr++;
		lzcs <<= 1;
	}
	return lzcr;
}

int __gte::_mv_from(u32 s,u32 *d){
	//EnterDebugMode(DEBUG_BREAK_OPCODE);
	switch(s){
		case 1:
		case 3:
		case 5:
		case 8:
		case 9:
		case 10:
		case 11:
			*d=(u32)(s32)(s16)_sregs16[s][0];
		break;
		case 7:
		case 16:
		case 17:
		case 18:
		case 19:
			*d=(u32)(u16)_regs16[s][0];
		break;
		case 15:
			*d=_regs[15]=SXY2;
			break;
		case 28:
		case 29:
			_regs[s]=GTE_OVR31(IR1 >> 7) | (GTE_OVR31(IR2 >> 7) << 5) | (GTE_OVR31(IR3 >> 7) << 10);
		default:
			*d=_regs[s];
		//	EnterDebugMode();
			break;
	}
	//printf("mv from %x %x\n",s,*d);
	return 0;
}

int __gte::_mv_to(u32 s,u32 *d){
	//printf("mv to %x %x\n",s,*d);
	//EnterDebugMode(DEBUG_BREAK_OPCODE);
	switch(s){
		case 15:
			SXY0 = SXY1;
			SXY1 = SXY2;
			SXY2 = *d;
			break;
		case 28://IRGB
			IR1 = (*d & 0x1f) << 7;
			IR2 = (*d & 0x3e0) << 2;
			IR3 = (*d & 0x7c00) >> 3;
			break;
		case 30:
			LZCR=_clz(*d);
	//	break;
		default:
			_regs[s]=*d;
			break;
		case 31:
		break;
	}
	return 0;
}

u32 __gte::_div(u16 num,u16 den){
	if(num > (den*2))
		return 0xffffffff;

	int shift = _clz( den ) - 16;
	int r1 = ( den << shift ) & 0x7fff;
	int r2,i= ( r1 + 0x40 ) >> 7 ;
	if(i > 255)
		r2=257;
	else
		r2 = (0x40000 / (i+0x100) + 1)>>1;
	//(0x40000/(i+0x100)+1)/2-0x101;
//	r2 = _div_table[ ( ( r1 + 0x40 ) >> 7 ) ] + 0x101;
	int r3 = ( ( 0x80 - ( r2 * ( r1 + 0x8000 ) ) ) >> 8 ) & 0x1ffff;
	u32 reciprocal = ( ( r2 * r3 ) + 0x80 ) >> 8;
	return (u32)( ( ( (u64) reciprocal * ( num << shift ) ) + 0x8000 ) >> 16 );
}

template <typename T, T M,T m,T sM, T sm,u32 sat,typename... Params> T __gte::GTE_ADD_(u32 fM,u32 fm,Params &&... args){
	T a,b;
	u8 ca;

	auto v={args...};
	a=0;
	//printf("44add");
	for(auto it=v.begin();it!=v.end();++it){
		b=*(it);
		//printf(" %llx+%llx",a,b);
		ca=SR(a,43) & 1;
		a += b;
	//	printf("=%llx-%c%c%c",a,48+ca,48+cb,48+c);
		a=GTE_OVR_<T,M,m,sM,sm,sat>(a,a,fM,fm);
		if(sizeof(T) == sizeof(u64)){
			u8 cb=SR(b,43) & 1;
			if(!(a & SL((u64)1,43))){
				if(ca && cb)
					FLAG |= fm;
				a  &= 0xFFFFFFFFFFF;
			}
		}
	}
//	printf("\n");
	return a;
}

template <typename T, T M,T m,T sM, T sm,u32 sat> T __gte::GTE_OVR_(T a,T b,u32 fM,u32 fm){
	if(b > M)
		FLAG |= fM;
	else  if(b <  m){
		FLAG |= fm;
		//if(!(b & 0x80000000000)) a  &= 0xFFFFFFFFFFF;
	}
	if((sat&1)){
		if(a>sM)
			a=sM;
		else if(a<sm)
			a=sm;
	}
	return a;
}

template <typename T, T M,T m,u32 sat> T __gte::GTE_OVR(T a,u32 fM,u32 fm){
	return GTE_OVR_<T,M,m,M,m,sat>(a,a,fM,fm);
}

};