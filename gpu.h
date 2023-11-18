#include "arkad.h"

#ifndef __GPUH__
#define __GPUH__

class GPU : public ICpuTimerObj,public ISerializable{
public:
	GPU();
	virtual ~GPU();
	virtual int Init(int,int,int ww=-1,int hh=-1,int wb=-1,int hb=-1);
	virtual int Destroy();
	virtual int Reset();
	virtual int Draw(HDC cr=NULL);
	virtual int Update();
	virtual int CreateBitmap(int,int);
	u32 I_INLINE _getScanLine(){return _line;};
	int DestroyBitmap();
	virtual int Run(u8 *,int)=0;
	virtual int Serialize(IStreamer *){return 0;};
	virtual int Unserialize(IStreamer *){printf("giocp\n");return 0;};
	//int Query(u32 a,void *p){printf("cazzo duro\n");if(a==ICORE_QUERY_OID){*((u32  *)p)=0x01000001;return 0;} return -1;};
	virtual int LoadSettings(void * &v);
protected:
	HBITMAP _bit;
	void *_screen,*_gpu_mem,*_gpu_regs,*_bits;
	int _width,_height,_displayWidth,_displayHeight,_hblank,_vblank;
	HWND _win;
	u32 _line,_x_inc,_y_inc,bytes_perline;
	u64 _gpu_status;
private:
	CRITICAL_SECTION _cr;
#ifdef __WIN32__
	BITMAPINFO bminfo;
	HDC hdc;
#else
	GdkWindow *_window;
	cairo_region_t *_cairoRegion;
#endif
};

#endif