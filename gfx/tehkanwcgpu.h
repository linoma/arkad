#include "gpu.h"

#ifndef __TEHKANWCGPUH__
#define __TEHKANWCGPUH__

namespace tehkanwc{

class tehkanwcGpu : public GPU{
public:
	tehkanwcGpu();
	virtual ~tehkanwcGpu();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update(u32 flags=0);
protected:
	void *_pal_ram,*_sprite_ram,*_tile_ram,*_char_ram;
	u16 *_palette;

	struct __tile{
		u16 *_dst,*_pal,_screen_width,_screen_height,_xx;
		u32 *_regs,_tileno,_palno,_height,_width;
		u8 *_tile_ram,*_char_ram,*_src,_yy;
		u64 _screen_end,_screen_start;

		union{
			struct{
				unsigned int _draw:1;
				unsigned int _init:1;
				unsigned int _idx:2;
				unsigned int _visible:1;
				unsigned int _flipx:1;
				unsigned int _flipy:1;
				unsigned int _char_width:5;
				unsigned int _char_height:5;
				unsigned int _alpha:2;
				unsigned int _layout:1;
			};
			u32 _value;
		};

		__tile();

		virtual void Draw(u16 *);
		virtual void Init(int,tehkanwcGpu &);
		virtual void Reset();
		virtual void _drawRow(u16 *pp);
		virtual int Save(){return -1;};

		friend class tehkanwcGpu;
	};

	struct __layer : public __tile{
		public:
			virtual void Init(int,tehkanwcGpu &);
			virtual void Draw(u16 *);
			virtual void Reset();
			virtual int Save();
			int _scrollx,_scrolly;
	} _fg_layer;

	struct __bg_layer : public __layer{
		public:
			virtual void Init(int,tehkanwcGpu &);
			virtual void Draw(u16 *);
	} _bg_layer;

	struct  __oam : public __layer{
		virtual void Init(int,tehkanwcGpu &);
		virtual void Draw(u16 *);
	} _oams;

	struct __tile *_layers[3]={&_bg_layer,&_oams,&_fg_layer};

private:
	u32 _cycles,_cr;
};

};

#endif