#include "arkad.h"
#include "game.h"

#ifndef __PS1ROMH__
#define __PS1ROMH__

namespace ps1{

typedef struct {
	unsigned char id[8];
    u32 text;
    u32 data;
    u32 pc0;
    u32 gp0;
    u32 t_addr;
    u32 t_size;
    u32 d_addr;
    u32 d_size;
    u32 b_addr;
	u32 b_size;
	u32 s_adr;
	u32 s_size;
    u32 SavedSP;
    u32 SavedFP;
    u32 SavedGP;
    u32 SavedRA;
    u32 SavedS0;
} PS1EXE_HEADER;

class RomStream : public ISOStream{
public:
	RomStream();
	RomStream(char *);
	virtual ~RomStream();
	virtual int Parse(IGame *,PS1EXE_HEADER *h=NULL);
	virtual int _getInfo(u32 *,void **){return 0;};
	virtual int SeekToStart(s64 a=0);
};

class PS1ROM : public Game{
public:
	PS1ROM();
	virtual ~PS1ROM();
	virtual int Open(char *,u32);
	virtual int Read(void *,u32,u32 *);
	virtual int Write(void *,u32,u32 *);
	virtual int Close();
	virtual int Query(u32 w,void *pv);
	virtual int Seek(s64,u32);
	virtual int Tell(u64  *);
protected:
	int _type;
	void *_header;
private:
};

class PS1CUEROM : public RomStream{
public:
	PS1CUEROM();
	PS1CUEROM(char *);
	virtual ~PS1CUEROM();
	virtual int Read(void *,u32,u32 *);
	virtual int Parse(IGame *,PS1EXE_HEADER *h=NULL);
	virtual int _getInfo(u32 *,void **);
protected:
	u32 _size;
	struct  __tracks{
		u32 _size,_number;
		string _filename,_type;

		__tracks(u32 v){_size=v;};
		__tracks(string s,u32 v){_size=v;_filename=s;};
	};
	vector<__tracks> _tracks;
};

class PS1ECMROM : public PS1CUEROM{
public:
	PS1ECMROM();
	PS1ECMROM(char *);
	virtual ~PS1ECMROM();
	virtual int Parse(IGame *,PS1EXE_HEADER *h=NULL);
};

};

#endif