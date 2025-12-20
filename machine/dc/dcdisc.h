#include "arkad.h"
#include "machine.h"
#include "game.h"

#ifndef ___DCDISCH__
#define ___DCDISCH__

namespace dc{

typedef  struct __dcxeheader{
	u32 _exe_pos;
	u32 _exe_size;
	u32 _exe_addr;
	u32 _boot_size;
	u32 _boot_pos;
	u32 _boot_addr;
	u32 _entry_addr;
} DCEXEHEADER;

class DcGame : public DiscGame{
public:
	DcGame();
	virtual ~DcGame();
	virtual int Open(char *,u32);
	virtual int NewHeader(void **,u32);
	virtual int CopyHeader(void *,void *,u32 f=0);
	virtual int _getSectorPosition(u32,void *);
};

class CDIStream : public DiscStream,public ISOStream{
public:
	CDIStream(char *p=0);
	virtual ~CDIStream();

	virtual int Read(void *buf,u32 sz,u32 *po=0);
	virtual int Parse(IGame *,void *h);
	virtual int _getFilePosition(char *,u64 **);
protected:
	int _parse_track(int idx,u32 *pofs);
	struct disc_meta {
		char hwareid[16];
		char makerid[16];
		char devinfo[16];
		char areasym[8];
		char periphs[8];//6 wince
		char prodnum[10];
		char prodver[6];
		char reldate[16];
		char bootnme[16];
		char company[16];
		char prodnme[128];
	} _meta;

	u32 _version;
	u16 _ns;
};

};

#endif