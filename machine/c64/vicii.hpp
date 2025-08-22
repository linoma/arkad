#ifndef __VICII_HPP__
#define __VICII_HPP__

#include <gpu.h>

namespace c64{

class VICII: public GPU{
public:
	VICII();
	virtual ~VICII();
	virtual int Run(u8 *,int cyc,void *obj);
	virtual int Init(void *,void *);
	virtual int Reset();
	virtual int Update(u32 flags=0);
protected:
	virtual int LoadSettings(void * &);
	virtual int write(u32,u16);
	virtual int read(u32,u16 *);
	int RenderLine(u32 flags=0);
private:
	u16 *_ioreg,_cycles;
	u8 *_mem;
};
};

#endif /* VICII_HPP */
