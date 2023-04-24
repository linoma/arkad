#include "pcmdac.h"

PCMDAC::PCMDAC(){
	_handle=0;
	_data=NULL;
}

PCMDAC::~PCMDAC(){
	Close();
}

int PCMDAC::Close(){
	if(!_handle)
		return 0;
	snd_pcm_drain(_handle);
    snd_pcm_close(_handle);
    return 0;
}

int PCMDAC::Open(int ch,int freq){
	if (snd_pcm_open(&_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
		return -1;
	return 0;
}