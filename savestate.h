#include "arkad.h"
#include <vector>

#ifndef __SAVESTATEH__
#define __SAVESTATEH__

using namespace std;;

typedef struct{
   u64 time;
   char fileName[12];
   int index;
} SSFILE,*LPSSFILE;

class SaveState : public FileStream{
public:
	SaveState(IMachine *);
	SaveState();
	virtual ~SaveState();
	virtual int GetVersion(u32 *);
	virtual int Load(char *);
	virtual int Save(char *);
private:
	IMachine *_machine;
	u32 _version;
};

class SaveStateManager : public vector<LPSSFILE>{
};

#endif