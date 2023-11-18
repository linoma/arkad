#include "arkad.h"

#ifdef __WIN32__
#include <initguid.h>

#define COBJMACROS
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <mmreg.h>
#include <audioclient.h>
#include <audiosessiontypes.h>

#else
#include <alsa/asoundlib.h>
#endif

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
	virtual int LoadSettings(void * &);
protected:
	virtual int _configure();
	u32 _n_write,_elapsed,_resample_step,_frequency,_period_size,_n_read,_max_elapsed,_last_sync;
private:
	void _reset(u32 flags=0);
	u8 *__samples;
	u32 _sz_samples,_pos_write,_pos_read,_channels,_pos,_pcm_sync;

	const int channels=2;
#ifdef __WIN32__
	IMMDevice *pDevice;
    IAudioClient *pAudioClient;
    IAudioRenderClient *pRenderClient;
#else
	snd_pcm_t *_handle;
#endif
};

#endif