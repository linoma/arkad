#include "sh2.h"

#ifdef _DEVELOPa
	#define SH2LOGP(a,...) if(BT(status,S_PAUSE|S_DEBUG_NEXT) == S_PAUSE){\
		DEVF(STR(%08X %04X %s) STR(\x20) STR(a) STR(\n),_pc,_opcode,## __VA_ARGS__);\
	}
#else
	#define SH2LOGP(a,...)
#endif

#define SH2LOG(...) SH2LOGP(NOARG,## __VA_ARGS__)
#define SH2LOGD(a,...)	sprintf(cc,STR(%s) STR(\x20) STR(a),## __VA_ARGS__);
#define SH2LOGE(){printf("%08X %04X UNKNOW\n",_pc,_opcode);EnterDebugMode();}

#define SH2BRANCH(a,b) _ppl._create_slot(a,b)

SH2Cpu::SH2Cpu() : CCore(){
	_ioreg=NULL;
}
//142b6 13c98 13cc4
SH2Cpu::~SH2Cpu(){
	//Destroy();
}

int SH2Cpu::_exec(u32 status){
	int res=1;

	RWPC(_pc,_opcode);
	switch(SR(_opcode,12)){
		case 0:
			switch(_opcode & 0x3f){
				case 2:
					REG(8)=_sr;
					SH2LOGP(SR\x2cR%d,"STC",Ri(8));
				break;
				case 3:{
					s32 a = REG(8);

					SH2LOGP(R%d,"BSRF",Ri(8));
					_pr=_pc+4;
					STORECALLLSTACK(_pr);
					SH2BRANCH(2,_pc+a+4);
					res++;
				}
				break;
				case 4:
				case 0x14:
				case 0x24:
				case 0x34:
					WB(REG_0+REG(8),(u8)REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c@[R%d+R0],"MOV.BS0",Ri(4),Ri(8));
				break;
				case 0x5:
				case 0x15:
				case 0x25:
				case 0x35:
					WW(REG_0+REG(8),REG(4));
					res+=04;
					SH2LOGP(R%d\x2c@[R0+R%d],"MOV.WS0",Ri(4),Ri(8));
				break;
				case 0x6:
				case 0x16:
				case 0x26:
				case 0x36:
					WL(REG_0+REG(8),REG(4));
					res+=04;
					SH2LOGP(R%d\x2c@[R0+R%d],"MOV.LS0",Ri(4),Ri(8));
				break;
				case 7:
				case 0x17:
				case 0x27:
				case 0x37:
					_macl=REG(8)*REG(4);
					res +=2;
					SH2LOGP(R%d\x2cR%d,"MULL",Ri(4),Ri(8));
				break;
				case 0x8:
					BC(_sr,SH_T);
					SH2LOG("CLRT");
				break;
				case 9:
					SH2LOG("NOP");
				break;
				case  0xa:
					REG(8)=_mach;
					SH2LOGP(MACH\x2cR%d,"STS",Ri(8));
				break;
				case 0xb:
					SH2BRANCH(2,_pr);
					//_pc=_pr-2;
					LOADCALLSTACK(_pr);
					res++;
					SH2LOG("RTS");
				break;
				case 0xe:
				case 0x1e:
				case 0x2e:
				case 0x3e:
					RL(REG(4)+REG_0,REG(8));
					res+=04;
					SH2LOGP(@[R0+R%d]\x2cR%d,"MOV.LL0",Ri(4),Ri(8));
				break;
				case 0x12:
					REG(8)=_gbr;
					SH2LOGP(GBR\x2cR%d,"STC",Ri(8));
				break;
				case 0x19:
					_sr &= ~(SH_M | SH_Q | SH_T);
					SH2LOG("DIV0U");
				break;
				case 0x1a:
					REG(8)=_macl;
					SH2LOGP(MACL\x2cR%d,"STS",Ri(8));
				break;
				case 0x22:
					REG(8)=_vbr;
					SH2LOGP(VBR\x2cR%d,"STC",Ri(8));
				break;
				case 0x23:
					SH2LOGP(R%d,"BRAF",Ri(8));
					SH2BRANCH(2,REG(8)+_pc+4);
					res++;
				break;
				case 0x29:
					REG(8) = _sr&SH_T;
					SH2LOGP(R%d,"MOVT",Ri(8));
				break;
				case 0x2a:
					REG(8)=_pr;
					SH2LOGP(PR\x2cR%d,"STS",Ri(8));
				break;
				case 0x2b:
					{
						u32 a;

						RL(REG_SP,a);
						REG_SP+=4;
						RL(REG_SP,_sr);
						REG_SP+=4;
						SH2BRANCH(2,a);
						_ppl.irq=1;
					}
					res++;
					SH2LOG("RTE");
					EnterDebugMode(DEBUG_BREAK_IRQ);
				break;
				case 0xc:
				case 0x1c:
				case 0x2c:
				case 0x3c:
				{
					u8 b;
					u32 a;

					a=REG(4)+REG_0;
					RB(a,b);
					REG(8)=(u32)(s32)(s8)b;
					res+=04;
					SH2LOGP(@[R0+R%d]\x2cR%d,"MOV.BL0",Ri(4),Ri(8));
				}
				break;
				case 0xd:
				case 0x1d:
				case 0x2d:
				case 0x3d:
				{
					u32 a =REG(4)+REG_0;
					u16 u;

					RW(a,u);
					REG(8)=(u32)(s32)(s16)u;
					res+=04;
					SH2LOGP(@[R0+R%d]\x2cR%d,"MOV.WL0",Ri(4),Ri(8));
				}
				break;
				case 0x1b:
				break;
				default:

					SH2LOGE();
				break;
			}
		break;
		case 1:
		{
			u32 a = REG(8)+ SL(_opcode&0xf,2);
			WL(a,REG(4));
			res+=04;
			SH2LOGP(R%d\x2c@[R%d+%d]\x20%x\x20%x,"MOV.LS4",Ri(4),Ri(8),SL(_opcode&0xF,2),REG(4),a);
		}
		break;
		case 2:
			switch(_opcode & 0xf){
				case 0:
					WB(REG(8),(u8)REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c@R%d,"MOV.BS",Ri(4),Ri(8));
				break;
				case 1:
					WW(REG(8),(u16)REG(4));//R4->(R8
					res+=04;
					SH2LOGP(R%d\x2c@R%d,"MOV.WS",Ri(4),Ri(8));
				break;
				case 2:
					WL(REG(8),REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c@R%d,"MOV.LS",Ri(4),Ri(8));
				break;
				case 4:
				{
					u32 a = REG(8)-1;
					REG(8)=a;
					WB(a,REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c@-R%d,"MOV.BM",Ri(4),Ri(8));
				}
				break;
				case 6:
				{
					u32 a = REG(8)-4;
					REG(8)-=4;
					WL(a,REG(4));//R4->(R8)
					res+=04;
					SH2LOGP(R%d\x2c@-R%d,"MOV.LM",Ri(4),Ri(8));
				}
				break;
				case 0x7:
					if(REG(8)&0x80000000)
						BS(_sr,SH_Q);
					else
						BC(_sr,SH_Q);
					if(REG(4)&0x80000000)
						BS(_sr,SH_M);
					else
						BC(_sr,SH_M);
					if(
						((_sr&SH_Q) && (_sr&SH_M)) ||
						(!(_sr&SH_Q) && !(_sr&SH_M)))
						BC(_sr,SH_T);
					else
						BS(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"DIV0S",Ri(4),Ri(8));
				break;
				case 8:
					if((REG(8) & REG(4))==0)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"TST",Ri(4),Ri(8));
				break;
				case 9:
					REG(8)&=REG(4);
					SH2LOGP(R%d\x2cR%d,"AND",Ri(4),Ri(8));
				break;
				case 0xa:
					REG(8)^=REG(4);
					SH2LOGP(R%d\x2cR%d,"XOR",Ri(4),Ri(8));
				break;
				case 0xb:
					REG(8)|=REG(4);
					SH2LOGP(R%d\x2cR%d,"OR",Ri(4),Ri(8));
				break;
				case 0xd:
					REG(8) = SL((u16)REG(4),16)|SR(REG(8),16);
					SH2LOGP(R%d\x2cR%d,"XTRCT",Ri(4),Ri(8));
				break;
				case 0xe:
					_macl=(u32)((u16)REG(8)*(u16)REG(4));
					SH2LOGP(R%d\x2cR%d,"MULU.W",Ri(4),Ri(8));
					res++;
				break;
				case 0xf:
					_macl=(u32)((s16)REG(8)*(s16)REG(4));
					SH2LOGP(R%d\x2cR%d,"MULS.W",Ri(4),Ri(8));
					res++;
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 3:
			switch(_opcode & 0xf){
				case 0:
					if(REG(8) == REG(4))
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPEQ",Ri(4),Ri(8));
				break;
				case 2:
					if(REG(8) >= REG(4))
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPHS",Ri(4),Ri(8));
				break;
				case 3:
					if((s32)REG(8) >= (s32)REG(4))
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPGE",Ri(4),Ri(8));
				break;
				case 0x4:
				{
					u32 a,oq,q,b;

					oq=_sr&SH_Q;
					if(REG(8)&0x80000000)
						BS(_sr,SH_Q);
					else
						BC(_sr,SH_Q);
					REG(8)=SL(REG(8),1)|(_sr&SH_T);
					a=REG(8);
					switch(oq){
						case 0:
							switch(_sr&SH_M){
								case 0:
									REG(8)-= REG(4);
									b=REG(8)>a;
									switch(_sr&SH_Q){
										case 0:
											if(b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
										default:
											if(!b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
									}
								break;
								default:
									REG(8)+=REG(4);
									b=REG(8)<a;
									switch(_sr&SH_Q){
										case 0:
											if(!b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
										default:
											if(b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
									}
								break;
							}
						break;
						default:
							switch(_sr&SH_M){
								case 0:
									REG(8)+=REG(4);
									b=REG(8)<a;
									switch(_sr&SH_Q){
										case 0:
											if(b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
										default:
											if(!b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
									}
								break;
								default:
									REG(8)-=REG(4);
									b=REG(8)>a;
									switch(_sr&SH_Q){
										case 0:
											if(!b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
										default:
											if(b)
												BS(_sr,SH_Q);
											else
												BC(_sr,SH_Q);
										break;
									}
								break;
							}
						break;
					}

					if(
						((_sr&SH_Q) && (_sr&SH_M)) ||
						(!(_sr&SH_Q) && !(_sr&SH_M))
						)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"DIV1",Ri(4),Ri(8));
				}
				break;
				case 5:{
					u64 v = REG(8);
					v *= REG(4);
					_macl=(u32)v;
					_mach=(u32)SR(v,32);
					SH2LOGP(R%d\x2cR%d,"DMULU",Ri(4),Ri(8));
				}
				break;
				case 6:
					if(REG(8) > REG(4))
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPHI",Ri(4),Ri(8));
				break;
				case 7:
					if((s32)REG(8) > (s32)REG(4))
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"CMPGT",Ri(4),Ri(8));
				break;
				case 8:
					REG(8)=REG(8)-REG(4);
					SH2LOGP(R%d\x2cR%d,"SUB",Ri(4),Ri(8));
				break;
				case 0xa:
				{
					u32 a,b;

					a=REG(8)-REG(4);
					b=REG(8);
					REG(8)=a-(_sr&SH_T);

					if(b<a)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					if(a<REG(8))
						BS(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"SUBC",Ri(4),Ri(8));
				}
				break;
				case 0xc:
					REG(8)+=REG(4);
					SH2LOGP(R%d\x2cR%d,"ADD",Ri(4),Ri(8));
				break;
				case 0xe:
				{
					u32 a,b;

					a=REG(8)+REG(4);
					b=REG(8);
					REG(8)=(_sr&SH_T)+a;

					if(b>a)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					if(a>REG(8))
						BS(_sr,SH_T);
					SH2LOGP(R%d\x2cR%d,"ADDC",Ri(4),Ri(8));
				}
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 4:
			switch(_opcode & 0x3f){
				case 0:
					if(REG(8) & 0x80000000)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					REG(8)=SL(REG(8),1);
					SH2LOGP(R%d,"SHLL",Ri(8));
				break;
				case 1:
					_sr=(_sr & ~SH_T) | (REG(8) & 1);
					REG(8)=SR(REG(8),1);
					SH2LOGP(R%d,"SHLR",Ri(8));
				break;
				case 2:
					REG(8)-=4;
					WL(REG(8),_mach);
					res++;
					SH2LOGP(MACH\x2c@-R%d,"STS.L",Ri(8));
				break;
				case 4:
				{
					if(SR(REG(8),31))
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					REG(8)=SL(REG(8),1)|(_sr&SH_T);
					SH2LOGP(R%d,"ROTL",Ri(8));
				}
				break;
				case 5:
					if(REG(8)&1)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					REG(8)=SR(REG(8),1)|(_sr&SH_T);
					SH2LOGP(R%d,"ROTR",Ri(8));
				break;
				case 6:
					RL(REG(8),_mach);
					REG(8)+=4;
					res++;
					SH2LOGP(@R%d+\x2cMACH,"LDS.L",Ri(8));
				break;
				case 7:
				{
					u32 a;

					RL(REG(8),a);
					REG(8)+=4;
					_sr= a & 0xFFF;
					_ppl.irq=1;
					res++;
					SH2LOGP(@R%d+\x2cSR,"LDC.L",Ri(8));
				}
				break;
				case 0x8:
					REG(8)=SL(REG(8),2);
					SH2LOGP(R%d,"SHLL2",Ri(8));
				break;
				case 9:
					REG(8)=SR(REG(8),2);
					SH2LOGP(R%d,"SHRL2",Ri(8));
				break;
				case 0xa:
					SH2LOGP(R%d\x2cMACH,"LDS",Ri(8));
					_mach=REG(8);
				break;
				case 0xb:
					SH2LOGP(R%d,"JSR",Ri(8));
					_pr=_pc+4;
					STORECALLLSTACK(_pr);
					SH2BRANCH(2,REG(8));
					res++;
				break;
				case 0xe:
					_sr=REG(8) & 0xFFF;
					_ppl.irq=1;
					SH2LOGP(R%d\x2cSR,"LDC",Ri(8));
				break;
				case 0x10:
					REG(8)--;
					if(REG(8) == 0)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d,"DT",Ri(8));
				break;
				case 0x11:
					if((s32)REG(8) >= 0)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d\x2cPZ,"CMPPZ",Ri(8));
				break;
				case 0x12:
					REG(8)-=4;
					WL(REG(8),_macl);
					res++;
					SH2LOGP(MACL\x2c@-R%d,"STS.L",Ri(8));
				break;
				case 0x15:
					if((s32)REG(8) > 0)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d,"CMPPL",Ri(8));
				break;
				case 0x16:
					RL(REG(8),_macl);
					REG(8)+=4;
					res++;
					SH2LOGP(@R%d+\x2cMACL,"LDS.L",Ri(8));
				break;
				case 0x17:
					RL(REG(8),_gbr);
					REG(8)+=4;
					res++;
					SH2LOGP(@R%d+\x2cGBR,"LDS.L",Ri(8));
				break;
				case 0x18:
					REG(8)=SL(REG(8),8);
					SH2LOGP(R%d,"SHLL8",Ri(8));
				break;
				case 0x19:
					REG(8)=SR(REG(8),8);
					SH2LOGP(R%d,"SHRL8",Ri(8));
				break;
				case 0x1a:
					SH2LOGP(R%d\x2cMACL,"LDS",Ri(8));
					_macl=REG(8);
				break;
				case 0x1e:
					_gbr=REG(8);
					SH2LOGP(R%d\x2cGBR,"LDC",Ri(8));
				break;
				case 0x21:
				{
					BC(_sr,SH_T);
					_sr |= (REG(8) & 1);
					REG(8) = (u32)SR((s32)REG(8),1);
					SH2LOGP(R%d,"SHAR",Ri(8));
				}
				break;
				case 0x22:
				{
					u32 a = REG(8) - 4;
					WL(a,_pr);
					REG(8)=a;
					res++;
					SH2LOGP(PR\x2c-R%d,"STSM",Ri(8));
				}
				break;
				case 0x24:
				{
					u32 a = SR(REG(8),31);
					REG(8)=SL(REG(8),1)|(_sr&SH_T);
					if(a)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d,"ROTCL",Ri(8));
				}
				break;
				case 0x25:
				{
					u32 a = REG(8)&1;
					REG(8)=SR(REG(8),1)|SL(_sr&SH_T,31);
					if(a)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(R%d,"ROTCR",Ri(8));
				}
				break;
				case 0x26:
					RL(REG(8),_pr);
					REG(8)+=4;
					res++;
					SH2LOGP(@R%d+\x2cPR,"LDS.L",Ri(8));
				break;
				case 0x28:
					REG(8)=SL(REG(8),16);
					SH2LOGP(R%d,"SHLL16",Ri(8));
				break;
				case 0x29:
					REG(8)=SR(REG(8),16);
					SH2LOGP(R%d,"SHLR16",Ri(8));
				break;
				case 0x2a:
					SH2LOGP(R%d\x2cPR,"LDS",Ri(8));
					_pr=REG(8);
				break;
				case 0x2b:
					SH2LOGP(@R%d,"JMP",Ri(8));
					SH2BRANCH(2,REG(8));
					res++;
				break;
				case 0x2e:
					_vbr=REG(8);
					SH2LOGP(R%d\x2cVBR,"LDC",Ri(8));
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 5:
		{
			u32 a = REG(4) + SL(_opcode&0xf,2);//(R4+d))->R8
			RL(a,REG(8));
			res+=04;
			SH2LOGP(@[R%d+%d]\x2cR%d,"MOV.LL4",Ri(4),(int)SL(_opcode&0xf,2),Ri(8));
		}
		break;
		case 6:
			switch(_opcode & 0xf){
				case 0:
				{
					u8 a;

					RB(REG(4),a);
					REG(8)=(u32)(s32)(s8)a;
					res+=04;
					SH2LOGP(@R%d\x2cR%d,"MOV.BL",Ri(4),Ri(8));
				}
				break;
				case 1:
					{
						u16 a;
						u32 b=REG(4);

						RW(b,a);
						REG(8)=(u32)(s32)(s16)a;
						res+=04;
						SH2LOGP(@R%d\x2cR%d,"MOV.WL",Ri(4),Ri(8));
						//printf("reg %d %x %x %x %x PC::%x\n",Ri(8),REG(8),a,b,__data,_pc);
					}
				break;
				case 2://(Rm)->Rn
					RL(REG(4),REG(8));
					res+=04;
					SH2LOGP(@R%d\x2cR%d\x20%x\x20%x,"MOV.LL",Ri(4),Ri(8),REG(8),REG(4));
				break;
				case 3:
					REG(8)=REG(4);
					SH2LOGP(R%d\x2cR%d,"MOV",Ri(4),Ri(8));
				break;
				case 4:
					{
						u8 a;

						RB(REG(4),a);
						REG(8)=(u32)(s32)(s8)a;
						if(Ri(4) != Ri(8))
							REG(4)++;
						res+=04;
						SH2LOGP(@R%d+\x2cR%d,"MOV.BP",Ri(4),Ri(8));
					}
					break;
				case 5:
					{
						u16 a;

						RW(REG(4),a);
						REG(8)=(u32)(s32)(s16)a;
						if(Ri(4) != Ri(8))
							REG(4)+=2;
						res+=04;
						SH2LOGP(@R%d+\x2cR%d,"MOV.WP",Ri(4),Ri(8));
					}
					break;
				case 6:
					RL(REG(4),REG(8));
					if(Ri(4)!=Ri(8))
						REG(4)+=4;
					res+=04;
					SH2LOGP(@R%d+\x2cR%d,"MOV.LP",Ri(4),Ri(8));
				break;
				case 7:
					REG(8)=~REG(4);
					SH2LOGP(R%d\x2cR%d,"NOT",Ri(4),Ri(8));
				break;
				case 8:
					REG(8)=MAKEHWORD((u8)SR(REG(4),8),(u8)REG(4));
					SH2LOGP(R%d\x2cR%d,"SWAPB",Ri(4),Ri(8));
				break;
				case 9:
					REG(8)=MAKELONG(SR(REG(4),16),(u16)REG(4));
					SH2LOGP(R%d\x2cR%d,"SWAPW",Ri(4),Ri(8));
				break;
				case 0xb:
					REG(8)=0-REG(4);
					SH2LOGP(R%d\x2cR%d,"NEG",Ri(4),Ri(8));
				break;
				case 0xC:
					REG(8) = (u32)(u8)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTU.B",Ri(4),Ri(8));
				break;
				case 0xd:
					REG(8) = (u32)(u16)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTU.W",Ri(4),Ri(8));
				break;
				case 0xe:
					REG(8) = (u32)(s32)(s8)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTS.B",Ri(4),Ri(8));
				break;
				case 0xf:
					REG(8) = (u32)(s32)(s16)REG(4);
					SH2LOGP(R%d\x2cR%d,"EXTS.W",Ri(4),Ri(8));
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 7:
			REG(8)+=(s32)(s16)(s8)_opcode;
			SH2LOGP(#%d\x2cR%d,"ADDI",(s32)(s8)_opcode,Ri(8));
		break;
		case 8:
			switch(SR(_opcode,8)&0xf){
				case 0:
				{
					u32 a = REG(4) + (_opcode&0xf);
					WB(a,(u8)REG_0);
					res+=04;
					SH2LOGP(@[R%d+%d]\x2cR0,"MOV.BS4",Ri(4),(_opcode&0xF));
				}
				break;
				case 1:
				{
					u32 a = REG(4) + SL(_opcode&0xf,1);
					WW(a,REG_0);
					res+=04;
					SH2LOGP(R0\x2c@[R%d+%d],"MOV.WS4",Ri(4),SL(_opcode&0xF,1));
				}
				break;
				case 4:
				{
					u32 a = REG(4) + (_opcode&0xf);
					u8 u;

					RB(a,u);
					REG_0=(u32)(s32)(s8)u;
					res+=04;
					SH2LOGP(@[R%d+%d]\x2cR0,"MOV.BL4",Ri(4),_opcode&0xF);
				}
				break;
				case 5:
				{
					u32 a = (REG(4) + SL(_opcode&0xf,1));
					u16 u;

					RW(a,u);
					REG_0=(u32)(s32)(s16)u;
					res+=04;
					SH2LOGP(@[R%d+%d]\x2cR0,"MOV.WL4",Ri(4),SL(_opcode&0xF,1));
				}
				break;
				case 8:
					if((s32)REG_0==(s32)(s8)_opcode)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(#%d\x2cR0,"CMPIM",(s32)(s8)_opcode);
				break;
				case 9:
				{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BT",a);
					if(BT(_sr,SH_T)){
						_pc=a-2;
						res+=2;
					}
				}
				break;
				case 0xb:
				{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BF",a);
					if(!BT(_sr,SH_T)){
						_pc=a-2;
						res+=2;
					}
				}
				break;
				case 0xd:
				{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BTS",a);
					if(BT(_sr,SH_T)){
						SH2BRANCH(2,a);
						res+=2;
					}
				}
				break;
				case 0xf:
				{
					u32 a = ((_pc+4)) + (int)SL((s8)_opcode,1);
					SH2LOGP(%x,"BFS",a);
					if(!BT(_sr,SH_T)){
						SH2BRANCH(2,a);
						res+=2;
					}
				}
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 9:
		{
			u32 a = ((_pc+4) + SL((u8)_opcode,1));
			u16 b;

			RW(a,b);
			REG(8)=(u32)(s32)(s16)b;
			res+=04;
			SH2LOGP(@[PC+%d]\x2cR%d,"MOV.WI",SL((u8)_opcode,1),Ri(8));
		}
		break;
		case 0xa:
		{
			s32 a = _opcode&0xFFF;
			if(a&0x800)
				a-=0x1000;
			SH2LOGP(%X,"BRA",_pc+SL(a,1)+4);
			SH2BRANCH(2,_pc+SL(a,1)+4);
			res++;
		}
		break;
		case 0xb:
			{
				s32 a = _opcode&0xFFF;
				if(a&0x800)
					a-=0x1000;
				SH2LOGP(%X,"BSR",_pc+SL(a,1)+4);
				_pr=_pc+4;
				STORECALLLSTACK(_pr);
				SH2BRANCH(2,_pc+SL(a,1)+4);
				res++;
			}
		break;
		case 0xC:
			switch(SR(_opcode,8)&0xf){
				case 5:
				{
					u32 a =(_gbr+SL((u8)_opcode,1));
					u16 u;

					RW(a,u);
					REG_0=(u32)(s32)(s16)u;
					res+=04;
					SH2LOGP(@[%d+gbr]\x2cR%d,"MOV.WLG",SL((u8)_opcode,1),0);
				}
				break;
				case 7:
					REG_0=((_pc+4) & ~3) + SL((u8)_opcode,2);
					res++;
					SH2LOGP(@[PC+%d]\x2cR0,"MOVA",SL((u8)_opcode,2));
				break;
				case 8:
					if((REG_0 & (u8)_opcode)==0)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGP(#%d\x2cR0,"TSTI",(u8)_opcode);
				break;
				case 9:
					REG_0&=(u32)(u8)_opcode;
					SH2LOGP(#%d\x2cR0,"ANDI",(u32)(u8)_opcode);
				break;
				case 0xa:
					REG_0^=(u32)(u8)_opcode;
					SH2LOGP(#%d\x2cR0,"XORI",(u32)(u8)_opcode);
				break;
				case 0xb:
					REG_0|=(u8)_opcode;
					SH2LOGP(#%d\x2cR0,"ORI",(u32)(u8)_opcode);
				break;
				default:
					SH2LOGE();
				break;
			}
		break;
		case 0xd:
		{
			u32 a = ((_pc+4) & ~3) + SL((u8)_opcode,2);

			RL(a,REG(8));
			res+=04;
			SH2LOGP(@[PC+%d]\x2cR%d\x20%x\x20%x,"MOV.LI",SL((u8)_opcode,2),Ri(8),REG(8),a);
		}
		break;
		case 0xe:
			REG(8)=(u32)(s32)(s8)_opcode;
			SH2LOGP(#%d\x2cR%d,"MOV",(int)(s8)_opcode,Ri(8));
		break;
		default:
			SH2LOGE();
		break;
	}
B:
	_pc+=2;
	if(_ppl.delay){
		if(--_ppl.delay==1)
			_pc=_ppl.pc;
	}
	else if(_ppl.irq){
		machine->OnEvent(0,(LPVOID)-1);
		_ppl.irq=0;
	}
	return res;
}

int SH2Cpu::Destroy(){
	//printf("Destroy %s\n",__FILE__);
	CCore::Destroy();
	if(_ioreg)
		delete []_ioreg;
	_ioreg=NULL;
	return 0;
}

int SH2Cpu::Reset(){
	CCore::Reset();
	_cycles=0;
	_pc=0;
	_sr=SH_I;
	_vbr=_gbr=0;
	_mach=_macl=0;
	_ppl._value=0;
	memset(_ioreg,0,(0x200+16)*sizeof(u32));
	return 0;
}

int SH2Cpu::Init(void *){
	u32 n=((0x10+0x200)*sizeof(u32)) + (0x100*sizeof(CoreMACallback)*2);
	if(!(_ioreg = new u8[n]))
		return -1;
	_regs=(u8 *)&_ioreg[0x200*sizeof(u32)];
	_portfnc_write = (CoreMACallback *)&((u32 *)_regs)[16];
	_portfnc_read = &_portfnc_write[0x100];
	memset(_ioreg,0,n);
	return 0;
}

int SH2Cpu::SetIO_cb(u32 a,CoreMACallback w,CoreMACallback r){
	_portfnc_write[SR(a,24)]=w;
	_portfnc_read[SR(a,24)]=r;
	return 0;
}

u16 SH2Cpu::rotate_left(u16 value, int n){
   u16 aux = value>>(16-n);
   return ((value<<n)|aux);
}

u16 SH2Cpu::rotxor(u16 val, u16 x){
	u16 res;

	res = val + rotate_left(val,2);
	res= rotate_left(res,4) ^ (res & (val ^ x));

	/*asm volatile(
		"movzw %1,%%rax\n"
		"rolw $2,%%ax\n"
		"movzw %1,%%rcx\n"
		"addw %1,%%ax\n"
		"xorw %2,%%cx\n"
		"movw %%ax,%%dx\n"
		"rolw $4,%%ax\n"
		"andw %%cx,%%dx\n"
		"xorw %%dx,%%ax\n"
		"movw %%ax,%0\n"
	: "=m"(res) : "m" (val),"m" (x) :  "rcx","rax","rdx","memory");
*/
	return res;
}

u32 SH2Cpu::_decrypt(u32 address){
	u16 val;

	address ^= _key1;
	val = rotxor(~(u16)address, (u16)_key2);
	val ^= ~(address >> 16);
	val = rotxor(val, _key2 >> 16);
	val ^= (u16)address ^ (u16)_key2;
	return val | (val << 16);
}

int SH2Cpu::Disassemble(char *dest,u32 *padr){
	u32 op,adr;
	char c[200],cc[100];
	u8 *p;

	*((u64 *)c)=0;
	*((u64 *)cc)=0;

	adr = *padr;
	RMAP_PC((adr&~3),p,R);

	sprintf(c,"%08X ",adr);

	*((u64 *)cc)=0;
	op=_opcode;
	RWPC(adr,_opcode);

	adr += 2;
	switch(SR(_opcode,12)){
		case 0:
			switch(_opcode & 0x3f){
				case 2:
					SH2LOGD(SR\x2cR%d,"STC",Ri(8));
				break;
				case  3:
					SH2LOGD(R%d,"BSRF",Ri(8));
				break;
				case 4:
				case 0x14:
				case 0x24:
				case 0x34:
					SH2LOGD(R%d\x2c@[R%d+R0],"MOV.BS",Ri(4),Ri(8));
				break;
				case 0x5:
				case 0x15:
				case 0x25:
				case 0x35:
					SH2LOGD(R%d\x2c@[R0+R%d],"MOV.WS0",Ri(4),Ri(8));
				break;
				case 0x6:
				case 0x16:
				case 0x26:
				case 0x36:
					SH2LOGD(R%d\x2c@[R0+R%d],"MOV.LS0",Ri(4),Ri(8));
				break;
				case 7:
				case 0x17:
				case 0x27:
				case 0x37:
					SH2LOGD(R%d\x2cR%d,"MULL",Ri(4),Ri(8));
				break;
				case 0x8:
					SH2LOGD(NOARG,"CLRT");
				break;
				case 0x9:
					SH2LOGD(NOARG,"NOP");
				break;
				case 0xb:
					SH2LOGD(NOARG,"RTS");
				break;
				case 0x12:
					SH2LOGD(GBR\x2cR%d,"STC",Ri(8));
				break;
				case 0x19:
					SH2LOGD(NOARG,"DIV0U");
				break;
				case 0x1a:
					SH2LOGD(MACL\x2cR%d,"STS",Ri(8));
				break;
				case 0x22:
					SH2LOGD(VBR\x2cR%d,"STC",Ri(8));
				break;
				case 0xe:
				case 0x1e:
				case 0x2e:
				case 0x3e:
					SH2LOGD(@[R0+R%d]\x2cR%d,"MOV.LL0",Ri(4),Ri(8));
				break;
				case 0x23:
					SH2LOGD(R%d,"BRAF",Ri(8));
				break;
				case 0x29:
					SH2LOGD(R%d,"MOVT",Ri(8));
				break;
				case 0x2a:
					SH2LOGD(PR\x2cR%d,"STS",Ri(8));
				break;
				case 0x2b:
					SH2LOGD(NOARG,"RTE");
				break;
				case 0xc:
				case 0x1c:
				case 0x2c:
				case 0x3c:
					SH2LOGD(@[R0+R%d]\x2cR%d,"MOV.BL0",Ri(4),Ri(8));
				break;
				case 0xd:
				case 0x1d:
				case 0x2d:
				case 0x3d:
					SH2LOGD(@[R0+R%d]\x2cR%d,"MOV.WL0",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 1:
			SH2LOGD(R%d\x2c@[R%d+%d],"MOV.LS4",Ri(4),Ri(8),SL(_opcode&0xF,2));
		break;
		case 2:
			switch(_opcode & 0xf){
				case 0:
					SH2LOGD(R%d\x2c@R%d,"MOV.BS",Ri(4),Ri(8));
				break;
				case 1:
					SH2LOGD(R%d\x2c@R%d,"MOV.WS",Ri(4),Ri(8));
				break;
				case 2:
					SH2LOGD(R%d\x2c@R%d,"MOV.LS",Ri(4),Ri(8));
				break;
				case 4:
					SH2LOGD(R%d\x2c@-R%d,"MOV.BM",Ri(4),Ri(8));
				break;
				case 6:
					SH2LOGD(R%d\x2c@-R%d,"MOV.LM",Ri(4),Ri(8));
				break;
				case 7:
					SH2LOGD(R%d\x2cR%d,"DIV0S",Ri(4),Ri(8));
				break;
				case 8:
					SH2LOGD(R%d\x2cR%d,"TST",Ri(4),Ri(8));
				break;
				case 9:
					SH2LOGD(R%d\x2cR%d,"AND",Ri(4),Ri(8));
				break;
				case 0xa:
					SH2LOGD(R%d\x2cR%d,"XOR",Ri(4),Ri(8));
				break;
				case 0xb:
					SH2LOGD(R%d\x2cR%d,"OR",Ri(4),Ri(8));
				break;
				case 0xd:
					SH2LOGD(R%d\x2cR%d,"XTRCT",Ri(4),Ri(8));
				break;
				case 0xe:
					SH2LOGD(R%d\x2cR%d,"MULU.W",Ri(4),Ri(8));
				break;
				case 0xf:
					SH2LOGD(R%d\x2cR%d,"MULS.W",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"");
				break;
			}
		break;
		case 3:
			switch(_opcode & 0xf){
				case 0:
					SH2LOGD(R%d\x2cR%d,"CMPEQ",Ri(4),Ri(8));
				break;
				case 0x2:
					SH2LOGD(R%d\x2cR%d,"CMPHS",Ri(4),Ri(8));
				break;
				case 0x3:
					SH2LOGD(R%d\x2cR%d,"CMPGE",Ri(4),Ri(8));
				break;
				case 4:
					SH2LOGD(R%d\x2cR%d,"DIV1",Ri(4),Ri(8));
				break;
				case 5:
					SH2LOGD(R%d\x2cR%d,"DMULU",Ri(4),Ri(8));
				break;
				case 6:
					SH2LOGD(R%d\x2cR%d,"CMPHI",Ri(4),Ri(8));
				break;
				case 7:
					SH2LOGD(R%d\x2cR%d,"CMPGT",Ri(4),Ri(8));
				break;
				case 8:
					SH2LOGD(R%d\x2cR%d,"SUB",Ri(4),Ri(8));
				break;
				case 0xa:
					SH2LOGD(R%d\x2cR%d,"SUBC",Ri(4),Ri(8));
				break;
				case 0xc:
					SH2LOGD(R%d\x2cR%d,"ADD",Ri(4),Ri(8));
				break;
				case 0xe:
					SH2LOGD(R%d\x2cR%d,"ADDC",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 4:
			switch(_opcode & 0x3f){
				case 0:
					SH2LOGD(R%d,"SHLL",Ri(8));
				break;
				case 1:
					SH2LOGD(R%d,"SHLR",Ri(8));
				break;
				case 2:
					SH2LOGD(MACH\x2c@-R%d,"STS.L",Ri(8));
				break;
				case 4:
					SH2LOGD(R%d,"ROTL",Ri(8));
				break;
				case 6:
					SH2LOGD(@R%d+\x2cMACH,"LDS.L",Ri(8));
				break;
				case 7:
					SH2LOGD(@R%d+\x2cSR,"LDC.L",Ri(8));
				break;
				case 8:
					SH2LOGD(R%d,"SHLL2",Ri(8));
				break;
				case 9:
					SH2LOGD(R%d,"SHRL2",Ri(8));
				break;
				case 0xa:
					SH2LOGD(R%d\x2cMACH,"LDS",Ri(8));
				break;
				case 0xb:
					SH2LOGD(R%d,"JSR",Ri(8));
				break;
				case 0xe:
					SH2LOGD(R%d\x2cSR,"LDC",Ri(8));
				break;
				case 0x10:
					if(--REG(8) == 0)
						BS(_sr,SH_T);
					else
						BC(_sr,SH_T);
					SH2LOGD(R%d,"DT",Ri(8));
				break;
				case 0x11:
					SH2LOGD(R%d\x2cPZ,"CMPPZ",Ri(8));
				break;
				case 0x12:
					SH2LOGD(MACL\x2c@-R%d,"STS.L",Ri(8));
				break;
				case 0x15:
					SH2LOGD(R%d,"CMPPL",Ri(8));
				break;
				case 0x16:
					SH2LOGD(@R%d+\x2cMACL,"LDS.L",Ri(8));
				break;
				case 0x17:
					SH2LOGD(@R%d+\x2cGBR,"LDS.L",Ri(8));
				break;
				case 0x18:
					SH2LOGD(R%d,"SHLL8",Ri(8));
				break;
				case 0x19:
					SH2LOGD(R%d,"SHRL8",Ri(8));
				break;
				case 0x1a:
					SH2LOGD(R%d\x2cMACL,"LDS",Ri(8));
				break;
				case 0x1e:
					SH2LOGD(R%d\x2cGBR,"LDC",Ri(8));
				break;
				case 0x21:
					SH2LOGD(R%d,"SHAR",Ri(8));
				break;
				case 0x22:
					SH2LOGD(PR\x2c-R%d,"STS.L",Ri(8));
				break;
				case 0x24:
					SH2LOGD(R%d,"ROTCL",Ri(8));
				break;
				case 0x25:
					SH2LOGD(R%d,"ROTCR",Ri(8));
				break;
				case 0x26:
					SH2LOGD(@R%d+\x2cPR,"LDS.L",Ri(8));
				break;
				case 0x28:
					SH2LOGD(R%d,"SHLL16",Ri(8));
				break;
				case 0x29:
					SH2LOGD(R%d,"SHRL16",Ri(8));
				break;
				case 0x2a:
					SH2LOGD(R%d\x2cPR,"LDS",Ri(8));
				break;
				case 0x2b:
					SH2LOGD(@R%d,"JMP",Ri(8));
				break;
				case 0x2e:
					SH2LOGD(R%d\x2cVBR,"LDC",Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 5:
			SH2LOGD(@[R%d+%d]\x2cR%d,"MOV.LL4",Ri(4),SL(_opcode&0xf,2),Ri(8));
		break;
		case 6:
			switch(_opcode & 0xf){
				case 0:
					SH2LOGD(@R%d\x2cR%d,"MOV.BL",Ri(4),Ri(8));
				break;
				case 1:
					SH2LOGD(@R%d\x2cR%d,"MOV.WL",Ri(4),Ri(8));
				break;
				case 2://(Rm)->Rn
					SH2LOGD(@R%d\x2cR%d,"MOV.LL",Ri(4),Ri(8));
				break;
				case 3:
					SH2LOGD(R%d\x2cR%d,"MOV",Ri(4),Ri(8));
				break;
				case 4:
					SH2LOGD(@R%d\x2cR%d,"MOV.BP",Ri(4),Ri(8));
					break;
				case 5:
					SH2LOGD(@R%d\x2cR%d,"MOV.WP",Ri(4),Ri(8));
					break;
				case 6:
					SH2LOGD(@R%d+\x2cR%d,"MOV.LP",Ri(4),Ri(8));
				break;
				case 7:
					SH2LOGD(R%d\x2cR%d,"NOT",Ri(4),Ri(8));
				break;
				case 8:
					SH2LOGD(R%d\x2cR%d,"SWAPW",Ri(4),Ri(8));
				break;
				case 9:
					SH2LOGD(R%d\x2cR%d,"SWAPW",Ri(4),Ri(8));
				break;
				case 0xb:
					SH2LOGD(R%d\x2cR%d,"NEG",Ri(4),Ri(8));
				break;
				case 0xc:
					SH2LOGD(R%d\x2cR%d,"EXTU.B",Ri(4),Ri(8));
				break;
				case 0xd:
					SH2LOGD(R%d\x2cR%d,"EXTU.W",Ri(4),Ri(8));
				break;
				case 0xe:
					SH2LOGD(R%d\x2cR%d,"EXTS.B",Ri(4),Ri(8));
				break;
				case 0xf:
					SH2LOGD(R%d\x2cR%d,"EXTS.W",Ri(4),Ri(8));
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 7:
			SH2LOGD(#%d\x2cR%d,"ADDI",(int)(s8)_opcode,Ri(8));
		break;
		case 8:
			switch(SR(_opcode,8)&0xf){
				case 0:
					SH2LOGD(@[R%d+%d]\x2cR0,"MOV.BS4",Ri(4),(_opcode&0xF));
				break;
				case 1:
					SH2LOGD(R0\x2c@[R%d+%d],"MOV.WS4",Ri(4),SL(_opcode&0xF,1));
				break;
				case 4:
					SH2LOGD(@[R%d+%d]\x2cR0,"MOV.BL4",Ri(4),(_opcode&0xF));
				break;
				case 5:
					SH2LOGD(@[R%d+%d]\x2cR0,"MOV.WL4",Ri(4),SL(_opcode&0xF,1));
				break;
				case 8:
					SH2LOGD(#%d\x2cR0,"CMPIM",(s32)(s8)_opcode);
				break;
				case 9:
				{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BT",a);
				}
				break;
				case 0xb:
				{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BF",a);
				}
				break;
				case 0xd:
				{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BTS",a);
				}
				break;
				case 0xf:
				{
					u32 a = ((adr+2)) + (int)SL((s8)_opcode,1);
					SH2LOGD(%x,"BFS",a);
				}
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 9:
			SH2LOGD(@[PC+%d]\x2cR%d,"MOV.WI",SL((u8)_opcode,1),Ri(8));
		break;
		case 0xa:
		{
			s32 a = _opcode&0xFFF;
			if(a&0x800)
				a-=0x1000;
			SH2LOGD(%X,"BRA",adr+SL(a,1)+2);
		}
		break;
		case 0xb:
			{
				s32 a = _opcode&0xFFF;
				if(a&0x800)
					a-=0x1000;
				SH2LOGD(%X,"BSR",adr+2+SL(a,1));
			}
		break;
		case 0xC:
			switch(SR(_opcode,8)&0xf){
				case 2:
					SH2LOGE();
				break;
				case 4:
					//SH2LOG("MOVBLG");
				break;
				case 5:
					SH2LOGD(@[%d+gbr]\x2cR%d,"MOV.WLG",SL((u8)_opcode,1),0);
				break;
				case 7:
					SH2LOGD(@[PC+%d]\x2cR0,"MOVA",SL((u8)_opcode,2));
				break;
				case 8:
					SH2LOGD(#%d\x2cR0,"TSTI",(u8)_opcode);
				break;
				case 9:
					SH2LOGD(#%d\x2cR0,"ANDI",(u32)(u8)_opcode);
				break;
				case 0xa:
					SH2LOGD(#%d\x2cR0,"XORI",(u32)(u8)_opcode);
				break;
				case 0xb:
					SH2LOGD(#%d\x2cR0,"ORI",(u32)(u8)_opcode);
				break;
				default:
					SH2LOGD(NOARG,"unkk");
				break;
			}
		break;
		case 0xd:
			SH2LOGD(@[PC+%d]\x2cR%d,"MOV.LI",SL((u8)_opcode,2),Ri(8));
		break;
		case 0xe:
			SH2LOGD(#%d\x2cR%d,"MOV",(int)(s8)_opcode,Ri(8));
		break;
		case 0xF:
		default:
			SH2LOGD(NOARG,"unkk");
		break;
	}
	sprintf(&c[8]," %6x ",_opcode);
A:
	strcat(c,cc);
	if(dest)
		strcpy(dest,c);
	*padr=adr;
	_opcode=op;
	return 0;
}

int SH2Cpu::_enterIRQ(int n,int v,u32 pc){
	LOGD("IRQ Enter %d %x\n",n,_pc);

	REG_SP -= 4;
	WL(REG_SP, _sr);
	REG_SP -= 4;
	WL(REG_SP, _pc);

	if (n > 15)
		BS(_sr,SH_I);
	else
		_sr = BS(BC(_sr,SH_I),SL(n,4));
	RLPC(_vbr + v * 4,_pc);
	EnterDebugMode(DEBUG_BREAK_IRQ);
	return 0;
}

int SH2Cpu::Query(u32 what,void *pv){
	switch(what){
		case ICORE_QUERY_SET_LOCATION:{
			u32 *p=(u32 *)pv;
			void *__tmp;

			RMAP_(p[0],__tmp,W);
			if(!__tmp) return -2;
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
				u32 *p=(u32 *)pv;
				switch(*p){
					default:
						*p=2;
						return 0;
					case 1:
						*p=4;
						return 0;
				}
			}
			return 0;
		case ICORE_QUERY_SET_REGISTER:
		{
			u32 v,*p=(u32 *)pv;
			v=p[0];
			if(v == (u32)-1){
				char *c;

				c=*((char **)&p[2]);
				if(!strcasecmp(c,"SR"))
					_sr=p[1];
				else
					return -1;
			}
			else
				REG_[v]=p[1];
		}
			return 0;
		case ICORE_QUERY_REGISTER:
			{
				u32 v,*p=(u32 *)pv;

				v=p[0];
				if(v == (u32)-1){
					char *c;

					c=*((char **)&p[2]);
					if(!strcasecmp(c,"SR"))
						v=_sr;
					else
						return -1;
				}
				else
					v=REG_[v];
				p[1]=v;
			}
			return 0;
		default:
			return CCore::Query(what,pv);
	}
	return -1;
}

int SH2Cpu::_dumpRegisters(char *p){
	char cc[1024];

	sprintf(p,"PC: %08X GBR: %08X PR: %08X MACL: %08X MACH: %08X\n\n",_pc,_gbr,_pr,_macl,_mach);
	for(int i=0;i<16;i++){
		sprintf(cc,"r%02d:%08x ",i,REG_[i]);
		strcat(p,cc);
		if((i&3)==3)
			strcat(p,"\n");
	}
	sprintf(cc,"\nVBR: %08X SR: %08X T:%c IL:%d Q:%c M:%c",_vbr,_sr,_sr&SH_T ? 49:48,SR(_sr&0xf0,4),_sr&SH_Q ? 49:48,_sr&SH_M ? 49:48);
	strcat(p,cc);
	return 0;
}

int SH2Cpu::SaveState(IStreamer *p){
#ifdef _DEVELOP
	if(CCore::SaveState(p))
		return -1;
	p->Write(_ioreg,0x210*sizeof(u32),0);
	p->Write(&_sr,sizeof(u32),0);
	p->Write(&_pr,sizeof(u32),0);
	p->Write(&_gbr,sizeof(u32),0);
	p->Write(&_vbr,sizeof(u32),0);
	p->Write(&_macl,sizeof(u32),0);
	p->Write(&_mach,sizeof(u32),0);
	p->Write(&_ppl,sizeof(_ppl),0);
#endif
	return 0;
}

int SH2Cpu::LoadState(IStreamer *p){
#ifdef _DEVELOP
	if(CCore::LoadState(p))
		return -1;
	p->Read(_ioreg,0x210*sizeof(u32),0);
	p->Read(&_sr,sizeof(u32),0);
	p->Read(&_pr,sizeof(u32),0);
	p->Read(&_gbr,sizeof(u32),0);
	p->Read(&_vbr,sizeof(u32),0);
	p->Read(&_macl,sizeof(u32),0);
	p->Read(&_mach,sizeof(u32),0);
	p->Read(&_ppl,sizeof(_ppl),0);
#endif
	return 0;
}