#include <gbadev.h>

#ifndef __GBABIOSH__
#define __GBABIOSH__

namespace gba{

class gbabios: public gbadev{
public:
	gbabios();
	virtual ~gbabios();
	int biosCall(u32);
	virtual int _enterIRQ(int n,u32 pc=0);
	virtual int Reset();
protected:
	int stop();
	int fastset(u32 dst,u32 src,u32 flags);
	int set(u32 dst,u32 src,u32 flags);
	int LZ77UnComp(u32 src,u32 dst);
	int Midi2Key();
	int RegisterRamReset(u8);
	int ObjAffineSet();
	int BgAffineSet();
private:
	void _resetMem(u32,u32);
};

};

#endif /* GBABIOS_H */
