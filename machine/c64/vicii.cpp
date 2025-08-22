#include "vicii.h"
#include "machine.h"

namespace c64{

u16 *VICII::_et=NULL;
u16 *VICII::_emt=NULL;

static u16 _dy_start,_dy_stop,_border_on,_ud_border_on,_vc;
u32 PALETTE_MOS[] = {
	RGB(0x00, 0x00, 0x00), RGB(0xfd, 0xfe, 0xfc),RGB(0xbe, 0x1a, 0x24), RGB(0x30, 0xe6, 0xc6),
	RGB(0xb4, 0x1a, 0xe2), RGB(0x1f, 0xd2, 0x1e),RGB(0x21, 0x1b, 0xae), RGB(0xdf, 0xf6, 0x0a),
	RGB(0xb8, 0x41, 0x04), RGB(0x6a, 0x33, 0x04),RGB(0xfe, 0x4a, 0x57), RGB(0x42, 0x45, 0x40),
	RGB(0x70, 0x74, 0x6f), RGB(0x59, 0xfe, 0x59),RGB(0x5f, 0x53, 0xfe), RGB(0xa4, 0xa7, 0xa2)
};

#define RGB888_555(a) ( SR(a&0xf80000,9)|SR(a & 0xf800,6) | SR((u8)a,3) )
#define prb(a) 		&((u8 *)_gpu_mem)[((a) & 0x7000) == 0x1000 ? 0x14000 + ((a)&0xfff) : (a)]
#define rb(a) 		*(prb(a))

static struct __sprite{
	union{
		u8 _status;
		struct{
			unsigned int _enabled:1;
			unsigned int _changed:1;
			unsigned int _render:1;
			unsigned int _double_x:1;
			unsigned int _double_y:1;
			unsigned int _mcolor:1;
			unsigned int _priority:1;
			unsigned int aa:1;
			unsigned int _idx:3;
		};
	};
	u16 _x,_y;
	u8 *_ioreg,*_mem;

	int draw(int line,u8 *tiles,u16 *p,u16 *pl){
		u8 collide;
		int i,pr;

		//return -1;
		if(!_enabled) return -1;
		if(_changed){
			u16 mask = SL(1,_idx);
			_x=_ioreg[(_idx<<1)] | SL(SR(_ioreg[0x10],_idx) & 1,8);
			_y=_ioreg[0x1+(_idx<<1)];
			_mcolor=SR(_ioreg[0x1c],_idx);
			_double_x=SR(_ioreg[0x1d],_idx);
			_double_y=SR(_ioreg[0x17],_idx);
			_priority=0;//SR(_ioreg[0x1b],_idx);
			_changed=0;
		}
		_render=0;
		if(line >= _y && line < (_y + SL(21,_double_y)))
			_render=1;
		collide=0;
		if(!_render) goto Z;
		//printf("sprite %d %u %u %u %u %u %u %08x\n",_idx,_x,_y,_mcolor,_priority,_double_x,_double_y,*(u32 *)tiles);
		i = SR((line-_y),_double_y)*3;
		p += _x + 24;
		tiles += i;
		i=2;
		pr=SL(1,_idx+4);
		if(_double_x)
			goto M;

		if(!_mcolor) goto B;
		for(int x=0;x<24;){
			u8 tl=tiles[i--];
			for(int xx=0;xx<8 && x<24;xx+=2,p-=2,tl>>=2,x+=2){
				u8 px;

				switch(tl&3){
					case 0:
						continue;
					case 1:
						px=_ioreg[0x25];
					break;
					case 2:
						px=_ioreg[0x26];
					break;
					case 3:
						px=_ioreg[0x27+_idx];
					break;
				}
				pl[x] |= pr;
				if(pl[x] & ~pr)
					collide |= 1;
				if((pl[x] & 0xf)){
					collide |= 2;
					if(_priority) continue;
				}
				p[0]=p[1]=RGB888_555(PALETTE_MOS[px&15]);
			}
		}
		goto Z;
B:
		for(int x=0;x<24;){
			u8 tl=tiles[i--];
			for(int xx=0;xx<8 && x<24;xx++,p--,x++,tl>>=1){
				if(tl&1){
					pl[x] |= pr;
					if(pl[x] & ~pr)
						collide |= 1;
					if((pl[x]&0xf)){
						collide |= 2;
						if(_priority) continue;
					}
					*p=RGB888_555(PALETTE_MOS[_ioreg[0x27+_idx]]);
				}
			}
		}
		goto Z;
M:
		p += 24;
		if(!_mcolor) goto N;
		for(int x=0;x<48;){
			u16 tl=VICII::_emt[tiles[i--]];
			for(int xx=0;xx<16 && x<48;xx+=2,p-=2,tl>>=2,x+=2){
				u8 px;

				switch(tl&3){
					case 0:
						continue;
					case 2:
						px=_ioreg[0x25];
					break;
					case 1:
						px=_ioreg[0x26];
					break;
					case 3:
						px=_ioreg[0x27+_idx];
					break;
				}
				pl[x] |= pr;
				if(pl[x] & ~pr) collide |= 1;
				if((pl[x]&0xf)){
					collide |= 2;
					if(_priority) continue;
				}
				p[0]=p[1]=RGB888_555(PALETTE_MOS[px&15]);
			}
		}
		goto Z;
N:
		for(int x=0;x<48;){
			u16 tl=VICII::_et[tiles[i--]];
			for(int xx=0;xx<16 && x<48;xx++,p--,tl>>=1,x++){
				u8 px;

				pl[x] |= pr;
				if(!(tl&1)) continue;
				if(pl[x] & ~pr) collide |= 1;
				if((pl[x]&0xf)){
					collide |= 2;
					if(_priority) continue;
				}
				p[0]=p[1]=RGB888_555(PALETTE_MOS[_ioreg[0x27+_idx]]);
			}
		}
Z:
		if(collide & 1) _ioreg[0x1e] |= SL(1,_idx);
		if(collide & 2) _ioreg[0x1f] |= SL(1,_idx);
		return 0;
	};

	int reset(){
		_status=0;
		_x=_y=0;
		return 0;
	};
} _sprites[8];

static struct __rendering{
	union{
		u32 _status;
		struct{
			unsigned int _enabled:1;
			unsigned int _fetch:1;
			unsigned int _render:1;
			unsigned int _mode:3;
		};
	};

	u8 _cycles,_col,_tile;
	u16 _x,*_bit;

	void initFrame(){
	};
	void initLine(){
		_x=0;
		_cycles=__cycles;
	};
	void flush(){
	//	u32 d = D_CYCLES(_cycles,,__cycles);
		_cycles=__cycles;
	};
} _rendering;

VICII::VICII(void *p) : GPU(),CPUTIMEROBJ(){//vic6567
	_width=0x180;
	_height=0x110;
	_timerobj::expired=65;
	_timerobj::type |= BV(31);
	_timerobj::obj=(ICpuTimerObj *)p;
}

VICII::~VICII(){

}

int VICII::Run(u8 *,int cyc,void *obj){
	int res,draw;

	res=draw=0;
	_cycles += cyc;
	for(;_cycles >= 65 && !res && !draw;_cycles -= 65){
		if(__line >= 0x10 && __line <= 0x11f){
			//flush();
			RenderLine(0);
			_rendering.initLine();
			draw=1;
			_sl+=_width;
		}
		if(__line==_lc){
			_ioreg[0x19] |= 1;
			if(_ioreg[0x1a] & 1){
				_ioreg[0x19] |= 0x80;
				//return 1;
				res = 1;
				//_cycles=0;
				//goto Z;
			}
		}
		if(++__line==312){
			_sl=(u16 *)_screen;
			__line=0;
			machine->OnEvent(ME_ENDFRAME,0);
			_cycles=0;
			_enabled=0;
			_vc=0;
			_row=0;
			_fetch=0;
			_rendering.initFrame();
			draw=1;
		}
	}
Z:
	_ioreg[0x11]=(_ioreg[0x11] & 0x7f)|SR(__line&0x100,1);
	_ioreg[0x12]=(u8)__line;
	return res;
}

int VICII::_remap(void *a,void *b){
	_ioreg=(u8 *)b;
	_mem=(u8 *)a;
	_gpu_mem=a;
	return 0;
}

int VICII::Init(void *a,void *b,void *t){
	if(GPU::Init(_width,_height,0,KB(8)))
		return -1;
	//_timer=(LPCPUTIMEROBJ)t;
	VICII::_et = ((u16 *)_screen + (_width *_height));
	VICII::_emt = &VICII::_et[256];
	matrix_line = (u8 *)&VICII::_emt[256];
	color_line=&matrix_line[40];
	_pl=(u16 *)&color_line[40];

	for (int i = 0; i < 256; i++){
		u16 ret = 0;
		for(u8 n=7,idx=i;idx;n--){
			ret |= SL(SR(idx&0x80,7),n) * 5 * BV((n>>1)<<1);
			idx=SL(idx,1);
		}
		_emt[i]=ret;
		ret = 0;
		for(u8 n=7,idx=i;idx;n--){
			ret |= SL(SL(SR(idx&0x80,7),n),n);
			idx=SL(idx,1);
		}
		_et[i]=ret*3;
	}

	for(int i=0;i<sizeof(_sprites)/sizeof(__sprite);i++){
		_sprites[i]._idx=i;
		_sprites[i]._ioreg=(u8 *)b;
	}
	return _remap(a,b);
}

int VICII::Reset(){
	GPU::Reset();
	Clear();
	_cycles=0;
	_mem[0x2a6]=1;
	for(int i=0x27;i<0x2e;i++)
		_ioreg[i]=i-0x26;
	_ioreg[0x2e]=12;
	_ioreg[0x25]=4;
	_ioreg[0x26]=0;
	_lc=0xffff;
	for(int i=0;i<sizeof(_sprites)/sizeof(__sprite);i++)
		_sprites[i].reset();
	_sl=(u16 *)_screen;
	return 0;
}

int VICII::Update(u32 flags){
	u16 *p;

	if(!(flags&1))
		goto Z;
	//if(!(p=(u16 *)_screen) || Clear())
	//	return -1;

	GPU::Update();
	Draw(NULL);
	Clear();
Z:
	return 0;
}

int VICII::LoadSettings(void * &){
	return -1;
}

int VICII::write(u32 a,u16 v){
	int rex=1;

	//printf("VICII::%s %x %x\n",__FUNCTION__,a,v);
	switch((u8)a){
		case 0x40:
			_base = SL(v & 3,14);
		break;
		case 0x18:
			vbase = v;
			matrix_base = ((v & 0xf0) << 6);
			char_base = ((v & 0x0e) << 10);
			bitmap_base = ((v & 0x08) << 10);
		break;
		case 0x11:{
			u16 nv=SL(v&0x80,1) | (u8)_lc;
			if(nv != _lc){
				_lc = nv;
				if(__line==_lc){
					_ioreg[0x19] |= 1;
					if(_ioreg[0x1a] & 1){
						_ioreg[0x19] |= 0x80;
						machine->OnEvent(1,1);
					}
				}
			}
			if (v & 8){
				_dy_start = 0x33;
				_dy_stop = 0xfb;
			}
			else{
				_dy_start = 0x37;
				_dy_stop = 0xf7;
			}
			_ioreg[0x11] = (v & 0x7f) | SR(__line & 0x100,1);
			//DLOG("VIC CR1 %x",v&0x7f);
		}
			rex=0;
		break;
		case 0x12:{
			u16 nv=(_lc & 0x100)|(u8)v;
			if(nv != _lc){
				_lc = nv;
				if(__line==_lc){
					_ioreg[0x19] |= 1;
					if(_ioreg[0x1a] & 1){
						_ioreg[0x19] |= 0x80;
						machine->OnEvent(1,1);
					}
				}
			}
		}
			rex=0;
		break;
		case 0x15:
			for(int i=0;i<sizeof(_sprites)/sizeof(__sprite);i++,v>>1)
				_sprites[i]._enabled=v;
		break;
		case 0x0:case 0x1:case 0x2:case 0x3:case 0x4:case 0x5:case 0x6:
		case 0x7:case 0x8:case 0x9:case 0xa:case 0xb:case 0xc:case 0xd:case 0xe:case 0xf:
			_sprites[(a&0xf) >> 1]._changed=1;
		break;
		case 0x17:
		case 0x1b:
		case 0x1c:
		case 0x1d:
			for(int i=0;i<sizeof(_sprites)/sizeof(__sprite);i++)
				_sprites[i]._changed=1;
		break;
		case 0x19:
			_ioreg[0x19] &= ~v & 0xf;
			if(_ioreg[0x1a] & _ioreg[0x19]){
				_ioreg[0x19] |= 0x80;
			//	machine->OnEvent(1,1);
			}
			else{
			//	_ioreg[0x19] &= ~0x80;
				machine->OnEvent(1,2);
			}
			//_ioreg[0x19] |= 0x70;
			rex=0;
		break;
		case 0x1a:
			_ioreg[0x1a] = (v&0xf);
			if(_ioreg[0x1a] & _ioreg[0x19]){
				_ioreg[0x19] |= 0x80;
				machine->OnEvent(1,1);
			}
			else{
				_ioreg[0x19] &= ~0x80;
				machine->OnEvent(1,2);
			}
			rex=0;
		break;
	}
	_flush();
	return rex;
}

int VICII::read(u32 a,u16 *){
	switch((u8)a){
		case 0x19:
			_ioreg[0x19]=0;
		break;
	}
	return 1;
}

int VICII::RenderLine(u32 flags,u32 cyc){
	if(__line >= 0x30 && __line <= 0xf7){
		u8 *mbp,*crp;

		if(__line==0x30)
			_enabled = SR(_ioreg[0x11] & 0x10,4);

		if(_enabled)
			_fetch= (__line&7) == (_ioreg[0x11]&7);

		if(_fetch){
			mbp=prb(_base | matrix_base | (_vc & 0x3ff));
			crp = &((u8 *)_palette)[(_vc | matrix_base) & 0x3ff];
			for (int i = 0; i < 40; ++i) {
				matrix_line[i] = *mbp++;
				color_line[i] = *crp++ & 0xf;
			}
		}
	}

	_mode = SR((_ioreg[0x11] & 0x60)|(_ioreg[0x16] & 0x10),4);
	if(_fetch)
		_render=1;
	if(__line >= _dy_stop || __line <= _dy_start || !_enabled || !_render)
		_border_on=1;
	else if(_enabled)
		_border_on=0;
	if(_border_on){
		memset(_pl,0,_width);
		switch(_mode){
			case 0:
			case 1:
			case 4:{
				for(int x=0;x<_width;x++)
					_sl[x]=RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
			}
			break;
			case 3:
				//printf("%x\n",_mode);
				//EnterDebugMode();
			case 2:
				u8 data = rb(_base | matrix_base | (_ioreg[0x11] & 0x40 ? 0x39ff : 0x3fff));
				for(int x=0;x<_width;x++)
					_sl[x]=RGB888_555(data&0xf);
				break;
		}
	}
	else{
		//memset(_pl,_mode+1,_width);
		switch(_mode){
			case 1:{//mct
				int c,x;
				u8 ch;

				for(x=0;x<0x20;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
				for(c=0;c<40;c++,x += 8){
					u8 tl,pal;

					u16 adr= SL((ch=matrix_line[c]),3);
					tl=rb(_base|char_base|adr+(_row&7));
					pal= color_line[c];
					if(pal&8){
						for(int xx=0;xx<8;xx+=2){
							u8 px;

							switch(tl&3){
								case 0:
								case 1:
								case 2:
									px=_ioreg[0x21+(tl&3)]&0xf;
								break;
								case 3:
									px=pal&7;
								break;
							}
							_sl[x + (7-xx)] =
							_sl[x + (6-xx)]=
								RGB888_555(PALETTE_MOS[px]);
							_pl[x+xx]=1;
							tl >>= 2;
						}
					}
					else{
						for(int xx=0;xx<8;xx++){
							u8 px =(tl & 1) ? pal : _ioreg[0x21];
							_sl[x + (7-xx)] =
								RGB888_555(PALETTE_MOS[px&0xf]);
							_pl[x+xx]=1;
							tl >>= 1;
						}
					}
				}

				for(;x<320+32*2;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
			}
			break;
			case 2:{
				int c,x;
				u8 ch;

				for(x=0;x<0x20;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
				for(c=0;c<40;c++,x += 8){
					u32 adr= _base | bitmap_base;
					//adr=0xe000;
					adr |= SL(_vc + c,3);
					u8 tl;// = ((u8 *)_gpu_mem)[adr + (_rc&7)];
					tl=rb(adr + (_row&7));

					ch = matrix_line[c];
					for(int xx=0;xx<8;xx++){
						u8 px;

						switch(tl&1){
							case 1:
								px=ch&0xf;
							break;
							case 0:
								px=ch>>4;
							break;
						}
						_sl[x + (7-xx)] = RGB888_555(PALETTE_MOS[px]);
						_pl[x+xx]=1;
						tl >>= 1;
					}
				}

				for(;x<320+32*2;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
			}
			break;
			case 3:{
				int c,x;
				u8 ch;

				ch =rb(_base | matrix_base | (_ioreg[0x11] & 0x40 ? 0x39ff : 0x3fff));
				for(x=0;x<0x20;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
				for(c=0;c<40;c++,x += 8){
					u32 adr= _base | (bitmap_base);
					//adr=0xe000;
					adr |= SL((_vc&0x3ff) + c,3);
					u8 tl;// = ((u8 *)_gpu_mem)[adr + (_rc&7)];
					tl=rb(adr + (_row&7));

					ch = matrix_line[c];
					u8 pal= color_line[c];

					for(int xx=0;xx<8;xx+=2){
						u8 px;

						switch(tl&3){
							case 0:
								px=_ioreg[0x21];
							break;
							case 2:
								px=ch&0xf;
							break;
							case 1:
								px=ch>>4;
							break;
							case 3:
								px=pal&0xf;
							break;
						}
						_sl[x + (7-xx)] =
						_sl[x + (6-xx)] = RGB888_555(PALETTE_MOS[px]);
						_pl[x+xx]=1;
						tl >>= 2;
					}
				}

				for(;x<320+32*2;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
			}
			break;
			case 0:{
				u8 ch,tl,pal,b;
				int x;

				b = rb(_base | matrix_base | (_ioreg[0x11] & 0x40 ? 0x39ff : 0x3fff)) & 3;
			//	b=0;
				for(x=0;x<0x20;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);

				for(int c=0;c<40;c++,x += 8){
					ch=matrix_line[c];
					u16 adr= SL(ch,3);
				//	printf("%x ",char_base|adr+(_rc&7));
					tl=rb(_base|char_base|adr+(_row&7));
					pal=color_line[c];
					for(int xx=0;xx<8;xx++){
						u8 px=(tl & 1) ? pal : _ioreg[0x21];
						_pl[x+xx]=1;
						_sl[x + (7-xx)] = RGB888_555(PALETTE_MOS[px&15]);
						tl >>= 1;
					}
				}

				for(;x<320+32*2;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
				//printf("\n");
			}
			break;
			case 4:{
				u8 ch,tl,pal,b;
				int x;

				b = rb(_base | matrix_base | (_ioreg[0x11] & 0x40 ? 0x39ff : 0x3fff)) & 3;
			//	b=0;
				for(x=0;x<0x20;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);

				for(int c=0;c<40;c++,x += 8){
					ch=matrix_line[c];
					u16 adr = SL(ch,3) |char_base+(_row&7);
					tl=rb(_base|(adr&0xf9ff));
					for(int xx=0;xx<8;xx++){
						u8 px;
						if(tl & 1)
							px=color_line[c];
						else if(ch&0x80)
							px = ch&0x40 ? _ioreg[0x21+3] : _ioreg[0x21+2];
						else
							px = ch&0x40 ? _ioreg[0x21+1] : _ioreg[0x21+0];
						_pl[x+xx]=1;
						_sl[x + (7-xx)] = RGB888_555(PALETTE_MOS[px&15]);
						tl >>= 1;
					}
				}

				for(;x<320+32*2;x++)
					_sl[x] = RGB888_555(PALETTE_MOS[_ioreg[0x20]]);
				//printf("\n");
			}
			break;
			default:
				printf("mode %d\n",_mode);
			break;
		}
	}
	//printf("%u %x %x %u %u\n",__line,_ioreg[0x11],_ioreg[0x16],_vc,_rc);
	if(_row==7){
		_vc += 40;
		_row=0;
		_render=0;
	}
	if(_fetch || _render){
		_row++;
		_render=1;
	}
	_ioreg[0x1e]=_ioreg[0x1f]=0;
	for(int i=0;i<sizeof(_sprites)/sizeof(__sprite);i++){
		u8 *p=prb(_base | matrix_base | 0x3f8);
		_sprites[i].draw(__line,
			prb(_base | SL(p[i],6)),_sl,_pl);
	}
	return 0;
}

int VICII::_flush(u32 param){
	_rendering.flush();
	return 0;
}

int VICII::_dumpRegisters(char *p){
	char c[200];

	sprintf(c,"VIC\n IE:%02X IF:%02X M:%x %04X M:%04x B:%04x C:%04x LC:%u CTRL1:%02X CTRL2:%02X\n",
		_ioreg[0x1a],_ioreg[0x19],_mode,_base,matrix_base,bitmap_base,char_base,_lc,
		_ioreg[0x11],_ioreg[0x16]);
	strcat(p,c);
	return 0;
}

};