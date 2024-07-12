
static char prs[][3]={"zr","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7",
	"s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"};

MIPSCORECPU::MIPSCORECPU() : CCore(){
	_regs=NULL;
	_io_regs=NULL;
	_freq=MHZ(33);
}

MIPSCORECPU::~MIPSCORECPU(){
	Destroy();
}

int MIPSCORECPU::Destroy(){
	CCore::Destroy();
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int MIPSCORECPU::Reset(){
	CCore::Reset();
	if(_regs)
		memset(_regs,0,sizeof(u32)*40);
	_jump=0;
	_irq_pending=0;
	return 0;
}

int MIPSCORECPU::Init(void *m){
	u32 n;

	if(!(_regs = new u8[n=40*sizeof(u32)]))
		return -1;
	memset(_regs,0,n);
	_mem=(u8 *)m;
	//_bk.push_back(0x80000000800b13c0);
	return 0;
}

int MIPSCORECPU::SetIO_cb(u32 a,CoreMACallback w,CoreMACallback r){
	if(!_portfnc_write && !_portfnc_read)
		return -1;
	if(w  && _portfnc_write)
		_portfnc_write[RMAP_IO(a)]=w;
	if(r && _portfnc_read)
		_portfnc_read[RMAP_IO(a)]=r;
	return 0;
}

int MIPSCORECPU::_dumpRegisters(char *p){
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

//8006e3f8
int MIPSCORECPU::_exec(u32 status){
	int ret;

	ret=1;
	RLPC(_pc,_opcode);
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
				case 0x20://'ixme add oveflow xcepion
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"ADD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x21:
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"ADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x22:
					REG_(RD(_opcode)) = REG_(RS(_opcode))-REG_(RT(_opcode));
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
					if((s32)REG_(RS(_opcode))>=0){
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
			if(REG_(RT(_opcode))!=REG_(RS(_opcode))){
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
			u8  b;
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

int MIPSCORECPU::Disassemble(char *dest,u32 *padr){
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

int MIPSCORECPU::Query(u32 what,void *pv){
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

int MIPSCORECPU:: _enterIRQ(int n,u32 pc){
	return _jump==0 ? 0 : 1;
}

static char prs[][3]={"zr","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7",
	"s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"};

MIPSCORECPU::MIPSCORECPU() : CCore(){
	_regs=NULL;
	_io_regs=NULL;
	_freq=MHZ(33);
}

MIPSCORECPU::~MIPSCORECPU(){
	Destroy();
}

int MIPSCORECPU::Destroy(){
	CCore::Destroy();
	if(_regs)
		delete []_regs;
	_regs=NULL;
	return 0;
}

int MIPSCORECPU::Reset(){
	CCore::Reset();
	if(_regs)
		memset(_regs,0,sizeof(u32)*40);
	_jump=0;
	_irq_pending=0;
	return 0;
}

int MIPSCORECPU::Init(void *m){
	u32 n;

	if(!(_regs = new u8[n=40*sizeof(u32)]))
		return -1;
	memset(_regs,0,n);
	_mem=(u8 *)m;
	//_bk.push_back(0x80000000800b13c0);
	return 0;
}

int MIPSCORECPU::SetIO_cb(u32 a,CoreMACallback w,CoreMACallback r){
	if(!_portfnc_write && !_portfnc_read)
		return -1;
	if(w  && _portfnc_write)
		_portfnc_write[RMAP_IO(a)]=w;
	if(r && _portfnc_read)
		_portfnc_read[RMAP_IO(a)]=r;
	return 0;
}

int MIPSCORECPU::_dumpRegisters(char *p){
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

//8006e3f8
int MIPSCORECPU::_exec(u32 status){
	int ret;

	ret=1;
	RLPC(_pc,_opcode);
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
				case 0x20://'ixme add oveflow xcepion
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"ADD",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x21:
					REG_(RD(_opcode)) = REG_(RS(_opcode))+REG_(RT(_opcode));
					__R3000F(%s\x2c%s\x2c%s,"ADDU",prs[RD(_opcode)],prs[RS(_opcode)],prs[RT(_opcode)]);
				break;
				case 0x22:
					REG_(RD(_opcode)) = REG_(RS(_opcode))-REG_(RT(_opcode));
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
					if((s32)REG_(RS(_opcode))>=0){
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
			if(REG_(RT(_opcode))!=REG_(RS(_opcode))){
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
			u8  b;
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

int MIPSCORECPU::Disassemble(char *dest,u32 *padr){
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

int MIPSCORECPU::Query(u32 what,void *pv){
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

int MIPSCORECPU:: _enterIRQ(int n,u32 pc){
	return _jump==0 ? 0 : 1;
}
