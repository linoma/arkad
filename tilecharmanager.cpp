#include "tilecharmanager.h"

TileCharManager::TileCharManager(){
	_pal_ram=_sprite_ram=_tile_ram=_char_ram=0;
}

TileCharManager::~TileCharManager(){
}

TileCharManager::__tile::__tile() : __graphics_manager(){
	_xx=_yy=0;
	_tileno=_palno=0;
		//int _y_inc[64],_x_inc[64];
	_src=0;
	_screen_end=_screen_start=0;
	_value=0;
	_trans_pen=0;
	_visible=0;
	_enabled=1;
	_setPlanesOffset(2,0,4);
	_setSize(8,8);
}

void TileCharManager::__tile::Init(int n,GPU &g){
	_idx=n;
	g.InitGM(this);
	_screen_start=(void *)_dst;
	_screen_end=(void *)((u16 *)_dst + _screen_width*_screen_height);
}

void TileCharManager::__tile::_drawRow(u16 *p){
	int xi,xx,ofs,px,x,xe,n,*pr,*pc;

	xi=1;
	if(_flipx){
		p += _char_width;
		xi=-1;
	}
	xe=_char_width;
	xx=_screen_width-_xx;
	if(xx <= 0)
		return;
	if(xx < xe)
		xe=xx;
	xx=(int)((u64)_screen_end - (u64)p);
	if(xx<=0)
		return;
	if(xx < xe)
		xe=xx;
	pc = _x_inc;
	pr = _y_inc;
	if(_swap_col) {
		pc = _y_inc;
		pr = _x_inc;
	}
	for(int i=0;i<xe;i++,_xx++){
		if(_xx < 0)
			goto Z;
		x=pc[i] + pr[_yy];
		xx= x;

		//x=((i&4)*2) + SL(i & 0x18,5+SR(_char_width,5)) + (_yy*16);
		//xx= x + (i&3);
		px=0;
		for(n =0;n<_planes;n++){
			ofs=(_tileno+_planes_ofs[n] + xx);
			if(_src[SR(ofs,3)] & SR(0x80,ofs&7))
				px |= SL(1,n);
		}
	//	printf("%x ",px);
		if(px != _trans_pen){
			_dst=p;
			*p=_translateColor(px+_palno);
		}
Z:
		p += xi;

	}
		//fprintf(lino,"\n");
}

u32 TileCharManager::__tile::_translateColor(u32 c){
	return _pal[c];
}

void TileCharManager::__tile::Draw(u16 *pp){
	int yi,xx;

	yi=_screen_width;
	if(_flipy){
		yi*=-1;
		pp += _screen_width*(_char_height-1);
	}
	_src = (u8 *)_char_ram;
	xx=_xx;
	//printf("%d:%x ",_xx,_tileno);
	for(_yy=0;_yy < _char_height;_yy++){
		_xx=xx;
		_drawRow(pp);
		pp+=yi;
	}
	_xx=xx;
}

void TileCharManager::__tile::Reset(){
	_draw=_init=_layout=0;
	_xx=0;
}

int TileCharManager::__tile::_extract(u32 offs){
	u32 data = *(u32 *)((u8 *)_tile_ram + offs);
	_tileno= ((u8)data | (data & 0xe000) >> 5);

	_xx = SR(data,24) | ((data & 0x1000) >> 4);
	int sy = (int)(u8)SR(data,16);

	_palno = 0x380+SR(data & 0xf00,4);
	_tileno=SL(_tileno,9);
	_flipx=_flipy=0;
	printf("extract\n");
	return 0;
}

int TileCharManager::__tile::_drawToMemory(u16 *dst,u32 *params,GPU &g){
	//idx=0x67c;
	u16 *p;

	_trans_pen=-1;
	p=_dst;
	_dst=dst;
	_screen_start=(void *)_dst;
	_screen_end=(void *)((u16 *)_dst + _screen_width*_screen_height);
	_xx=0;
	_visible=1;

	if(_multiple){
		_extract(params[0]);
		__tile::Draw(dst);
	}
	else
		Draw(dst);
	params[1]=MAKELONG(_screen_width,_screen_height);
	params[4]=MAKELONG(_char_width,_char_height);
	_dst=p;
	_screen_start=(void *)_dst;
	_screen_end=(void *)((u16 *)_dst + _screen_width*_screen_height);
	return 0;
}

int TileCharManager::__tile::_setPlanesOffset(int n,...){
	va_list arg;

	va_start(arg, n);
	_planes=n;
	for(int i=0;i<n;i++)
		_planes_ofs[i]=va_arg(arg,int);
	va_end(arg);
	return 0;
}

int TileCharManager::__tile::_setSize(int w,int h){
	_char_width=w;
	_char_height=h;
	for(int i=0;i<h;i++)
		_y_inc[i]=i*0x10;
	for(int i=0;i<w;i++)
		_x_inc[i]=((i&4)*2)+SL(i&0x18,5+SR(_char_width,5)) + (i&3);
	return 0;
}

int TileCharManager::__tile::_setHorzOffset(int n,...){
	va_list arg;

	va_start(arg, n);
	for(int i=0;i<n;i++)
		_y_inc[i]=va_arg(arg,int);
	va_end(arg);
	return 0;
}

int TileCharManager::__tile::_setVertOffset(int n,...){
	va_list arg;

	va_start(arg, n);
	for(int i=0;i<n;i++)
		_x_inc[i]=va_arg(arg,int);
	va_end(arg);
	return 0;
}