#include "arkad.h"
#include "pcmdac.h"

#ifndef __AICAH__
#define __AICAH__

namespace dc{

class AICA: public PCMDAC{
public:
	AICA();
	virtual ~AICA();
	virtual int Init(int,void *,void *,u32);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
protected:
	virtual int write(u32,u32);
	virtual int read(u32,u32 *);
private:
	u8 *_ioreg;
};

};

#endif
