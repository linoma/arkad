#include "gpu.h"

#ifndef __BLKTIGERGPUH__
#define __BLKTIGERGPUH__

namespace blktiger{

#undef PPU_REG
#undef PPU_REG_


#define PPU_REG_(a,b)	((u8 *)a)[b]
#define PPU_REG(a)	PPU_REG_(_gpu_regs,a)

class BlkTigerGpu : public GPU{
public:
	BlkTigerGpu();
	virtual ~BlkTigerGpu();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int);
	virtual int Update();
protected:
	void *_pal_ram,*_sprite_ram,*_tile_ram,*_char_ram;
	u16 *_palette,_bank;

	struct __tile{
		u16 *_dst,*_pal,_screen_width,_screen_height,_xx;
		u32 *_regs,_tileno,_palno,_height,_width;
		u8 *_tile_ram,*_char_ram,*_src,_yy;
		void *_screen_end;

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
		virtual void Init(int,BlkTigerGpu &);
		virtual void Reset();
		virtual void _drawRow(u16 *pp);
		virtual int Save(){return -1;};

		friend class BlkTigerGpu;
	};

	struct __layer : public __tile{
		public:
			virtual void Init(int,BlkTigerGpu &);
			virtual void Draw(u16 *);
			virtual void Reset();
			virtual int Save();
			int _scrollx,_scrolly;
	} __layers[2];

	struct __txlayer : public __layer{
		public:
			virtual void Init(int,BlkTigerGpu &);
			virtual void Draw(u16 *);
			virtual void _drawRow(u16 *pp);
		protected:
	} __txlayers;

	struct  __oam : public __layer{
		virtual void Init(int,BlkTigerGpu &);
		void Draw(u16 *);
	} __oams[1];

	struct __tile *_layers[4]={&__layers[0],&__layers[1],&__oams[0],&__txlayers};

	u8 *_simm;
private:
	u32 _cycles,_cr;
};

};
#endif