#include "gpu.h"

#ifndef __CPS2GPUH__

#define  __CPS2GPUH__
namespace cps2{


class CPS2GPU : public GPU{
public:
	CPS2GPU();
	virtual ~CPS2GPU();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update(u32 flags=0);
private:
	u32 _cycles,_cr;
};

};


#endif