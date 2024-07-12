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
	u32 _n_write,_resample_step,_frequency,_period_size,_n_read,_max_elapsed,_last_sync;
	void *_spu_mem,*_spu_regs;
private:
	void _reset(u32 flags=0);
	u8 *__samples;
	u32 _sz_samples,_pos_write,_pos_read,_channels,_pos,_pcm_sync,_pcm_buffer_size,_tmaxr,_tmaxw,_tsamples;
	struct __reverber{
		u8 *__samples;
		int _count,_vol,_dvol,_dtime,_time;
		union{
			u32 _status;
			struct{
				unsigned  int _enable:1;
			};
		};
		__reverber();
		~__reverber();

		int _init();
		int _reset();
	} _reverber;
	const int _pcm_channels=2;
	const int _pcm_frequency=44100;
	const int _pcm_bytes_seconds=_pcm_channels*_pcm_frequency*sizeof(u16);
#ifdef __WIN32__
	IMMDevice *pDevice;
    IAudioClient *pAudioClient;
    IAudioRenderClient *pRenderClient;
#else
	snd_pcm_t *_handle;
#endif
};

#endif