#include "arkad.h"
#include <alsa/asoundlib.h>

#ifndef __PCMDACH__
#define __PCMDACH__

class PCMDAC: public Runnable{
public:
	PCMDAC();
	virtual ~PCMDAC();
	virtual int Close();
	virtual int Open(int,int);
	virtual int Reset();
	virtual int Destroy();
	virtual int Write(void *,u32);
	virtual void OnRun();
	virtual int Start();
	virtual int Stop();
protected:
	virtual int _configure();
	u32 _n_write,_elapsed,_resample_step,_frequency,_period_size,_n_read;
private:
	void _reset();
	u8 *__samples;
	u32 _sz_samples,_pos_write,_pos_read,_channels;
	snd_pcm_t *_handle;
};

#endif