#include "pcmdac.h"
#include "ccore.h"

#ifndef __POPEYESPUH__
#define __POPEYESPUH__

namespace POPEYE{

class Popeye;

#include "ay8910.h.inc"

class PopeyeSpu : public PCMDAC{
public:
	PopeyeSpu();
	virtual ~PopeyeSpu();
	virtual int Init(Popeye &);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
protected:
	struct  __ay8910 _pwms;

	u16 *_samples;
	u32 _cycles;

	friend class Popeye;
private:
	const int _freq=55930;
};

};

#endif