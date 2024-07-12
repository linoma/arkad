#include "gpu.h"
#include "tilecharmanager.h"

#ifndef __1943GPUH__
#define __1943GPUH__

namespace M1943{

#undef PPU_REG
#undef PPU_REG_


#define PPU_REG_(a,b)	((u8 *)a)[b]
#define PPU_REG(a)	PPU_REG_(_gpu_regs,a)

class M1943Gpu : public GPU,public TileCharManager{
public:
	M1943Gpu();
	virtual ~M1943Gpu();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update();
	virtual int Load(void *);
	virtual int InitGM(GM *);
protected:
	struct __1943tile : public __tile{
		u32 _translateColor(u32 c);
	};

	struct __layer : public __1943tile{
		public:
			virtual void Init(int,M1943Gpu &);
			virtual void Draw(u16 *);
			virtual void Reset();
			virtual int Save();
			virtual int _extract(u32);
			virtual int _drawToMemory(u16 *,u32 *params,GPU &g);

			int _scrollx,_scrolly;
	} __bglayers[2];

	struct __fglayer : public __1943tile{
		public:
			virtual void Init(int,M1943Gpu &);
			virtual void Draw(u16 *);
	} __fglayers;

	struct  __oam : public __1943tile{
		virtual void Init(int,M1943Gpu &);
		virtual void Draw(u16 *);
		virtual void Reset();
		u32 _translateColor(u32 c);
		u8 *_priority;
	} __oams;
private:
	u32 _cycles,_cr;
	u8 *_proms;
};

};
#endif