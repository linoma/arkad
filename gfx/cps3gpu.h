#include "gpu.h"

#ifndef __CPS3GPUH__
#define __CPS3GPUH__

class CPS3GPU : public GPU{
public:
	CPS3GPU();
	virtual ~CPS3GPU();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update(u32 flags=0);
protected:
	void _set_bank(int);

	void *_pal_ram,*_sprite_ram,*_tile_ram,*_char_ram,*_ss_ram,*_ss_regs;
	u32 _cram_bank;

	struct __tile{
		u16 *_dst,*_pal,_screen_width,_screen_height;
		u32 *_regs,_tileno,_palno,_height,_width;
		u8 *_tile_ram,*_char_ram,*_src;
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
			};
			u32 _value;
		};

		__tile(){
			_value=0;
			_char_width=16;
			_char_height=16;
			_visible=1;
		};

		virtual void Draw(u16 *);
		virtual void Init(int,CPS3GPU &);
		virtual void Reset(){
			_draw=_init==0;
		}
		virtual void _drawRow(u16 *pp);

		friend class CPS3GPU;
	};

	struct __ss_layer : __tile{
		virtual void Init(int,CPS3GPU &);
		virtual void Draw();
		virtual void _drawRow(u16 *pp);
	} _ss_layer;

	struct __layer : public __tile{
		public:
			virtual void Init(int,CPS3GPU &);
			void Draw(u32 d,u32 e,u32 f);
			virtual void Reset();
			virtual void Init(u32,u32,u32);

		protected:
			u32 _a,_b,_c;
			int _gscrollx,_gscrolly,_ypos2;
	} _layers[4];

	struct  __oam : public __layer{
		int xpos,ypos,gscroll,_xscale,_yscale;
		int whichbpp,whichpal,global_xflip,global_yflip,global_alpha,global_bpp,global_pal;

		virtual void Reset();
		virtual void Draw(u32 d,u32 e,u32 f);
		virtual void Init(u32,u32,u32);
		virtual void Draw(u16 *);
		virtual void _drawRow(u16 *pp);
	} _oams[2048];

	const int _scan_cycles=1589;
private:
	u32 _cycles,_cr;
};

#endif