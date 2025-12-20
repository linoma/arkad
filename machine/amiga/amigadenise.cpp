#include "amigadenise.h"
#include "amiga500m.h"
#include <map>

namespace amiga{

#define AMIGARGB(c) (SR((c) & 0xf00,7)|SL((c)&0x0f0,2)|SL((c) & 15,11))

#define FETCH_PLANEPIXEL(plane) {\
	u32 a__= MAKELONG((_display._regs[REG_BPL1PTL + plane * 2]),\
		(_display._regs[REG_BPL1PTH+ plane * 2]));\
	a__ &= KB(512)-1;ACHIPREG(REG_BPL1DAT + plane) = *(u16 *)(_mem + a__);\
	_display._regs[REG_BPL1DAT + plane]=SWAP16(ACHIPREG(REG_BPL1DAT + plane));\
	a__+=2; _display._regs[REG_BPL1PTL+ plane * 2]=(LOWORD(a__));\
	_display._regs[REG_BPL1PTH+ plane * 2]=(HIWORD(a__));}

#define GET_PLANEPIXEL_ODD(pix,planes,obitoffs){\
	pix = (_display._regs[REG_BPL1DAT] >> obitoffs) & 1;\
	if (planes >= 3){\
		pix |= ((_display._regs[REG_BPL3DAT] >> obitoffs) & 1) << 2;\
		if (planes >= 5)\
			pix |= ((_display._regs[REG_BPL5DAT] >> obitoffs) & 1) << 4;\
	}\
}

#define GET_PLANEPIXEL_EVEN(pix,planes,ebitoffs){\
	pix = 0;\
	if (planes >= 2){\
		pix |= ((_display._regs[REG_BPL2DAT] >> ebitoffs) & 1) << 1;\
		if (planes >= 4){\
			pix |= ((_display._regs[REG_BPL4DAT] >> ebitoffs) & 1) << 3;\
			if (planes >= 6)\
				pix |= ((_display._regs[REG_BPL6DAT] >> ebitoffs) & 1) << 5;\
		}\
	}\
}

Denise::Denise() : GPU(){
	_display._mhpos=227;
	_display._mvpos=312;
	_width=720;
	_height=568;
}

Denise::~Denise(){
}

int Denise::Run(u8 *,int cyc,void *obj){
	int res;

	if((_cycles += cyc) < _scanline_cycles)
		return 0;
	_cycles -= _scanline_cycles;
	((amiga500m &)*(_machine=(amiga500m *)((LPCPUTIMEROBJ)obj)->param));//._pulseHSync(390);
	if(__line++ == 0){
		res=5;
		goto A;
	}
	res = 0;
//	if(_cycles >= 455){
//		res=-455;
//		_cycles -= 455;
//	}
	//res=0;
	if(__line==256)
		RenderLine();
	if(__line >= _vblank_end){//312 pal 262 ntsc _vblank_end
		if(_display._status._lace){
			_display._status._lof ^=1;
			ACHIPREG(REG_VPOSR) = (ACHIPREG(REG_VPOSR) & 0xFF7F)|SL(_display._status._lof,7);
		}
		machine->OnEvent(ME_ENDFRAME,0);
		__line=0;
		_cycles=0;
		res=0;
	}
A:
	ACHIPREG(REG_VHPOSR) = (u8)__line;
	ACHIPREG(REG_VPOSR) = (ACHIPREG(REG_VPOSR) & 0x80) | (__line & 0x100);
	//if(__line > 255) EnterDebugMode();
	return res;
}

int Denise::Init(void *a,void *b,u32 f){
	if(GPU::Init(_width,_height,0,KB(1)))//vb-start 246
		return -1;//59.59Hz
	_ioreg=(u16 *)b;
	_mem=(u8 *)a;
	_setBlankArea(0,_display._mvpos,0,_display._mhpos,59.94,f);
	_palette=((u8 *)_screen0 + _width*_height * 2);
	_dblpf_bitplanes[0]=(u8 *)&((u16 *)_palette)[50];
	_dblpf_bitplanes[1]=&_dblpf_bitplanes[0][256];
	for (int j = 0; j < 64; j++){
		int pf1pix = ((j >> 0) & 1) | ((j >> 1) & 2) | ((j >> 2) & 4);
		int pf2pix = ((j >> 1) & 1) | ((j >> 2) & 2) | ((j >> 3) & 4);
		_dblpf_bitplanes[0][j] = (pf1pix || !pf2pix) ? pf1pix : (pf2pix + 8);
		_dblpf_bitplanes[1][j] = pf2pix ? (pf2pix + 8) : pf1pix;
	}
	return 0;
}

int Denise::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	//_pcm_sync=stoi(m["pcm_sync"]);
	//_gpu_status |= GPU_STATUS_MULTISAMPLE_BILINEAR;
	return GPU::LoadSettings(v);
}

int Denise::write(u32 a,u16 v){
	u8 reg;
	int res =1;

	_display._regs[reg=(u8)SR(a,1)]=SWAP16(v);
	switch(reg){
		case REG_VPOSW:
			_display._status._lof=SR(v,7);
			ACHIPREG(REG_VPOSR) = (ACHIPREG(REG_VPOSR) & 0xFF7F)|SL(_display._status._lof,7);
			res=0;
			break;
		case REG_DMACON:
			_display._status._le = _display._status._lv &&
					(v & (DMACON_BPLEN | DMACON_DMAEN)) == (DMACON_BPLEN | DMACON_DMAEN);
			_display._status._se = _display._status._sv &&
					(v & (DMACON_SPREN | DMACON_DMAEN)) == (DMACON_SPREN | DMACON_DMAEN);
			res=0;
		break;
		case REG_BPLCON0:
			_display._status._lace=SR(v,13);
		case REG_BPLCON2:
		case REG_CLXCON:
			_display._changed._control=1;
		break;
		case REG_BPLCON1:
		case REG_BPL1PTH:
		case REG_BPL1PTL:
			_display._changed._layers=1;
		break;
		case REG_SPR0DATA:  case REG_SPR1DATA:  case REG_SPR2DATA:  case REG_SPR3DATA:
		case REG_SPR4DATA:  case REG_SPR5DATA:  case REG_SPR6DATA:  case REG_SPR7DATA:
			_display._sprites[(reg-REG_SPR0DATA)/4]._comparitor=1;
			_display._sprites[(reg-REG_SPR0DATA)/4]._live=0;
			_display._changed._sprites=1;
		//	if(reg==REG_SPR0DATA)
		//	printf("REG_SPR%uDATA %x\n",(reg-REG_SPR0DATA)/4,v);
		break;
		case REG_SPR0CTL:   case REG_SPR1CTL:   case REG_SPR2CTL:   case REG_SPR3CTL:
		case REG_SPR4CTL:   case REG_SPR5CTL:   case REG_SPR6CTL:   case REG_SPR7CTL:
			_display._sprites[(reg-REG_SPR0CTL)/4]._comparitor=0;
			_display._sprites[(reg-REG_SPR0CTL)/4]._ctl=1;
		//	if(reg==REG_SPR0CTL)
		//	printf("REG_SPR%uCTL %x\n",(reg-REG_SPR0CTL)/4,v);
			_display._changed._sprites=1;
		break;
		case REG_SPR0PTL:   case REG_SPR1PTL:   case REG_SPR2PTL:   case REG_SPR3PTL:
		case REG_SPR4PTL:   case REG_SPR5PTL:   case REG_SPR6PTL:   case REG_SPR7PTL:
			_display._changed._sprites=1;
			_display._sprites[(reg-REG_SPR0PTL)/2]._reload=1;
			_display._sprites[(reg-REG_SPR0PTL)/2]._live=1;
			//printf("REG_SPR%uPTL\n",(reg-REG_SPR0PTL)/2);
		break;
		case REG_DIWSTRT:
		case REG_DIWSTOP:
			_display._changed._win=1;
		break;
		case REG_DDFSTRT:
		case REG_DDFSTOP:
			_display._changed._size=1;
		break;
		case REG_AUD0LCH:
        case REG_AUD0LCL:
        case REG_AUD0LEN:
        case REG_AUD0PER:
        case REG_AUD0VOL:
        case REG_AUD0DAT:
        case REG_AUD1LCH:
        case REG_AUD1LCL:
        case REG_AUD1LEN:
        case REG_AUD1PER:
        case REG_AUD1VOL:
        case REG_AUD1DAT:
        case REG_AUD2LCH:
        case REG_AUD2LCL:
        case REG_AUD2LEN:
        case REG_AUD2PER:
        case REG_AUD2VOL:
        case REG_AUD2DAT:
        case REG_AUD3LCH:
        case REG_AUD3LCL:
        case REG_AUD3LEN:
        case REG_AUD3PER:
        case REG_AUD3VOL:
        case REG_AUD3DAT:
			printf("cazzo %x\n",reg*2);
		break;
		case REG_COLOR00:	case REG_COLOR01:	case REG_COLOR02:	case REG_COLOR03:
		case REG_COLOR04:	case REG_COLOR05:	case REG_COLOR06:	case REG_COLOR07:
		case REG_COLOR08:	case REG_COLOR09:	case REG_COLOR10:	case REG_COLOR11:
		case REG_COLOR12:	case REG_COLOR13:	case REG_COLOR14:	case REG_COLOR15:
		case REG_COLOR16:	case REG_COLOR17:	case REG_COLOR18:	case REG_COLOR19:
		case REG_COLOR20:	case REG_COLOR21:	case REG_COLOR22:	case REG_COLOR23:
		case REG_COLOR24:	case REG_COLOR25:	case REG_COLOR26:	case REG_COLOR27:
		case REG_COLOR28:	case REG_COLOR29:	case REG_COLOR30:	case REG_COLOR31:
			((u16 *)_palette)[reg-REG_COLOR00]=
				AMIGARGB(_display._regs[reg]);
		break;
	}

	return res;
}

int Denise::read(u32,u16 *){
	return 0;
}

int Denise::Reset(){
	_display.reset();
	ACHIPREG(REG_DENISEID)=0xffff;
	ACHIPREG(REG_BEAMCON0)=0x20;
	GPU::Reset();
	Clear();
	_cycles=0;
	return 0;
}

int Denise::Update(u32 flags){
	_display._status._draw=flags;
	return 0;
}

u16 inline _sprite_get_word(u8 idx){
	u16 ret;

	ret = idx&1;
	idx &= ~1;
	for(u8 i=7;idx;i--){
		ret |= SL(SL(SR(idx&0x80,7),i),i);
		idx=SL(idx,1);
	}
	return ret;
}

u32 Denise::_sprite_get_pixel(u32 x){
	u32 pix,collide;

	pix=0;
	for(int i=0;i<8;i++){
		if(_display._sprites[i]._comparitor){
			if(_display._sprites[i]._off==0){
				u32 hstart=SL((u8)_display._regs[REG_SPR0POS+i*4],1)|(_display._regs[REG_SPR0CTL+i*4]&1);

				if(hstart==x){
					u16 v[2];

					v[0]=_display._regs[REG_SPR0DATA + i*4];
					v[1]=_display._regs[REG_SPR0DATB + i*4];

					//_display._sprites[i]._shift = _display._sprite_bytes[(u8)v[0]] | (_display._sprite_bytes[v[0] >> 8] << 16) |
					//	(_display._sprite_bytes[(u8)v[1]] << 1) | (_display._sprite_bytes[v[1] >> 8] << 17);
					_display._sprites[i]._shift=_sprite_get_word(v[0]) | (_sprite_get_word(v[0]>>8)<<16) | (_sprite_get_word(v[1])<<1)
						|(_sprite_get_word(v[1]>>8)<<17);
					_display._sprites[i]._off=16;
				}
			}

			if(_display._sprites[i]._off){
				_display._sprites[i]._off--;
				pix |= (_display._sprites[i]._shift & 0xc0000000) >> (16 + 2 * (7 - i));
				_display._sprites[i]._shift <<= 2;
			}
		}
	}
	if(!pix)
		return 0;
	collide = pix | (pix >> 1);
	//collide |= (collide & ormask[_display._regs[REG_CLXCON] >> 12]) >> 2;
	collide = (collide & 1) | ((collide >> 3) & 2) | ((collide >> 6) & 4) | ((collide >> 9) & 8);
	//_display._regs[REG_CLXDAT] |= spritecollide[collide];
	ACHIPREG(REG_CLXDAT) = SWAP16(_display._regs[REG_CLXDAT]);

	for (int pair = 0; pix; pair++, pix >>= 4){
		if (pix & 0x0f){
			u32 result = (collide << 6) | (pair << 10);

			if (_display._regs[REG_SPR1CTL + 8 * pair] & 0x0080)
				return (pix & 0xf) | 0x10 | result;
			if (pix & 3)
				return (pix & 3) | 0x10 | (pair << 2) | result;
			return ((pix >> 2) & 3) | 0x10 | (pair << 2) | result;
		}
	}
	return pix;
}

int Denise::_sprite_update_dma(u32 y){
	u16 v,ctl;
	u32 vstart,vstop;
	int n,lino;

	lino=0;
	if(!_display._status._se) return 1;
	n=(_display._regs[REG_DDFSTRT]-0x14)/4;
	if(n>8)	n=8;
	for(int i=0;i<n;i++){
		if(_display._sprites[i]._reload && _display._sprites[i]._live){
			//_display._sprites[i]._pt=BELE32(ACHIPREG32_(_ioreg,REG_SPR0PTH + i * 4));
			_display._sprites[i]._pt=_display._regs[REG_SPR0PTL + i * 4];
			_display._sprites[i]._pt |=	SL(_display._regs[REG_SPR0PTH + i * 4],16);
	//printf("%d %x ",i,_display._sprites[i]._pt);lino++;
			if(_display._sprites[i]._pt > MB(1)) continue;
			//_display._sprites[i]._pt = BELE32(ACHIPREG32_(_ioreg,REG_SPR0PTH + i * 4));
			v=SWAP16(*(u16 *)(_mem + (_display._sprites[i]._pt & (MB(1)-1))));
			_display._sprites[i]._pt +=2;
			_display._regs[(REG_SPR0POS + i*4)]=v;

			v=SWAP16(*(u16 *)(_mem + (_display._sprites[i]._pt & (MB(1)-1))));
			_display._sprites[i]._pt +=2;
			_display._regs[(REG_SPR0CTL + i*4)]=v;

			//_display._regs[REG_SPR0PTL + i * 4]=_display._sprites[i]._pt&0x7fff;
			//_display._regs[REG_SPR0PTH + i * 4]=SR(_display._sprites[i]._pt,15)&3;

			//ACHIPREG32_(_ioreg,REG_SPR0PTH + i * 4)=BELE32(_display._sprites[i]._pt);
			_display._sprites[i]._comparitor=0;
			_display._sprites[i]._reload=0;
		}

		ctl=_display._regs[REG_SPR0CTL + i*4];
		vstart = (_display._regs[REG_SPR0POS + i*4] >> 8) | ((ctl << 6) & 0x100);
		vstop = (ctl >> 8) | ((ctl << 7) & 0x100);
		//if(!i) printf("%u %x %x %x\n",i,ctl,vstart,vstop);
		if(y==vstart){
			_display._sprites[i]._comparitor=1;
		//	if(!i) printf("%u %x %x %x\n",i,ctl,vstart,vstop);
		}
		if(y==vstop){
			_display._sprites[i]._comparitor=0;
			_display._sprites[i]._ctl=0;
			_display._sprites[i]._reload=1;
			_display._regs[(REG_SPR0POS + i*4)]=0;
			_display._regs[(REG_SPR0CTL + i*4)]=0;
		}
		if(_display._status._se && _display._sprites[i]._live && _display._sprites[i]._comparitor){
			v=*(u16 *)(_mem + (_display._sprites[i]._pt & (MB(1)-1)));
			_display._regs[(REG_SPR0DATA + i*4)]=SWAP16(v);
			_display._sprites[i]._pt+=2;
			v=*(u16 *)(_mem + (_display._sprites[i]._pt & (MB(1)-1)));
			_display._regs[(REG_SPR0DATB + i*4)]=SWAP16(v);
			_display._sprites[i]._pt+=2;
			//ACHIPREG32_(_ioreg,REG_SPR0PTH + i * 4)=SWAP32(_display._sprites[i]._pt);
			//_display._sprites[i]._reload=1;
			//_display._sprites[i]._live=1;
			//_display._sprites[i]._comparitor=1;
		}
	}
	//if(lino) printf("\n");
	return 0;
}

int Denise::RenderLine(u32 flags){
	amiga500dev::COPPERITEM *ci;
	u16 *p;
	u32 attr,xc,yc;

	if(!(p=(u16 *)_screen))
		return -1;
	if(!_display._status._draw)
		goto Z;
A:
	{
		ci=NULL;
		vector<amiga500dev::COPPERITEM> &opcodes=((amiga500m &)(*_machine))._agnus._copper._opcodes;
		auto it=opcodes.begin();
		if(it != opcodes.end())
			ci = &*it;

		attr=xc=yc=0;
		//if(_display._regs[REG_BPLCON0]==0xf00)
		//	write(REG_BPLCON0*2,0x52);
		for(int yy=0;yy<_display._mvpos*2;yy++){
			u16 *p0,*pp;
			int x,y=yy/2;
			u32 spripix,c,dd;

			dd=0;
			pp=p;
			if((yy&1) && (attr & 1)){
				memcpy(pp,p - _width,_width*sizeof(u16));
				goto A9;
			}
			attr &= ~(1|2|0x20);
			for(;ci && ci->_y < y && it != opcodes.end();){
				it++;
				ci= &*it;
			}
			_display._sprite._hide();
			_sprite_update_dma(y);

			for(x=0;x<(_display._mhpos*2);x++){
				for(;ci && ci->_x <= x && ci->_y==y && it != opcodes.end();){
					//if(!dd) printf("%u:%d:%d\t",y,_display._status._le,_display._planes);
					//printf("%u-%x:%x ",x,ci->_r*2,ci->_val);dd++;

					switch(ci->_r){
						case REG_INTREQ:
							//EnterDebugMode();
						case REG_COP1LCH:
						case REG_COP2LCH:
						case REG_COP2LCL:
						case REG_COP1LCL:
						case REG_COPJMP1:
						case REG_COPJMP2:
						case REG_DMACON:{
							u32 d[]={0xdff000|((u32)ci->_r*2),SWAP16(ci->_val),AM_WORD};
							cpu->Query(ICORE_QUERY_MEMORY_WRITE,d);
						}
						break;
						default:
							ACHIPREG(ci->_r)=(ci->_val);
							write(ci->_r*2,(ci->_val));
						break;
					}
					it++;
					ci=&*it;
				}

				c=((u16 *)_palette)[0];
				c=SL(c,16)|c;

				if(!(attr & 0x10)){
					Clear(c);
					attr |= 0x10;
				}

				if(_display._changed._control){
					u16 r=_display._regs[REG_BPLCON0];
					_display._planes = SR(r & (BPLCON0_BPU0 | BPLCON0_BPU1 | BPLCON0_BPU2),12);
					_display._status._hires = SR(r,15);
					_display._status._ham = SR(r,11);
					_display._status._dblpf = SR(r,10);

					_display._layers[_display.ODD]._priority=_display._regs[REG_BPLCON2]&7;
					_display._layers[_display.EVEN]._priority=SR(_display._regs[REG_BPLCON2],3)&7;

					_display._layers[_display.ODD]._mask=SR(_display._regs[REG_CLXCON],6)&0x15;
					_display._layers[_display.EVEN]._mask=SR(_display._regs[REG_CLXCON],6)&0x2a;
					//printf(" con %u %x %d %d %d P:%d %x %x",y,r,_display._ham,_display._status._hires,_display._dblpf,_display._planes,_display._regs[REG_DMACON],_display._regs[REG_COLOR00]);
				}

				if(_display._changed._size || _display._changed._control){
					_display._start = (_display._regs[REG_DDFSTRT] & 0xfc) * 2;
					_display._start += _display._status._hires ? 9 : 17;

					_display._stop = (_display._regs[REG_DDFSTOP] & 0xfc) * 2;
					_display._stop += _display._status._hires ? (9 + 15) : (17 + 15);

					if( _display._status._hires && ( (_display._regs[REG_DDFSTRT] ^ _display._regs[REG_DDFSTOP]) & 0x04) )
						_display._stop += 8;
					//printf(" size %u %u %u",y,_display._start,_display._stop);
				}

				if(_display._changed._win){
					int vstart = _display._regs[REG_DIWSTRT] >> 8;
					int vstop = _display._regs[REG_DIWSTOP] >> 8;

					int hstart = (u8)_display._regs[REG_DIWSTRT];
					int hstop = (u8)_display._regs[REG_DIWSTOP];

					/*	if (m_diwhigh_valid){
						vstart |= (CUSTOM_REG(REG_DIWHIGH) & 7) << 8;
						vstop  |= ((CUSTOM_REG(REG_DIWHIGH) >> 8) & 7) << 8;
						hstart |= ((CUSTOM_REG(REG_DIWHIGH) >> 5) & 1) << 8;
						hstop  |= ((CUSTOM_REG(REG_DIWHIGH) >> 13) & 1) << 8;
					}
					else*/
					{
						if((_display._regs[REG_DIWSTOP] & 0x8000)==0)
							vstop |= 0x100;
						hstop |= 0x100;
					}

					if (hstop < hstart){
						hstart = 0;
						hstop = 0x1ff;
					}

					if(hstart < 0x80)
						hstart=0x80;
					if(hstop > 0x1c1){
						hstop=0x1c1;
						if(hstart >_display._start)
							hstart=_display._start;
					}
					if(vstart < 0x2c)
						vstart=0x2c;
					if(vstop > 0x138)
						vstop=0x138;
					if((_display._win.top != vstart || _display._win.bottom != vstop)){
						attr &= ~4;
					}
					_display._win.left=hstart;
					_display._win.right=hstop;
					_display._win.top=vstart;
					_display._win.bottom=vstop;

					attr &= ~1;
					//printf(" win %u %lu %lu %lu %lu %lu %u:%u\n",y,_display._win.left,_display._win.top,_display._win.right,_display._win.bottom,
					//	_display._mvpos-(_display._win.bottom-_display._win.top),_display._start,_display._stop);
				}

				if(!(attr & 0x20) && x>=_display._start){
					attr |=0x20;
					//printf(" X:%u",x);
					u16 r=_display._regs[REG_BPLCON1];
					_display._layers[_display.ODD]._width = r & 0xf;//odd
					_display._layers[_display.EVEN]._width = ( r >> 4 ) & 0xf;
					if(_display._status._hires){
						_display._layers[_display.ODD]._ofs=15+_display._layers[_display.ODD]._width*2;
						_display._layers[_display.EVEN]._ofs=15+_display._layers[_display.EVEN]._width*2;
					}
					else{
						if(_display._regs[REG_DDFSTRT] & 0x400){
							_display._layers[_display.ODD]._width = ( _display._layers[_display.ODD]._width + 8 ) & 0x0f;
							_display._layers[_display.EVEN]._width = ( _display._layers[_display.EVEN]._width + 8 ) & 0x0f;
						}
						_display._layers[_display.ODD]._ofs=15+_display._layers[_display.ODD]._width;
						_display._layers[_display.EVEN]._ofs=15+_display._layers[_display.EVEN]._width;
					}
					for(int i=0;i<_display._planes;i++)
						_display._regs[REG_BPL1DAT + i]=0;
				}
			//	if(_display._changed._val) printf("\n");
				_display._changed._val=0;
				spripix=_sprite_get_pixel(x);

				if (_display._status._le && _display._planes > 0 &&
					y >= _display._win.top && y < _display._win.bottom){
						u8 pix;
						int pfpix0 = 0, pfpix1 = 0, collide;

					if(!(attr&4)){
						yc=SR(_height - (_display._win.bottom-_display._win.top),2);
						pp=p=(u16 *)_screen + _width * yc;
						//pp=p=(u16 *)_screen;// + _width * SR(_height - y,2);
					}
					attr |= 1|4;
					//(((_width/2)-_display._win.right+_display._win.left)/2);//fixme
					if(x >= _display._start){//odd

						if(x <= _display._stop + _display._layers[_display.ODD]._width){
							if(_display._layers[_display.ODD]._ofs==15){
								for(int i=0;i<_display._planes;i+=2){
									FETCH_PLANEPIXEL(i);
								}
							}
							GET_PLANEPIXEL_ODD(pix,_display._planes,_display._layers[_display.ODD]._ofs);
							pfpix0 |= pix;
							_display._layers[_display.ODD]._ofs--;
							if(_display._status._hires){
								if(_display._layers[_display.ODD]._ofs < 0){
									_display._layers[_display.ODD]._ofs=15;
									for(int i=0;i<_display._planes;i+=2){
										FETCH_PLANEPIXEL(i);
									}
								}
								GET_PLANEPIXEL_ODD(pix,_display._planes,_display._layers[_display.ODD]._ofs);
								pfpix1 |= pix;
								_display._layers[_display.ODD]._ofs--;
							}
							else
								pfpix1 |= pfpix0 & 0x15;

							if(_display._layers[_display.ODD]._ofs < 0)
								_display._layers[_display.ODD]._ofs=15;
						}

						if(x <= _display._stop + _display._layers[_display.EVEN]._width){//even
							if(_display._layers[_display.EVEN]._ofs==15){
								for(int i=1;i<_display._planes;i+=2){
									FETCH_PLANEPIXEL(i);
								}
							}
							GET_PLANEPIXEL_EVEN(pix,_display._planes,_display._layers[_display.EVEN]._ofs);
							pfpix0 |= pix;
							_display._layers[_display.EVEN]._ofs--;

							if(_display._status._hires){
								if(_display._layers[_display.EVEN]._ofs < 0){
									_display._layers[_display.EVEN]._ofs=15;
									for(int i=1;i<_display._planes;i+=2){
										FETCH_PLANEPIXEL(i);
									}
								}
								GET_PLANEPIXEL_EVEN(pix,_display._planes,_display._layers[_display.EVEN]._ofs);
								pfpix1 |= pix;
								_display._layers[_display.EVEN]._ofs--;
							}
							else
								pfpix1 |= pfpix0 & 0x2a;

							if(_display._layers[_display.EVEN]._ofs < 0)
								_display._layers[_display.EVEN]._ofs=15;
						}
					}

					collide = pfpix0 ^ _display._regs[REG_CLXCON];
					if ((collide & _display._layers[_display.ODD]._mask) == 0)
						_display._regs[REG_CLXDAT] |= (spripix >> 5) & 0x01e;
					if ((collide & _display._layers[_display.EVEN]._mask) == 0)
						_display._regs[REG_CLXDAT] |= (spripix >> 1) & 0x1e0;
					if ((collide & (_display._layers[_display.ODD]._mask | _display._layers[_display.EVEN]._mask)) == 0)
						_display._regs[REG_CLXDAT] |= 0x001;

					collide = pfpix1 ^ _display._regs[REG_CLXCON];
					if ((collide & _display._layers[_display.ODD]._mask) == 0)
						_display._regs[REG_CLXDAT] |= (spripix >> 5) & 0x01e;
					if ((collide & _display._layers[_display.EVEN]._mask) == 0)
						_display._regs[REG_CLXDAT] |= (spripix >> 1) & 0x1e0;
					if ((collide & (_display._layers[_display.EVEN]._mask | _display._layers[_display.ODD]._mask)) == 0)
						_display._regs[REG_CLXDAT] |= 0x001;
					ACHIPREG(REG_CLXDAT) = SWAP16(_display._regs[REG_CLXDAT]);
					if(x >= _display._win.left && x < _display._win.right){
						pix = spripix & 0x1f;
						s8 pri = (spripix >> 10);

						if(!(attr & 2)){
							attr |= 2;
							xc=SR(_display._mhpos*2 - (_display._win.right - _display._win.left ),2) + 8;
							pp += xc;
						}
						if(_display._status._ham)
							c=0;
						else if(_display._status._dblpf){
							if (pix){
								if ((pfpix0 & 0x15) && _display._layers[_display.ODD]._priority <= pri)
									pix = 0;
								if ((pfpix0 & 0x2a) && _display._layers[_display.EVEN]._priority <= pri)
									pix = 0;
							}

							if (pix)
								c = (((u16 *)_palette)[pix]);
							else
								c = (((u16 *)_palette)[
									_dblpf_bitplanes[(_display._regs[REG_BPLCON2] >> 6) & 1][pfpix0]]);

							pix = spripix & 0x1f;
							if (pix){
								if ((pfpix1 & 0x15) && _display._layers[_display.ODD]._priority <= pri)
									pix = 0;
								if ((pfpix1 & 0x2a) && _display._layers[_display.EVEN]._priority <= pri)
									pix = 0;
							}

							if (pix)
								c |= SL((((u16 *)_palette)[pix]),16);
							else
								c |= SL((((u16 *)_palette)[
									_dblpf_bitplanes[(_display._regs[REG_BPLCON2] >> 6) & 1][pfpix1]]),16);
							//c=0x1f001f;
						}
						else{
							if (spripix && _display._layers[_display.EVEN]._priority > pri){
								c=(((u16 *)_palette)[pix]);
								c|=SL(c,16);
								//c=0x1f001f;
							}
							else{
								c=(((u16 *)_palette)[pfpix0]);
								c|=SL((((u16 *)_palette)[pfpix1]),16);
							}
							//c=0x1f001f;
						}
						//c=0x1f001f;
						*(u32 *)pp=c;
						pp += 2;
					}
				}
			}

			if(y >= _display._win.top && y < _display._win.bottom)	{
				for (int pl = 0; pl < _display._planes; pl += 2){
					u32 a__= MAKELONG((_display._regs[REG_BPL1PTL + pl * 2]),
						(_display._regs[REG_BPL1PTH+ pl * 2]));
					a__ += (s16)SWAP16(ACHIPREG(REG_BPL1MOD));
					_display._regs[REG_BPL1PTL+ pl * 2]=(LOWORD(a__));
					_display._regs[REG_BPL1PTH+ pl * 2]=(HIWORD(a__));
				}
				for (int pl = 1; pl < _display._planes; pl += 2){
					u32 a__= MAKELONG((_display._regs[REG_BPL1PTL + pl * 2]),
						(_display._regs[REG_BPL1PTH+ pl * 2]));
					a__ += (s16)SWAP16(ACHIPREG(REG_BPL2MOD));
					_display._regs[REG_BPL1PTL+ pl * 2]=(LOWORD(a__));
					_display._regs[REG_BPL1PTH+ pl * 2]=(HIWORD(a__));
				}
			}
A9:
			if((attr & 5))
				p += _width;
			//if(dd) printf("\n");
		}
		//memcpy(_screen0,_screen,_width*_height*sizeof(u16));
	}
	GPU::Update();
	Draw(NULL);
Z:
	return 0;
}

int Denise::_dumpRegisters(char *p){
	char cc[200];

	sprintf(cc,"PLANES %c-%c:%c-%c %d CON0:%04X 1:%04X 2:%04X 3:%04X 4:%04X\n",
		_display._status._le ? 49:48,_display._status._lv ? 49:48,
		_display._status._se ? 49:48,_display._status._sv ? 49:48,
		_display._planes,SWAP16(ACHIPREG_(_ioreg,REG_BPLCON0)),SWAP16(ACHIPREG_(_ioreg,REG_BPLCON1))
		,SWAP16(ACHIPREG_(_ioreg,REG_BPLCON2)),SWAP16(ACHIPREG_(_ioreg,REG_BPLCON3))
		,SWAP16(ACHIPREG_(_ioreg,REG_BPLCON4)));
	strcat(p,cc);
	for(int i=0;i<6;i++){
		sprintf(cc," %1d:%08X",i,
			MAKELONG(_display._regs[REG_BPL1PTL+ i * 2],(_display._regs[REG_BPL1PTH + i * 2])));
		strcat(p,cc);
	}
	sprintf(cc,"\n S:%ux%u W:%lu,%lux%lu,%lu\n",_display._start,_display._stop,
		_display._win.left,_display._win.top,_display._win.right,_display._win.bottom);
	strcat(p,cc);
	for(int i=0;i<8;i++){
		sprintf(cc," %04X-%04X:%06X",_display._regs[REG_SPR0CTL+i*4],
		_display._regs[REG_SPR0POS+i*4],
			MAKELONG(_display._regs[REG_SPR0PTL+ i * 2],
				(_display._regs[REG_SPR0PTH + i * 2])));
		strcat(p,cc);
		if((i&3)==3) strcat(p,"\n");
	}
	strcat(p,"\n");
	return 0;
}

int Denise::__display::reset(){
	_status._val=0;
	_changed._val=0;
	_sprite._reset();
	memset(_regs,0,sizeof(_regs));
	return 0;
};

Denise::__display::__display(){
	_status._lv=1;
	_status._sv=1;
}

};