#include "arkad.h"
#include "icore.h"

#ifndef __GPUH__
#define __GPUH__

typedef struct __graphics_manager{
	__graphics_manager(){
		_regs=0;
		_dst=_pal=0;
		_screen_width=_screen_height=0;
		_color_ram=_char_ram=_tile_ram=0;
	};
	u16 *_dst,*_pal,_screen_width,_screen_height;
	u32 *_regs;
	u8 *_tile_ram,*_char_ram,*_color_ram;
} GM;

#define GPU_STATUS_MULTISAMPLE(a) 				SL((u64)(a&7),60)
#define GPU_STATUS_TRANSFORM_MATRIX 			SL((u64)1,59)
#define GPU_STATUS_MULTISAMPLE_VALUE(a) 		SR((u64)a,60)
#define GPU_STATUS_MULTISAMPLE_BILINEAR 		GPU_STATUS_MULTISAMPLE(1)

class GPU : public ICpuTimerObj,public ISerializable{
public:
	GPU();
	virtual ~GPU();
	virtual int Init(int,int,int f=0,u32 ex=0);
	virtual int Destroy();
	virtual int Reset();
	virtual int Draw(HDC cr=NULL);
	virtual int Update(u32 flags=0);
	virtual int CreateBitmap(int,int);
	u32 I_INLINE _getScanLine(){return __line;};
	int DestroyBitmap();
	virtual int Run(u8 *,int,void *)=0;
	virtual int Serialize(IStreamer *){return 0;};
	virtual int Unserialize(IStreamer *){printf("giocp\n");return 0;};
	virtual int MoveResize(int,int,int,int);
	//int Query(u32 a,void *p){printf("Query\n");if(a==ICORE_QUERY_OID){*((u32  *)p)=0x01000001;return 0;} return -1;};
	virtual int LoadSettings(void * &v);
	virtual int InitGM(GM *);
	virtual int _setBlankArea(int,int,int hs=0,int he=-1,float vhz=60,float chz=MHZ(4));
protected:
	virtual int Clear();
	virtual int _viewProjection(float *fv);
	HBITMAP _bit;
	void *_screen,*_gpu_mem,*_gpu_regs,*_bits,*_palette,*_screen0;
	int _width,_height,_displayWidth,_displayHeight,_hblank,_vblank,_format,_hz,_scanline_cycles,_vblank_end;

	HWND _win;
	u32 _x_inc,_y_inc,bytes_perline;
	u64 _gpu_status;
private:
	CRITICAL_SECTION _cr;
	s32 _mtxView[6];
#ifdef __WIN32__
	BITMAPINFO bminfo;
	HDC hdc;
#else
	GdkWindow *_window;
	cairo_region_t *_cairoRegion;
#endif
};

#endif