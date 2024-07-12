#include "tehkanwcgpu.h"
#include "utils.h"


namespace tehkanwc{

tehkanwcGpu::tehkanwcGpu() :  GPU(){
	_palette=NULL;
}

tehkanwcGpu::~tehkanwcGpu(){
}

int tehkanwcGpu::Run(u8 *,int cyc,void *obj){
	if((_cycles+=cyc)<381)
		return 0;
	_cycles-=381;
	if(++__line==240)
		return 1;
	if(__line==256){
		__line=0;
		machine->OnEvent((u32)-1,0);
	}
	return 0;
}

int tehkanwcGpu::Init(){
	if(GPU::Init(256,240,0,0x800))//vb-start 246
		return -1;//59.59Hz
	_palette = ((u16 *)_screen + _width*_height);

	for(int i=0;i<sizeof(_layers)/sizeof(struct __tile *);i++){
		if(_layers[i])
			_layers[i]->Init(i,*this);
	}
	return 0;
}

int tehkanwcGpu::Reset(){
	GPU::Reset();
	if(_screen)
		memset(_screen,0,_width*_height*sizeof(u16));
	for(int i=0;i<sizeof(_layers)/sizeof(struct __tile *);i++){
		if(_layers[i])
			_layers[i]->Reset();
	}
	_cycles=0;
	return 0;
}

int tehkanwcGpu::Update(u32 flags){
	if(Clear()) return -1;

	int scrollx=0x1ff & *(u16 *)&((u8 *)_gpu_regs)[0xec00];
	int scrolly=*(u16 *)&((u8 *)_gpu_regs)[0xec02];

	int flipx=SR(*((u8 *)_gpu_regs + 0xf860),6)&1;
	int flipy=SR(*((u8 *)_gpu_regs + 0xf870),6)&1;
	{
		u16 *p;

		//xBRG_444 cc=SL(c&0xf00,3) | SL(c&0xf,6) | SR(c&0xf0,3);
		//xBGR_444 cc=SL(c&0xf00,3)
		p=(u16 *)_pal_ram;
		for(int i=0;i<0x300;i++){
			int c,cc;

			c=p[i];
			c=SR(c,8)|SL(c&0xf,8);
			cc=SL(c&0xf00,3) | SL(c&0xf,1) | SL(c&0xf0,2);
			_palette[i]=cc;
		}
	//	printf("scroll X:%x Y:%x flip: %x %x\n",scrollx,scrolly,flipx,flipy);
	}

	for(int i=0;i<sizeof(_layers)/sizeof(struct __tile *);i++){
		if(!_layers[i])
			continue;
		_layers[i]->Reset();
		if(_layers[i]->_visible){
			struct __layer *p=(struct __layer *)_layers[i];
			p->_scrollx=scrollx;
			p->_scrolly=scrolly;
			p->Draw((u16 *)_screen);
		}
	}
	GPU::Update();
	Draw(NULL);
	return 0;
}

tehkanwcGpu::__tile::__tile(){
	_value=0;
	_char_width=8;
	_char_height=8;
	_visible=1;
}

void tehkanwcGpu::__tile::Init(int n,tehkanwcGpu &g){
	_idx=n;
	_char_ram=(u8 *)g._char_ram;
	_tile_ram=(u8 *)g._gpu_mem;
	_dst=(u16 *)g._screen;
	_regs=(u32 *)g._gpu_regs;
	_pal=(u16 *)g._palette;

	_screen_width=g._width;
	_screen_height=g._height;
	_screen_start=(u64)g._screen;
	_screen_end=(u64)_screen_start + _screen_width*_screen_height*sizeof(u16);
}

void tehkanwcGpu::__tile::_drawRow(u16 *p){
	int xi;

	xi=1;
	if(_flipx){
		p += _char_width-1;
		xi=-1;
	}
	for(int i=0;i<_char_width && _xx<_screen_width && (u64)p < _screen_end;i+=2,_xx++){
		int px=_src[_tileno + ((i&0x8)*4) +(_xx & 3) + ((_yy&7)*4) + ((_yy & 0x8)*8)];
		if(px&0xf)
			*p=_pal[(_palno+(px&0xf))];
		p+=xi;
		if((px=SR(px,4)))
			*p=_pal[(_palno+px)];
		p+=xi;
	}
}

void tehkanwcGpu::__tile::Draw(u16 *pp){
	int yi,xx;

	yi=_screen_width;
	if(_flipy){
		yi=-_screen_width;
		pp+=_screen_width*(_char_height-1);
	}
	_src = ((u8 *)_char_ram);
	xx=_xx;
	for(_yy=0;_yy<_char_height;_yy++){
		_xx=xx;
		_drawRow(pp);
		pp+=yi;
	}
	_xx=xx;
}

void tehkanwcGpu::__tile::Reset(){
	_draw=_init=_layout=0;
	_xx=0;
}

void tehkanwcGpu::__layer::Draw(u16 *_dst){
	int yy,offset,scrollx,scrolly;

	if(!_visible)
		return;
	u16 *dst=_dst;
	//printf("draw");
	for(int row=0,yy=offset=0;row <30;row++,yy += _char_height){
		u16 *pp=dst;

		for(int col=0,_xx=0;col<32;col++,_xx+=_char_width,offset++){
			u16 data=MAKEHWORD(((u8 *)_tile_ram)[offset&0x3ff],((u8 *)_tile_ram)[0x400+(offset&0x3ff)]);
			_tileno = ((u8)data | SR(data  & 0x1000,4))* 32;
			_palno = (SR(data,8) & 0xf)*16;
			//printf(" %x",data);
			_flipx = SR(data,14);
			_flipy = SR(data,15);

			__tile::Draw(pp);
			pp += _char_width;
		}
		dst += _screen_width*_char_height;
	}
	//printf("\n");
}

void tehkanwcGpu::__layer::Init(int n,tehkanwcGpu &g){
	__tile::Init(n,g);
	_scrollx=_scrolly=0;
}

void tehkanwcGpu::__layer::Reset(){
	__tile::Reset();
	_scrollx=_scrolly=0;
}

int tehkanwcGpu::__layer::Save(){
	return -1;
}

void tehkanwcGpu::__bg_layer::Init(int n,tehkanwcGpu &g){
	__layer::Init(n,g);
	_char_width=16;
	_char_ram += 0x14000;
	_tile_ram += 0x1000;
	_pal+=0x200;
}

void tehkanwcGpu::__bg_layer::Draw(u16 *_dst){
	int yy,offset;

	if(!_visible)
		return;
	u16 *dst=_dst;
	//printf("draw %d %d\n",_scrolly,_scrollx);
	offset=0;
	_src = ((u8 *)_char_ram);
	for(int row=0,yy=0;row <30 && (u64)dst - _screen_end;row++,yy += _char_height){
		u16 *pp=dst;
		offset=SR(yy+_scrolly+16,3)*32 + SR(_scrollx,4);

		for(int col=0,_xx=0;col<17;col++,_xx+=_char_width,offset++){
			u16 data=((u16 *)_tile_ram)[offset&0x3ff];
			_tileno = (u8)data | SR(data&0x3000,4);
			_palno = SR(data & 0xf00,4);
			_flipy = SR(data,15);
			_flipx = SR(data,14);
			_tileno *= 64;
			__tile::Draw(pp);
			pp += _char_width;
		}
A:
		dst += _screen_width*_char_height;
	}
}

void tehkanwcGpu::__oam::Init(int n,tehkanwcGpu &g){
	__layer::Init(n,g);
	_char_width=16;
	_char_height=16;
	_tile_ram=(u8 *)g._sprite_ram;
	_char_ram+=0x4000;
	_pal += 0x100;
}

void tehkanwcGpu::__oam::Draw(u16 *dst){
	if(!_visible)
		return;
	for (int offs = 0; offs < 0x400; offs += 4){
		u32 data= *((u32 *)&_tile_ram[offs]);
		//printf(" %x",data);
		_tileno = (u8)data | SR(data&0x800,3);
		_palno = SR(data & 0x700,4);
		_flipx = SR(data,14);
		_flipy = SR(data,15);
		int xx = (u8)SR(data,16) - 128;
		int sy = SR(data,24);
		_tileno *= 128;
		_xx=0;
		__tile::Draw(&dst[(sy*_screen_width)+xx]);
	}
}

};