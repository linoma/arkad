#include "arkad.h"

#ifndef __GPUH__
#define __GPUH__

class GPU : public  ICpuTimerObj{
public:
	GPU();
	virtual ~GPU();
	virtual int Init(int,int,int,int);
	virtual int Destroy();
	virtual int Reset();
	virtual int Draw(cairo_t* cr=NULL);
	virtual int Update();
	virtual int CreateBitmap(int,int);
	int DestroyBitmap();
	//IObject
	virtual int Query(u32,void *){return -1;};
protected:
	HBITMAP _bit;
	void *_screen,*_gpu_mem,*_gpu_regs;
	int _width,_height,_displayWidth,_displayHeight,_hblank,_vblank;
	HWND _win;
	u32 _line;
	u64 _gpu_status;
private:
	u32 _cr;
};

#endif