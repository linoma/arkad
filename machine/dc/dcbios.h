#ifndef __DCBIOSH__
#define __DCBIOSH__

#include <dcdev.h>

namespace dc {

#define SYSINFO_ID_ADDR 0x8C001010
#define FONT_TABLE_ADDR 0xa0100020

#define GET_RAM_PTR(a,b) a = (u32 *)&_mem[MI_RAM + ((b) & (MS_RAM-1))];

class dcbios: public ASIC{
public:
	dcbios();
	virtual ~dcbios();
	virtual int Reset();
	virtual int Init(int,void *,void *,u32);
protected:
	virtual int Load(IGame *pg,char *fn);
	int gdrom_hle_proc();
	int flashrom_hle_proc();
	int system_hle_proc();
	int sys_font_hle_proc();
private:
		/* add your private declarations */
};

};


#endif
