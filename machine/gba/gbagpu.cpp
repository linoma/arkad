#include "gbam.h"

namespace gba{

u8 gbagpu::__oam::_data[]={0};

#define MAKEZPIXEL(a,b,c) ((u16)(a | (b << 4) | (c << 8)))

#define NULLZPIXEL 	((u16)0xFFF)
#define NULLPIZEL 	NULLZPIXEL
#define NULLZVALUE	0xf
#define GETZPRTSRC(value) (value & 0xf)
#define GETZIDXSRC(value) ((value >> 4) & 0xf)
#define GETZPRTTAR(value) ((value >> 12) & 0xf)
#define GETZIDXTAR(value) ((value >> 16) & 0xf)

gbagpu::gbagpu() : GPU(),TileCharManager(){
	_width=240;
	_height=160;
	_swapBuffer[0] = &gbagpu::_rgbNormalLine;
	_swapBuffer[1] = &gbagpu::_rgbAlphaLayerLine;
	_swapBuffer[2] = &gbagpu::_rgbFadeLineUp;
	_swapBuffer[3] = &gbagpu::_rgbFadeLineDown;
	_swapBuffer[4] = &gbagpu::_rgbNormalLineNoOAM;
	_swapBuffer[5] = &gbagpu::_rgbAlphaLayerLineNoOAM;
	_swapBuffer[6] = &gbagpu::_rgbFadeLineUpNoOAM;
	_swapBuffer[7] = &gbagpu::_rgbFadeLineDownNoOAM;
	__oam::_data[__oam::_visible]=1;
}

gbagpu::~gbagpu(){
}

int gbagpu::Run(u8 *,int cyc,void *obj){
	int res;

	if((_cycles += cyc) < 1232)
		return 0;
	res=0;
	for(;_cycles>=1232 && !res;_cycles -= 1232){
		if(__line==0) {
			Clear();
			_startDrawFrame();
		}
		if(__line < 160){
			RenderLine();
			if(IOREG(REG_DISPSTAT) & 0x10)
				res |= 2;
		}
		if(__line == SR(IOREG(REG_DISPSTAT),8) && (IOREG(REG_DISPSTAT) & 0x20)){
			IOREG(REG_DISPSTAT) |= 4;
			res |= 4;
		}
		else
			IOREG(REG_DISPSTAT) &= ~4;
		if(__line++ == 160){
			IOREG(REG_DISPSTAT) |= 1;
			if((IOREG(REG_DISPSTAT) & 8)){
				res |= 1;
				goto A;
			}
		}
	}
	if(__line >= 228){
		machine->OnEvent(ME_ENDFRAME,0);
		__line=0;
		_cycles=0;
		IOREG(REG_DISPSTAT) &= ~7;
	}
A:
	if(res & 2)
		machine->OnEvent(2,(res & 5) ? 1 : 0);
	if(res & 4)
		machine->OnEvent(4,res & 1);
	//res &= 1;
	IOREG(REG_VCOUNT)=__line;
	return res;
}

int gbagpu::Init(void *a,void *b){
	if(GPU::Init(_width,_height,0,MB(1)))//vb-start 246
		return -1;//59.59Hz
	_ioreg=(u32 *)b;
	_mem=(u8 *)a;

	_palette = _pal_ram;
	pSourceBuffer=(u32 *)&((u16 *)_screen)[_width*_height];
	pTargetBuffer=&pSourceBuffer[_width];
	pOAMBuffer=&pTargetBuffer[_width];
	pZBuffer=&pOAMBuffer[_width];
	pWinBuffer=&pZBuffer[_width];

	_tabColor = (u8 *)&pWinBuffer[_width];
	_tabColorI = &_tabColor[0x220];
	Source=(__layer **)&_tabColorI[0x220];
	Target=Source+5;
	_obj_priority[0]=(u8 *)&Target[5];
	_obj_priority[1]=(u8 *)&_obj_priority[0][132];
	_obj_priority[2]=(u8 *)&_obj_priority[1][132];
	_obj_priority[3]=(u8 *)&_obj_priority[2][132];

	_rotMatrix=(__rotMatrix*)&_obj_priority[3][132];

	for(int i=0;i<sizeof(__layers)/sizeof(__layer);i++){
		__layers[i].Init(i,_mem,_ioreg,*this);
		_layers.push_back(&__layers[i]);
	}
	for(int i=0;i<sizeof(__oams)/sizeof(__oam);i++)
		__oams[i].Init(i,_mem,_ioreg,*this);
	for(int i=0;i<17;i++){
		for(int i1=0;i1<32;i1++){
			_tabColor[i*32+i1] = (u8)(i1 * i >> 4);
			_tabColorI[i*32+i1] = (u8)((31 - i1) * i >> 4);
		}
	}
	_win[0].Visible=_win[1].Visible=1;

	return 0;
}
/*
int gbagpu::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	//_pcm_sync=stoi(m["pcm_sync"]);
	//_gpu_status |= GPU_STATUS_MULTISAMPLE_BILINEAR;
	return GPU::LoadSettings(v);
}
*/

int gbagpu::Reset(){
	GPU::Reset();
	Clear();
	_cycles=0;
	for(auto it=_layers.begin();it!=_layers.end();it++)
		(*it)->Reset();
	for(int i=0;i<sizeof(__oams)/sizeof(__oam);i++)
		__oams[i].Reset();
	for(int i=0;i<4;i++){
		memset(_obj_priority[i],0xff,130);
		_obj_priority[i][129] = 0;
	}
	memset(_rotMatrix,0,32*sizeof(__rotMatrix));
	Source[0]=Target[0]=0;
	_getPixelSprite=&gbagpu::_getPXSprite;
	_changed._value=0;
	return 0;
}

int gbagpu::RenderLine(){
	pSB=pSourceBuffer;
	pTB=pTargetBuffer;
	pOB=pOAMBuffer;
	pZB=pZBuffer;
	pDB=(u16 *)_screen + __line*_width;

	if(IOREG(REG_DISPCNT) & 0x80){
		memset(pDB,255,_width*2);
		return 0;
	}
	if(_changed._winSize){
		__win *w = &_win[0];
		w->left = (u8)(IOREG(0x40) >> 8);
		w->right = (u8)IOREG(0x40);
		if(w->left > w->right)
			w->right = _width;
		w = &_win[1];
		w->left = (u8)(IOREG(0x42) >> 8);
		w->right = (u8)IOREG(0x42);
		if(w->left > w->right)
			w->right = _width;
		w = &_win[0];
		w->top = (u8)(IOREG(0x44) >> 8);
		w->bottom = (u8)IOREG(0x44);
		if(w->top > w->bottom)
			w->bottom = _height;
		w = &_win[1];
		w->top = (u8)(IOREG(0x46) >> 8);
		w->bottom = (u8)IOREG(0x46);
		if(w->top > w->bottom)
			w->bottom = _height;
			_changed._winSize=0;
	}
	if(_changed._winControl){
		__win_control *winc = &_win[0];
		for(int i=0;i<4;i++)
			winc->EnableBg[i] = (IOREG(REG_WININ) & (1 << i)) ? &__layers[i] : NULL;
		winc->EnableObj = (u8)((IOREG(REG_WININ) & 0x10) >> 4);
		winc->EnableBlend = (u8)((IOREG(REG_WININ) & 0x20) >> 5);
		winc = &_win[1];
		for(int i=0;i<4;i++)
			winc->EnableBg[i] = (IOREG(REG_WININ) & (1 << (i + 8))) ? &__layers[i] : NULL;
		winc->EnableObj = (u8)((IOREG(REG_WININ) & 0x1000)>> 12);
		winc->EnableBlend = (u8)((IOREG(REG_WININ) & 0x2000) >> 13);
		winc = &_winOut;
		for(int i=0;i<4;i++)
			winc->EnableBg[i] = (IOREG(REG_WINOUT) & (1 << i)) ? &__layers[i] : NULL;
		winc->EnableObj = (u8)((IOREG(REG_WINOUT) & 0x10) >> 4);
		winc->EnableBlend = (u8)((IOREG(REG_WINOUT) & 0x20) >> 5);
		winc = &_winOam;
		for(int i=0;i<4;i++)
			winc->EnableBg[i] = (IOREG(REG_WINOUT) & (1 << (i + 8))) ? &__layers[i] : NULL;
		winc->EnableObj = (u8)((IOREG(REG_WINOUT) & 0x1000) >> 12);
		winc->EnableBlend = (u8)((IOREG(REG_WINOUT) & 0x2000) >> 13);
		_changed._winControl=0;
	}
	if(_changed._mosaic){
		u8 i = (u8)(IOREG(REG_MOSAIC) & 0x0f);
		__layers[0]._xMosaic = i;
		__layers[1]._xMosaic = i;
		__layers[2]._xMosaic = i;
		__layers[3]._xMosaic = i;
		i = (u8)((IOREG(REG_MOSAIC) >> 4) & 0x0f);
		__layers[0]._yMosaic = i;
		__layers[1]._yMosaic = i;
		__layers[2]._yMosaic = i;
		__layers[3]._yMosaic = i;
		__oam::_data[1] = (u8)((IOREG(REG_MOSAIC) >> 8) & 0xF);
		__oam::_data[2] = (u8)((IOREG(REG_MOSAIC) >> 12) & 0xF);
		_changed._mosaic=0;
	}
	if(_changed._blend){
		u16 v = IOREG(REG_BLENDCNT);
		switch(_blend=SR(v,6)&3){
			case 0:
				_getPixelSprite = &gbagpu::_getPXSprite;
			break;
			case 1:
				_getPixelSprite = &gbagpu::_getPXSpriteAlpha;
			break;
			default:
				_getPixelSprite = &gbagpu::_getPXSpriteBrightness;
			break;
		}
		_blendSO=SR(v,4);
		_blendSB=SR(v,5);
		_blendTO=SR(v,12);
		_blendTB=SR(v,13);
		_blendSA=(v&0x2f) ? 1 : 0;
		_blendTA=(v&0x2f00) ? 1 : 0;
		_buildLayersList();
		_loadAlphaIndex(IOREG(REG_BLENDV));
		_loadBrightnessIndex(IOREG(REG_BLENDY));
		_changed._blend=0;
	}
	for(int i = 0;i<sizeof(__layers)/sizeof(__layer);i++){
		if(__layers[i]._changed._value)
			__layers[i]._load();
	}
	_oamDraw=1;
	switch(_mode){
		case 0:
			if(_winEnable)
				_drawLineModeTileWindow();
			else{
				_drawLineModeTile();
				_drawLineOAM(0,_width);
			}
		break;
		case 1:
			if(_winEnable)
				_drawLineModeTileWindow();
			else{
				_drawLineModeTile();
				_drawLineOAM(0,_width);
			}
		break;
		case 3:
			if(_winEnable)
				_drawLineMode3Window();
			else{
				_drawLineMode3();
				_drawLineOAM(0,_width);
			}
		break;
		default:
			printf("draw %u\n",_mode);
		break;
	}
	if(_winOam.Enable) printf("obj window draw\n");
	(this->*_swapBuffer[(_oamDraw * 4) + _blend])(0,_width);
Z:
	return 0;
}

int gbagpu::Update(u32 flags){
	GPU::Update();
	Draw(NULL);
Z:
	return 0;
}

int gbagpu::write(u32 a,u32 v){
	switch(SR(a,24)){
		case 4:
			switch((u8)a){
				case 0:
					switch((_mode=v&7)){
						case 0:
							for(int i=0;i<4;i++){
								__layers[i]._drawPixel = (DRAWPIXEL)&gbagpu::_drawPixelMode0;
								__layers[i]._initDrawLine  = (INITDRAWLINE)&gbagpu::_initLayerMode0;
								__layers[i]._postDrawPixel  = NULL;//PostdrawPixelMode0;
								__layers[i]._postDrawLine = NULL;
								__layers[i]._drawMode=1;
							}
						break;
						case 1:
							for(int i=0;i<2;i++){
                               __layers[i]._drawPixel = (DRAWPIXEL)&gbagpu::_drawPixelMode0;
								__layers[i]._initDrawLine  = (INITDRAWLINE)&gbagpu::_initLayerMode0;
								__layers[i]._postDrawPixel = NULL;//PostdrawPixelMode0;
								__layers[i]._postDrawLine=NULL;
								__layers[i]._drawMode=1;
							}
                            __layers[2]._drawPixel     = (DRAWPIXEL)&gbagpu::_drawPixelMode1;
                            __layers[2]._initDrawLine  = (INITDRAWLINE)&gbagpu::_initLayerMode1;
                            __layers[2]._postDrawPixel = (POSTDRAWPIXEL)&gbagpu::_postDrawPixelMode1;
                            __layers[2]._postDrawLine  = (POSTDRAWLINE)&gbagpu::_postDrawLineMode1;
							__layers[2]._drawMode=2;

							__layers[3]._drawPixel         = NULL;
							__layers[3]._initDrawLine      = NULL;
							__layers[3]._postDrawLine=NULL;
							__layers[3]._enabled = 0;
						break;
						case 2:
							for(int i=0;i<2;i++){
								__layers[i]._drawPixel     = NULL;
								__layers[i]._initDrawLine  = NULL;
								__layers[i]._postDrawPixel      = NULL;
								__layers[i]._postDrawLine=NULL;
								__layers[i]._drawMode=0;
							}
							for(int i=2;i<4;i++){
								__layers[i]._drawPixel     = (DRAWPIXEL)&gbagpu::_drawPixelMode1;
								__layers[i]._initDrawLine  = (INITDRAWLINE)&gbagpu::_initLayerMode1;
								__layers[i]._postDrawPixel      = (POSTDRAWPIXEL)&gbagpu::_postDrawPixelMode1;
								__layers[i]._postDrawLine=(POSTDRAWLINE)&gbagpu::_postDrawLineMode1;
								__layers[i]._drawMode=2;
							}
						break;
						case 3:
							for(int i=0;i<4;i++){
								__layers[i]._enabled = 0;
								__layers[i]._drawMode=0;
								__layers[i]._drawPixel = (DRAWPIXEL)NULL;
								__layers[i]._initDrawLine = NULL;
								__layers[i]._postDrawPixel = NULL;
								__layers[i]._postDrawLine=NULL;
							}
							__layers[2]._drawPixel = (DRAWPIXEL)&gbagpu::_drawPixelMode3;
							__layers[2]._initDrawLine = (INITDRAWLINE)&gbagpu::_initLayerMode3;
							__layers[2]._postDrawPixel = (POSTDRAWPIXEL)&gbagpu::_postDrawPixelMode3;
							__layers[2]._postDrawLine  = (POSTDRAWLINE)&gbagpu::_postDrawLineMode3;
							__layers[2]._drawMode=3;
						break;
						default:
							for(int i=0;i<4;i++){
								__layers[i]._enabled = 0;
								__layers[i]._drawMode=0;
								__layers[i]._drawPixel = (DRAWPIXEL)NULL;
								__layers[i]._initDrawLine = NULL;
								__layers[i]._postDrawPixel = NULL;
								__layers[i]._postDrawLine=NULL;
							}
						break;
					}
					if((__layers[0]._enabled = ((v & 0x100) >> 8)))
						__layers[0]._changed._control=1;
					if((__layers[1]._enabled = ((v & 0x200) >> 9)))
						__layers[1]._changed._control=1;
					if((__layers[2]._enabled = ((v & 0x400) >> 10)))
						__layers[2]._changed._control=1;
					if((__layers[3]._enabled = ((v & 0x800) >> 11)))
						__layers[3]._changed._control=1;

					_oamEnable=SR(v,12);
					_oamMap=SR(v,6);
					_win[0].Enable = (u8)((v & 0x2000) >> 13);
					_win[1].Enable = (u8)((v & 0x4000) >> 14);
					_winOam.Enable = (u8)(((v & 0x8000) >> 15) & _oamEnable);
					_winEnable = _win[0].Enable | SL(_win[1].Enable,1);
					_changed._blend=1;
					for(int i=0;i<sizeof(__oams) / sizeof(__oam);i++)
						__oams[i]._changed=1;
				break;
				case 8:
				case 9:
					__layers[0]._changed._control=1;
				break;
				case 10:
				case 11:
					__layers[1]._changed._control=1;
				break;
				case 12:
				case 13:
					__layers[2]._changed._control=1;
				break;
				case 14:
				case 15:
					__layers[3]._changed._control=1;
				break;
				case 16:
				case 17:
				case 18:
				case 19:
					__layers[0]._changed._scroll=1;
				break;
				case 20:
				case 21:
				case 22:
				case 23:
					__layers[1]._changed._scroll=1;
				break;                                             //803cd16
				case 24:
				case 25:
				case 26:
				case 27:
					__layers[2]._changed._scroll=1;
				break;
				case 28:
				case 29:
				case 30:
				case 31:
					__layers[3]._changed._scroll=1;
				break;
				case 32:
				case 33:
				case 34:
				case 35:
					__layers[2]._changed._matrix=1;
				break;
				case 36:
				case 37:
				case 38:
				case 39:
					__layers[2]._changed._matrix=1;
				break;
				case 40:
				case 41:
				case 42:
				case 43:
					__layers[2]._changed._center=1;
				break;
				case 44:
				case 45:
				case 46:
				case 47:
					__layers[2]._changed._center=1;
				break;
				case 48:
				case 49:
				case 50:
				case 51:
					__layers[3]._changed._matrix=1;
				break;
				case 52:
				case 53:
				case 54:
				case 55:
					__layers[3]._changed._matrix=1;
				break;
				case 56:
				case 57:
				case 58:
				case 59:
					__layers[3]._changed._center=1;
				break;
				case 60:
				case 61:
				case 62:
				case 63:
					__layers[3]._changed._center=1;
				break;
				case 64:
				case 65:
				case 66:
				case 67:
					_changed._winSize=1;
				break;
				case 68:
				case 69:
				case 70:
				case 71:
					_changed._winSize=1;
				break;
				case 72:
				case 73:
				case 74:
				case 75:
					_changed._winControl=1;
				break;
				case 76:
				case 77:
					_changed._mosaic=1;
				break;
				case 0x50:
				case 0x51:
				case 0x52:                                                 //80022cc  //803b44
				case 0x53:
				case 0x54:
				case 0x55:
					_changed._blend=1;
				break;
			}
		break;
		case 7:
			if((a&6) != 6)
				__oams[SR(a&0x3ff,3)]._changed=1;
		break;
	}
	return 1;
}

int gbagpu::read(u32,u32 *){return -1;}

int gbagpu::InitGM(GM *p){
	if(GPU::InitGM(p)) return -1;
	p->_char_ram=(u8 *)_char_ram;
	p->_tile_ram=(u8 *)_tile_ram;
	p->_pal=(u16 *)_pal_ram;
	return 0;
}

void gbagpu::_initLayerMode0(struct __layer *p){
	u32 y1,tileY;

	if(p->_mosaic && p->_yMosaic)
		y1 = (u32)((__line - (__line % p->_yMosaic)) + p->_scrolly);
	else
		y1 = (u32)(__line + p->_scrolly);
	tileY=(SR(y1,3) & p->_height);
	p->_yy=SL(y1 & 7,3);
	p->_src = p->__tile_ram + SL(tileY & 0x1f,6);
	if(p->_width != 31 && tileY > 31)
		p->_src += 0x800*2;
}

u32 gbagpu::_drawPixelMode0(struct __layer *p){
	u32 x1,tileX;

	x1 = _xx;
	if(p->_mosaic && p->_xMosaic)
       x1 -= (x1 % p->_xMosaic);
	x1 += p->_scrollx;
	//if((x1 & 7) == 0){
		tileX = (x1 >> 3) & p->_width;
		if(tileX > 31 && p->_width != 31)
			tileX = (tileX & 0x1F) + 0x400;
		p->_tileno = ((u16 *)p->_src)[tileX & 0xfffff];
	//}
	tileX = (p->_tileno & 0x3FF) << 6;
	if(p->_tileno & 0x400){
		tileX += 7 - (x1 & 7);
		x1 = ~x1;
	}
	else
		tileX += (x1 & 7);
	tileX += (p->_tileno & 0x800) ? 56 - p->_yy : p->_yy;
	if(p->_palette){
		tileX = (u32)p->__char_ram[tileX];
		if(tileX == 0)
			return (u32)-1;
		return p->_pal[tileX];
	}
	tileX = (u32)p->__char_ram[tileX/2];
	tileX = SR(tileX,SL(x1 & 1,2)) & 0xf;
	if(tileX  == 0)
		return (u32)-1;
	return p->_pal[tileX + SR(p->_tileno & 0xf000,8)];
}

void gbagpu::_initLayerMode1(struct __layer *p){//at start render line
	p->_rot[2][0] = p->_rot[1][0];
	p->_rot[2][1] = p->_rot[1][1];
	p->_src=p->__tile_ram;
}

u32 gbagpu::_drawPixelMode1(struct __layer *p){
	int xTile,yTile,y2,y3;
	u8 width;

	width = (u8)(p->_width - 1);
	xTile = (y2 = p->_rot[2][0] >> 8) >> 3;
	yTile = (y3 = p->_rot[2][1] >> 8) >> 3;
	if(p->_wrap || (xTile >= 0 && xTile < p->_width && yTile >= 0 && yTile < p->_width)){
		if((width = p->__char_ram[((y3 & 7) << 3) + (y2 & 7) +
				(p->_src[((yTile & width) << p->_log2) + (xTile & width)] << 6)
			]) == 0)
			return (u32)-1;
		return p->_pal[width];
	}
	return -1;
}

void gbagpu::_postDrawLineMode1(struct __layer *p){//at end line
	p->_rot[1][0] += p->_rotMatrix[1];
	p->_rot[1][1] += p->_rotMatrix[3];
}

void gbagpu::_postDrawPixelMode1(struct __layer *p){
	p->_rot[2][0] += p->_rotMatrix[0];
	p->_rot[2][1] += p->_rotMatrix[2];
}

void gbagpu::_drawLineModeTile(){
	u32 i5w,*dst;
	u16 zValueSrc,zValueTar,*o;
	__layer *p;
	u8 prtSrc,idxSrc;

	i5w=0;
	for(int i=0;i<4 && (p=Source[i]);i++){
		(((gbagpu *)this)->*p->_initDrawLine)(p);
		if(p->_postDrawLine)
			i5w=1;
	}
	for(int i=0;i<4 && (p=Target[i]);i++){
		(((gbagpu *)this)->*p->_initDrawLine)(p);
		if(p->_postDrawLine)
			i5w=1;
	}
	dst=pSourceBuffer;
	if(i5w)
		goto B;

	switch(_blend){
		case 0:
			for(_xx=0;_xx<_width;_xx++){
				i5w=-1;
				zValueSrc = (u16)NULLZPIXEL;
				for(int i=0;p=Source[i];i++){
					i5w=(((gbagpu *)this)->*p->_drawPixel)(p);
					if(i5w != -1){
						zValueSrc = (u16)MAKEZPIXEL(p->_priority,p->_idx,p->_type);
						break;
					}
				}
				*dst++=i5w;
				pZBuffer[_xx] = (u32)zValueSrc;
			}
		break;
		case 1:
			pTB = (u32 *)pTargetBuffer;
			for(_xx=0;_xx<_width;_xx++,dst++,pTB++){
               *pTB = *dst = (u32)-1;
               zValueSrc = (u16)NULLZPIXEL;
               i5w=-1;
               for(int i1 = 0;p=Source[i1];i1++){
                   if(i5w==-1 && (i5w = (((gbagpu *)this)->*p->_drawPixel)(p)) != -1){
					   prtSrc = p->_priority;
					   idxSrc = p->_idx;
                       zValueSrc = (u16)MAKEZPIXEL(prtSrc,idxSrc,p->_type);
                       *dst = (u32)i5w;
                       break;
                   }
               }
               zValueTar = (u16)NULLZPIXEL;
               i5w=-1;
               for(int i1=0;p=Target[i1];i1++){
                   if(i5w==-1 && (i5w = (((gbagpu *)this)->*p->_drawPixel)(p)) != -1){
                       if(*dst != (u32)-1){
                           if(p->_priority > prtSrc && p->_type != 2)
                               i5w |= 0x80000000;
                           else if(p->_priority < prtSrc || (p->_priority == prtSrc && idxSrc > p->_idx)){
                               *dst |= 0x80000000;
                               zValueSrc = (u16)NULLZPIXEL;
                           }
                       }
                       *pTB =(u32)((i5w & 0x80000000) | (u16)i5w);
                       zValueTar = (u16)MAKEZPIXEL(p->_priority,p->_idx,p->_type);
                       break;
                   }
               }
               pZBuffer[_xx] = (u32)(zValueSrc | (zValueTar << 12));
           }
       break;
       case 2:
       case 3:
           for(_xx=0;_xx<_width;_xx++){
               zValueSrc = (u16)NULLZPIXEL;
               i5w=-1;
               for(int i1=0;p=Source[i1];i1++){
					if((i5w = (((gbagpu *)this)->*p->_drawPixel)(p)) != -1){
                       zValueSrc = MAKEZPIXEL(p->_priority,p->_idx,p->_type);
                       i5w |= 0x80000000;
                       break;
                   }
               }
               if(!i5w && !_blendSB)
                   i5w = 0x80000000;
               *dst++ = (u32)((i5w & 0x80000000) | (u16)i5w);
               pZBuffer[_xx] = (u32)zValueSrc;
           }
       break;
	}
	return;
B:
	switch(_blend){
       case 0:
           for(_xx = 0;_xx < _width;_xx++){
               zValueSrc = (u16)NULLZPIXEL;
               i5w=-1;
               for(int i1=0;p=Source[i1];i1++){
                   if(i5w == -1 && (i5w = (this->*p->_drawPixel)(p)) != -1)
                       zValueSrc = MAKEZPIXEL(p->_priority,p->_idx,p->_type);
                   if(p->_postDrawPixel != NULL)
                       (this->*p->_postDrawPixel)(p);
               }
               *dst++ = (u32)i5w;
				pZBuffer[_xx]= (u32)zValueSrc;
           }
       break;
       case 1:
           pTB = (u32 *)pTargetBuffer;
           for(_xx=0;_xx<_width;_xx++,dst++,pTB++){
               *pTB = *dst = (u32)-1;
               zValueSrc = (u16)NULLZPIXEL;
               i5w=-1;
               for(int i1 = 0;p=Source[i1];i1++){
                   if(i5w == -1 && (i5w = (this->*p->_drawPixel)(p)) != -1){
					   prtSrc = p->_priority;
					   idxSrc = p->_idx;
                       zValueSrc = (u16)MAKEZPIXEL(prtSrc,idxSrc,p->_type);
                       *dst = (u32)i5w;
                   }
                   if(p->_postDrawPixel != NULL)
                       (this->*p->_postDrawPixel)(p);
               }
               zValueTar = (u16)NULLZPIXEL;
               i5w=-1;
               for(int i1=0;p=Target[i1];i1++){
                   if(i5w == -1 && (i5w = (this->*p->_drawPixel)(p)) != -1){
                       if(*dst != (u32)-1){
                           if(p->_priority > prtSrc && p->_type != 2)
                               i5w |= 0x80000000;
                           else if(p->_priority < prtSrc || (p->_priority == prtSrc && idxSrc > p->_idx)){
                               *dst |= 0x80000000;
                               zValueSrc = (u16)NULLZPIXEL;
                           }
                       }
                     /*  else if((!Source[0] || Target[1]) && (i5w & 0x40000000)){//3d layer
                           zValueSrc = MAKEZPIXEL(p->_priority,p->_idx,p->_type);
                           *dst = (u32)i5w;
                           i5w = -1;
                           if(p->_postDrawPixel != NULL)
                               (this->*p->_postDrawPixel)(p);
                           continue;
                       }*/
                       *pTB =(u32)((i5w & 0x80000000) | (u16)i5w);
                       zValueTar = MAKEZPIXEL(p->_priority,p->_idx,p->_type);
                   }
                   if(p->_postDrawPixel != NULL)
                       (this->*p->_postDrawPixel)(p);
               }
               pZBuffer[_xx] = (u32)(zValueSrc | (zValueTar << 12));
           }
       break;
       case 2:
       case 3:
           for(_xx=0;_xx<_width;_xx++){
               zValueSrc = (u16)NULLZPIXEL;
               i5w=-1;
               for(int i1=0;p=Source[i1];i1++){
					if(i5w == -1 && (i5w = (this->*p->_drawPixel)(p)) != -1){
                       zValueSrc =MAKEZPIXEL(p->_priority,p->_idx,p->_type);
                       if(p->_type != 1)
                           i5w |= 0x80000000;
					}
					if(p->_postDrawPixel != NULL)
                       (this->*p->_postDrawPixel)(p);
               }
               if(!i5w && !_blendSB)
                   i5w = 0x80000000;
               *dst++ = (u32)((i5w & 0x80000000) | (u16)i5w);
               pZBuffer[_xx] = (u32)zValueSrc;
           }
       break;
   }
   for(int i1=0;p=Source[i1];i1++){
       if(p->_postDrawLine != NULL)
           (this->*p->_postDrawLine)(p);
   }
   for(int i1=0;p=Target[i1];i1++){
       if(p->_postDrawLine != NULL)
           (this->*p->_postDrawLine)(p);
   }
}

void gbagpu::_drawLineModeTileWindow(){
	__win_control *winc;
	__layer *S[6],*T[6],**p1,*pLayer,*E[6];
	u8 nCS,nCT,CountPoint,nCE;
	u8 n,i2,i5,prtSrc,idxSrc;
	u16 zValueSrc,zValueTar;
	u32 i5w,*p;
	int i,xStart;
	__point_window ptLine[5];
	GETSPRITEPIXEL oldGetPixelSprite;
	__win *Win[2],*w;
	u8 check;
	int x,i1,CountWindow;
	u64 control;

	for(CountWindow = -1,x = 0,i1 = 1;x < 2;x++,i1 <<= 1){
		if(!(_winEnable & i1) || !_win[x].Visible)
           continue;
		w = &_win[x];
		if(w->left == w->right || w->right < w->left)
           continue;
		if(w->Enable && __line >= w->top && __line < w->bottom)
			Win[++CountWindow] = w;
	}
	if(CountWindow > 0 && Win[1]->left <= Win[0]->left){
		w = Win[0];
		Win[0] = Win[1];
		Win[1] = w;
	}
	CountPoint = 0;
	x = i = 0;
	for(i1 = 0;i1 <= CountWindow;i1++){
		check = 0;
		w = Win[i1];
		if(w->left > x){
			if(w->left > w->right && w->right >= i){
				i +=ptLine[CountPoint].width = (u16)((x = w->right) - i);
				ptLine[CountPoint++].winc = w;
				w->right = (u8)(240-1);
			}
			i += ptLine[CountPoint].width = (u16)(w->left - x);
			ptLine[CountPoint++].winc = &_winOut;
		}
		if(w->left < w->right && x < w->right){
			if(i1 != CountWindow && Win[i1+1]->left < w->right){
				x = Win[i1+1]->left - x;
				check = 1;
			}
			else
				x = w->right;
			i +=ptLine[CountPoint].width = (u16)(x - i);
			if(i > 240)
				ptLine[CountPoint].width = (u16)(240 - (i - 240));
			ptLine[CountPoint++].winc = w;
			if(!check)
				continue;
			x = Win[i1+1]->right;
			i +=ptLine[CountPoint].width = (u16)(x - i);
			if(i > 240)
				ptLine[CountPoint].width = (u16)(240 - (i - 240));
			ptLine[CountPoint++].winc = Win[i1+1];
			if(i >= w->right)
               continue;
			x = w->right;
			i +=ptLine[CountPoint].width = (u16)(x - i);
			if(i > _width)
				ptLine[CountPoint].width = (u8)(240 - (i - 240));
			ptLine[CountPoint++].winc = w;
		}
	}
	if(i < 240){
		ptLine[CountPoint].width = (u16)(240 - i);
		ptLine[CountPoint++].winc = &_winOut;
	}
	/*
	printf("W %d %x ",CountPoint,_winEnable);
	for(int i=0;i<CountPoint;i++)
		printf("%u ",ptLine[i].width);
	printf("\n");*/
	p1 = Source;
	for(;*p1;p1++)
		(this->*(*p1)->_initDrawLine)(*p1);
	p1 = Target;
	for(;*p1;p1++)
		(this->*(*p1)->_initDrawLine)(*p1);

	oldGetPixelSprite = _getPixelSprite;
	control=_control;
	for(_xx = i2 = 0;i2<CountPoint;i2++){
		winc = ptLine[i2].winc;
		i = ptLine[i2].width;
		nCS = nCT = nCE = 0;
		xStart = _xx;
		if(winc->EnableBlend && (control & 0x18)==8){
			_blend=1;
			p1 = Source;
			for(;*p1;p1++){
				if(winc->EnableBg[(*p1)->_idx])
					S[nCS++] = *p1;
				else if((*p1)->_postDrawPixel != NULL)
					E[nCE++] = *p1;
			}
			p1 = Target;
			for(;*p1;p1++){
               if(winc->EnableBg[(*p1)->_idx])
					T[nCT++] = *p1;
               else if((*p1)->_postDrawPixel != NULL)
					E[nCE++] = *p1;
			}
		}
		else{
			if(winc->EnableBlend || _winOam.Enable)
				_control = control;
			else
				_blend = 0;
			p1 = Source;
			for(;*p1;p1++){
				if(winc->EnableBg[(*p1)->_idx])
					S[nCS++] = *p1;
				else if((*p1)->_postDrawPixel != NULL)
					E[nCE++] = *p1;
			}
			p1 = Target;
			for(;*p1;p1++){
				if(winc->EnableBg[(*p1)->_idx])
					S[nCS++] = *p1;
				else if((*p1)->_postDrawPixel != NULL)
					E[nCE++] = *p1;
			}
			for(n=0;n<nCS;n++){
               pLayer = S[n];
               for(i5 = (u8)(n + 1);i5 < nCS;i5++){
                   if(pLayer->_priority > S[i5]->_priority ||
                       (pLayer->_priority == S[i5]->_priority && pLayer->_idx > S[i5]->_idx)){
                       S[n] = S[i5];
                       S[i5] = pLayer;
                       pLayer = S[n];
                   }
               }
			}
		}
		p = (u32 *)pSourceBuffer + _xx;
		switch(_blend){
			case 0:
				_getPixelSprite = &gbagpu::_getPXSprite;
				for(;i > 0;i--,_xx++){
					zValueSrc = (u16)NULLZPIXEL;
					for(p1 = S,i5w=-1,i1=nCS;i1 > 0;i1--){
                       pLayer = *p1++;
                       if(i5w == -1 && (i5w = (this->*pLayer->_drawPixel)(pLayer)) != -1)
                           zValueSrc = MAKEZPIXEL(pLayer->_priority,pLayer->_idx,pLayer->_type);
                       if(pLayer->_postDrawPixel != NULL)
                           (this->*pLayer->_postDrawPixel)(pLayer);
					}
					*p++ = i5w;
					pZBuffer[_xx] = zValueSrc|(SL(nCS,24));
					for(i1=0;i1 <nCE;i1++)
                       (this->*E[i1]->_postDrawPixel)(E[i1]);
				}
			break;
			case 1:
				_getPixelSprite = &gbagpu::_getPXSpriteAlpha;
				pTB = (u32 *)pTargetBuffer + _xx;
				for(;i>0;i--,pTB++,p++,_xx++){
					*pTB = *p = (u32)-1;
					zValueSrc = (u16)NULLZPIXEL;
					prtSrc=idxSrc=(u8)NULLZPIXEL;
					for(p1 = S,i5w=-1,i1=nCS;i1 > 0;i1--){
						pLayer = *p1++;
						if(i5w == -1 && (i5w = (this->*pLayer->_drawPixel)(pLayer)) != -1){
							prtSrc = pLayer->_priority;
							idxSrc = pLayer->_idx;
							zValueSrc = (u16)MAKEZPIXEL(prtSrc,idxSrc,pLayer->_type);
							*p = i5w;
						}
						if(pLayer->_postDrawPixel != NULL)
							(this->*pLayer->_postDrawPixel)(pLayer);
					}
					zValueTar = (u16)NULLZPIXEL;
					for(p1 = T,i5w = -1,i1=nCT;i1 > 0;i1--){
						pLayer = *p1++;
						if(i5w == -1 && (i5w = (this->*pLayer->_drawPixel)(pLayer)) != -1){
                           if(*p != (u32)-1){
                               if(pLayer->_priority > prtSrc && pLayer->_type != 2)
                                   i5w |= 0x80000000;
                               else if(pLayer->_priority < prtSrc || (pLayer->_priority == prtSrc && idxSrc > pLayer->_idx)){
                                   *p |= 0x80000000;
                                   zValueSrc = (u16)NULLZPIXEL;
                               }
                           }
                           *pTB = (u32)((i5w & 0x80000000) | (u16)i5w);
                           zValueTar = MAKEZPIXEL(pLayer->_priority,pLayer->_idx,pLayer->_type);
						}
						if(pLayer->_postDrawPixel != NULL)
                           (this->*pLayer->_postDrawPixel)(pLayer);
					}
					pZBuffer[_xx] = (u32)(zValueSrc | (zValueTar << 12)) | (SL(nCS,24));
					for(i1=0;i1 <nCE;i1++)
						(this->*E[i1]->_postDrawPixel)(E[i1]);
				}
           break;
           case 2:
           case 3:
				_getPixelSprite = &gbagpu::_getPXSpriteBrightness;
				for(;i > 0;i--,_xx++){
					zValueSrc = (u16)NULLZPIXEL;
					for(p1 = S,i5w=-1,i1=nCS;i1 > 0;i1--){
						pLayer = *p1++;
						if(i5w == -1 && (i5w = (this->*pLayer->_drawPixel)(pLayer)) != -1){
							zValueSrc = (u16)MAKEZPIXEL(pLayer->_priority,pLayer->_idx,pLayer->_type);
							if(pLayer->_type != 1)
								i5w |= 0x80000000;
						}
						if(pLayer->_postDrawPixel != NULL)
							(this->*pLayer->_postDrawPixel)(pLayer);
					}
					if(!i5w && !_blendSB)
						i5w = 0x80000000;
					*p++ = (u32)((i5w & 0x80000000) | (u16)i5w);
					pZBuffer[_xx] = (u32)zValueSrc|(SL(nCS,24));
					for(i1=0;i1 <nCE;i1++)
						(this->*E[i1]->_postDrawPixel)(E[i1]);
				}
			break;
		}
		if(winc->EnableObj || _winOam.Enable){
			_oamDraw = 1;
			_drawLineOAM((u16)xStart,_xx);
		}
		else
			_oamDraw=0;
		(this->*_swapBuffer[(_oamDraw * 4) + _blend])(xStart,_xx);
	}
	p1 = Source;
	for(;*p1;p1++){
       if((*p1)->_postDrawLine != NULL)
           (this->*(*p1)->_postDrawLine)(*p1);
	}
	p1 = Target;
	for(;*p1;p1++){
       if((*p1)->_postDrawLine != NULL)
           (this->*(*p1)->_postDrawLine)(*p1);
	}
	_getPixelSprite = oldGetPixelSprite;
	_control=control;
}

void gbagpu::_drawLineMode3(){
	u32 i5w,*dst;
	struct __layer *p;

	dst=pSourceBuffer;
	p=&__layers[2];
	if(p->_enabled)
       i5w = (u16)MAKEZPIXEL(p->_priority,p->_idx,p->_type);
	else
       i5w = (u32)NULLZPIXEL;
	(((gbagpu *)this)->*p->_initDrawLine)(p);
	for(_xx=0;_xx<_width;_xx++){
       *dst++ = (((gbagpu *)this)->*p->_drawPixel)(p);
       (((gbagpu *)this)->*p->_postDrawPixel)(p);
		pZBuffer[_xx] = (u32)i5w;
	}
	(((gbagpu *)this)->*p->_postDrawLine)(p);
	if(_blend != 1 || !p->_enabled)
       return;
    memset(pTargetBuffer,0xff,(_width/2)*sizeof(u32));
}

void gbagpu::_drawLineMode3Window(){
	printf("%s\n",__FUNCTION__);
}

u32 gbagpu::_drawPixelMode3(struct __layer *p){
/*	u16 value,*p;
   s32 x,y;

   *lcd.pCurrentZBuffer++ = rot->iLayer;
   p = (u16 *)rot->pBuffer;
   if(!rot->bRot)
       value = bgrtorgb(p[(rot->bMosaic ? rot->x - (rot->x % lcd.xMosaic) : rot->x) + rot->bg[1]]);
   else{
       x = rot->bg[0] >> 8;
       y = rot->bg[1] >> 8;
       if(x < 0 || y < 0 || x > 239 || y > 159)
           value = 0;
       else
           value = bgrtorgb(p[x + y * 240]);
       rot->bg[0] += rot->rotMatrix[0];
       rot->bg[1] += rot->rotMatrix[2];
   }*/
	int x,y;

	x=p->_rot[2][0] >> 8;
	y=p->_rot[2][1] >> 8;
	if(x < 0 || y < 0 || x > 239 || y > 159)
		return 0;
    return ((u16 *)p->_src)[x + y * 240];
}

void gbagpu::_postDrawLineMode3(struct __layer *p){
	p->_rot[1][0] += p->_rotMatrix[1];
	p->_rot[1][1] += p->_rotMatrix[3];
}

void gbagpu::_postDrawPixelMode3(struct __layer *p){
	p->_rot[2][0] += p->_rotMatrix[0];
	p->_rot[2][1] += p->_rotMatrix[2];
}

void gbagpu::_initLayerMode3(struct __layer *p){
	p->_rot[2][0] = p->_rot[1][0];
	p->_rot[2][1] = p->_rot[1][1];
	p->_src=p->_tile_ram;
if(p->_mosaic) printf("mosaic\n");
   /*if(__layers[2]._enabled){
       if(lcd.layers[2].bMosaic != 0 && (lcd.xMosaic != 0 || lcd.yMosaic != 0))
           rot.bMosaic = 1;
       else{
           if(lcd.layers[2].rotMatrix[0] != 256 || lcd.layers[2].rotMatrix[3] != 256 || lcd.layers[2].rotMatrix[1] != 0 || lcd.layers[2].rotMatrix[2] != 0){
               rot.rotMatrix[0] = lcd.layers[2].rotMatrix[0];
               rot.rotMatrix[1] = lcd.layers[2].rotMatrix[1];
               rot.rotMatrix[2] = lcd.layers[2].rotMatrix[2];
               rot.rotMatrix[3] = lcd.layers[2].rotMatrix[3];
               rot.bg[0] = lcd.layers[2].CurrentX;
               rot.bg[1] = lcd.layers[2].CurrentY;
               rot.bRot = 1;
           }
           else
               rot.bRot = 0;
       }
   }
   if(!rot.bRot)
       rot.bg[1] = (rot.bMosaic ? (yScr - (yScr % lcd.xMosaic)) : yScr) * 240;
   else{
       lcd.layers[2].CurrentX += rot.rotMatrix[1];
       lcd.layers[2].CurrentY += rot.rotMatrix[3];
   }*/
}

void gbagpu::_drawLineMode4(){
	printf("%s\n",__FUNCTION__);
}
void gbagpu::_drawLineMode4Window(){
	printf("%s\n",__FUNCTION__);
}
u32 gbagpu::_drawPixelMode4(struct __layer *){
	printf("%s\n",__FUNCTION__);
	return 0;
}
void gbagpu::_postDrawLineMode4(struct __layer *){
	printf("%s\n",__FUNCTION__);
}
void gbagpu::_postDrawPixelMode4(struct __layer *){
	printf("%s\n",__FUNCTION__);
}

void gbagpu::_drawLineMode5(){
	printf("%s\n",__FUNCTION__);
}
void gbagpu::_drawLineMode5Window(){
	printf("%s\n",__FUNCTION__);
}
u32 gbagpu::_drawPixelMode5(struct __layer *){
	printf("%s\n",__FUNCTION__);
	return 1;
}
void gbagpu::_postDrawLineMode5(struct __layer *){
	printf("%s\n",__FUNCTION__);
}
void gbagpu::_postDrawPixelMode5(struct __layer *){
	printf("%s\n",__FUNCTION__);
}

void gbagpu::_drawLineOAM(u16 xStart,u16 xEnd){
	__oam *p;

	p=__oams;
	for(int i=0;i<128;i++,p++){
		if(p->_changed){
			u16 *pp=(u16 *)((u8 *)_sprite_ram + (i*8));
			u8 op=p->_priority;
			p->_enabled=0;
			if((u8)pp[0] < 161){
				p->_load(pp);
				p->_enabled=1;

				if(p->_rotated){
					u16 *m;

					p->_matrix= &_rotMatrix[p->_idxMatrix];
					m=(u16 *)((u8 *)_sprite_ram + SL(p->_idxMatrix,5));
					p->_matrix->PA=m[3];
					p->_matrix->PB=m[7];
					p->_matrix->PC=m[0xb];
					p->_matrix->PD=m[0xf];
				}
				else if(p->_double)
					p->_enabled=0;
			}
			p->_changed=0;
			if(op != p->_priority){
				u8 i2,*p4=_obj_priority[p->_priority];
				p4[i2=p4[129]++]=i;
				if(p->_index != 0xff){
					int i1;
					u8 *p1,*p2;

					p1 = p2 = _obj_priority[op];
					p1[p->_index] = 0xFF;
					p1[129]--;
					i1=0;
					for(int n=0;n<128;n++,p1++){
						if(*p1 != 0xFF)	__oams[*p2++ = *p1]._index = i1++;
					}
					for(;i1<128;i1++) *p2++ = 0xFF;
				}
				p->_index=i2;
				__oam::_qsort(p4,0,i2);
				for(int n=0;n<i2;n++) __oams[*p4++]._index=n;
			}
		}
	}
	if(__oam::_data[__oam::_visible]==0) return;

	for(int i=3;i>=0;i--){
		u8 *b = _obj_priority[i];
		for(;*b != 0xFF;b++){
           p = &__oams[*b];
           if(p->_enabled == 0)
               continue;
           (this->*p->_drawPixel)(p,xStart,xEnd);
		}
	}
}

void gbagpu::_startDrawFrame(){
	__layers[2]._rot[1][0] = __layers[2]._rot[0][0];
	__layers[2]._rot[1][1] = __layers[2]._rot[0][1];

	__layers[3]._rot[1][0] = __layers[3]._rot[0][0];
	__layers[3]._rot[1][1] = __layers[3]._rot[0][1];
}

int gbagpu::_getPointsWindow(__point_window *ptLine){
	u8 CountPoint;
	u8 n,i2,i5;
	u32 i5w,*p;
	int i,xStart;
	__win *Win[2],*w;
	u8 check;
	int x,i1,CountWindow;

	for(CountWindow = -1,x = 0,i1 = 1;x < 2;x++,i1 <<= 1){
		if(!(_winEnable & i1) || !_win[x].Visible)
           continue;
		w = &_win[x];
		if(w->left == w->right || w->right < w->left)
           continue;
		if(w->Enable && __line >= w->top && __line < w->bottom)
			Win[++CountWindow] = w;
	}
	if(CountWindow > 0 && Win[1]->left <= Win[0]->left){
		w = Win[0];
		Win[0] = Win[1];
		Win[1] = w;
	}
	CountPoint = 0;
	x = i = 0;
	for(i1 = 0;i1 <= CountWindow;i1++){
		check = 0;
		w = Win[i1];
		if(w->left > x){
			if(w->left > w->right && w->right >= i){
				i +=ptLine[CountPoint].width = (u16)((x = w->right) - i);
				ptLine[CountPoint++].winc = w;
				w->right = (u8)(240-1);
			}
			i += ptLine[CountPoint].width = (u16)(w->left - x);
			ptLine[CountPoint++].winc = &_winOut;
		}
		if(w->left < w->right && x < w->right){
			if(i1 != CountWindow && Win[i1+1]->left < w->right){
				x = Win[i1+1]->left - x;
				check = 1;
			}
			else
				x = w->right;
			i +=ptLine[CountPoint].width = (u16)(x - i);
			if(i > 240)
				ptLine[CountPoint].width = (u16)(240 - (i - 240));
			ptLine[CountPoint++].winc = w;
			if(!check)
				continue;
			x = Win[i1+1]->right;
			i +=ptLine[CountPoint].width = (u16)(x - i);
			if(i > 240)
				ptLine[CountPoint].width = (u16)(240 - (i - 240));
			ptLine[CountPoint++].winc = Win[i1+1];
			if(i >= w->right)
               continue;
			x = w->right;
			i +=ptLine[CountPoint].width = (u16)(x - i);
			if(i > _width)
				ptLine[CountPoint].width = (u8)(240 - (i - 240));
			ptLine[CountPoint++].winc = w;
		}
	}
	if(i < _width){
		ptLine[CountPoint].width = (u16)(240 - i);
		ptLine[CountPoint++].winc = &_winOut;
	}
	return CountPoint;
}

void gbagpu::_buildLayersList(){
	__layer *p;
	int n,n1;

	n=n1=0;
	Source[1]=Target[1]=0;
	switch(_blend){
		default:
           for(int i=0;i<4;i++){
               p = __layers;
               for(int i1=0;i1<4;i1++,p++){
                   if(p->_enabled != 0 && p->_priority == i && p->_visible && p->_drawPixel != NULL){
                       p->_type = IOREG(REG_BLENDCNT) & BV(i1) ? 1 : 0;
                       Source[n++] = p;
                   }
               }
           }
       break;
       case 1:
           for(int i=0;i<4;i++){
               p = __layers;
               for(int i1=0;i1<4;i1++,p++){
                   if(p->_enabled != 0 && p->_priority == i && p->_visible && p->_drawPixel != NULL){
                       if(IOREG(REG_BLENDCNT) & BV(i1)){
                           p->_type = 1;
                           Source[n++] = p;
                       }
                       else{
                           Target[n1++] = p;
                           p->_type = (IOREG(REG_BLENDCNT) & BV(i1 + 8)) ? 2 : 0;
                       }
                   }
               }
           }
       break;
	}
	Source[n]=Target[n1]=0;
}

void gbagpu::_rgbAlphaLayerLine(u16 xStart,u16 xEnd){
	u8 r,g,b,*tpeva,*tpevb;
	u32 cols,col1,col2,col;

	pTB = &((u32 *)pTargetBuffer)[xStart];
	pZB = &((u32 *)pZBuffer)[xStart];
	for(xEnd -= xStart;xEnd != 0;xEnd--,pZB++){
		r = 0;
		g = 1;
		col2 = *pTB++;
		cols = *pOB;
		*pOB++ = (u32)-1;
		if((col1 = *pSB++) < 0x80000000){
			b = 0;
			if(col2 >= 0x80000000)
				col2 = (u32)-1;
			else if((cols & 0x04000000))
				cols = (u32)-1;
		}
		else if(col2 < 0x80000000){
			if(cols & 0x04000000)
				cols = (u32)-1;
			if(cols == (u32)-1){
				b=GETZIDXTAR(*pZB);
				if(b != NULLZVALUE){
					//col1=-1;
					if(__layers[b]._type != 2)
						col1=-1;
					else if(col1==-1 && SR(*pZB,24))
						col1 = *(u16 *)_palette;
				}
				else if(col1 == -1){
					col1 = *(u16 *)_palette;
					col2=-1;
				}
			}
			b=0;
		}
		else{
			col2 = col1 = *(u16 *)_palette;
			b = 1;
		}
		if(cols != (u32)-1 && !(cols & 0x01000000)){
			if((cols & 0x80000000)){
				if(_blendSO){
					col2 = col1;
					col1 = (u16)cols;
					r = 1;
					if(Source[0] == 0 && Target[1] == 0)
						g = 0;
				}
				else{
					col2 = (u16)cols;
					r = 2|((cols & 0x30000) << 4);
				}
			}
			else{
				if(_blendSO){
                   col1 = (u16)cols;
                   r = 1;
				}
				else{
					if(_blendTO){
						if(Source[0] == 0) {
							col2 = (u16)cols;
							col1 = (u32)-1;
							r = 2|((cols & 0x30000) << 4);
						}
						else{
							if(!b){
                               col2 = (u16)cols;
                               r = 2|((cols & 0x30000) << 4);
							}
							else if(_blendSB==0/* && Target[6] == 0*/){// Scribblenauts
								col1 = (u16)cols;
								col2 = (u32)-1;
								r = 1;
							}
                       }
                   }
                   else{
                       col1 = (u16)cols;
                       col2 = (u32)-1;
                       r = 1;
                   }
               }
			}
			cols = (u32)-1;
		}

		if(col1 == (u32)-1)
           col = col2;
		else if(col2 == (u32)-1)
			col = col1;
		else{
		/*	if((col1 & 0x40000000)){
               g = (u8)(((col1 & 0x00FF0000) >> 20) + 1);
				if(g > 1 && (r & 0xF) == 2){
					cols = ((u32 *)pZBuffer)[((u64)pTarget - (u64)pTargetBuffer) >> 2];
					if((r >> 4) < GETZPRTSRC(cols))
						g = 0;
				}
				tpeva = _tabColor + (g << 5);
				tpevb = _tabColor + ((16 - g) << 5);
           }
           else if((col2 & 0x40000000)){
				r = (u8)(((col2 & 0x00FF0000) >> 20) + 1);
				if(g){
                   tpeva = _tabColor + (r << 5);
                   tpevb = _tabColor + ((16 - r) << 5);
				}
				else{
                   tpevb = _tabColor + (r << 5);
                   tpeva = _tabColor + ((16 - r) << 5);
				}
			}
			else*/{
				tpeva = _peva;
				tpevb = _pevb;
			}
          	if((r = (u8)(tpeva[(col1 >> 10) & 0x1F] + tpevb[(col2 >> 10) & 0x1F])) > 31)
				r = 31;
          	if((g = (u8)(tpeva[(col1 >> 5) & 0x1F] + tpevb[(col2 >> 5) & 0x1F])) > 31)
				g = 31;
          	if((b = (u8)(tpeva[col1 & 0x1F] + tpevb[col2 & 0x1F])) > 31)
				b = 31;
          	col = (u32)((r << 10)| (g << 5) | b);
       }
		if(cols != (u32)-1){
			tpeva = _peva;
			tpevb = _pevb;
			if(_blendTO){
				col2 = (u16)cols;
				col1 = ((u32 *)pZBuffer)[((u64)pTB - (u64)pTargetBuffer) >> 2];
				if(GETZPRTSRC(col1) > (u8)(cols >> 16)){
					tpeva = _tabColor;
					tpevb = &_tabColor[16 << 5];
				}
				col1 = col;
			}
			else{
				col1 = (u16)cols;
				col2 = col;
			}
			if((r = (u8)(tpeva[(col1 >> 10) & 0x1F] + tpevb[(col2 >> 10) & 0x1F])) > 31)
				r = 31;
           col = r << 10;
           if((r = (u8)(tpeva[(col1 >> 5) & 0x1F] + tpevb[(col2 >> 5) & 0x1F])) > 31)
               r = 31;
           col |= r << 5;
           if((r = (u8)(tpeva[col1 & 0x1F] + tpevb[col2 & 0x1F])) > 31)
               r = 31;
           col |= r;
		}
		/*if(col & 0x40000000){
			col1 = ((col & 0xF00000) >> 20) + 1;
			b = (u8)((col & 0x1F) * col1 >> 4);
			g = (u8)(((col >> 5) & 0x1F) * col1 >> 4);
			r = (u8)(((col >> 10) & 0x1F) * col1 >> 4);
          	col = (u32)((r << 10)| (g << 5) | b);
		}*/
		*pDB++ = (u16)col;
	}
}

void gbagpu::_rgbNormalLine(u16 xStart,u16 xEnd){
	u16 r;
	u32 o,b;

	for(;xStart < xEnd;pOB++,xStart++){
		b = (u32)*pSB++;
		if((o = *pOB) != (u32)-1){
			*pOB = (u32)-1;
           if((o & 0x81000000) != 0){
               if((r = (u16)(_peva[(o >> 10) & 0x1F] + _pevb[(b >> 10) & 0x1F])) > 31)
					r = 31;
               *pDB = (u16)(r << 10);
               if((r = (u16)(_peva[(o >> 5) & 0x1F] + _pevb[(b >> 5) & 0x1F])) > 31)
					r = 31;
               *pDB |= (u16)(r << 5);
               if((r = (u16)(_peva[o & 0x1F] + _pevb[b & 0x1F])) > 31)
					r = 31;
               *pDB++ |= r;
           }
           else
               *pDB++ = (u16)o;
       }
       else if(b != (u32)-1)
           *pDB++ = (u16)(b & 0x7FFF);
       else
			*pDB++ = *(u16 *)_palette;
	}
}

void gbagpu::_rgbFadeLineDown(u16 xStart,u16 xEnd){
	u16 col1;
	u32 col,cols,*p;
	u8 r,g,b;

	for(p = pSB + xEnd - xStart;pSB < p;pSB++){
       cols = col = *pOB;
       *pOB++ = (u32)-1;
       if(col == (u32)-1){
           col = *pSB;
           col1 = (u16)col;
           if(col1 == (u16)-1)
               col1 = *(u16 *)_palette;
       }
       else{
           col1 = (u16)col;
           col = cols;
       }
       if(!(col & 0x80000000)){
           b = (u8)(col1 & 0x1F);
           g = (u8)((col1 >> 5) & 0x1F);
           r = (u8)((col1 >> 10) & 0x1F);
           r -= _pevy[r];
           g -= _pevy[g];
          	b -= _pevy[b];
           col1 = (u16)((r << 10)| (g << 5) | b);
       }
       else if(cols != (u32)-1 && (cols & 0x01000000)){
           col1 = (u16)cols;
           col = *pSB;
           if((r = (u8)(_peva[(col1 >> 10) & 0x1F] + _pevb[(col >> 10) & 0x1F])) > 31) r = 31;
           if((g = (u8)(_peva[(col1 >> 5) & 0x1F] + _pevb[(col >> 5) & 0x1F])) > 31) g = 31;
           if((b = (u8)(_peva[col1 & 0x1F] + _pevb[col & 0x1F])) > 31) b = 31;
           col1 = (u16)((r << 10)| (g << 5) | b);
       }
       *pDB++ = (u16)col1;
   }
}

void gbagpu::_rgbFadeLineUp(u16 xStart,u16 xEnd){
	u16 col1;
	u32 col,cols,*p;
	u8 r,g,b;

	for(p = pSB + xEnd - xStart;pSB < p;pSB++){
       cols = col = *pOB;
       *pOB++ = (u32)-1;
       if(col == (u32)-1){
           col = *pSB;
           col1 = (u16)col;
           if(col1 == (u16)-1)
               col1 = *(u16 *)_palette;
       }
       else{
           col1 = (u16)col;
           col = cols;
       }
       if(!(col & 0x80000000)){
           b = (u8)(col1 & 0x1F);
           g = (u8)((col1 >> 5) & 0x1F);
           r = (u8)((col1 >> 10) & 0x1F);
           if((r += _pevyI[r]) > 31) r = 31;
           if((g += _pevyI[g]) > 31) g = 31;
           if((b += _pevyI[b]) > 31) b = 31;
           col1 = (u16)((r << 10)| (g << 5) | b);
       }
       else if(cols != (u32)-1 && (cols & 0x01000000)){
           col1 = (u16)cols;
           col = *pSB;
           if((r = (u8)(_peva[(col1 >> 10) & 0x1F] + _pevb[(col >> 10) & 0x1F])) > 31) r = 31;
           if((g = (u8)(_peva[(col1 >> 5) & 0x1F] + _pevb[(col >> 5) & 0x1F])) > 31) g = 31;
           if((b = (u8)(_peva[col1 & 0x1F] + _pevb[col & 0x1F])) > 31) b = 31;
           col1 = (u16)((r << 10)| (g << 5) | b);
       }
       *pDB++ = (u16)col1;
   }
}

void gbagpu::_rgbAlphaLayerLineNoOAM(u16 xStart,u16 xEnd){
	u8 r,*tpeva,*tpevb;
	u32 *pTarget,col1,col2,col;

	pTarget = &((u32 *)pTargetBuffer)[xStart];
	pZB = &((u32 *)pZBuffer)[xStart];
	pOB += (xEnd - xStart);
	for(;xStart < xEnd;xStart++,pZB++){
		col2 = *pTarget++;
		if((col1 = *pSB++) < 0x80000000){
			if(col2 >= 0x80000000)
				col2 = (u32)-1;
		}
		else if(col2 < 0x80000000){
			r=GETZIDXTAR(*pZB);
			if(r != NULLZVALUE){
				if(__layers[r]._type != 2)
					col1=-1;
				else if(col1 == -1 && SR(*pZB,24))
					col1 = *(u16 *)_palette;
			}
		}
		else
			col2 = col1 = *(u16 *)_palette;
		if(col1 == (u32)-1)
			col = col2;
		else if(col2 == (u32)-1)
			col = col1;
		else{
			/*if(col1 & 0x40000000){
				r = (u8)((col1 & 0x00FF0000) >> 20);
				tpeva = _tabColor + (r << 5);
				tpevb = _tabColor + ((15 - r) << 5);
			}
			else*/{
				tpeva = _peva;
				tpevb = _pevb;
			}
			if((r = (u8)(tpeva[(col1 >> 10) & 0x1F] + tpevb[(col2 >> 10) & 0x1F])) > 31)
				r = 31;
			col = r << 10;
			if((r = (u8)(tpeva[(col1 >> 5) & 0x1F] + tpevb[(col2 >> 5) & 0x1F])) > 31)
				r = 31;
			col |= (r << 5);
			if((r = (u8)(tpeva[col1 & 0x1F] + tpevb[col2 & 0x1F])) > 31)
				r = 31;
			col |= r;
		}
		/*if(col & 0x40000000){
				col1 = ((col & 0xF00000) >> 20) + 1;
			col2 = ((col & 0x1F) * col1 >> 4);
			r = (u8)(((col >> 5) & 0x1F) * col1 >> 4);
			col2 |= r << 5;
			r = (u8)(((col >> 10) & 0x1F) * col1 >> 4);
				col = col2 | (r << 10);
		}*/
		*pDB++ = (u16)col;
	}
}

void gbagpu::_rgbNormalLineNoOAM(u16 xStart,u16 xEnd){
	u32 b;

	pOB += (xEnd - xStart);
	for(;xStart < xEnd;xStart++){
			b = (u32)*pSB++;
			if(b != (u32)-1)
			*pDB++ = (u16)(b & 0x7FFF);
		else
				*pDB++ = *(u16 *)_palette;
	}
}

void gbagpu::_rgbFadeLineDownNoOAM(u16 xStart,u16 xEnd){
	u16 col1;
	u32 col;
	u8 r,g,b;

	pOB += (xEnd - xStart);
	for(;xStart < xEnd;xStart++){
		col = *pSB++;
		col1 = (u16)col;
		if(col1 == (u16)-1)
				col1 = *(u16 *)_palette;
		if(!(col & 0x80000000)){
			b = (u8)(col1 & 0x1F);
			g = (u8)((col1 >> 5) & 0x1F);
			r = (u8)((col1 >> 10) & 0x1F);
			r -= _pevy[r];
			g -= _pevy[g];
				b -= _pevy[b];
			col1 = (u16)((r << 10)| (g << 5) | b);
		}
		*pDB++ = (u16)col1;
	}
}

void gbagpu::_rgbFadeLineUpNoOAM(u16 xStart,u16 xEnd){
	u16 col1;
	u32 col;
	u8 r,g,b;

	pOB += (xEnd - xStart);
	for(;xStart<xEnd;xStart++){
		col = *pSB++;
		col1 = (u16)col;
		if(col1 == (u16)-1)
			col1 = *(u16 *)_palette;
		if(!(col & 0x80000000)){
			b = (u8)(col1 & 0x1F);
			g = (u8)((col1 >> 5) & 0x1F);
			r = (u8)((col1 >> 10) & 0x1F);
			if((r += _pevyI[r]) > 31) r = 31;
			if((g += _pevyI[g]) > 31) g = 31;
			if((b += _pevyI[b]) > 31) b = 31;
			col1 = (u16)((r << 10)| (g << 5) | b);
		}
		*pDB++ = (u16)col1;
	}
}

void gbagpu::_loadBrightnessIndex(u16 v){
   u8 i;

   _evy = (u8)((i = (u8)(v & 31)) > 16 ? 16 : i);
   _pevy = _tabColor + (_evy << 5);
   _pevyI = _tabColorI + (_evy << 5);
}

void gbagpu::_loadAlphaIndex(u16 v){
	u8 i;

	_eva = ((i = (u8)(v & 31)) > 16 ? 16 : i);
	_evb = ((i = (u8)((v >> 8) & 31)) > 16 ? 16 : i);
	_peva = _tabColor + (_eva << 5);
	_pevb = _tabColor + (_evb << 5);
}

u32 gbagpu::_getPXSprite(__oam *p,u16 xPos,u16 iColor){
	if(p->_priority > GETZPRTSRC(pZBuffer[xPos]))
		return (u32)-1;
	switch(p->_mode){
		case 0:
		case 3:
			_oamDraw = 0;
			return (u32)iColor;
		case 1:
			_oamDraw = 0;
			if(_blendTA)
               return (u32)(0x81000000 | iColor);
			return iColor;
		case 2:
			_oamDraw = 0;
			pWinBuffer[xPos] = 1;
			return pOAMBuffer[xPos];
        default:
			return (u32)-1;
	}
}

u32 gbagpu::_getPXSpriteBrightness(__oam *p,u16 xPos,u16 iColor){
	if(p->_priority > GETZPRTSRC(pZBuffer[xPos]))
       return (u32)-1;
	switch(p->_mode){
		case 0:
		case 3:
			_oamDraw = 0;
			if(!_blendSO)
               return (u32)(0x80000000 | iColor);
           return (u32)iColor;
		case 1:
			_oamDraw = 0;
			if(_blendTA)
               return (u32)(0x81000000 | iColor);
			return (u32)iColor;
       case 2:
			_oamDraw = 0;
			pWinBuffer[xPos] = 1;
			return ((u32 *)pOAMBuffer)[xPos];
       default:
			return (u32)-1;
	}
}

u32 gbagpu::_getPXSpriteAlpha(__oam *p,u16 xPos,u16 iColor){
	u8 prtSrc,idxSrc,prtTar,idxTar;
	u32 value;

	switch(p->_mode){
       case 0:
       //case 3:
			prtSrc = GETZPRTSRC((value = ((u32 *)pZBuffer)[xPos]));
			prtTar = GETZPRTTAR(value);
			idxTar = GETZIDXTAR(value);
        //           idxSrc = GETZIDXSRC(value);
			if(_blendSO){
				if(prtSrc != NULLZVALUE){
					if(p->_priority <= prtSrc){
						_oamDraw = 0;
                       return (u32)(0x80000000 | iColor);
                   }
                   return (u32)-1;
               }
               else if(idxTar != NULLZVALUE){
                   if(__layers[idxTar]._type == 2){
						_oamDraw = 0;
                       if(p->_priority > prtTar)
                           return (u32)(0x80000000 | iColor);
                       return (u32)iColor;
                   }
                   else{
                       if(p->_priority <= prtTar){
							_oamDraw = 0;
                           return (u32)iColor;
                       }
                       return (u32)-1;
                   }
               }
           }
           else if(_blendTO){
               if(prtSrc != NULLZVALUE){
                   if(prtTar != NULLZVALUE){
                       if(p->_priority < prtTar){
							_oamDraw = 0;
                           if(p->_priority <= prtSrc)
                               return (u32)iColor;
                           return (u32)(0x80000000 | iColor);
                       }
                       else if(p->_priority == prtTar){
							_oamDraw = 0;
                           if(prtTar > prtSrc)
                               return (u32)(0x80000000 | iColor);
                           return (u32)iColor;
                       }
                       else{
                           if(__layers[idxTar]._type == 2){
								_oamDraw = 0;
                               return (u32)(0x84000000 | iColor);
                           }
                           return (u32)-1;
                       }
                   }
                   else{
						_oamDraw = 0;
						if(p->_priority <= prtSrc)
							return (u32)iColor;
						return (u32)(0x04000000 | iColor);
                   }
               }
               else if(prtTar != NULLZVALUE){
					_oamDraw = 0;
					if(p->_priority > prtTar)
						return (u32)(0x04000000 | iColor);
					else if(p->_priority == prtTar){
						if(__layers[idxTar]._type == 2)
                           return (u32)(0x80000000 | iColor);
						return (u32)iColor;
					}
					return (u32)iColor;
               }
           }
           else{
               if(prtSrc < p->_priority){
					_oamDraw = 0;
                   return (u32)(0x80000000|iColor);
               }
               if(prtTar < p->_priority)
                   return (u32)-1;
           }
       break;
       case 1:
			if(p->_priority > (prtTar = GETZPRTTAR((value = ((u32 *)pZBuffer)[xPos]))))
				return (u32)-1;
			idxSrc = GETZIDXSRC(value);
			if(idxSrc != NULLZVALUE && __layers[idxSrc]._type == 0){
				if(!_blendTA){
					_oamDraw = 0;
					return (u32)iColor;
				}
				return (u32)-1;
			}
			_oamDraw = 0;
			if(idxSrc == NULLZVALUE){
				if(prtTar == NULLZVALUE)
					return (u32)iColor;
				if(__layers[GETZIDXTAR(value)]._type != 2)
					return (u32)iColor;
			}
			return (u32)(iColor | 0x81000000);
		case 2:
			_oamDraw = 0;
			pWinBuffer[xPos] = 1;
			return ((u32 *)pOAMBuffer)[xPos];
	}
	_oamDraw = 0;
	return (u32)iColor;
}

void gbagpu::_drawPixelSprite(__oam *p,u16 xStart,u16 xEnd){
	s16 xPos,sx,sy;
	u8 *p1,b;
	u16 x8,y8,sxEnd,x;
	u32 *o;

	if(__line < (xPos = p->_y) || __line >= xPos + (sy = p->_height))
		return;
	y8 = (u16)(__line - xPos);
	if((xPos = p->_x) > xEnd || (xPos + (sxEnd = sx = p->_width)) < xStart)
		return;
	//printf("%d %d %d %d %d\n",p->_idx,p->_x,p->_y,p->_width,p->_height);
	o = (u32 *)pOAMBuffer + xPos;
	if(p->_mosaic != 0 && __oam::_data[__oam::_yMosaic])
		y8 = (u16)(y8 - (y8 % __oam::_data[__oam::_yMosaic]));
	y8 = (u16)(p->_flipy ? sy - 1 - y8 : y8);
	p1 = &p->_char_ram[(y8 & 0x7) << 3];
    p1 += ((p->_tileno + ((y8 >> 3) << p->_char_height)) << 5);
	if(((u64)p1 - (u64)p->_char_ram) > 0xA3FFF)
		return;
	if((sy = (s16)(xEnd - xPos)) < sx)
		sxEnd = (u16)sy;
	for(x=0;x < sxEnd;x++,o++,xPos++){
		if(xPos < xStart)
			continue;
		if(p->_mosaic != 0 && __oam::_data[__oam::_xMosaic])
			x8 = (u16)(x - (x % __oam::_data[__oam::_xMosaic]));
		else
			x8 = x;
		x8 = (u16)((y8 = (u16)(p->_flipx ? sx - 1 - x8 : x8)) & 0x7);
		if((y8 = p1[((y8 >> 3) << 6) + x8]) == 0)
			continue;
		y8 = p->_pal[y8];
		*o = (this->*_getPixelSprite)(p,xPos,y8)|(p->_priority << 16);
	}
}

void gbagpu::_drawPixelSpritePalette(__oam *p,u16 xStart,u16 xEnd){
	s16 xPos,sx,sy;
	u8 *p1;
	u16 x8,y8,sxEnd,x;
	u32 *o;

	if(__line < (xPos = p->_y) || __line >= xPos + (sy = p->_height))
		return;
	y8 = (u16)(__line - xPos);
	if((xPos = p->_x) > xEnd || (xPos + (sxEnd = sx = p->_width)) < xStart)
		return;
	o = (u32 *)pOAMBuffer + xPos;
	if(p->_mosaic != 0 && __oam::_data[__oam::_yMosaic])
		y8 = (u16)(y8 - (y8 % __oam::_data[__oam::_yMosaic]));
	y8 = (u16)(p->_flipy ? sy - 1 - y8 : y8);
	p1 = &p->_char_ram[(y8 & 0x7) << 2];
   	p1 += ((p->_tileno + ((y8 >> 3) << p->_char_height)) << 5);
	if(((u64)p1 - (u64)p->_char_ram) > 0xA3FFF)
		return;
	if((sy = (s16)(xEnd - xPos)) < sx)
		sxEnd = (u16)sy;
	for(x=0;x < sxEnd;x++,o++,xPos++){
		if(xPos < xStart)
			continue;
		if(p->_mosaic != 0 && __oam::_data[__oam::_xMosaic])
           x8 = (u16)(x - (x % __oam::_data[__oam::_xMosaic]));
		else
			x8 = x;
		x8 = (u16)((y8 = (u16)(p->_flipx ? sx - 1 - x8 : x8)) & 0x7);
		y8 = p1[((y8 >> 3) << 5) + (x8 >> 1)];
		if((y8 = (u16)((y8 >> ((x8 & 0x1) << 2)) & 0xF)) == 0)
			continue;
		y8 = p->_pal[y8 + (u16)SL((u16)p->_idxPalette,4)];
		*o = (this->*_getPixelSprite)(p,xPos,y8)|(p->_priority << 16);
	}
}

void gbagpu::_drawPixelSpriteRot(__oam *p,u16 xStart,u16 xEnd){
	u16 iColor;
	int xc,yc,y2,y3,x3,y,xc2,yTile,x2PA,x2PC;
	u8 sx,sy,x;
	s16 yPos,xPos;
	u32 PA,PC,*o;
	u8 xTile,xSubTile,ySubTile,yScr,xNbTile,yNbTile;

	xNbTile = sx = p->_width;
	yNbTile = sy = p->_height;
	xc = sx >> 1;
	yc = sy >> 1;
	if(p->_double){
		sx <<= 1;
		sy <<= 1;
	}
	if((yScr = __line) < (yPos = p->_y) || yScr >= yPos + sy)
		return;
	y = yScr - yPos;
	o = (u32 *)pOAMBuffer + (xPos = p->_x);
	xc2 = (0 - (sx >> 1));
	y2 = (y - (sy >> 1)) << 8;
	if(p->_matrix == NULL){
        p->_matrix = _rotMatrix;
    }
	x2PA = xc2 * (PA = p->_matrix->PA << 8) + (y2 * p->_matrix->PB);
	x2PC = xc2 * (PC = p->_matrix->PC << 8) + (y2 * p->_matrix->PD);
	if((x3 = xEnd - xPos) < sx)
		sx = (u8)x3;
	for(x = 0; x < sx;x++,o++,x2PA += PA,x2PC += PC,xPos++){
		if(xPos < xStart)
			continue;
		x3 = (x2PA >> 16) + xc;
		y3 = (x2PC >> 16) + yc;
		if(!(x3 >= 0 && x3 < xNbTile && y3 >= 0 && y3 < yNbTile))
			continue;
		xTile = (u8)(x3 >> 3);
		yTile = (y3 >> 3) << p->_char_height;
		xSubTile = (u8)(x3 & 0x07);
		ySubTile = (u8)(y3 & 0x07);
		if((iColor = p->_tile_ram[((p->_tileno + (xTile << 1) + yTile) << 5) + xSubTile + (ySubTile << 3)]) == 0)
			continue;
		*o =  (this->*_getPixelSprite)(p,xPos,p->_pal[iColor])|(p->_priority << 16);
	}
}

void gbagpu::_drawPixelSpriteRotPalette(__oam *p,u16 xStart,u16 xEnd){
	u16 iColor;
    int xc,yc,y2,y3,x3,y,xc2,yTile,x2PA,x2PC;
    u8 sx,sy,x;
    s16 yPos,xPos;
    u32 PA,PC,*o;
    u8 xTile,xSubTile,ySubTile,yScr,xNbTile,yNbTile;

    xNbTile = sx = p->_width;
    yNbTile = sy = p->_height;
    xc = sx >> 1;
    yc = sy >> 1;
    sx <<= p->_double;
    sy <<= p->_double;
    if((yScr = __line) < (yPos = p->_y) || yScr >= yPos + sy)
        return;
    y = yScr - yPos;
    o = (u32 *)pOAMBuffer + (xPos = p->_x);
    xc2 = (0 - (sx >> 1));
    y2 = (y - (sy >> 1)) << 8;
    if(p->_matrix == NULL)
        p->_matrix = _rotMatrix;
    x2PA = xc2 * (PA = p->_matrix->PA << 8) + (y2 * p->_matrix->PB);
    x2PC = xc2 * (PC = p->_matrix->PC << 8) + (y2 * p->_matrix->PD);
    if((x3 = xEnd - xPos) < sx)
        sx = (u8)x3;
    for(x = 0; x < sx;x++,o++,x2PA += PA,x2PC += PC,xPos++){
        if(xPos < xStart)
            continue;
        x3 = (x2PA >> 16) + xc;
        y3 = (x2PC >> 16) + yc;
        if(!(x3 >= 0 && x3 < xNbTile && y3 >= 0 && y3 < yNbTile))
            continue;
        xTile = (u8)(x3 >> 3);
        yTile = (y3 >> 3) << p->_char_height;
        xSubTile = (u8)(x3 & 0x07);
        ySubTile = (u8)(y3 & 0x07);
        iColor = p->_tile_ram[((p->_tileno + xTile + yTile) << 5) + (xSubTile >> 1) + (ySubTile << 2)];
        if((iColor = (u16)((iColor >> ((xSubTile & 0x1) << 2)) & 0xf)) == 0)
            continue;
        iColor = p->_pal[iColor + SL(p->_idxPalette,4)];
        *o = (this->*_getPixelSprite)(p,xPos,iColor)|(p->_priority << 16);
    }
}

#ifdef _DEVELOP
int gbagpu::Dump(char **pp){
	__layer **l;
	char cc[20],*p;

	p=*pp;
	strcat(p,"\n");
		for(int i=0;i<4;i++){
		sprintf(cc,"%1x %2u,%2u %1x\t",__layers[i]._drawMode,__layers[i]._width,__layers[i]._height,__layers[i]._priority);
		strcat(p,cc);
	}

	strcat(p,"\n\nS ");
	for(l=Source;*l;l++){
		sprintf(cc,"%X",(*l)->_idx);
		strcat(p,cc);
	}
	strcat(p,"\tT ");
	for(l=Target;*l;l++){
		sprintf(cc,"%X",(*l)->_idx);
		strcat(p,cc);
	}
	for(int i=0;i<2;i++){
		sprintf(cc,"\t %dS ",i);
		strcat(p,cc);
		for(l=Source;*l;l++){
			if(!_win[i].EnableBg[(*l)->_idx]) continue;
			sprintf(cc,"%X",(*l)->_idx);
			strcat(p,cc);
		}
		strcat(p,"\tT ");
		for(l=Target;*l;l++){
			if(!_win[i].EnableBg[(*l)->_idx]) continue;
			sprintf(cc,"%X",(*l)->_idx);
			strcat(p,cc);
		}
	}

	strcat(p,"\tSO ");
	for(l=Source;*l;l++){
		if(!_winOut.EnableBg[(*l)->_idx]) continue;
		sprintf(cc,"%X",(*l)->_idx);
		strcat(p,cc);
	}
	strcat(p,"\tT ");
	for(l=Target;*l;l++){
		if(!_winOut.EnableBg[(*l)->_idx]) continue;
		sprintf(cc,"%X",(*l)->_idx);
		strcat(p,cc);
	}

	//strcat(p,"\n\n");
	return 0;
}
#endif

void gbagpu::__oam::_qsort(u8 *tab,int left,int right){
	u8 *p,*p1,*p2,value,value1;
	int i;

	if(left >= right)
		return;
	value = *(p = p2 = &tab[left]);
	*p = *(p1 = &tab[(left + right) >> 1]);
	*p1 = value;
	for(p1 = p,value = *p++,i = left + 1;i<=right;i++,p++){
		if(*p <= value)
			continue;
		value1 = *(++p1);
		*p1 = *p;
		*p = value1;
	}
	value = *p2;
	*p2 = *p1;
	*p1 = value;
	_qsort(tab,left,(i = p1 - tab) - 1);
	_qsort(tab,i + 1,right);
}

int gbagpu::__layer::_load(){
	u16 cnt=IOREG(REG_BGCNT(_idx));

	_priority=cnt;
	_mosaic=SR(cnt,6);
	_palette=SR(cnt,7);
	_wrap=SR(cnt,13);
	__tile_ram=(u8 *)_tile_ram + SL(SR(cnt,8) & 31,11);
	__char_ram=(u8 *)_char_ram + SL(cnt & 0xC,12);

	//printf("%d %u %x %x %x %u %u\n",_idx,_drawMode,cnt,SL(SR(cnt,8) & 31,11),SL(cnt&0xC,12),_scrollx,_scrolly);
	switch(_drawMode){
		case 1:
			switch(SR(cnt,14) & 0x3){
				case 0:
					_width = 31;
					_height = 31;
				break;
				case 1:
					_width = 63;
					_height = 31;
				break;
				case 2:
					_width = 31;
					_height = 63;
				break;
				case 3:
					_width = 63;
					_height = 63;
				break;
			}
		break;
		case 2://rotate
			switch (SR(cnt,14)&3) {
				case 0:
					_width = 16;
					_height = 16;
				break;
				case 1:
					_width = 32;
					_height = 32;
				break;
				case 2:
					_width = 64;
					_height = 64;
				break;
				case 3:
					_width = 128;
					_height = 128;
				break;
			}
		break;
		case 3:
			switch (SR(cnt,14)&3) {
				case 0:
					_width = 16;
					_height = 16;
				break;
				case 1:
					_width = 32;
					_height = 32;
				break;
				case 2:
					_width = 64;
					_height = _palette ? 32 : 64;
				break;
				case 3:
					if(_palette){
						_width = 64;
						_height = 64;
					}
					else{
						_width = 128;
						_height = 128;
					}
				break;
			}
		break;
		case 0:
			if(_idx==2){

			}
		break;
	}

	_log2=::_log2(_width);

	_scrollx = IOREG(REG_BGXOFS(_idx)) & 0x3FF;
	_scrolly = IOREG(REG_BGYOFS(_idx)) & 0x3FF;

	if(_idx > 1){
		if(_changed._matrix){
			_rotMatrix[0] = (s16)IOREG(REG_BGPA(_idx));
			_rotMatrix[1] = (s16)IOREG(REG_BGPB(_idx));
			_rotMatrix[2] = (s16)IOREG(REG_BGPC(_idx));
			_rotMatrix[3] = (s16)IOREG(REG_BGPD(_idx));
		}
		if(_changed._center){
			u32 s;

			if(((s = (IOREG32_(_ioreg,REG_BGRXOFS(_idx)) & 0x0FFFFFFF)) & 0x08000000) != 0)
				s = -(0x10000000 - s);
			_rot[0][0] = _rot[1][0] = s;

			if(((s = (IOREG32_(_ioreg,REG_BGRYOFS(_idx)) & 0x0FFFFFFF)) & 0x08000000) != 0)
				s = -(0x10000000 - s);
			_rot[0][1] = _rot[1][1] = s;
		}
	}
	_changed._value=0;
	return 0;
}

void gbagpu::__layer::Reset(){
	__char_layer::Reset();
	_enabled=0;
	_drawMode=0;
	_mosaic=0;
	_changed._value=0;
}

int gbagpu::__layer::Init(int n,void *a,void *b,gbagpu &g){
	__char_layer::Init(n,g);
	_scrollx=_scrolly=0;
	_ioreg=b;
	_mem=(u8 *)a;
	_visible=1;
	return 0;
}

void gbagpu::__oam::Reset(){
	_index=0xff;
	_enabled=0;
	_priority=0x7;
}

int gbagpu::__oam::Init(int n,void *a,void *b,gbagpu &g){
	g.InitGM(this);
	_idx=n;
	_index=0xff;
	_ioreg=b;
	_mem=(u8 *)a;
	_pal += 0x100;
	_char_ram +=0x10000;
	_tile_ram +=0x10000;
	return 0;
}

int gbagpu::__oam::_load(void *m){
	u16 v = ((u16 *)m)[0];
	_palette=!(SR(v,13) & 1);
	_mode=SR(v,10);
	_double=SR(v,9);
	_shape=SR(v,14);

	if((_y=(u8)v) > 200)
		_y = _y-255;
	if((_rotated=SR(v,8)) == 0){
		_mosaic=SR(v,12);
		_idxMatrix=0;
		if(_palette)
			_drawPixel=&gbagpu::_drawPixelSpritePalette;
		else
			_drawPixel=&gbagpu::_drawPixelSprite;
	}
	else{
		_mosaic=0;
		if(_palette)
			_drawPixel=&gbagpu::_drawPixelSpriteRotPalette;
		else
			_drawPixel=&gbagpu::_drawPixelSpriteRot;
	}
	v = ((u16 *)m)[1];
	if((_x=(v&0x1ff)) > 255)
		_x=_x-512;
	if(_rotated) _idxMatrix=SR(v,9);
	_flipx=SR(v,12);
	_flipy=SR(v,13);
	_size=SR(v,14);

	v = ((u16 *)m)[2];
	_priority=SR(v,10)&3;
	_idxPalette=SR(v,12);
	_tileno=v;

	if(!_palette && !(IOREG(REG_DISPCNT)&0x40))
		_tileno &= ~1;

	u8 sizes_x[0x10] ={8,16,32,64,16,32,32,64, 8, 8,16,32,0,0,0,0};
	u8 sizes_y[0x10] ={8,16,32,64, 8, 8,16,32,16,32,32,64,0,0,0,0};
	u8 i=(SL(_shape,2)+_size);
	_width=sizes_x[i];
	_height=sizes_y[i];
     i = (u16)(IOREG(REG_DISPCNT) & 0x40 ? (!_palette ? _width >> 2 : _width >> 3) : 32);
	_char_height=i ? _log2(i) : 0;
	return 0;
}

};