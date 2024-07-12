#include "gpu.h"
#include <vector>

using namespace std;

#ifndef __TILECHARMANAGERH__
#define __TILECHARMANAGERH__


class TileCharManager{
public:
	TileCharManager();
	virtual ~TileCharManager();
protected:
	void *_pal_ram,*_sprite_ram,*_tile_ram,*_char_ram;

	struct __tile : GM{
		s16 _xx,_yy;
		u32 _tileno,_palno,_planes_ofs[64],_trans_pen;
		int _y_inc[64],_x_inc[64];
		u8 *_src;
		void *_screen_end,*_screen_start;

		union{
			struct{
				unsigned int _enabled:1;
				unsigned int _draw:1;
				unsigned int _init:1;
				unsigned int _idx:3;
				unsigned int _visible:1;
				unsigned int _flipx:1;
				unsigned int _flipy:1;
				unsigned int _char_width:6;
				unsigned int _char_height:6;
				unsigned int _alpha:2;
				unsigned int _layout:1;
				unsigned int _planes:4;
				unsigned int _swap_col:1;
				unsigned int _multiple:1;
			};
			u32 _value;
		};

		__tile();

		virtual void Draw(u16 *);
		virtual void Init(int,GPU &);
		virtual void Reset();
		virtual void _drawRow(u16 *pp);
		virtual int Save(){return -1;};
		virtual u32 _translateColor(u32);
		virtual int _extract(u32);
		virtual int _drawToMemory(u16 *,u32 *params,GPU &g);
		virtual int _setPlanesOffset(int,...);
		virtual int _setSize(int,int);
		virtual int  _setHorzOffset(int,...);
		virtual int  _setVertOffset(int,...);
		friend class GPU;
	};

	struct __char_layer  :  GM{
		int _width,_height,_scrollx,_scrolly;
		__tile _tile;

		__char_layer(){};
		virtual void Draw(u16 *);
		virtual void Init(int,GPU &);
		virtual void Reset();
	};
	vector<struct __tile *>_layers;
};


#endif