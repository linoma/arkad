#include "pcmdac.h"
#include "utils.h"


static FILE *fp=0;
static int lino=0;

PCMDAC::PCMDAC() : Runnable(240000){
	_handle=0;
	__samples=NULL;
	Reset();
}

PCMDAC::~PCMDAC(){
	Destroy();
}

int PCMDAC::Start(){
	return -1;
}

int PCMDAC::Stop(){
	return -1;
}

int PCMDAC::Close(){
	if(_handle){
		snd_pcm_drain(_handle);
		snd_pcm_close(_handle);
		_handle=0;
	}
    return 0;
}

int PCMDAC::Open(int ch,int freq){
	if (snd_pcm_open(&_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0)
		return -1;
	if (snd_pcm_set_params(_handle,SND_PCM_FORMAT_S16_LE,SND_PCM_ACCESS_RW_INTERLEAVED,2,44100,0, 500000) < 0)
		return -2;
	if(!(__samples=new u16[(_sz_samples=ch*44100*3)]))
		return -3;
	Configure();

	_sz_samples*=2;
	_frequency=freq;
	_channels=ch;
	_resample_step=(freq*4096) / 44100;

	//printf("resample %u\n",_resample_step);
	Reset();
	return 0;
}

int PCMDAC::Destroy(){
	printf("%s destroy\n",__FILE__);
	if(fp) fclose(fp);
	fp=0;
	Runnable::Destroy();
	Close();
	if(__samples)
		delete []__samples;
	__samples=NULL;
	_sz_samples=0;
	return 0;
}

int PCMDAC::Reset(){
	if(__samples)
		memset(__samples,0,_sz_samples);
	_pos_write=0;
	_pos_read=_sz_samples/2;
	_n_write=0;
	_elapsed=0;
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
	d=&__samples[_pos_write/2];

	for(sz=SL(sz,11),pos=0;pos < sz;pos+=_resample_step){
		d[0] = s[SR(pos,12)];
		d[1] = s[SR(pos,12)];

	//	if(fp) fwrite(d,1,4,fp);
		d+=2;
		if((_pos_write += 4) >= _sz_samples){
			_pos_write = 0;
			d = __samples;
		}
		_n_write += 4;
	}
	LeaveCriticalSection(&_cr);
	return 0;
}

void PCMDAC::OnRun(){
	snd_pcm_sframes_t frames;

	_elapsed+=240;
	if(fp==0) fp=fopen("audio.raw","wb");

	EnterCriticalSection(&_cr);
	//if(!BT(_status,BV(16)))
		//goto A;
	if(_elapsed >= 960){
		//printf("fff %u %u %u %u %u\n",_elapsed,_n_write,_resample_step,_pos_read,_pos_write);
		//fixmee
		if(_n_write){
			if(_n_write < 44100)
				_resample_step  -= 256;
			else
				_resample_step  += 256;
			//_resample_step=((_frequency*4096)/44100)/((_frequency*4)/_n_write);
		}
		_n_write=0;
		_elapsed=0;
	}

	int frame_bytes = (snd_pcm_format_width(SND_PCM_FORMAT_S16_LE) / 8)*2;

	for(int r=_period_size*2;r>0;){
		int n = _sz_samples - _pos_read;
		if(n > r)
			n=r;
		//if(fp) fwrite(&_samples[_pos_read/2],n,1,fp);
		frames = snd_pcm_writei(_handle, &__samples[_pos_read/2], n);
		//frames=0;
		if (frames == -EAGAIN)
            continue;
		//if (frames < 0)
			//frames = snd_pcm_recover(_handle, frames, 0);
		if (frames < 0) {
			printf("snd_pcm_writei %d %d %u %u %s\n",r,n,_pos_read,_sz_samples,snd_strerror(frames));
			return;
		}
		//printf("frames %u %ld %d %d lino:%d\n",GetTickCount(),frames,n,frame_bytes,lino++);
	//	n=frames*frame_bytes;
		if((_pos_read += n*2) >= _sz_samples)
			_pos_read = 0;
		r-= n;
	}
A:
	LeaveCriticalSection(&_cr);
}

int PCMDAC::Configure(){
	/*snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
	int err;

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(_handle, hwparams);
*/
	snd_pcm_uframes_t l,ll;
	static snd_output_t *output = NULL;

	l=ll=0;
	snd_pcm_get_params(_handle,&l,&ll);
	_period_size=(u32)ll;

//	snd_output_stdio_attach(&output, stdout, 0);
	//snd_pcm_dump(_handle, output);

	return -1;
}