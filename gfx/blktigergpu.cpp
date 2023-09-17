#include "blktigergpu.h"
#include "utils.h"

namespace blktiger{

#ifdef _DEVELOP
static FILE *lino=0;
#endif

BlkTigerGpu::BlkTigerGpu() : GPU(){
	_simm=NULL;
	_bank=0;
#ifdef _DEVELOPa
	if(!lino)
		lino=fopen("gpu.txt","wb");
#endif
}

BlkTigerGpu::~BlkTigerGpu(){
#ifdef _DEVELOP
	if(lino)
		fclose(lino);
	lino=0;
#endif
}

int BlkTigerGpu::Init(){
	if(GPU::Init(256,224))//vb-start 246
		return -1;//59.59Hz
	if(!(_screen = malloc(_width*_height*sizeof(u16) + 0x10000*8 + 0x400*sizeof(u16))))
		return -1;
	_palette = ((u16 *)_screen + _width*_height*sizeof(u16));
	_simm=(u8 *)&_palette[0x400];
	for(int i=0;i<sizeof(_layers)/sizeof(struct __tile *);i++){
		if(_layers[i])
			_layers[i]->Init(i,*this);
	}
	return 0;
}

int BlkTigerGpu::Reset(){
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

int BlkTigerGpu::Run(u8 *,int cyc){
	if((_cycles+=cyc)<381)
		return 0;
	_cycles-=381;
	if(++_line==224)
		return 1;
	if(_line==262){
		_line=0;
		machine->OnEvent((u32)-1,0);
	}
	return 0;
}

int BlkTigerGpu::Update(){
	memset(_screen,0,_width*_height*2);
	{
		u8 *p;

		//xBRG_444
		p=(u8 *)_pal_ram;
		for(int i=0;i<0x400;i++){
			int c,cc;

			c=p[i]|SL(p[i+0x400],8);
			cc=SL(c&0xf00,3) | SL(c&0xf,6) | SR(c&0xf0,3);
			_palette[i]=cc;
		}
	}

	for(int i=0;i<sizeof(_layers)/sizeof(struct __tile *);i++){
		if(!_layers[i])
			continue;
		_layers[i]->Reset();
		if(_layers[i]->_visible)
			_layers[i]->Draw((u16 *)_screen);
	}
	GPU::Update();
	Draw(NULL);
	return 0;
}

BlkTigerGpu::__tile::__tile(){
	_value=0;
	_char_width=16;
	_char_height=16;
	_visible=1;
}

void BlkTigerGpu::__tile::Init(int n,BlkTigerGpu &g){
	_idx=n;
	_char_ram=(u8 *)g._char_ram;
	_tile_ram=(u8 *)g._gpu_mem;
	_dst=(u16 *)g._screen;
	_regs=(u32 *)g._gpu_regs;
	_pal=(u16 *)g._palette;

	_screen_width=g._width;
	_screen_height=g._height;
	_screen_end=(void *)((u16 *)g._screen + _screen_width*_screen_height);
}

void BlkTigerGpu::__tile::_drawRow(u16 *p){
	int xi;

	xi=1;
	if(_flipx){
		p += _char_width;
		xi=-1;
	}
	for(int i=0;i<_char_width && _xx<_screen_width;i++,_xx++){
		int xx,ofs;
		int x=((i&4)*2)+((i&8)*32)+(_yy*16);

		xx= x + (i&3);
		ofs=_tileno+0x100000+4+xx;
		int col,px=0;;
		col =_src[(ofs)/8] & SR(0x80,ofs&7);
		if(col)
			px|=8;
		ofs=_tileno+0x100000+xx;
		col =_src[(ofs)/8] & SR(0x80,ofs&7);
		if(col)
			px|=4;
		ofs=_tileno+4+xx;
		col =_src[(ofs)/8] & SR(0x80,ofs&7);
		if(col)
			px|=2;
		ofs=_tileno+0+xx;
		col =_src[(ofs)/8] & SR(0x80,ofs&7);
		if(col)
			px|=1;
		//fprintf(lino," %x ",xx);
		if(px != 15)
			*p=_pal[_palno+px];
		p+=xi;
	}
		//fprintf(lino,"\n");
}

void BlkTigerGpu::__tile::Draw(u16 *pp){
	int yi,xx;

	yi=_screen_width;
	if(_flipy){
		yi=-_screen_width;
		pp+=_screen_width*_char_width;
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

void BlkTigerGpu::__tile::Reset(){
	_draw=_init=_layout=0;
	_xx=0;
}

void BlkTigerGpu::__layer::Draw(u16 *_dst){
	int yy,offset,scrollx,scrolly;

	if(!_visible)
		return;

	u16 *dst=_dst;

	offset=0;
	if(_layout) {
		printf("draw layyer layout %d\n",_idx);
		goto A;
	}
	//printf("layer %d %d %d\n",_idx,_scrollx,_scrolly);


	for(int row=0,yy=_scrolly%16;row <16;row++,yy += _char_height){
		u16 *pp=dst;
		if(yy < 0)
			continue;
#ifdef _DEVELOP
		if(lino)
		fprintf(lino,"%d %d  %d\t",_scrollx,_scrolly,_yy);
#endif
		for(int col=0,_xx=0;col<16;col++,_xx+=_char_width){
			scrollx=SR(_scrollx+_xx,4);
			scrolly=SR(_scrolly+16+yy,4);

			offset = SL(scrollx & 0xf,1) + SL(scrolly & 0xf,5) + SL(scrollx & 0x70,5) + SL(scrolly & 0x30,8);

			u16 data=*((u16 *)&_tile_ram[offset]);
			_tileno = (data & 0x7ff)*0x200;

			_palno = (SR(data,11) & 0xf)*16;
			_flipy = 0;
			_flipx = SR(data,15);

			__tile::Draw(pp);
			//fprintf(lino,"\n");
			pp += _char_width;
		}
#ifdef _DEVELOP
		if(lino)
			fprintf(lino,"\n");
#endif
		if(yy>-1)
			dst += _screen_width*_char_height;
	}
#ifdef _DEVELOP
	if(lino)
		fflush(lino);
#endif
//	fclose(lino);
	//lino=0;
A:
	offset=0;
}

void BlkTigerGpu::__layer::Init(int n,BlkTigerGpu &g){
	__tile::Init(n,g);
	_scrollx=_scrolly=0;
}

void BlkTigerGpu::__layer::Reset(){
	__tile::Reset();
	//_scrollx=_scrolly=0;
}

int BlkTigerGpu::__layer::Save(){
	u16 *dst,*bits,screen_width,screen_height,offset;
	int res=-1;

	screen_width=_screen_width;
	screen_height=_screen_height;
	_screen_width=2048;
	_screen_height=1024;//256 tile x row

	if(!(dst = (u16 *)malloc(_screen_width*_screen_height*sizeof(u16))))
		return -1;
	memset(bits=dst,0,_screen_width*_screen_height*sizeof(u16));
#ifdef _DEVELOP
	if(lino)
		fprintf(lino,"\n\n");
#endif
	for(int y=0,offset=0;y<_screen_height;y+=256){
		u16 *p=dst;


		for(int x=0;x<_screen_width;x+=256){
			u16 *pm=&p[x];

			for(int row=0,yy=0;row < SR(256,4);row++,yy += _char_height){
				u16 *pp=pm;

				for(int col=0,_xx=0;col<SR(256,4);col++,_xx+=_char_width,offset+=2){
					u16 data=*((u16 *)&_tile_ram[offset]);
					_tileno = (data & 0x7ff)*0x200;

					_palno = (SR(data,11) & 0xf)*16;
					_flipy = 0;
					_flipx = SR(data,15);

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

void BlkTigerGpu::__txlayer::_drawRow(u16 *p){
	int xi;

	xi=1;
	if(_flipx){
		p += _char_width;
		xi=-1;
	}

	for(int i=0;i<_char_width;i++){
		int px,xx,ofs;
		int x=((i&4)*2)+(_yy*16);

		xx= x + (i&3);
		px=0;
		ofs=_tileno+4+xx;
		int col =_src[(ofs)/8] & SR(0x80,ofs&7);
		if(col)
			px |= 2;
		ofs=_tileno+0+xx;
		col =_src[(ofs)/8] & SR(0x80,ofs&7);
		if(col)
			px |= 1;
		if(px != 3)
			*p = _pal[_palno+px];
		p+=xi;
	}
}

void BlkTigerGpu::__txlayer::Draw(u16 *_dst){
	if(!_visible) return;

	u16 *dst=_dst;
	int  yy,offset=0;

	for(int row=0,yy=-16;row <32;row++,yy+=_char_height){
		u16 *pp=dst;

		for(int col=0,_xx=0;col<32;col++,offset++,_xx+=_char_width){
			if(yy<0) continue;
			//offset=(col & 0x0f) + ((liinee & 0x0f) << 4) + ((col & 0x30) << 4) + ((row & 0x70) << 6);
			u8 data=_tile_ram[offset+0x400];
			_tileno = (128*(_tile_ram[offset]|SL(data & 0xe0,3)));
		//	if(_tileno > 0x8000) continue;
			_palno = 0x300+SL(data & 0x1f,2);

			_flipy = 0;
			_flipx = 0;

			__tile::Draw(pp);

			pp += _char_width;
		}
		if(yy >=0)
			dst += _screen_width*_char_height;
	}
}

void BlkTigerGpu::__txlayer::Init(int n,BlkTigerGpu &g){
	__layer::Init(n,g);
	_tile_ram = ((u8 *)g._pal_ram)-0x800;
	_char_ram-=0x8000;
	_char_height=_char_width=8;
}

void BlkTigerGpu::__oam::Init(int n,BlkTigerGpu &g){
	__tile::Init(n,g);
	_tile_ram=(u8 *)g._sprite_ram;
	_char_ram+=0x40000;
}

void BlkTigerGpu::__oam::Draw(u16 *dst){
	if(!_visible) return;

	//printf("oam %d %d\n",_screen_width,_screen_height);
	for (int offs = 0x200; offs >= 0; offs -= 4){
		int attr = _tile_ram[offs + 1];
		_tileno= (_tile_ram[offs] | (attr & 0xe0) << 3);
	//	if(!_tileno) continue;
		_xx = _tile_ram[offs + 3]- ((attr & 0x10) << 4) +8;

		if(_xx<0 || _xx > _screen_width) continue;
		int sy = _tile_ram[offs + 2]-16;
		if(sy<0 || sy>_screen_height) continue;
		_tileno=SL(_tileno,9);
	//	if(data)
		//printf("%x %x\n",_tileno,data);
		_palno = 0x200+SL(attr & 0x07,4);
		_flipx = SR(attr & 0x08,3);

		/*if (flip_screen())
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
		}
		*/

		/*m_gfxdecode->gfx(2)->transpen(bitmap, cliprect,
				code,
				color,
				flipx, flip_screen(),
				sx + 128, sy + 6, 15);
		*/
		//printf("oam %d %d %d %x\n",sx,sy,offs/4,_tileno);
		__tile::Draw(&dst[((sy)*_screen_width)+_xx]);
	}
}

};