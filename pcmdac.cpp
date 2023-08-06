#include "pcmdac.h"
#include "utils.h"

#ifdef _DEVELOP
static FILE *fp=0;
static u32 lino=0;
static FILE *flog=0;
#endif

PCMDAC::PCMDAC() : Runnable(120000){
	_handle=0;
	__samples=NULL;
	Reset();
}

PCMDAC::~PCMDAC(){
	Destroy();
}

void PCMDAC::_reset(){
	_pos_write=0;
	_pos_read=2*44100*2;
	_n_write=0;
	_n_read=0;
	_elapsed=0;
}

int PCMDAC::Start(){
	if(hasStatus(PLAY)) return 1;
	_reset();
	addStatus(PLAY);
	return 0;
}

int PCMDAC::Stop(){
	if(!hasStatus(PLAY)) return -1;
	clearStatus(PLAY);
	return 0;
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
	if(!(__samples=new u8[(_sz_samples=2*44100*5*+sizeof(u16))]))
		return -3;
	_frequency=freq;
	_channels=ch;

	_configure();

	_resample_step=(freq*4096) / 44100;
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

int PCMDAC::Reset(){
	if(__samples)
		memset(__samples,0,_sz_samples);
	_reset();
	Stop();

#ifdef _DEVELOPa
	if(fp==0){
		WAVHDR w;

		if((fp=fopen("audio.wav","wb")))
			fwrite(&w,sizeof(WAVHDR),1,fp);
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
	for(sz=SL(sz,11),pos=0;pos < sz;pos+=_resample_step){
		d[0] = s[SR(pos,12)];
		if(_channels==1)
			d[1] = d[0];
#ifdef _DEVELOP
		if(fp)
			fwrite(d,1,4,fp);
		lino+=4;
#endif
		if((_pos_write += 4) >= _sz_samples-3){
			_pos_write = 0;
			d = (u16 *)__samples;
		}
		else
			d+=2;
		_n_write += 4;
	}
	Start();
	LeaveCriticalSection(&_cr);
	return 0;
}

void PCMDAC::OnRun(){
	snd_pcm_sframes_t frames;

	EnterCriticalSection(&_cr);
	if(!hasStatus(PLAY))
		goto A;
	_elapsed += 61;
	//if(!BT(_status,BV(16)))
		//goto A;
	if(_elapsed >= 960){
#ifdef _DEVELOP
		if(flog)
			fprintf(flog,"\trp %u w:%u r:%u %u rp:%u wp:%u\n",_elapsed,_n_write,_n_read,_resample_step,_pos_read,_pos_write);
#endif
		//fixmee
		if(_n_write){
			if(_n_write < _n_read)
				_resample_step  -= 256;
			else
				_resample_step  += 256;
			//_resample_step=((_frequency*4096)/44100)/((_frequency*4)/_n_write);
		}
		else
			Stop();
		_n_write=0;
		_n_read=0;
		_elapsed=0;
	}

	///int frame_bytes = (snd_pcm_format_width(SND_PCM_FORMAT_S16_LE) / 8)*2;
#ifdef _DEVELOPa
	if(flog){
		fprintf(flog,"rr w:%u r:%u %u",_pos_write,_pos_read,_sz_samples);
		fflush(flog);
	}
#endif
	for(int r=_period_size*2;r>0;){
		int n = (_sz_samples - _pos_read);
		if(n > _period_size)
			n=_period_size;
		frames = snd_pcm_writei(_handle, &__samples[_pos_read], n);
		if (frames == -EAGAIN)
            continue;
		if (frames < 0)
			frames = snd_pcm_recover(_handle, frames, 0);
		if (frames < 0) {
			printf("snd_pcm_writei %d %d %u %u %s\n",r,n,_pos_read,_sz_samples,snd_strerror(frames));
			return;
		}
		n=frames*4;
#ifdef _DEVELOPa
		if(flog)
			fprintf(flog,"\t%u %u %ld",_pos_read,n,frames);
		//if(fp)
			//fwrite(&__samples[_pos_read],n,1,fp);
		//lino += n;
#endif
		if((_pos_read += n) >= (_sz_samples-4))
			_pos_read = 0;
		_n_read+=n;
		r-= n;
		///r=0;
#ifdef _DEVELOP
		if(flog)
			fprintf(flog,"\n");
#endif
	}
A:
	LeaveCriticalSection(&_cr);
}

int PCMDAC::_configure(){
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

	u32 t = 1000000/((44100*2*4096)/_period_size);

	//printf("tt  %d %u\n",SL(t,12),_period_size);
	_set_time(SL(t,12));
//	printf("pz %lu %lu\n",l,ll);
	//snd_output_stdio_attach(&output, stdout, 0);
	//snd_pcm_dump(_handle, output);

	return 0;
}