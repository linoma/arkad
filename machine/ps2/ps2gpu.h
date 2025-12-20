#include <gre.h>
#include "general_device.h"

#ifndef __PS2GPUH__
#define __PS2GPUH__

namespace ps2{

class ps2gpu: public GRE{
public:
	ps2gpu();
	virtual ~ps2gpu();
	virtual int Init(int,void *,void *,u32);
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update(u32 flags=0);
protected:
	virtual int write(u32,u64);
	virtual int read(u32,u64 *);
	virtual int _createTexture(void *,void *,void *dst=0);
	int _bindTexture(u64,u64,u64);
};

};

#endif /* PS2GPU_H */
