#include "popeyegpu.h"
#include "utils.h"
#include "machine.h"

namespace POPEYE{

PopeyeGpu::PopeyeGpu() : GPU(),TileCharManager(){
	_width=512;
	_height=448;
}

PopeyeGpu::~PopeyeGpu(){
}

int PopeyeGpu::Init(){
	if(GPU::Init(_width,_height,0,0x500))//vb-start 246
		return -1;//59.59Hz
	_palette = ((u16 *)_screen + (_width *_height));

	_oam.Init(1,*this);
	_fglayer.Init(2,*this);
	_bglayer.Init(0,*this);

	_layers.push_back(&_bglayer);
	_layers.push_back(&_oam);
	_layers.push_back(&_fglayer);

	return 0;
}

int PopeyeGpu::Reset(){
	GPU::Reset();
	if(_screen)
		memset(_screen,0,(0x500+(_width*_height))*sizeof(u16));
	for(int i=0;i<_layers.size();i++){
		_layers[i]->Reset();
		_layers[i]->_visible=1;
	}
	_cycles=0;
	_palette_bank=-1;
	return 0;
}

int PopeyeGpu::Run(u8 *,int cyc,void *obj){
	if((_cycles+=cyc) < _scanline_cycles)
		return 0;
	_cycles-=_scanline_cycles;
	if(++__line==_vblank)
		return 1;
	if(__line==_vblank_end){
		__line=0;
		machine->OnEvent(ME_ENDFRAME,0);
	}
	return 0;
}

int PopeyeGpu::Update(){
	GPU::Clear();
	for(int i=0;i<_layers.size();i++){
		if(!_layers[i]->_enabled) continue;
		_layers[i]->Reset();
		_layers[i]->Draw((u16 *)_screen);
	}
	GPU::Update();
	Draw(NULL);
	return 0;
}

int PopeyeGpu::InitGM(GM *p){
	if(GPU::InitGM(p))
		return -1;
	p->_char_ram=(u8 *)_char_ram;
	p->_tile_ram=(u8 *)_gpu_mem;
	p->_pal=(u16 *)_palette;
	p->_regs=(u32 *)_gpu_regs;
	p->_color_ram=(u8 *)_pal_ram;
	return 0;
}

int PopeyeGpu::Load(void *m){
	_prom=(u8 *)m;
	for (int i = 0;i < 0x100;i++)
		_prom[0x40+i]=(_prom[0x40+i] & 0xf)|SL(_prom[0x140+i]&0xf,4);
	return 0;
}

static I_INLINE u32 I_FASTCALL _color(u8 c){
	u32 r;

	r=SL(7-(c&7),2);
	r|=SL(7-(SR(c,3)&7),7);
	r|=SL(7-SR(c&0xe0,5),12);
	return r;
}

int PopeyeGpu::_setPaletteBank(u8 n){
	u8 v;

	if(!((v=n^_palette_bank) & 15))
		return 0;
	if(v & 8){
		int ii = SL(n & 8,1);
		for (int i = 0; i < 16; i++)
			((u16 *)_palette)[i]=_color(_prom[ii+i]);
		for (int i = 0; i < 16; i++){
			((u16 *)_palette)[16 + (2 * i) ]=0;
			((u16 *)_palette)[16 + (2 * i) + 1]=_color(_prom[ii+32+i]);
		}
	}
	if(v&7){
		int ii = SL(n & 7,5);
		for (int i = 0; i < 32; i++)
			((u16 *)_palette)[48+i]=_color(_prom[ii+i+64]);
	}
	_palette_bank=n;
	return 0;
}

void PopeyeGpu::__layer::Draw(u16 *_dst){
	int col,row;

	if(!_visible)
		return;
	u16 *dst=_dst+_screen_width*6;
	_palno = 0;
	_flipy = 0;
	_flipx = 0;
	for(row=1;row <56*2;row+=2){
		u16 *pp=dst;
		_xx=0;
		for(col=0;col<63;col++,_xx+=_char_width){
			_tileno = SL(row-SR(_scrollx,3),6)+col;
			_tileno=SL(_tileno,3);
			__tile::Draw(pp);
			pp += _char_width;
		}
		dst += _screen_width*_char_height;
	}
}

void PopeyeGpu::__layer::Init(int n,PopeyeGpu &g){
	__tile::Init(n,g);
	_setSize(8,8);
	_setPlanesOffset(4,7,6,5,4);
	_char_ram = _tile_ram + 0x2000;
	_setHorzOffset(8,0,0,0,0,0,0,0,0);
	_setVertOffset(8,0,0,0,0,0,0,0,0);
	_trans_pen=-1;
}

void PopeyeGpu::__fg_layer::Draw(u16 *_dst){
	int offset,col,row;

	if(!_visible)
		return;
	u16 *dst=_dst;
	offset=64;
	_flipy = 0;
	_flipx = 0;
	for(row=0;row <28;row++){
		u16 *pp=dst;
		_xx=0;
		for(col=0;col<32;col++,_xx+=_char_width,offset++){
			_tileno = SL(((u8 *)_tile_ram)[offset],6);
			_palno = 0x10 + SL(_color_ram[offset] & 0xf,1);
			__tile::Draw(pp);
			pp += _char_width;
		}
		dst += _screen_width*_char_height;
	}
}

void PopeyeGpu::__fg_layer::Init(int n,PopeyeGpu &g){
	__tile::Init(n,g);
	_setSize(16,16);
	_setPlanesOffset(1,0);
	_char_ram += 0x800;
	_scrollx=_scrolly=0;
	_setHorzOffset(16,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0,0);
	_setVertOffset(16,0,0,8,8,16,16,24,24,32,32,40,40,48,48,56,56);
	_swap_col=1;
}

void PopeyeGpu::__oam::Init(int n,PopeyeGpu &g){
	__tile::Init(n,g);
	_setSize(16,16);
	_tile_ram=(u8 *)g._sprite_ram;
	_char_ram += 0x1000;
	_setHorzOffset(16,65543, 65542, 65541, 65540, 65539, 65538, 65537, 65536, 7, 6, 5, 4, 3, 2, 1, 0 );
	_setVertOffset(16,120 ,112, 104, 96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0);
	_setPlanesOffset(2,131072,0);
	_swap_col=1;
	_multiple=1;
}

void PopeyeGpu::__oam::Draw(u16 *dst){
	int xx;

	if(!_visible) return;
	for (int offs = 4; offs < 0x280; offs += 4){
		u8 *p =((u8 *)_tile_ram + offs);
		int sy = 480-SL(p[1],1);
		if(sy < 0 || sy > _screen_height)
			continue;
		_xx = SL(p[0],1)-8;
		if(_xx<0 || _xx > _screen_width)
			continue;
		_tileno = ((p[2] & 0x7f) + SL(p[3]&0x10,3) + SL(p[3]&4,6)) ^ 0x1ff;
		_palno = 48+SL(p[3]&7,2);
		_tileno = SL(_tileno,7);
		_flipy = SR(p[3],3);
		_flipx = SR(p[2],7);
		__tile::Draw(&dst[((sy)*_screen_width)+_xx]);
	}
}

int PopeyeGpu::__oam::_extract(u32 offs){
	u8 *p =(u8 *)_tile_ram + offs*4;

	_tileno= ((p[2] & 0x7f) + SL(p[3]&0x10,3) + SL(p[3]&4,6)) ^ 0x1ff;
	//	if(!_tileno) continue;
	_xx = SL(p[0],1);
	int sy = SL(p[1],1);
	_palno = 48+SL(p[3] & 7,2);
	//printf("%d %d %x %x\n",sy,_xx,_tileno,_palno);
	_tileno=SL(_tileno,7);//+sy*16;
	_flipy = SR(p[3],3);
	_flipx = SR(p[2],7);
	//_flipy=_flipx = 0;
	_screen_width=_char_width;
	_screen_height=_char_height;
	return 0;
}

};