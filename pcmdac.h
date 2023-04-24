#include "arkad.h"
#include <alsa/asoundlib.h>

#ifndef __PCMDACH__
#define __PCMDACH__

class PCMDAC{
public:
	PCMDAC();
	virtual ~PCMDAC();
	virtual int Close();
	virtual int Open(int,int);
protected:
	snd_pcm_t *_handle;
	void *_data,*_pcm_regs;
};

#endif