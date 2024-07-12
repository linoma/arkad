#include "cps3gpu.h"
#include "machine.h"
#include "cps3m.h"

extern IMachine *machine;

CPS3GPU::CPS3GPU() : GPU(){

}

CPS3GPU::~CPS3GPU(){
}

int CPS3GPU::Init(){
	/*
    Measured clocks:
        Video DAC = 8.602MHz (384 wide mode) ~ 42.9545MHz / 5
                    10.73MHZ (496 wide mode) ~ 42.9545MHz / 4
        H = 15.73315kHz
        V = 59.59Hz
        H/V ~ 264 lines
*/
	if(GPU::Init(384,224))
		return -1;//59.59Hz
	for(int i=0;i<sizeof(_layers)/sizeof(struct __layer);i++)
		_layers[i].Init(i,*this);
	for(int i=0;i<sizeof(_oams)/sizeof(struct __oam);i++)
		_oams[i].__layer::Init(i,*this);
	_ss_layer.Init(0,*this);
	return 0;
}

int CPS3GPU::Reset(){
	_cram_bank=0;
	GPU::Reset();
	if(_screen)
		memset(_screen,0,_width*_height*sizeof(u16));
	__line=0;
	_cycles=0;
	for(int i=0;i<sizeof(_layers)/sizeof(struct __layer);i++)
		_layers[i].Reset();
	for(int i=0;i<sizeof(_oams)/sizeof(struct __oam);i++)
		_oams[i].Reset();
	return 0;
}

int CPS3GPU::Run(u8 *,int cyc,void *obj){
	if((_cycles+=cyc) < _scan_cycles)
		return 0;
	_cycles-= _scan_cycles;
	if(++__line==224)
		return 12;
	if(__line==264){
		__line=0;
		_cycles=0;
		machine->OnEvent(ME_ENDFRAME,0);
	}
	return 0;
}

void CPS3GPU::_set_bank(int n){
	_cram_bank=n&7;
/*	for(int i=0;i<sizeof(_layers)/sizeof(struct __layer);i++)
		_layers[i]._char_offset=SL(_cram_bank,20);
	for(int i=0;i<sizeof(_oams)/sizeof(struct __oam);i++)
		_oams[i]._char_offset=SL(_cram_bank,20);
		*/
}

//static int gscrollx,gscrolly;

int CPS3GPU::Update(u32 flags){
	u32 *pp=(u32 *)_gpu_mem;

	if(Clear()) return -1;

	_ss_layer.Reset();
	for(int i=0;i<sizeof(_layers)/sizeof(struct __layer);i++)
		_layers[i].Reset();
	for(int i=0;i<sizeof(_oams)/sizeof(struct __oam);i++)
		_oams[i].Reset();
	//goto A;
	for (int i=0;i<0x800;i++,pp+=4) {
		u32 data=pp[0];

		if ((data&0xf0000000) == 0x80000000)
			break;
		u32 start = (data&0x7ff0);
		int length	= SR(data&0x01ff0000,16);

		u32 *p = (u32 *)_gpu_mem + start*4;

		for (int j=0; j<length; j++,p+=4) {
			u32 value3 = p[2];
			//printf("oam %d %x %x %lx %x\n",j,start,length,(u64)p - (u64)_gpu_mem,value3);
			int ysize2 = ((value3 & 0xc)>>2);
			if (ysize2==0)
				continue;
			int xsize2 = value3 & 3;
			if (xsize2==0) {
				int tilemapnum = SR(value3 & 0x30,4);
			//	if(tilemapnum)
				//	continue;
				_layers[tilemapnum].Init(data,p[1],value3);
				_layers[tilemapnum].Draw(data,p[1],value3);
				//printf("\n");
				continue;
			}
			//if(!_oams[i/4]._init)
			_oams[i].Init(data,pp[1],pp[2]);
			_oams[i].Draw(p[0],p[1],value3);
		}
	}
	//for(int i=0;i<sizeof(_layers)/sizeof(struct __layer);i++)
		//_layers[i].Draw();
	//if(!BT(_gpu_status,BV(0)|BV(1)))
		//goto A;
	_ss_layer.Draw();
A:
Z:
	GPU::Update();
	Draw(NULL);
	return 0;
}

void CPS3GPU::__layer::Init(u32 a,u32 b,u32 c){
	int n;

	_init=1;
	_a=a;
	_b=b;
	_c=c;
	n = SR(a&0x70000000,28);
	_gscrolly = (PPU_REG16_(_regs,n*4)&0x03ff);
	_gscrollx = (PPU_REG16_(_regs,n*4+2)&0x3ff);
	_height = ((c & 0x7f000000)>>24)+1;
	_ypos2 = (b & 0x000003ff);
}

void CPS3GPU::__layer::Draw(u32 d,u32 e,u32 f){
	if(_draw || !_visible)
		return;
	u32 *regs=&_regs[(0x20+_idx*0x10)/4];
	if (!(regs[1]&0x8000))
		return;
	//fixme
	//_draw=1;
	int scrolly =  (u16)regs[0] + 4;
	u32 mapbase =  (regs[2] & 0x7f0000)>>16;//80000
	u32 linebase=  (regs[2]&0x7f000000)>>24;
	int linescroll_enable = (regs[1]&0x4000);

	int lino =0;

	mapbase = mapbase << 10;
	linebase = linebase << 10;
	//int ypos2 = (e & 0x000003ff);

	//printf("ld %d %d %d %u %d %d %x %d\n",_idx,ypos2,scrolly,_height,gscrolly,gscrollx,*((u32 *)_tile_ram + mapbase),_screen_width);
	//scrolly=0;
	//_char_height=8;
	u32 *mem=(u32 *)_tile_ram;
	for(int yy=0;yy<_height;yy++){
		int cury_pos = _ypos2 + _gscrolly - yy;
		cury_pos = ~cury_pos;
		cury_pos -= 18;

		cury_pos &= 0x3ff;

		//cury_pos=yy*16;
		//printf("y: %d %d %d\n",n,cury_pos,ypos2);

		int line = (cury_pos + scrolly) & 0x3ff;
		int tileline = SR(line,4)+1;
		int tilesubline = line & 15;

		if(cury_pos>=_screen_height-1 || (cury_pos-tilesubline)<0)
			continue;
		int scrollx = SR(regs[0],16);
		if (linescroll_enable)
			scrollx += (mem[linebase+((line+16-4) & 0x3ff)] >> 16) & 0x3ff;
		//scrollx=0;
		for (int x=0;x<(_screen_width/16);x++) {
			u32 dat;

			dat = mem[mapbase + ((tileline & 63) * 64) + ((x+scrollx / 16) & 63) ];
			_tileno = SR(dat,17)*256;
			_palno = (dat & 0x1ff);
			//bpp = (dat & 0x200)>>9;
			_flipy  = (dat & 0x800)>>11;
			_flipx  = (dat & 0x1000)>>12;
			_alpha = SR(dat&0x400,10);
			_palno <<= 6;
			if (!(dat & 0x200))
				_palno <<= 2;
			__tile::Draw(&_dst[((cury_pos-tilesubline) * _screen_width) + (x*16)]);
		}
	}

	//if(lino) EnterDebugMode();
}

void CPS3GPU::__layer::Reset(){
	__tile::Reset();
}

void CPS3GPU::__layer::Init(int n,CPS3GPU &g){
	__tile::Init(n,g);
	_idx=n;
}

void CPS3GPU::__oam::Draw(u32 d,u32 e,u32 f){
	int tilestable[4] = { 8,1,2,4 };

//	if(!_init) return;
	//printf("oam draw %x %x %x\n",d,e,f);

	_height = tilestable[SR(f & 0xc,2)];
	_width = tilestable[f&3];
	u32 tileno = (d & 0xfffe0000)>>17;
	int xpos2 = (e & 0x03ff0000)>>16;
	_ypos2 = (e & 0x000003ff);
	_flipx = (d & 0x00001000)>>12;
	_flipy = (d & 0x00000800)>>11;
	_alpha = (d & 0x00000400)>>10;

	int ysizedraw2 = ((f & 0x7f000000)>>24);
	int xsizedraw2 = ((f & 0x007f0000)>>16);

	u32 const xinc = (xsizedraw2 << 16) / _width;
	u32 const yinc = (ysizedraw2 << 16) / _height;

	_xscale = SR(xinc,4);
	_yscale = SR(yinc,4);
	if((u16)_xscale)
		_xscale += (1<<16) / 16;
	if((u16)_yscale)
		_yscale += (1<<16) / 16;

	_width--;
	_height--;

	_flipx ^= global_xflip;
	_flipy ^= global_yflip;

	if (!_flipx)
		xpos2 += (xsizedraw2 / 2);
	else
		xpos2 -= (xsizedraw2 / 2);

	_ypos2 += (ysizedraw2 / 2);
	if (!_flipx)
		xpos2 -= ((_width + 1) * xinc) >> 16;
	else
		xpos2 += (_width * xinc) >> 16;

	if (_flipy)
		_ypos2 -= (_height * yinc) >> 16;

	_palno = whichpal ? global_pal : (d&0x1ff);
	_palno=SL(_palno,6);

	if ((whichbpp && !global_bpp) || (!whichbpp && !(d&0x200)))
		_palno=SL(_palno,2);

	_tileno=SL(tileno,8);
	if(!_xscale) _xscale=0x10000;
	if(!_yscale) _yscale=0x10000;

	for (int xx = 0; xx <= _width; xx++){
		int current_xpos;

		if (!_flipx)
			current_xpos = (xpos + xpos2 + ((xx * xinc) >> 16));
		else
			current_xpos = (xpos + xpos2 - ((xx * xinc) >> 16));

		current_xpos += _gscrollx;
		current_xpos += 1;
		current_xpos &= 0x3ff;
		if (current_xpos & 0x200)
			current_xpos -= 0x400;

		if(current_xpos < 0 || current_xpos >= _screen_width){
			_tileno  += _height*256;
			continue;
		}
		for (int yy = 0; yy <= _height; yy++){
			int current_ypos;

			if (_flipy)
				current_ypos = (ypos + _ypos2 + ((yy * yinc) >> 16));
			else
				current_ypos = (ypos + _ypos2 - ((yy * yinc) >> 16));

			current_ypos += _gscrolly;
			current_ypos = 0x3ff - current_ypos;
			current_ypos -= 17;
			current_ypos &= 0x3ff;

			if (current_ypos & 0x200)
				current_ypos -= 0x400;

			if(current_ypos >= 0 && current_ypos  < _screen_height)
				Draw(&_dst[ (current_ypos *_screen_width) + current_xpos]);
			_tileno += 256;
		}
	}
	//_tileno=tileno;
}

void CPS3GPU::__oam::Reset(){
	__layer::Reset();
	_init=0;
}

void CPS3GPU::__oam::Init(u32 a,u32 b,u32 c){
	//if(_init) return;
	xpos =			(b&0x03ff0000)>>16;
	ypos =			(b&0x000003ff);
	int n = SR(a&0x70000000,28);
	_gscrolly = (PPU_REG16_(_regs,n*4)&0x03ff);
	_gscrollx = (PPU_REG16_(_regs,n*4+2)&0x3ff);
	//length	=		(a&0x01ff0000)>>14; // how many entries in the sprite table
	whichbpp =		(c&0x40000000)>>30; // not 100% sure if this is right, jojo title / characters
	whichpal =		(c&0x20000000)>>29;
	global_xflip =	(c&0x10000000)>>28;
	global_yflip =	(c&0x08000000)>>27;
	global_alpha =	(c&0x04000000)>>26; // alpha / shadow? set on sfiii2 shadows, and big black image in jojo intro
	global_bpp =	(c&0x02000000)>>25;
	global_pal =	(c&0x01ff0000)>>16;
	_init=1;
}

void CPS3GPU::__oam::_drawRow(u16 *p){
	int xi,xe,x0;

	xe=SR(_char_width*_xscale,16);
	xi=1;
	x0=SL(_char_width,12)/xe;
	xe=SL(xe,12);

	if(_flipx){
		p+=_char_width-1;
		xi*=-1;
	}
	//if(xi>4096) printf("drawrow %d\n",xi);
	for(int xx=0;xx<xe && (u64)p < _screen_end && (u64)p > _screen_start;xx+=x0){
		u8 col = _src[SR(xx,12)^3];
		if(col)
			*p=_pal[_palno+col];
		p+=xi;
	}
}

void CPS3GPU::__oam::Draw(u16 *pp){
	int yi,ye,y0,yp;
	u8 *src;

	if(!_visible) return;
	yi=_screen_width;
	//_yscale=0x10000;
	//_xscale=0x10000;
	ye=SR(_char_height*_yscale,16);
	if(_flipy){
		yi*=-1;
		pp+=_screen_width*(_char_height-1);
	}
	y0=SL(_char_height,12)/ye;
	src = ((u8 *)_char_ram + _tileno);
	for(int yy=yp=0;yy<ye;yy++){
		_src = src + (_char_width * SR(yp,12));
		_drawRow(pp);
		pp+=yi;
		yp+=y0;
	}
}

void CPS3GPU::__tile::Init(int n,CPS3GPU &g){
	_char_ram=(u8 *)g._char_ram;
	_tile_ram=(u8 *)g._gpu_mem;
	_dst=(u16 *)g._screen;
	_regs=(u32 *)g._gpu_regs;
	_pal=(u16 *)g._pal_ram;
	_screen_width=g._width;
	_screen_height=g._height;
	_screen_start=(u64)g._screen;
	_screen_end=(u64)((u16 *)g._screen + _screen_width*_screen_height);
}

void CPS3GPU::__tile::_drawRow(u16 *p){
	int xi;

	xi=1;
	if(_flipx){
		p += _char_width-1;
		xi=-1;
	}
	for(int xx=0;xx<_char_width && (u64)p < _screen_end;xx++){
		u8 col = _src[(xx)^3];
		if(col)
			*p=_pal[_palno+col];
		p+=xi;
	}
}

void CPS3GPU::__tile::Draw(u16 *pp){
	int yi;

	yi=_screen_width;
	if(_flipy){
		yi=-_screen_width;
		pp+=_screen_width*(_char_height-1);
	}
	_src = ((u8 *)_char_ram + _tileno);
	for(int yy=0;yy<_char_height;yy++,_src += _char_width){
		_drawRow(pp);
		pp+=yi;
	}
}

void CPS3GPU::__ss_layer::Init(int n,CPS3GPU &g){
	__tile::Init(n,g);

	_tile_ram=(u8 *)g._ss_ram;
	_char_ram=_tile_ram+0x8000;
	_regs=(u32 *)g._ss_regs;
	_height=_char_height=8;
	_width=_char_width=8;
	_width=512;
}

#define SS_REG(a) SS_REG_(_regs,a)

void CPS3GPU::__ss_layer::Draw(){
	if(!_visible) return;

	u16 *dst=_dst;

	int scrolly = (s16)SS_REG(0x22);
	u32 pal = SL(SS_REG(0x26),9);
	int offset=0;;//scrolly;//(abs(scrolly)/8)*32 - 32;

	//printf("ss %d %d %x %d\n",scrolly,(s16)SS_REG(16),pal,offset);

	int lino=0;

	for(int line=0;line <_screen_height;line+=8,dst += _screen_width*8){
		u16 *pp=dst;
		//offset +=4;
		//int offset = ((line + scrolly) / 8 * 256) & 0x3fff;
		//int rowscroll = m_ss_ram[((line + scrolly - 1) & 0x1ff)*2 + 0x2000];
		for(int x=0;x<64;x++,offset+=4){
			if(x>47)
				continue;
			u32 data = *(u32 *)((u8 *)_tile_ram + offset);
			_tileno = SR(data & 0x1ff0000,10);
			_palno = (data & 0x1f);

			_palno = pal+SL(_palno,3);
			_flipy = (data & 0x4000) >> 14;
			_flipx = (data & 0x8000) >> 15;

			__tile::Draw(pp);
			pp += 8;
		}
	}
}

void CPS3GPU::__ss_layer::_drawRow(u16 *p){
	int xi;

	xi=1;
	if(_flipx){
		p+=_char_width-1;
		xi=-1;
	}
	for(int xx=0;xx<_char_width && (u64)p < (u64)_screen_end;xx+=2){
		u8 px = _src[(xx)^2];
		u8 col = (px&15);
		if(col)
			*p=_pal[_palno+(col^1)];
		p+=xi;
		col=SR(px,4);
		if(col)
			*p=_pal[_palno+(col^1)];
		p+=xi;
	}
}