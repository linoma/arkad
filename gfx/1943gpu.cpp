#include "1943gpu.h"
#include "utils.h"

namespace M1943{

#ifdef _DEVELOP
static FILE *lino=0;
#endif


M1943Gpu::M1943Gpu() : GPU(),TileCharManager(){
	_width=256;
	_height=256;
}

M1943Gpu::~M1943Gpu(){
}

int M1943Gpu::Init(){
	if(GPU::Init(_width,_height,0,(0x500+_width*_height)*2))//vb-start 246
		return -1;//59.59Hz
	_palette = ((u16 *)_screen + (_width *_height));

	_layers.push_back(&__bglayers[0]);//bg2
	_layers.push_back(&__bglayers[1]);
	_layers.push_back(&__oams);
	_layers.push_back(&__fglayers);

	__bglayers[1].Init(0,*this);
	__bglayers[0].Init(1,*this);
	__oams.Init(2,*this);
	__fglayers.Init(3,*this);

	__oams._priority=(u8 *)&((u16 *)_palette)[0x500];

	float f[6];

	f[0]=cos(RADIANS(-90))*1;//(_width/(float)_height)
	f[1]=sin(RADIANS(-90))*1;//(_height/(float)_width)
	f[2]=-f[1];
	f[3]=f[0];

	f[5]=1;
	f[4]=-1;
	//_viewProjection(f);
	return 0;
}

int M1943Gpu::Reset(){
	GPU::Reset();
	if(_screen)
		memset(_screen,0,(0x500+(_width*_height))*sizeof(u16));
	for(int i=0;i<_layers.size();i++){
		_layers[i]->Reset();
	}
	_cycles=0;
	return 0;
}

int M1943Gpu::Run(u8 *,int cyc,void *obj){
	if((_cycles+=cyc)<381)
		return 0;
	_cycles-=381;
	if(++__line==246)
		return 1;
	if(__line==262){
		__line=0;
		machine->OnEvent((u32)-1,0);
	}
	return 0;
}

int M1943Gpu::Update(){
	GPU::Clear();
	for(int i=0;i<_layers.size();i++){
		if(!_layers[i]->_enabled)
			continue;
		_layers[i]->Reset();
		_layers[i]->Draw((u16 *)_screen);
	}
	GPU::Update();
	Draw(NULL);
	return 0;
}

int M1943Gpu::InitGM(GM *p){
	if(GPU::InitGM(p)) return -1;
	p->_char_ram=(u8 *)_char_ram;
	p->_tile_ram=(u8 *)_gpu_mem;
	p->_pal=(u16 *)_palette;
	p->_regs=(u32 *)_gpu_regs;
	return 0;
}

u32 M1943Gpu::__1943tile::_translateColor(u32 c){
	//printf("%x;%x ",c,_pal[c]);
	//if((c=_pal[c]) == 0) return 0x7fff;
	return _pal[_pal[c]];
}

void M1943Gpu::__layer::Draw(u16 *_dst){
	int yy,offset,scrollx,scrolly,col,row,ofs;
	u16 *dst,data;

	if(!_visible)//0bg 1bg2
		return;
	ofs = SL(SR(_scrollx,13),11) + SL((u8)SR(_scrollx,5),3);
	yy=0;
	dst=&_dst[yy];
	for(row=0;row <8;row++,yy += _char_height){
		u16 *pp=dst;
		_xx=0;
		_xx=-(_scrollx&31);
		for(col=0;_xx<256;col++,_xx+=_char_width){
			offset=(col*8)+(row) + ofs;
			data= ((u16 *)_tile_ram)[offset];
			_tileno = (data & 0x1ff);
			_palno = SL(_idx,8)+(0x180)+SL(SR(data,10) & 0xf,4);

			_flipy = SR(data,15);
			_flipx = SR(data,14);
			_tileno *=0x800;
			__tile::Draw(pp);
B:
			pp += _char_width;
		}
A:
		dst += _screen_width*_char_height;
	}
}

void M1943Gpu::__layer::Init(int n,M1943Gpu &g){
	__1943tile::Init(n,g);
	_tile_ram= _char_ram + 0x98000 + (n)*0x8000;
	_setSize(32,32);
	_char_ram += 0x8000+(n)*0x40000;
	if(n){
		_setPlanesOffset(4,0,4,0x40000,0x40004);
	}
	else{
		_setPlanesOffset(4,0,4,0x100000,0x100004);
	}
	_scrollx=_scrolly=0;
//	_swap_col=1;
}

void M1943Gpu::__layer::Reset(){
	__1943tile::Reset();
	//_scrollx=_scrolly=0;
}

int M1943Gpu::__layer::Save(){
	u16 *dst,*bits,screen_width,screen_height,offset;
	int res=-1;

	screen_width=_screen_width;
	screen_height=_screen_height;
	_screen_width=256*20;
	_screen_height=256;//256 tile x row

	if(!(dst = (u16 *)malloc(_screen_width*_screen_height*sizeof(u16))))
		return -1;
	_dst=dst;
		_screen_start=(void *)_dst;
	_screen_end=(void *)((u16 *)_dst + _screen_width*_screen_height);
	memset(bits=dst,0,_screen_width*_screen_height*sizeof(u16));

	for(int y=0,offset=0;y<_screen_height;y+=256){
		u16 *p=dst;

		for(int x=0;x<_screen_width;x+=256){
			u16 *pm=&p[x];
			offset = ((x / 2048) *8)+ (y%2048);
			for(int row=0,yy=0;row < SR(256,5);row++,yy += _char_height){
				u16 *pp=pm;

				for(int col=0,_xx=0;col<SR(256,5);col++,_xx+=_char_width,offset+=8){
					u16 data= ((u16 *)_tile_ram)[offset];
					_tileno = (data & 0x1ff);
			_palno = SL(_idx,8)+(0x180)+SL(SR(data,10) & 0xf,4);

			_flipy = SR(data,15);
			_flipx = SR(data,14);
			//_tileno=01;
			_tileno *=0x800;
		//	if((row & 1) && (col&1))
					__tile::Draw(pp);

					pp += _char_width;
				}
				pm += _screen_width*_char_height;
			}

			//p += 256*_screen_width;
		}

		dst += 255*_screen_width;

		for(int x=0;x<_screen_width;x++)
			*dst++ = 0x1f|0x3e0|0x7c00;
	}

	{
		char s[1024];

		sprintf(s,"layer %d.bmp",_idx);
		SaveBitmap((char *)s,_screen_width,_screen_height,0,16,(u8 *)bits);
	}
	free(bits);

	res=0;
A:
	_screen_width=screen_width;
	_screen_height=screen_height;
	return res;

}

int M1943Gpu::__layer::_extract(u32 offs){
	u32 data = *(u32 *)((u8 *)_tile_ram + offs);
	_tileno= ((u8)data | (data & 0xe000) >> 5);

	_xx = SR(data,24) | ((data & 0x1000) >> 4);
	int sy = (int)(u8)SR(data,16);

	_palno = 0x380+SR(data & 0xf00,4);
	_tileno=SL(_tileno,9);
	_flipx=_flipy=0;
	return 0;
}

int M1943Gpu::__layer::_drawToMemory(u16 *dst,u32 *params,GPU &g){
	_trans_pen=-1;
	_screen_width=256;
	_screen_height=256;
	_dst=dst;
	_screen_start=(void *)_dst;
	_screen_end=(void *)((u16 *)_dst + _screen_width*_screen_height);
	_scrollx=params[3];
	_xx=0;
	_visible=1;
	__layer::Draw(dst);
	return 0;
}

void M1943Gpu::__fglayer::Draw(u16 *_dst){
	int offset;

	if(!_visible)
		return;

	u16 *dst=_dst+_screen_width*_char_height*4;
	for(int row=4;row <29;row++){
		u16 *pp=dst;
		_xx=0;
		for(int col=0;col<32;col++,_xx +=_char_width){
			offset=(row)*32+col;
			u16 data=MAKEHWORD(_tile_ram[offset],_tile_ram[offset+0x400]);
			_tileno = SL((u8)data|SR(data & 0xe000,5),7);

			_palno = 0x100+SR(data & 0x1f00,6);
			_flipy = 0;
			_flipx = 0;

			__tile::Draw(pp);
			pp += _char_width;//_screen_width*_char_height;
		}
		dst += _screen_width*_char_height;
	}
}

void M1943Gpu::__fglayer::Init(int n,M1943Gpu &g){
	__1943tile::Init(n,g);
	_tile_ram = ((u8 *)g._pal_ram) - 0x400;
	_char_height=_char_width=8;
	_swap_col=0;
}

void M1943Gpu::__oam::Init(int n,M1943Gpu &g){
	_priority=NULL;
	__1943tile::Init(n,g);
	_setSize(16,16);
	_tile_ram=(u8 *)g._sprite_ram;
	_char_ram+=0x8000 + 0x40000+0x10000;
	_setPlanesOffset(4,0,4,0x100000,0x100004);
}

void M1943Gpu::__oam::Reset(){
	if(_priority)
		memset(_priority,0,_screen_width*_screen_height);
}

void M1943Gpu::__oam::Draw(u16 *dst){
	int xx;

	if(!_visible)
	return;

	for (int offs = 0; offs < 0x1000; offs += 32){
		u32 data = *(u32 *)((u8 *)_tile_ram + offs);
		if(!data) continue;
		_tileno= ((u8)data | (data & 0xe000) >> 5);
	//	if(!_tileno) continue;
		_xx = SR(data,24) | ((data & 0x1000) >> 4);
		if(_xx<0 || _xx > _screen_width)
			continue;
		int sy = (int)(u8)SR(data,16);
		if(sy<0 || sy>_screen_height)
			continue;
		_palno = 0x380+SR(data & 0xf00,4);
		_tileno=SL(_tileno,9);
		_flipy=_flipx = 0;
		if((*(u16 *)((u8 *)_regs + 0x804) & 0x40)){
			sy = 240-sy;
			_xx= 240-_xx;
			_flipy=1;
			_flipx =1;
		}
		__tile::Draw(&dst[(sy*_screen_width)+_xx]);
	}
	//printf("\n");
}

u32 M1943Gpu::__oam::_translateColor(u32 c){
	u32 n=(u64)_dst -(u64)_screen_start;
	//printf("%u %x %x",n/2,_priority[n/2],c);
//	printf(" %x\n",c);
	if(_priority[n/2])
		return *_dst;
	_priority[n/2]=0xff;
	c=_pal[c];
	return _pal[c];
}

int M1943Gpu::Load(void *m){
	u8 *p=_proms=(u8 *)m;

	for (int i = 0; i < 0x100; i++){
		int bit0, bit1, bit2, bit3;

		// red component
		bit0 = (p[i] >> 0) & 0x01;
		bit1 = (p[i] >> 1) & 0x01;
		bit2 = (p[i] >> 2) & 0x01;
		bit3 = (p[i] >> 3) & 0x01;
		const int r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		// green component
		bit0 = (p[i + 0x100] >> 0) & 0x01;
		bit1 = (p[i + 0x100] >> 1) & 0x01;
		bit2 = (p[i + 0x100] >> 2) & 0x01;
		bit3 = (p[i + 0x100] >> 3) & 0x01;
		const int g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		// blue component
		bit0 = (p[i + 0x200] >> 0) & 0x01;
		bit1 = (p[i + 0x200] >> 1) & 0x01;
		bit2 = (p[i + 0x200] >> 2) & 0x01;
		bit3 = (p[i + 0x200] >> 3) & 0x01;
		const int b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		((u16 *)_palette)[i]= SR(r,3)|SL(SR(g,3),5)|SL(SR(b,3),10);
	}

	p += 0x300;

	for (int i = 0x00; i < 0x80; i++){//fglayer
		const u8 n = (p[i] & 0x0f) | 0x40;
		((u16 *)_palette)[i+0x100]= n;
	}

	for (int i = 0x80; i < 0x180; i++){//bg
		const u8 n = ((p[0x200 + (i - 0x080)] & 0x03) << 4) |
				((p[0x100 + (i - 0x080)] & 0x0f) << 0);
		((u16 *)_palette)[i+0x100]= n;
	}

	for (int i = 0x180; i < 0x280; i++){//bg
		const u8 n =
				((p[0x400 + (i - 0x180)] & 0x03) << 4) |
				((p[0x300 + (i - 0x180)] & 0x0f) << 0);
		((u16 *)_palette)[i+0x100]= n;
	}

	for (int i = 0x280; i < 0x380; i++){//oams
		const u8 n =
				((p[0x600 + (i - 0x280)] & 0x07) << 4) |
				((p[0x500 + (i - 0x280)] & 0x0f) << 0);
		((u16 *)_palette)[i+0x100]= n|0x80;
	}

	return 0;
}

};