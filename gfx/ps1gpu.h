#include "gre.h"
#include "general_device.h"

#ifndef __PS1GPUH__
#define __PS1GPUH__

namespace ps1{

class PS1GPU : public GRE{
public:
	PS1GPU();
	virtual ~PS1GPU();
	int Init();
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update(u32 flags=0);
protected:
	void _createColor(u32,u32,float *);
	virtual int _beginDraw(int);
	virtual int write(u32,u32);
	virtual int read(u32,u32 *);
	virtual int _createTexture(void *,void *,void *dst=0);
	virtual int _drawQuad(u32,u16,u16,u16 w=8,u16 h=8,u16 s=0,u16 t=0,u64 id=0,u32 f=1);
	virtual int _drawPolygon(u32 *,u64 id=0,u32 f=1);
	virtual int _bindTexture(u64,int w=8,int h=8);
	virtual int _drawLine(u32 *data,u32 nv=2,u32 f=0);

	struct __env{
		struct  __draw{
			u32 _tex_id;
			s16 _x,_y,_w,_h,_ox,_oy;
			u16 *_fb;
			int _x_inc,_y_inc;
			union{
				struct{
					unsigned int _tex_page:5;
					unsigned int _blend:2;
					unsigned int _tex_mode:2;
					unsigned int _dither:1;
					unsigned int _draw:1;
					unsigned int _textured:1;
					unsigned int _flipX:1;
					unsigned int _flipY:1;
				};
				u32 _value;
			};
		} _draw;

		struct __texture{
			union{
				u32 _value;
				struct{
					unsigned int w:5;
					unsigned int h:5;
					unsigned int x:5;
					unsigned int y:5;
				};
			};
		} _texture;
		struct __dma{
			u16 x,y,w,h,_x,_y,*dst,*src,xs,ys;
			u32 _value,_pos;
			int _transfer(int,u32 *,void *);
		} _dma;
		struct __display{
			u8 _mode;
			s16 _x,_y,_w,_h;
			u16 *_fb;
			s32 _x_inc,_y_inc;
		} _display;

		union{
			struct{
				unsigned int texX:4;
				unsigned int texY:1;
				unsigned int blend:2;
				unsigned int texC:2;
				unsigned int dither:1;
				unsigned int render:1;
				unsigned int mask:1;
				unsigned int draw:1;
				unsigned int interlace:1;
				unsigned int reverse:1;
				unsigned int texturing:1;
				unsigned int resH0:1;
				unsigned int resH1:2;
				unsigned int resV:1;
				unsigned int mode:1;
				unsigned int colors:1;
				unsigned int interlaceV:1;
				unsigned int enable:1;
				unsigned int irq:1;
				unsigned int dma:1;
				unsigned int cmdr:1;
				unsigned int sendr:1;
				unsigned int recvr:1;
				unsigned int dir:2;
				unsigned int line:1;
			};
			u32  _status;
		};
		int _change(int,int,int,int);
		int _reset(int,int,int,int);
	} _env;

private:
	u32 _cycles,_cr;
	GLuint _iublend;

	struct __command<u32> _fifo;
};

};

#endif