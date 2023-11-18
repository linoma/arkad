#include "pcmdac.h"
#include "utils.h"
#include <map>
#include <string>

using namespace std;

#ifdef _DEVELOP
static FILE *fp=0;
static FILE *flog=0;
#endif

static u32 lino=0,_tmaxw=0,_tmaxr=0;

PCMDAC::PCMDAC() : Runnable(120000){
#ifdef __WIN32__
	pDevice = NULL;
	pAudioClient = NULL;
    pRenderClient = NULL;
#else
	_handle=0;
#endif
	__samples=NULL;
	Reset();
}

PCMDAC::~PCMDAC(){
	Destroy();
}

void PCMDAC::_reset(u32 flgs){
	//_pos_write=0;
	//_pos_read=SR(_sz_samples,1);
	_n_write=0;
	_n_read=0;
	_elapsed=0;
	if(__samples)
		memset(__samples,0,_sz_samples);
	_last_sync=GetTickCount();
}

int PCMDAC::Start(){
	if(hasStatus(PLAY))
		return 1;
	EnterCriticalSection(&_cr);
	addStatus(PLAY);
#ifdef __WIN32__
	if(pAudioClient)
		pAudioClient->Start();
#else
	if(_handle){
	//	snd_pcm_start(_handle);
		snd_pcm_pause(_handle,0);
		//snd_pcm_reset(_handle);
	}
#endif
	LeaveCriticalSection(&_cr);
	return 0;
}

int PCMDAC::Stop(){
	if(!hasStatus(PLAY))
		return -1;
	EnterCriticalSection(&_cr);
	clearStatus(PLAY);
#ifdef __WIN32__
	if(pAudioClient)
		pAudioClient->Stop();
#else
	if(_handle){
		snd_pcm_pause(_handle,1);
		//snd_pcm_drop(_handle);
		//snd_pcm_reset(_handle);
	}
#endif
	_reset();
	LeaveCriticalSection(&_cr);
	return 0;
}

int PCMDAC::Close(){
#ifdef __WIN32__
	if(pDevice){
		pDevice->Release();
		pDevice=NULL;
	}
	if(pAudioClient){
		pAudioClient->Release();
		pAudioClient=NULL;
	}
	if(pRenderClient){
		pRenderClient->Release();
		pRenderClient=NULL;
	}
#else
	if(_handle){
		snd_pcm_drain(_handle);
		snd_pcm_close(_handle);
		_handle=0;
	}
#endif
    return 0;
}

int PCMDAC::Open(int ch,int freq){
#ifdef __WIN32__
	HRESULT hr;
	int res;
	IMMDeviceEnumerator *pEnumerator = NULL;
	WAVEFORMATEX *waveformat = NULL,wf;

	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,(void**)&pEnumerator);
    if(hr) return -1;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    res--;
    if(hr) goto  A;
	hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL,NULL, (void**)&pAudioClient);
	res--;
	if(hr) goto A;
  //  hr = pAudioClient->GetMixFormat(&waveformat);
    res--;
	//if(hr) goto A;

	memset(&wf,0,sizeof(WAVEFORMATEX));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = channels;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = 4;
    wf.wBitsPerSample = 16;
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nChannels * (16 / 8),
	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,0x88100000,0,0,&wf,NULL);
    res--;
	if(hr) goto A;

	hr = pAudioClient->GetService(IID_IAudioRenderClient, (void **)&pRenderClient);
	res--;
	if(hr) goto A;
	res=0;
A:
	if(pEnumerator)
		pEnumerator->Release();
	if(res)
		return res;
B:
#else
	if (snd_pcm_open(&_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0)
		return -1;
	if (snd_pcm_set_params(_handle,SND_PCM_FORMAT_S16_LE,SND_PCM_ACCESS_RW_INTERLEAVED,channels,44100,0, 500000) < 0)
		return -2;
#endif
	if(!(__samples=new u8[(_sz_samples=channels*44100*2*sizeof(u16))]))
		return -3;
	_frequency=freq;
	_channels=ch;
	_configure();
#ifdef _DEVELOPa
	flog=fopen("log.log","w+");
#endif
	Reset();
	return 0;
}

int PCMDAC::Destroy(){
#ifdef _DEVELOP
	if(fp){
		WAVHDR w={{'R','I','F','F'},lino+36,{'W','A','V','E','f','m','t',' '},16,1,2,44100,176400,4,16,{'d','a','t','a'},lino};
		fseek(fp,0,SEEK_SET);
		fwrite(&w,sizeof(WAVHDR),1,fp);
		fclose(fp);
	}
	fp=0;
	if(flog)
		fclose(flog);
	flog=0;
#endif
	Runnable::Destroy();
	Close();
	if(__samples)
		delete []__samples;
	__samples=NULL;
	_sz_samples=0;
	return 0;
}

#define PCM_DELTA 4410
#define PCM_MIN (PCM_DELTA*5)
#define PCM_MAX (PCM_MIN-(PCM_DELTA*2))

int PCMDAC::Reset(){
	Stop();
	if(__samples)
		memset(__samples,0,_sz_samples);
	_pos=0;
	_reset();
	_pos_write=PCM_DELTA*2;//
	_pos_read=0;
	_tmaxw=_tmaxr=0;
	_resample_step = (_frequency * 4096) / 44100;
#ifdef _DEVELOPa
	if(fp==0){
		WAVHDR w;

		if((fp=fopen("audio.wav","wb")))
			fwrite(&w,sizeof(WAVHDR),1,fp);
		lino=0;
	}
#endif
	return 0;
}

int PCMDAC::Write(void *p,u32 sz){
	u16 *s,*d;
	u32 pos;

	if(!p || !sz || !__samples)
		return -1;
	EnterCriticalSection(&_cr);
	//if(fp) fwrite(p,sz,1,fp);
	s=(u16 *)p;
	d=(u16 *)&__samples[_pos_write];
	for(sz=SL(sz,12);_pos < sz;_pos+=_resample_step){
		if(_channels==1){
			d[0] = s[SR(_pos,12)];
			d[1] = d[0];
		}
		else{
			d[0]= s[SR(_pos,11)];
			d[1]= s[SR(_pos,11)+2];
		}
#ifdef _DEVELOP
		if(fp){
			fwrite(d,1,4,fp);
			lino+=4;
		}
#endif
		if((_pos_write += 4) >= _sz_samples-2){
			_pos_write = 0;
			d = (u16 *)__samples;
		}
		else
			d+=2;
		_n_write += 4;
	}
	_pos &= 0x1fff;
	LeaveCriticalSection(&_cr);
	Start();
	return 0;
}

void PCMDAC::OnRun(){
	s64 frames;

	EnterCriticalSection(&_cr);
	if(!hasStatus(PLAY))
		goto _exit;
	_elapsed = GetTickCount();
	lino++;
	if((_elapsed - _last_sync) > 980){
	//	printf("lino %d %u\n",lino,_elapsed-_last_sync);
		_last_sync=_elapsed;
		lino=0;
#ifdef _DEVELOP
		if(flog)
			fprintf(flog,"\trp %u w:%u r:%u %u rp:%u wp:%u \n",_elapsed,_n_write,_n_read,_resample_step,_pos_read,_pos_write);
#endif
		//fixme again!!! enjoy
		if(_n_write){
			int i;

			_tmaxr += _n_read,
			_tmaxw += _n_write;
			i=_tmaxw-_tmaxr;

			//printf("d %d TR:%u TW:%u %d\n",i,_tmaxr,_tmaxw,_resample_step);
			if(!_pcm_sync || !(i < (_period_size/2) || i > _period_size))
				goto A;
			if(i < (_period_size/2))
				_resample_step -= 256;
			else if(i > 22050)
				_resample_step += 256;
			else{
				i=_n_write-_n_read;
			//printf(" %d %u R:%u W:%u ",i,_resample_step,_n_read,_n_write);
				if(i < 0)
					_resample_step -= 256;
				else
					_resample_step += 256;
			}
A:
			i=0;
//printf("\n");
			//_resample_step=((_frequency*4096)/44100)/((_frequency*4)/_n_write);
		}
		_n_write = 0;
		_n_read = 0;
		_elapsed = 0;
	}
C:
	///int frame_bytes = (snd_pcm_format_width(SND_PCM_FORMAT_S16_LE) / 8)*2;
#ifdef _DEVELOP
	if(flog){
		fprintf(flog,"rr w:%u r:%u %u",_pos_write,_pos_read,_sz_samples);
	}
#endif
	for(int r=_period_size*2;r>0;){
		int n = (_sz_samples - _pos_read);
		if(n > _period_size)
			n=_period_size;
#ifdef __WIN32__
		u8 *buffer;

		if(pRenderClient->GetBuffer(n, &buffer)){
			//printf("pcm buffer err %u\n",n);
			goto _exit;
		}
		memcpy(buffer,&__samples[_pos_read], n*2*2);

		frames=n;
		pRenderClient->ReleaseBuffer(n,0);
#else
		frames = snd_pcm_writei(_handle, &__samples[_pos_read], n);
		if (frames == -EAGAIN)
            continue;
		if (frames < 0){
			//snd_pcm_pause(_handle,0);
			//printf("snd_pcm_writei %d %d %u %u mw:%u %s\n",r,n,_pos_read,_sz_samples,_tmaxw,snd_strerror(frames));
			frames = snd_pcm_recover(_handle, frames, 0);
		}
		if (frames <= 0) {
			//printf("snd_pcm_writei %d %d %u %u %s\n",r,n,_pos_read,_sz_samples,snd_strerror(frames));
			goto _exit;
		}
#endif
		n=frames*sizeof(u16)*2;
#ifdef _DEVELOPa
		if(flog)
			fprintf(flog,"\t%u %u %ld",_pos_read,n,frames);
		if(fp)
			fwrite(&__samples[_pos_read],n,1,fp);
		//lino += n;
#endif
		if((_pos_read += n) >= (_sz_samples-4))
			_pos_read = 0;
		_n_read+=n;
		r -= n;
		///r=0;
	}
#ifdef _DEVELOP
		if(flog){
		//	fprintf(flog,"\n");
			fflush(flog);
		}
#endif
_exit:
	LeaveCriticalSection(&_cr);
}

int PCMDAC::_configure(){
#ifdef __WIN32__
	HRESULT hr;
	REFERENCE_TIME default_period = 0;

    hr = pAudioClient->GetDevicePeriod(&default_period, NULL);
    const float period_millis = default_period / 10000.0f;
    const float period_frames = period_millis * 44100 / 1000.0f;

    printf("%.5f %.5f %u\n",period_millis,period_frames,_sz_samples);
	_period_size=ceil(period_frames);
#else
	/*snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
	int err;

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(_handle, hwparams);
*/
	snd_pcm_uframes_t l,ll;
	///static snd_output_t *output = NULL;

	l=ll=0;
	snd_pcm_get_params(_handle,&l,&ll);
	///ll=snd_pcm_avail(_handle);
	_period_size=(u32)ll;
#endif
	u32 t = (1000000.0f/44100.0f) *_period_size;

	printf("tt  %d %u %u\n",t,_period_size,_sz_samples);
	_set_time(t);
	_max_elapsed=1000000/SL(t,13);
//	printf("pz %lu %lu\n",l,ll);
	//snd_output_stdio_attach(&output, stdout, 0);
	//snd_pcm_dump(_handle, output);
	return 0;
}

int PCMDAC::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	_pcm_sync=stoi(m["pcm_sync"]);
	return 0;
}