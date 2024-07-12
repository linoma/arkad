#include "gpu.h"
#include "tilecharmanager.h"

#ifndef __POPEYEGPUH__
#define __POPEYEGPUH__

namespace POPEYE{

#undef PPU_REG
#undef PPU_REG_

#define PPU_REG_(a,b)	((u8 *)a)[b]
#define PPU_REG(a)		PPU_REG_(_gpu_regs,a)

class PopeyeGpu : public GPU,public TileCharManager{
public:
	PopeyeGpu();
	virtual ~PopeyeGpu();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update();
	virtual int InitGM(GM *);
	virtual int Load(void *);
protected:
	int _setPaletteBank(u8);
	u8 *_prom,_palette_bank;

	struct __layer : public __tile{
		public:
			__layer() :__tile() {_scrollx=0;_scrolly=0;};
			virtual void Init(int,PopeyeGpu &);
			virtual void Draw(u16 *);

			int _scrollx,_scrolly;
	} _bglayer;

	struct __fg_layer : public __layer{
		public:
			virtual void Init(int,PopeyeGpu &);
			virtual void Draw(u16 *);

			friend class PopeyeGpu;
	} _fglayer;

	struct  __oam : public __tile{
		virtual void Init(int,PopeyeGpu &);
		virtual void Draw(u16 *);
		virtual int _extract(u32 offs);
	} _oam;
private:
	u32 _cycles;
};

};
#endif