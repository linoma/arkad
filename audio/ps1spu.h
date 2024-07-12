#include "arkad.h"
#include "pcmdac.h"

#ifndef __PS1SPUH__
#define __PS1SPUH__

namespace ps1{

class PS1M;

class PS1SPU : public PCMDAC{
public:
	PS1SPU();
	virtual ~PS1SPU();
	virtual int Init(PS1M &,void *,void  *);
	void write_reg(u32,u32);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
protected:
	virtual int _playAudioFile(char *,u32 pos=0);

	virtual int _closeAudioFile();
private:
	union{
		struct{
			unsigned int _cdae:1;
			unsigned int _eae:1;
			unsigned int _cdar:1;
			unsigned int _ear:1;
			unsigned int _rtm:2;
			unsigned int _irqe:1;
			unsigned int _rve:1;
			unsigned int _nfr:2;
			unsigned int _nfs:4;
			unsigned int _mute:1;
			unsigned int _enable:1;
		};
		u16 _control16;
		u64 _control;
	};
	u32 _mem_adr_start,_cycles;
	u16 *_reverbb,*_samples;
	s16 _volL,_volR;

	struct{
		u32 _pos;
		FILE *_fp;
	} _track;

	int _xa_decode_sector(void *,void *,u32);
	friend class PS1M;
};

};


#endif