#include "gpu.h"

#ifndef __AMIGADENISEH__
#define __AMIGADENISEH__

namespace amiga{

class amiga500m;

class Denise : public GPU{
public:
	Denise();
	virtual ~Denise();
	virtual int Run(u8 *,int cyc,void *obj);
	virtual int Init(void *,void *,u32);
	virtual int Reset();
	virtual int Update(u32 flags=0);
protected:
	virtual int LoadSettings(void * &);
	virtual int write(u32,u16);
	virtual int read(u32,u16 *);
	virtual int _dumpRegisters(char *);
	int RenderLine(u32 flags=0);
	int _sprite_update_dma(u32 y);
	u32 _sprite_get_pixel(u32 x);
	u16 _ham_get_color(u16);

	struct __display{
		enum:u8{ODD,EVEN,SPRITE};
		RECT _win;
		int _planes,_start,_stop,_lborder,_mhpos,_mvpos;
		u16 _regs[0x200],_ham_color;

		struct __layer{
			union{
				u8 _status;
				struct{
					unsigned int _visible:1;
				};
			};
			int _width,_ofs,_mask,_priority;
		} _layers[2];

		struct __sprite_obj{
			u32 _off,_shift,_pt;
			union{
				u8 _status;
				struct{
					unsigned int _changed:1;
					unsigned int _reload:1;
					unsigned int _live:1;
					unsigned int _ctl:1;
					unsigned int _comparitor:1;
				};
			};
			int _reset(){
				_status=0;
				_off=0;
				_shift=0;
				return 0;
			};
		};

		struct __sprite : __layer{
			struct __sprite_obj _sprites[8];

			int _hide(){
				for(int i=0;i<8;i++)
					_sprites[i]._off=0;
				return 0;
			};

			int _reset(){
				for(int i=0;i<8;i++)
					_sprites[i]._reset();
				return 0;
			};

		} _sprite;

		struct __sprite_obj *_sprites=_sprite._sprites;

		struct __layer *__layers[3]={&_layers[0],&_layers[1],&_sprite};

		union{
			struct{
				unsigned int _control:1;
				unsigned int _win:1;
				unsigned int _size:1;
				unsigned int _layers:1;
				unsigned int _sprites:1;
				unsigned int _pal:1;
				unsigned int _vpos:1;
			};
			u32 _val;
		} _changed;

		union{
			u8 _val;
			struct{
				unsigned int _le:1;
				unsigned int _se:1;
				unsigned int _lof:1;
				unsigned int _lace:1;
				unsigned int _ham:1;
				unsigned int _dblpf:1;
				unsigned int _hires:1;
				unsigned int _e0:1;
				unsigned int _lv:1;
				unsigned int _sv:1;
				unsigned int _bp0v:1;
				unsigned int _bp1v:1;
				unsigned int _bp2v:1;
				unsigned int _bp3v:1;
				unsigned int _bp4v:1;
				unsigned int _bp5v:1;
				unsigned int _draw:1;
			};
		} _status;

		int reset();
		__display();
	} _display;

private:
	u16 *_ioreg,_cycles;
	u8 *_mem, *_dblpf_bitplanes[2];
	amiga500m *_machine;

	friend class amiga500m;
};

};


#endif