#include <gpu.h>
#include "tilecharmanager.h"

#ifndef __GBAGPUH__
#define __GBAGPUH__

namespace gba{

class gbagpu: public GPU,public TileCharManager{
public:
	gbagpu();
	virtual ~gbagpu();
	virtual int Run(u8 *,int cyc,void *obj);
	virtual int Init(void *,void *);
	virtual int Reset();
	virtual int Update(u32 flags=0);
	virtual int InitGM(GM *p);
protected:
	virtual int write(u32,u32);
	virtual int read(u32,u32 *);
	int RenderLine();
#ifdef _DEVELOP
	virtual int Dump(char **);
#endif
	struct __layer;
	struct __oam;
	struct __point_window;

	void _initLayerMode0(struct __layer *);
	u32 _drawPixelMode0(struct __layer *);
	void _initLayerMode1(struct __layer *);
	u32 _drawPixelMode1(struct __layer *);
	void _postDrawLineMode1(struct __layer *);
	void _postDrawPixelMode1(struct __layer *);

	void _drawLineModeTile();
	void _drawLineModeTileWindow();

	void _drawLineMode3();
	void _drawLineMode3Window();
	void _initLayerMode3(struct __layer *);
	u32 _drawPixelMode3(struct __layer *);
	void _postDrawLineMode3(struct __layer *);
	void _postDrawPixelMode3(struct __layer *);

	void _drawLineMode4();
	void _drawLineMode4Window();
	void _initLayerMode4(struct __layer *);
	u32 _drawPixelMode4(struct __layer *);
	void _postDrawLineMode4(struct __layer *);
	void _postDrawPixelMode4(struct __layer *);

	void _drawLineMode5();
	void _drawLineMode5Window();
	void _initLayerMode5(struct __layer *);
	u32 _drawPixelMode5(struct __layer *);
	void _postDrawLineMode5(struct __layer *);
	void _postDrawPixelMode5(struct __layer *);

	void _drawLineOAM(u16,u16);

	int _getPointsWindow(__point_window *);
	void _startDrawFrame();

	void _buildLayersList();
	void _rgbAlphaLayerLine(u16,u16);
	void _rgbNormalLine(u16,u16);
	void _rgbFadeLineDown(u16,u16);
	void _rgbFadeLineUp(u16,u16);
	void _rgbAlphaLayerLineNoOAM(u16,u16);
	void _rgbNormalLineNoOAM(u16,u16);
	void _rgbFadeLineDownNoOAM(u16,u16);
	void _rgbFadeLineUpNoOAM(u16,u16);

	void _loadBrightnessIndex(u16 v);
	void _loadAlphaIndex(u16 v);

	u32 _getPXSprite(__oam *,u16,u16);
	u32 _getPXSpriteBrightness(__oam *,u16,u16);
	u32 _getPXSpriteAlpha(__oam *,u16,u16);

	void _drawPixelSprite(__oam *,u16,u16);
	void _drawPixelSpritePalette(__oam *,u16,u16);
	void _drawPixelSpriteRot(__oam *,u16,u16);
	void _drawPixelSpriteRotPalette(__oam *,u16,u16);

	typedef u32 (gbagpu::*DRAWPIXEL)(struct __layer *);
	typedef void (gbagpu::*INITDRAWLINE)(struct __layer *);
	typedef void (gbagpu::*POSTDRAWPIXEL)(struct __layer *);
	typedef void (gbagpu::*POSTDRAWLINE)(struct __layer *);
	typedef void (gbagpu::*OAMDRAWPIXEL)(struct __oam *,u16,u16);
	typedef u32 (gbagpu::*GETSPRITEPIXEL)(struct __oam *,u16,u16);
	typedef void (gbagpu::*SWAPBUFFER)(u16,u16);

	struct __rotMatrix{
		s16 PA,PB,PC,PD;
	};

	struct __layer : __char_layer{
		u8 *__char_ram,*__tile_ram;
		union{
			struct{
				unsigned int _visible:1;
				unsigned int _drawMode:3;
				unsigned int _mosaic:1;
				unsigned int _wrap:1;
				unsigned int _priority:2;
				unsigned int _palette:1;
				unsigned int _type:2;
				unsigned int _xMosaic:4;
				unsigned int _yMosaic:4;
				unsigned int _log2:3;
			};
		};
		union{
			struct{
				unsigned int _control:1;
				unsigned int _scroll:1;
				unsigned int _matrix:1;
				unsigned int _center:1;
			};
			u32 _value;
		} _changed;

		s16 _rotMatrix[4];
		s32 _rot[3][2];

		virtual void Reset();
		virtual int Init(int,void *,void *,gbagpu &);
		virtual int _load();

		DRAWPIXEL _drawPixel;
		POSTDRAWPIXEL _postDrawPixel;
		INITDRAWLINE _initDrawLine;
		POSTDRAWLINE _postDrawLine;

		protected:
			void *_ioreg;
			u8 *_mem;
	} __layers[4];

	struct __oam : GM{
		static void _qsort(u8 *tab,int left,int right);
		static u8 _data[10];
		enum : u8{
			_visible=0,_xMosaic,_yMosaic
		};

		union{
			struct{
				unsigned int _enabled:1;
				unsigned int _changed:1;
				unsigned int _shape:2;
				unsigned int _size:2;
				unsigned int _rotated:1;
				unsigned int _double:1;
				unsigned int _mode:2;
				unsigned int _palette:1;
				unsigned int _mosaic:1;
				unsigned int _flipx:1;
				unsigned int _flipy:1;
				unsigned int _priority:3;
				unsigned int _idxPalette:4;
				unsigned int _idxMatrix:5;
				unsigned int _tileno:10;
				unsigned int _char_height:3;
			};
		};
		u8 _idx,_index,_width,_height;
		s16 _x,_y;
		void Reset();
		int Init(int,void *,void *,gbagpu &);
		int _load(void *);

		OAMDRAWPIXEL _drawPixel;
		__rotMatrix *_matrix;
		protected:
		void *_ioreg;
		u8 *_mem;
	} __oams[128];

	struct __win_control{
		__layer * EnableBg[4];
		u8 EnableObj;
		u8 EnableBlend;
	} _winOut;

	struct : __win_control{
		u8 Enable;
	} _winOam;

	struct __win : __win_control{
		u8 Enable,Visible;
		u8 left,top;
		u8 right,bottom;
	} _win[2];

	struct __point_window{
		u16 width;
		__win_control *winc;
	};

private:
	struct __layer **Source,**Target;
	u32 *_ioreg,_cycles,*pSourceBuffer,*pTargetBuffer,*pOAMBuffer,*pZBuffer,*pWinBuffer,*pSB,*pTB,*pOB,*pZB;
	u16 *pDB;
	u8 *_mem,*_tabColor,*_tabColorI,*_peva,*_pevb,*_pevy,*_pevyI,*_obj_priority[4],_xx;
	__rotMatrix *_rotMatrix;
	GETSPRITEPIXEL _getPixelSprite;
	SWAPBUFFER _swapBuffer[8];

	union {
		u64 _control;
		struct {
			unsigned int _mode:3;
			unsigned int _blend:3;
			unsigned int _blendSO:1;
			unsigned int _blendSB:1;
			unsigned int _blendTO:1;
			unsigned int _blendTB:1;
			unsigned int _blendSA:1;
			unsigned int _blendTA:1;
			unsigned int _winEnable:2;
			unsigned int _oamEnable:1;
			unsigned int _oamDraw:1;
			unsigned int _eva:5;
			unsigned int _evb:5;
			unsigned int _evy:5;
			unsigned int _xMosaic:4;
			unsigned int _yMosaic:4;
			unsigned int _oamMap:1;
		};
	};
	union{
		u32 _value;
		struct{
			unsigned int _control:1;
			unsigned int _winSize:1;
			unsigned int _winControl:1;
			unsigned int _mosaic:1;
			unsigned int _blend:1;
			unsigned int _layer:1;
		};
	} _changed;
};

};

#endif /* GBAGPU_H */
