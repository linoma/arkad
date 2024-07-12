#include "gpu.h"
#include "tilecharmanager.h"

#ifndef __BLKTIGERGPUH__
#define __BLKTIGERGPUH__

namespace blktiger{

#undef PPU_REG
#undef PPU_REG_


#define PPU_REG_(a,b)	((u8 *)a)[b]
#define PPU_REG(a)	PPU_REG_(_gpu_regs,a)

class BlkTigerGpu : public GPU,public TileCharManager{
public:
	BlkTigerGpu();
	virtual ~BlkTigerGpu();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update(u32 flags=0);
	virtual int InitGM(GM *p);
protected:
	u16 *_palette;

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
		protected:
	} __txlayers;

	struct  __oam : public __layer{
		virtual void Init(int,BlkTigerGpu &);
		void Draw(u16 *);
	} __oams;

	u8 *_simm;
private:
	u32 _cycles,_cr;
};

};
#endif