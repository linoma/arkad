#include "dcdev.h"

namespace dc{

#define SH34_AM 0x1fffffff

static struct __intc{
	struct{
	} _intc;
	u32 _priority[128];

	int reset(){
		return 0;
	};

	int Init(int,void *a,void *b,u32 f){
		return 0;
	};

	int write(u32 a,u32 v){
		switch(a){
		}
		return 1;
	};
} _intc;

static struct __tmu{
	u8 _tocr,_tstr,_tstr2;

	struct __channel{
		u32 _tcnt,_cycles,_tcor,_freq,_step,_count;
		u8 *_ioreg,_idx_ofs;

		union{
			u16 _value;
			u32 _value32;
			struct{
				unsigned int _tpsc:3;
				unsigned int _ckeg:2;
				unsigned int _unie:1;
				unsigned int _icpe:2;
				unsigned int _unf:1;
				unsigned int _icpf:1;
				unsigned int _a:6;
				unsigned int _enabled:1;
				unsigned int _changed:1;
				unsigned int _idx:2;
				unsigned int _step_shift:4;
			};
		} _tcr;

		int Init(int n,void *,void *b,u32 f){
			_tcr._value32=0;
			_freq=f;
			_tcr._idx=n;
			_idx_ofs=n*12;
			_ioreg=(u8 *)b;
			return 0;
		};

		int reset(){
			_tcnt=0xffffffff;
			_tcor=0xffffffff;
			_tcr._value=0;
			_tcr._enabled=0;
			_tcr._changed=0;
			_count=0;
			_cycles=__cycles;
			return 0;
		};

		int update(u32 *o){
			int res=0;
			u32 d,dd;

			if(!_tcr._enabled)
				goto Z;
			d=D_CYCLES(_cycles,_freq,__cycles);
			if(!SR(d,_tcr._step_shift))
				goto Z;
			_cycles=__cycles;
			dd =_tcnt;
			_tcnt-=d;
			//printf("data %llx %x\n",__data,_tcnt);
			if(_tcnt < dd)
				goto Z;
			res=1;
			_tcnt=_tcor;
			*(u16 *)&_ioreg[SH4_TCR0_ADDR + _idx_ofs] |=BV(8);
			Z:
			*(u32 *)&_ioreg[SH4_TCNT0_ADDR + _idx_ofs]=*o=_tcnt;
			return res;
		};//8C008520

		int enable(int v){
			_tcr._enabled=v;
			_tcr._changed=1;
			_cycles=__cycles;
			return 0;
		}

		int write(u32 a,u32 v){
			switch(a){
				case SH4_TCOR0_ADDR:
				case SH4_TCOR1_ADDR:
				case SH4_TCOR2_ADDR:
					_tcor=v;
					_tcr._changed=1;
				break;
				case SH4_TCNT0_ADDR:
				case SH4_TCNT1_ADDR:
				case SH4_TCNT2_ADDR:
				break;
				case SH4_TCR0_ADDR:
				case SH4_TCR1_ADDR:
				case SH4_TCR2_ADDR:{
					const int tcnt_div[8] = { 4, 16, 64, 256, 1024, 1, 1, 1 };

					_tcr._value=v;
					_step=tcnt_div[v&7];
					_tcr._step_shift=_log2(_step);
					_tcr._changed=1;
				}
				//printf("tmu %d cnt %x %u %u %u\n",_tcr._idx,_tcr._value,_step,_tcr._step_shift,_tcr._enabled);
				break;
			}
			return 0;
		};
	} _ch[3];

	int reset(){
		_tstr=0;
		_tocr=0;
		for(int i=0;i<sizeof(_ch)/sizeof(__channel);i++)
			_ch[i].reset();
		return 0;
	};

	int Init(int,void *a,void *b,u32 f){
		for(int i=0;i<sizeof(_ch)/sizeof(__channel);i++)
			_ch[i].Init(i,a,b,f);
		return 0;
	};

	int write(u32 a,u32 v){
		switch(a){
			case SH4_TOCR_ADDR:
			break;
			case SH4_TSTR_ADDR:
				_tstr=(u8)v;
				for(int i=0;i<sizeof(_ch)/sizeof(__channel);i++,v>>=1)
					_ch[i].enable(v&1);
			break;
		}
		return 1;
	};
} _tmu;

static struct __dmac{
	struct __channel{
		u8 *_mem,*_ioreg;
		u32 _freq,_len,_src,_dst;

		union{
			u32 _control;
			struct {
				unsigned int _DE:1;
				unsigned int _TE:1;
				unsigned int _IE:1;
				unsigned int _3:1;
				unsigned int _TS:3;
				unsigned int _TM:1;
				unsigned int _RS:4;//8
				unsigned int _SM:2;//12
				unsigned int _DM:2;//14
				unsigned int _AL:1;
				unsigned int _AM:1;
				unsigned int _RL:1;
				unsigned int _DS:1;
				unsigned int _23:4;
				unsigned int _DTC:1;
				unsigned int _DSA:3;
				unsigned int _STC:1;
				unsigned int _SSA:3;
				unsigned int _idx:2;
			};
		};

		int reset(){
			_control=0;
			_len=0;
			return 0;
		};

		int Init(int n,void *,void *b,u32 f){
			_freq=f;
			_ioreg=(u8 *)b;
			_idx=n;
			return 0;
		};

		int write(u32 a,u32 v){
			switch(a){
				case SH4_SAR0_ADDR:
				case SH4_SAR1_ADDR:
				case SH4_SAR2_ADDR:
				case SH4_SAR3_ADDR:
					_src=v;
				break;
				case SH4_DAR0_ADDR:
				case SH4_DAR1_ADDR:
				case SH4_DAR2_ADDR:
				case SH4_DAR3_ADDR:
					_dst=v;
				break;
				case SH4_DMATCR0_ADDR:
				case SH4_DMATCR1_ADDR:
				case SH4_DMATCR2_ADDR:
				case SH4_DMATCR3_ADDR:
					_len=v;
				break;
				case SH4_CHCR0_ADDR:
				case SH4_CHCR1_ADDR:
				case SH4_CHCR2_ADDR:
				case SH4_CHCR3_ADDR:
					_control=v;
					return 2;
				break;
			}
			return 1;
		};

		int _do_transfer(u32 a,void *r){
			return 0;
		}

		int start(ASIC &g){
			if(!_DE) return 1;
			if(!_len) _len=0x1000000;
			g.do_dma(this);
			return 0;
		};

		int end(){
			if(_IE) machine->OnEvent(17+_idx,1);
			_DE=0;
			_TE=1;
			SH4REG(SH4_DMATCR0_ADDR+SL(_idx,2)) =(SH4REG(SH4_DMATCR0_ADDR+SL(_idx,2)) & SH34_AM) | _len;
			SH4REG(SH4_SAR0_ADDR+SL(_idx,2)) =(SH4REG(SH4_SAR0_ADDR+SL(_idx,2)) & SH34_AM) | _src;
			SH4REG(SH4_DAR0_ADDR+SL(_idx,2)) =(SH4REG(SH4_DAR0_ADDR+SL(_idx,2)) & SH34_AM) | _dst;
			SH4REG(SH4_CHCR0_ADDR+SL(_idx,2)) = _control;
			return 0;
		};

	} _ch[4];

	int reset(){
		for(int i=0;i<sizeof(_ch)/sizeof(__channel);i++)
			_ch[i].reset();
		return 0;
	};

	int Init(int,void *a,void *b,u32 f){
		for(int i=0;i<sizeof(_ch)/sizeof(__channel);i++)
			_ch[i].Init(i,a,b,f);
		return 0;
	};
	u32 _dmaor;
} _dmac;

struct __maple_device{
	u32 _id,_port;

	int reset(){
		return 0;
	};

	virtual int write(u8 *,u32,u8 *){
		return 0;
	};

	__maple_device(){
		_id=0;
		_port=0;
	};

	int response(u8 *m,u8 code,u8 host,u8 len){
		m[0]=code;
		m[1]=_port;
		m[2]=host;
		m[3]=len-1;
		*(u32 *)&m[4]=_id;
		return 0;
	};
};

struct __dc_controller : __maple_device{
	__dc_controller() : __maple_device(){
		_id=0x01000000;
	};

	virtual int write(u8 *mem,u32,u8 *adr_r){
		printf("dc cont %x\n",*mem);
		switch(*mem){
			case 1:
				response(adr_r,5,0x20,29);
				return 0;
			case 9:
				response(adr_r,58,0x20,4);
				*(u16 *)&adr_r[8]=-1;
				return 0;
		}
	};
} dc_joy;

struct __maple{
	__maple_device *_devices[4];

	union{
		u32 _status;
		struct{
			unsigned int _enabled:1;
			unsigned int _changed:1;
			unsigned int _vblank:1;
			unsigned int _transfer:1;
			unsigned int _state:3;
		};
	};

	u32 _src,_cycles;
	u8 *_mem,*_ioreg;

	vector< pair<u32,vector<u32>> > _packets;

	__maple(){
		memset(_devices,0,sizeof(_devices));
		_devices[0]=&dc_joy;
	}

	int write(u32 a,u32 v){
		switch(a){
			case SB_MDEN:
				_transfer=v;
			break;
			case SB_MDTSEL:
				_vblank=v;
				printf("maple write %x %x %x\n",a,v,SB_MDEN);
			break;
			case SB_MDST:{
				u32 old=_enabled;
				_enabled= v;
				if(!old && _enabled && _transfer && !_vblank){
					_state=1;
					_do_transfer();
				}
			}
			break;
		}
		return 1;
	};

	int update(){
		switch(_state){
			case 0:
				if(!_vblank)
					return 0;
				_state=1;
			case 1:
				_do_transfer();
				break;
			case 2:
				for (auto pkt=_packets.begin();pkt!=_packets.end();pkt++){
					u32 *p;

					p=(u32 *)&_mem[MI_RAM + ((*pkt).first & (MS_RAM - 1))];
					//printf("maple answer %x %u\n",(*pkt).first,(u32)(*pkt).second.size());
					//memcpy(p,(*pkt).second.data(),(*pkt).second.size()*sizeof(u32));
				}
				_packets.clear();
				machine->OnEvent(12,1);
				_state=0;
				_enabled=0;
			break;
		}

		return 0;
	};

	int reset(){
		_cycles=__cycles;
		_status=0;
		for(int i=0;i<sizeof(_devices)/sizeof(__maple_device *);i++){
			if(_devices[i])
				_devices[i]->reset();
		}
		return 0;
	};

	int Init(int,void *a,void *b,u32){
		_mem=(u8 *)a;
		_ioreg=(u8 *)b;
		return reset();
	}
protected:
	int _do_transfer(){
		int last;
		u8 *mem;

		_src=ASIC_REG(SB_MDSTAR);
		last=0;
		_state=2;
		//EnterDebugMode();
		while(!last){

			mem=&_mem[MI_RAM + (_src & (MS_RAM - 1))];
/*
 * out++ = i->length | (i->dst_port << 16);

        *out++ = ((uint32)i->recv_buf) & MEM_AREA_CACHE_MASK;

        *out++ = (i->cmd & 0xff) | (maple_addr(i->dst_port, i->dst_unit) << 8)
                 | ((i->dst_port << 6) << 16)
                 | ((i->length & 0xff) << 24);
*/
			u32 header_1 = *(u32 *)mem;
			u32 header_2 = *(u32 *)&mem[4] & 0x1FFFFFE0;

			last = (header_1 >> 31) == 1;		// is last transfer ?
			u32 plen = (u8)header_1 + 1;	// transfer length (32-bit unit)
			u32 maple_op = (header_1 >> 8) & 7;
			u32 *p=(u32 *)&_mem[MI_RAM + ((_src+8) & (MS_RAM - 1))];
			//printf("mapple %x %x %x:%x\n",_src,maple_op,header_1,header_2);
			_src += 2 * 4 + plen * 4;
			_state=2;
			switch((header_1 >> 8) & 7){
				case 0:{
				//	printf("start %x ",p[0]);
					{
						int port;
						port=(header_1 >> 16) & 3;
						if(_devices[port])
							_devices[port]->write((u8 *)&mem[8],plen,&_mem[MI_RAM + (header_2 & (MS_RAM - 1))]);
						else
						{
							*(u32 *)&_mem[MI_RAM + (header_2 & (MS_RAM - 1))]=-1;
						}
				//		printf("port %x:%u ",port,SR(p[0],14)&3);
					}
					_packets.push_back({header_2,{(u32)-1}});
				//	printf("\n");
				}
				break;
				case 2://occupy
				case 3://reset
				case 4://occupy cancel
				case 7://nop
					printf("maple op %x\n",(header_1 >> 8) & 7);
				break;
			}
		}
		return 0;
	};
} _maple;

struct __pvr_dma{
	u32 pvr_addr;
	u32 sys_addr;
	u32 size;
	u8 sel;
	u8 dir;
	u8 flag;
	u8 start;

	int do_transfer(u32 v){
		printf("dma pvr %x %x %x %x\n",pvr_addr,sys_addr,size,v);
		return 0;
	};

	int write(u32 a,u32 v){
		//_changed=1;
		switch(a){
			default:
			break;
		}
		return 1;
	};
} _pvr_dma;

ASIC::ASIC() : PVR2(),AICA(),SH4Cpu(){
	_game=NULL;
}

ASIC::~ASIC(){

}

int ASIC::Init(int,void *a,void *b,u32 f){
	if(SH4Cpu::Init(a,0x300000))
		return -2;
	a=CCore::_mem;
	b=SHCORECPU::_ioreg=CCore::_mem + MI_IO;
	_gpu_mem=CCore::_mem + MI_VRAM;
	_ioreg=(u8 *)b;
	if(PVR2::Init(0,a,b,f))
		return -3;
	if(AICA::Init(0,a,b,f))
		return -4;
	_tmu.Init(0,a,&_ioreg[(MS_IO-1) - 0x10000],f);
	_dmac.Init(0,a,_ioreg,f);
	_maple.Init(0,a,b,f);
	_mmu.init(&_ioreg[(MS_IO-1) - 0x10000]);

	SetIO_cb(0x10000000,(CoreMACallback)&ASIC::fn_tafifo_poly_w,0);
	SetIO_cb(0x10800000,(CoreMACallback)&ASIC::fn_tafifo_yuv_w,0);
	return 0;
}

int ASIC::Reset(){
	if(_game) delete _game;
	_game=NULL;
	SH4Cpu::Reset();
	REG_SP=0x8d000000;
	PVR2::Reset();
	AICA::Reset();
	_tmu.reset();
	_dmac.reset();
	_maple.reset();

	IOREG(SB_BASE + SB_SBREV)=0xB;
	IOREG(SB_BASE + SB_G2ID)=0x12;
	IOREG(SB_BASE + SB_G1SYSM)=((0x0<<4) | (0x1));
	return 0;
}

int ASIC::Update(){
	u32 r=PVR2_REG(PVR2_SPG_VBLANK_INT);
	if(__line == ((r>>16)&0x3ff))
		return 4;
	if(__line == (r&0x3ff)) _maple.update();
	return 0;
}

int ASIC::Destroy(){
	PVR2::Destroy();
	AICA::Destroy();
	SH4Cpu::Destroy();
	return 0;
}

int ASIC::write(u32 a,u32 v){
	switch(a-SB_BASE){
		case SB_ISTNRM:
			ASIC_REG(SB_ISTNRM) &= ~(v|0xc0000000);
			return 0;
		case SB_C2DSTAT:
		case SB_LMMODE0:
			break;
	//	default:
	//		printf("%s %x %x %x %x %x\n",__FUNCTION__,a,a-SB_BASE,SB_LMMODE0,v);
	}
	return 1;
}

int ASIC::read(u32,u32 *){
	return 1;
}

int ASIC::_enterIRQ(int n,int v,u32 pc){
	u32 ln,lx,le;
	int level;

	ASIC_REG(SB_ISTNRM) |= BV(n);
	if (IOREG(0x005f6800+SB_ISTERR))
		IOREG(0x005f6800+SB_ISTNRM) |= IST_ERROR;
	else
		IOREG(0x005f6800+SB_ISTNRM) &= ~IST_ERROR;

	if (IOREG(0x005f6800+SB_ISTEXT))
		IOREG(0x005f6800+SB_ISTNRM) |= IST_G1G2EXTSTAT;
	else
		IOREG(0x005f6800+SB_ISTNRM) &= ~IST_G1G2EXTSTAT;

	ln=IOREG(0x005f6800+SB_ISTNRM) & IOREG(0x005f6800+SB_IML6NRM);
	lx=IOREG(0x005f6800+SB_ISTEXT) & IOREG(0x005f6800+SB_IML6EXT);
	le=IOREG(0x005f6800+SB_ISTERR) & IOREG(0x005f6800+SB_IML6ERR);
	if (ln | lx | le){
		level = 6;
		goto A;
	}

	ln=IOREG(0x005f6800+SB_ISTNRM) & IOREG(0x005f6800+SB_IML4NRM);
	lx=IOREG(0x005f6800+SB_ISTEXT) & IOREG(0x005f6800+SB_IML4EXT);
	le=IOREG(0x005f6800+SB_ISTERR) & IOREG(0x005f6800+SB_IML4ERR);
	if (ln | lx | le){
		level= 4;
		goto A;
	}

	ln=IOREG(0x005f6800+SB_ISTNRM) & IOREG(0x005f6800+SB_IML2NRM);
	lx=IOREG(0x005f6800+SB_ISTEXT) & IOREG(0x005f6800+SB_IML2EXT);
	le=IOREG(0x005f6800+SB_ISTERR) & IOREG(0x005f6800+SB_IML2ERR);
	if (ln | lx | le){
		level= 2;
		goto A;
	}
	return -1;
A:
	return SH4Cpu::_enterIRQ(level,0x600);
}

s32 ASIC::fn_tafifo_poly_w(u32 a,pvoid,pvoid data,u32){
	//printf("%s %x %llx\n",__FUNCTION__,a,*(u64 *)data);
	u64 v = *(u64 *)data;
	PVR2::write(0x10000000,(u32)v);
	return PVR2::write(0x10000000,(u32)SR(v,32));
}

s32 ASIC::fn_tafifo_yuv_w(u32 a,pvoid,pvoid,u32){
	printf("%s %x\n",__FUNCTION__,a);
	return 0;
}

s32 ASIC::fn_aica_regs_w(u32 a,pvoid mem,pvoid data,u32 f){
	printf("%s %x\n",__FUNCTION__,a);
	return 1;
}

s32 ASIC::fn_asic_regs_w(u32 a,pvoid mem,pvoid pdata,u32 f){
	switch((u8)SR(a,8)){
		case 0x68:
		case 0x69:
			switch(a-SB_BASE){
				case SB_C2DSTAT:
					_dmac._ch[2].write(SH4_DAR2_ADDR,*(u32 *)pdata);
				break;
				case SB_C2DLEN:{
					u32 a = *(u32 *)pdata;
					_dmac._ch[2].write(SH4_DMATCR2_ADDR,a>>5);
				}
				break;
				case SB_C2DST:{
					u32 a=*(u32 *)pdata;
					a=SH4REG(SH4_CHCR2_ADDR)|BV(2)|BV(0);
					printf("SB_C2DST %x %x %x\n",a,ASIC_REG(SB_C2DSTAT),ASIC_REG(SB_C2DLEN));
					if(_dmac._ch[2].write(SH4_CHCR2_ADDR,a) == 2)
						_dmac._ch[2].start(*this);
				}
				break;
				case SB_ISTNRM:
				case SB_ISTEXT:
				case SB_ISTERR:
				case SB_IML4NRM:
				case SB_IML6NRM:
				case SB_IML6ERR:
				break;
				default:
					printf("sys %x %x %x %x\n",a,*(u32 *)pdata,a-SB_BASE,SB_ISTNRM);
				break;
			}
		break;
		case 0x6c:
			switch(a-SB_BASE){
				default:
					printf("mapple %x %x\n",a,*(u32 *)pdata);
				break;
				case SB_MDSTAR:
				case SB_MDTSEL:
				case SB_MDEN:
				case SB_MDST:
					_maple.write(a-SB_BASE,*(u32 *)pdata);
				break;
			}
		break;
		case 0x70://gdrom
			printf("gdrom %x %x\n",a,*(u32 *)pdata);
		break;
		case 0x74://g1
			printf("g1 %x %x\n",a,*(u32 *)pdata);
		break;
		case 0x78://g2
			switch(a-SB_BASE){
				default:
					printf("g2 %x %x\n",a,*(u32 *)pdata);
				break;
				case SB_ADSUSP:
				case SB_E1SUSP:
				case SB_E2SUSP:
				case SB_DDSUSP:
				//	EnterDebugMode();
				break;
			}
		break;
		case 0x7c://pvr if
			switch(a-SB_BASE){
				case SB_PDST:
					_pvr_dma.do_transfer(*(u32 *)pdata);
				break;
				case SB_PDSTAP:
				case SB_PDSTAR:
				case SB_PDLEN:
				case SB_PDDIR:
				case SB_PDTSEL:
				case SB_PDEN:
				case SB_PDAPRO:
					_pvr_dma.write(a-SB_BASE,*(u32 *)pdata);
				break;
				default:
					printf("pvr2 if %x %x %x\n",a,*(u32 *)pdata,SB_PDST);
				break;
			}
		break;
		case 0x80:
		case 0x81:
			return PVR2::write(a,*(u32 *)pdata);
	}
	return ASIC::write(a,*(u32 *)pdata);
}

s32 ASIC::fn_sh4_regs_r(u32 a,pvoid mem,pvoid data,u32 f){
	switch(SH4REGI(a)){
		case SH4_TCNT0_ADDR:
		case SH4_TCR0_ADDR:
			_tmu._ch[0].update((u32 *)data);
		break;
		case SH4_TCNT1_ADDR:
		case SH4_TCR1_ADDR:
			_tmu._ch[1].update((u32 *)data);
		break;
		case SH4_PCTRA:
			SH4REG(SH4_PCTRA)=*(u32 *)data=0;
		break;
	}
	return 1;
}

s32 ASIC::fn_sh4_regs_w(u32 a,pvoid mem,pvoid data,u32 f){
	switch(SH4REGI(a)){
		case SH4_TOCR_ADDR:
		case SH4_TSTR_ADDR:
			_tmu.write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_TCOR0_ADDR:
		case SH4_TCNT0_ADDR:
		case SH4_TCR0_ADDR:
			_tmu._ch[0].write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_TCOR1_ADDR:
		case SH4_TCNT1_ADDR:
		case SH4_TCR1_ADDR:
			_tmu._ch[1].write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_SAR0_ADDR:
		case SH4_DAR0_ADDR:
		case SH4_DMATCR0_ADDR:
		case SH4_CHCR0_ADDR:
			_dmac._ch[0].write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_SAR1_ADDR:
		case SH4_DAR1_ADDR:
		case SH4_DMATCR1_ADDR:
		case SH4_CHCR1_ADDR:
			_dmac._ch[1].write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_SAR2_ADDR:
		case SH4_DAR2_ADDR:
		case SH4_DMATCR2_ADDR:
		case SH4_CHCR2_ADDR:
			_dmac._ch[2].write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_SAR3_ADDR:
		case SH4_DAR3_ADDR:
		case SH4_DMATCR3_ADDR:
		case SH4_CHCR3_ADDR:
			_dmac._ch[3].write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_DMAOR_ADDR:
		break;
		case SH4_CCR:
		break;
		case SH4_PCTRA:
			SH4REG(SH4_PDTRA)=0;
		break;
		case SH4_IPRA:
		case SH4_IPRB:
		case SH4_IPRC:
		case SH4_IPRD:
			_intc.write(SH4REGI(a),*(u32 *)data);
		break;
		case SH4_QACR0:
		case SH4_QACR1:
			//printf("%s PC:%x %x %x:%x\n",__FUNCTION__,_pc,a,SH4REGI(a),*(u32 *)data);
		break;
		default:
			//printf("%s PC:%x %x %x:%x\n",__FUNCTION__,_pc,a,SH4REGI(a),*(u32 *)data);
		break;
	}
	return 1;
}

int ASIC::do_dma(void *obj){
	int incs, incd, size;
	u32 src, dst,count;
	u8 *psrc,*pdst;
	__dmac::__channel *dma;

	const int dmasize[8] = { 8, 1, 2, 4, 32, 0, 0, 0 };

	dma = (__dmac::__channel *)obj;
	incd = dma->_DM;
	incs = dma->_SM;
	size = dmasize[dma->_TS];

	if(incd == 3 || incs == 3)
		return 0;
#define RMAP_(a,b,c)\
	if(a >= 0x0 && a <= 0x001fffff) b=&_mem[MI_BIOS+(a & (MS_BIOS-1))];\
	else if(a >= 0x00200000 && a <= 0x0021ffff) b=&_mem[MI_FLASH + (a & (MS_FLASH-1))];\
	else if(a >= 0x00500000 && a <= 0x005fffff) {b=&_mem[MI_IO +(a & (MS_IO-1))];__bus |= MAIOMASK;}\
	else if(a >= 0x00600000 && a <= 0x006007ff) {b=&_mem[MI_IO +(a & (MS_IO-1))];__bus |= MAIOMASK;}\
	else if(a >= 0x00700000 && a <= 0x0070ffff) {b=&_mem[MI_IO +(a & (MS_IO-1))];__bus |= MAIOMASK;}\
	else if(a >= 0x00710000 && a <= 0x0071000f) {b=0;}\
	else if(a >= 0x00800000 && a <= 0x009fffff) {b=0;}\
	else if(a >= 0x04000000 && a <= 0x05ffffff) b=&_mem[MI_VRAM+(a & (MS_VRAM-1))];\
	else if(a >= 0x08000000 && a <= 0x0bffffff) {b=0;}\
	else if(a >= 0x0c000000 && a <= 0x0fffffff) b=&_mem[MI_RAM + (a & (MS_RAM-1))];\
	else if((a >= 0x10000000 && a <= 0x107fffff) || (a >= 0x12000000 && a <= 0x127fffff)) {b=0;__bus |= MAIOMASK;}\
	else if((a >= 0x10800000 && a <= 0x10ffffff) || (a >= 0x12800000 && a <= 0x12ffffff)) {__bus |= MAIOMASK;b=0;}\
	else if(a >= 0x11000000 && a <= 0x117fffff) b=&_mem[MI_TEXRAM + (a & (MS_TEXRAM-1))];\
	else if(a >= 0x13000000 && a <= 0x137fffff) {printf("%x\n",a);b=0;}\
	else if(a >= 0x14000000 && a <= 0x17ffffff) {b=0;}\
	else if(a >= 0x18000000 && a <= 0x1bffffff) {b=0;}\
	else if(a >= 0x1c000000 && a <= 0x1c000fff) b=&_mem[MI_DCACHE+(a & 0xfff)];\
	else if(a >= 0x1e000000 && a <= 0x1e000fff) b=&_mem[MI_DCACHE+(a & 0xfff)];\
	else if(a >= 0xe0000000 && a <= 0xe3ffffff) {b=0;}\
	else if(a >= 0xf0000000 && a <= 0xf7ffffff) {b=0;__bus |= MAIOMASK;}\
	else if(a >= 0xff000000 && a <= 0xffffffff) {b=&_mem[MI_IO+(MS_IO-1) - 0x10000 + SH4REGI(a)];__bus |= MAIOMASK;}\
	else {printf("def unk mem %x %x %x\n",a,_pc,_mmu._regs[(SH4_MMUCR)]);b=0;}

	src   = dma->_src;
	dst   = dma->_dst;
	count = dma->_len;
printf("DMA %d %x %x C:%08x L:%u %u\n",dma->_idx,dma->_src,dma->_dst,dma->_control,dma->_len,size);
	/*
	if (timermode == 1) // timer actvated after a time based on the number of words to transfer
	{
		m_dma_timer_active[channel] = 1;
		m_dma_timer[channel]->adjust(cycles_to_attotime(2*count+1), channel);
	}
	else if (timermode == 2) // timer activated immediately
	{
		m_dma_timer_active[channel] = 1;
		m_dma_timer[channel]->adjust(attotime::zero, channel);
	}*/

	src &= SH34_AM;
	dst &= SH34_AM;

	switch(size){
		case 1: // 8 bit
			for(;count > 0; count --){
				if(incs == 2)
					src --;
				if(incd == 2)
					dst --;
				if(incs == 1)
					src ++;
				if(incd == 1)
					dst ++;
			}
			break;
		case 2: // 16 bit
			src &= ~1;
			dst &= ~1;
			for(;count > 0; count --){
				if(incs == 2) src -= 2;
				if(incd == 2) dst -= 2;
				if(incs == 1) src += 2;
				if(incd == 1) dst += 2;
			}
			break;
		case 8: // 64 bit
			src &= ~7;
			dst &= ~7;
			for(;count > 0; count --){
				u64 value;

				if(incs == 2) src -= 8;
				if(incd == 2) dst -= 8;

				RQ_(src,value,R,NOARG);WQ_(dst,value,W,NOARG);

				if(incs == 1) src += 8;
				if(incd == 1) dst += 8;
			}
			break;
		case 4: // 32 bit
			src &= ~3;
			dst &= ~3;
			for(;count > 0; count --){
				u32 value;
				if(incs == 2)
					src -= 4;
				if(incd == 2)
					dst -= 4;
				RL_(src,value,R,NOARG);
				dma->_do_transfer(value,&value);
				WL_(dst,value,W,NOARG);
				//*(u32 *)&pdst[dst]=*(u32 *)&psrc[src];
				if(incs == 1)
					src += 4;
				if(incd == 1)
					dst += 4;
			}
			break;
		case 32:
			src &= ~31;
			dst &= ~31;
			for(;count > 0; count --){
				u64 value;

				if(incs == 2)
					src -= 32;
				if(incd == 2)
					dst -= 32;
				RQ_(src,value,R,NOARG);WQ_(dst,value,W,NOARG);
				RQ_(src+8,value,R,NOARG);WQ_(dst+8,value,W,NOARG);
				RQ_(src+16,value,R,NOARG);WQ_(dst+16,value,W,NOARG);
				RQ_(src+24,value,R,NOARG);WQ_(dst+24,value,W,NOARG);
				if(incs == 1)
					src += 32;
				if(incd == 1)
					dst += 32;
			}
			break;
	}
	dma->_src=src;
	dma->_dst=dst;
	dma->_len=count;
	dma->end();
	return 0;
}

int ASIC::descrambl_buffer(const u8 *src, u8 *dst, u32 size){
	return _descrambler.descrambl(src,dst,size);
}

u32 ASIC::__descrambler::rand(){
	_seed = (_seed * 2109 + 9273) & 0x7fff;
	return (u16)(_seed + 0xc000);
}

int ASIC::__descrambler::load_chunk(const u8* &src, u8 *ptr, u32 sz){
	if(sz > MB(2)) return -1;
	sz /= 32;

	for (u32 i = 0; i < sz; i++)
		_idx[i] = i;
	for (int i = sz - 1; i >= 0; --i){
		int x = (rand() * i) >> 16;
		{
			int v=_idx[x];
			_idx[x]=_idx[i];
			_idx[i]=v;
		}
		memcpy(ptr + 32 * _idx[i], src, 32);
		src += 32;
	}
	return 0;
}

int ASIC::__descrambler::descrambl(const u8 *src, u8 *dst, u32 size){
	u32 chunksz;

	if(!_idx && !(_idx = new int[MB(2)/32])) return -1;
	_seed = (u32)(u16)size;
	for (chunksz = MB(2); chunksz >= 32; chunksz >>= 1){
		while (size >= chunksz){
			load_chunk(src, dst, chunksz);
			size -= chunksz;
			dst += chunksz;
		}
	}
	if (size)
		memcpy(dst, src, size);
	return 0;
}

};