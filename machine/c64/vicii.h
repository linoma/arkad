#ifndef __VICIIH__
#define __VICIIH__

#include <gpu.h>

namespace c64{

class VICII: public GPU,private CPUTIMEROBJ{
public:
	VICII(void *p=NULL);
	virtual ~VICII();
	virtual int Run(u8 *,int cyc,void *obj);
	virtual int Init(void *,void *,void *);
	virtual int Reset();
	virtual int Update(u32 flags=0);

	static u16 *_et,*_emt;

protected:
	virtual int LoadSettings(void * &);
	virtual int write(u32,u16);
	virtual int read(u32,u16 *);
	int RenderLine(u32 flags=0,u32 cyc=0);
	virtual int _remap(void *,void *);
	virtual int _dumpRegisters(char *p);
private:
	int _flush(u32 param=0);

	u16 _lc,_base,_tile;
	u8 *matrix_line,*color_line,_row;
	u16 matrix_base,char_base,bitmap_base,vbase,*_pl,*_sl;
	union{
		u32 _status;
		struct{
			unsigned int _enabled:1;
			unsigned int _fetch:1;
			unsigned int _render:1;
			unsigned int _mode:3;
		};
	};
	u16 _cycles;
	u8 *_mem,*_ioreg;
};

};

#endif /* VICII_HPP */
