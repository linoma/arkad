#include "gpu.h"
#include "gui.h"
#include "utils.h"
#include "settings.h"

extern GUI gui;

#ifdef __WIN32__
#define CR 2
#define CB 0
#else
#define CR 0
#define CB 2
#endif

#define STPIXEL(p1,r,g,b,br){\
	register u32  c__=SR((r)*br,7);\
	if(c__>255)  c__=255;\
	p1[CR]=c__;\
	c__=SR((g)*br,7);\
	if(c__>255)  c__=255;\
	p1[1]=c__;\
	c__=SR((b)*br,7);\
	if(c__>255)  c__=255;\
	p1[CB]=c__;\
}

GPU::GPU(){
	_bit=NULL;
	_screen=NULL;
	__line=0;
	_gpu_status=0;
	_x_inc=_y_inc=4096;
	_width=_height=0;
	_hz=60;
	InitializeCriticalSection(&_cr);
#ifdef __WIN32__
	hdc=NULL;
#else
	_window=0;
	_cairoRegion=NULL;
#endif
}

GPU::~GPU(){
	Destroy();
}

int GPU::Clear(){
	if(!_screen) return -1;
	memset(_screen,0,_width*_height*sizeof(u16));
	return 0;
}

int GPU::Draw(HDC cr){
	int res;
	HDC _ca;

	res=-1;
	//return res;
	EnterCriticalSection(&_cr);
	if(_bit == NULL || _win==NULL)
		goto A;
#ifdef __WIN32__
	if(!(_ca=cr)){
		RECT rc;

		_ca=GetDC(gui._getDisplay());
		gui.GetClientSize(&rc);
		SetWindowOrgEx(_ca,0,-rc.top,NULL);
	}
	BitBlt(_ca,0,0,_displayWidth,_displayHeight,hdc,0,0,SRCCOPY);
	if(!cr)
		ReleaseDC(gui._getDisplay(),_ca);
#else
	GdkDrawingContext *drawingContext;

	//cr=NULL;
	drawingContext=NULL;
	if(!(_ca=cr)){
		if(!_window && !(_window = gtk_widget_get_window(GTK_WIDGET(_win))))
			goto A;
		if(!_cairoRegion && !(_cairoRegion = cairo_region_create()))
			goto B;
		if(!(drawingContext = gdk_window_begin_draw_frame(_window, _cairoRegion)))
			goto B;
		_ca = gdk_drawing_context_get_cairo_context(drawingContext);
		if(!_ca)
			goto B;
	}
	gdk_cairo_set_source_pixbuf (_ca, _bit, 0,0);
	cairo_paint(_ca);
B:
	if(!cr){
		if(drawingContext)
			gdk_window_end_draw_frame(_window, drawingContext );
	}
	res=0;
#endif
A:
	LeaveCriticalSection(&_cr);
	return res;
}

int GPU:: Update(u32 flags){
	int res,x,y;
	char *p,*p1,*po;
	u16 *ps,*pss;
	u32 xx,yy;

	res=-2;
	EnterCriticalSection(&_cr);
	if(!_bit || !_screen)
		goto A;
	res--;
	//g_object_ref(_bit);
	if((p = po=(char *)_bits) == NULL)
		goto B;
	pss = (u16 *)_screen;
	y=x=xx=yy=0;
	if(_gpu_status & GPU_STATUS_TRANSFORM_MATRIX){
		s64 x1=SR(_mtxView[2],12)*_width;
		s64 x2=SR(_mtxView[5],12)*_width;
		char *p2=(char *)_screen0;

		for(y=0;y<_height;y++,p2 += _width*2){
			p1=p2;
			xx=(y*_mtxView[1])+_mtxView[4];
			s64 y3 = SR((s64)y*_mtxView[3],12)*_width;
			s64 x3=x2;
			for(x=0;x<_width;x++,p1+=2,x3+=x1){
				u32 c;

				int x0=(x*_mtxView[0]) + xx;

				c =((u16 *)_screen)[y3+x3 + SR(x0,12)];

				*((u16 *)p1)=c;
			}
		}

		pss = (u16 *)_screen0;
	}

	y=x=xx=yy=0;
	switch(GPU_STATUS_MULTISAMPLE_VALUE(_gpu_status)){
		case 0:
			goto C;
	}

	/*multisample
	 *
	*/

	p1=p;
	ps=pss;
	for(xx+=_x_inc;x<_displayWidth;x++,p1+=3,xx+=_x_inc){
		u32 c = ps[SR(xx,12)];

		STPIXEL(p1,SL(c&31,3),SR(c&0x3e0,2),SR(c&0x7c00,7),128);
	}

	y++;
	yy+=_y_inc;
	p+=bytes_perline;

	for(;y<_displayHeight-1;y++,yy+=_y_inc,p+=bytes_perline){
		p1=p;
		ps = (u16 *)pss + SR(yy,12) * _width;
		u16 *psu = (u16 *)pss + SR(yy-_y_inc,12) * _width;
		u16 *psd = (u16 *)pss + SR(yy+_y_inc,12) * _width;

		xx=0;

		u32 c = ps[SR(xx,12)];

		int r=SL(c&31,3);
		int g=SR(c&0x3e0,2);
		int b=SR(c&0x7c00,7);

		STPIXEL(p1,r,g,b,128);

		p1+=3;

		for(x=1,xx+=_x_inc;x<_displayWidth-1;x++,p1+=3,xx+=_x_inc){
			c = ps[SR(xx,12)];

			r=SL(c&31,3);
			g=SR(c&0x3e0,2);
			b=SR(c&0x7c00,7);

			r=SL(r,2);
			g=SL(g,2);
			b=SL(b,2);

			c = ps[SR(xx-_x_inc,12)];
			r+=SL(c&31,3);
			g+=SR(c&0x3e0,2);
			b+=SR(c&0x7c00,7);

			c = ps[SR(xx+_x_inc,12)];
			r+=SL(c&31,3);
			g+=SR(c&0x3e0,2);
			b+=SR(c&0x7c00,7);

			c = psu[SR(xx,12)];
			r+=SL(c&31,3);
			g+=SR(c&0x3e0,2);
			b+=SR(c&0x7c00,7);

			c = psd[SR(xx,12)];
			r+=SL(c&31,3);
			g+=SR(c&0x3e0,2);
			b+=SR(c&0x7c00,7);

			if((r=SR(r,3))>255)
				r=255;
			if((g=SR(g,3))>255)
				g=255;
			if((b=SR(b,3))>255)
				b=255;

			STPIXEL(p1,r,g,b,128);
		}
		c = ps[SR(xx,12)];

		r=SL(c&31,3);
		g=SR(c&0x3e0,2);
		b=SR(c&0x7c00,7);

		STPIXEL(p1,r,g,b,128);
	}

	ps = (u16 *)pss + SR(yy,12) * _width;
	p1=p;
	for(x=xx=0,xx+=_x_inc;x<_displayWidth;x++,p1+=3,xx+=_x_inc){
		u32 c = ps[SR(xx,12)];

		STPIXEL(p1,SL(c&31,3),SR(c&0x3e0,2),SR(c&0x7c00,7),128);
	}
	goto B;
C:
	//no multisaple

	for(;y<_displayHeight;y++,yy+=_y_inc,p+=bytes_perline){
		p1=p;
		ps = (u16 *)pss + SR(yy,12) * _width;
		xx=0;
		for(x=0;x<_displayWidth;x++,p1+=3,xx+=_x_inc){
			u32 c = ps[SR(xx,12)];

			STPIXEL(p1,SL(c&31,3),SR(c&0x3e0,2),SR(c&0x7c00,7),128);
		}
	}
B:
	BC(_gpu_status,0xF);
	res=0;

	//g_object_unref(_bit);
A:
	LeaveCriticalSection(&_cr);
	return res;
}

int GPU::CreateBitmap(int w,int h){
	int res;
	HBITMAP bit,aa;

	if(_displayWidth == w && _displayHeight==h)
		return 1;
	res=-1;
	EnterCriticalSection(&_cr);
#ifdef __WIN32__
	BITMAP bm;
	HDC dc;

	dc=CreateCompatibleDC(NULL);
	memset(&bminfo,0,sizeof(BITMAPINFO));
	bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bminfo.bmiHeader.biWidth = w;
	bminfo.bmiHeader.biHeight = -h;
	bminfo.bmiHeader.biPlanes = 1;
	bminfo.bmiHeader.biBitCount = 24;
	bminfo.bmiHeader.biCompression = BI_RGB;
	bit = CreateDIBSection(dc,&bminfo,DIB_RGB_COLORS,(LPVOID *)&_bits,NULL,0);
	if(!bit)
		goto A;
	DestroyBitmap();
	_bit=bit;
	hdc=dc;
	GetObject(_bit,sizeof(BITMAP),&bm);
	bytes_perline = bm.bmWidthBytes;
	SelectObject(hdc,_bit);
#else
	bit=gdk_pixbuf_new(GDK_COLORSPACE_RGB,false,8,w,h);
	if(!bit)
		goto A;
	DestroyBitmap();
	_bit=bit;
	gdk_pixbuf_fill(bit,0);
	_bits = gdk_pixbuf_get_pixels(_bit);
	bytes_perline = gdk_pixbuf_get_rowstride(_bit);
#endif

	_displayWidth=w;
	_displayHeight=h;

	_x_inc = SL(_width,12) / _displayWidth;
	_y_inc = SL(_height,12) / _displayHeight;

	res=0;
A:
	LeaveCriticalSection(&_cr);
	return res;
}

int GPU::DestroyBitmap(){
	if(_bit)
		DeleteObject(_bit);
#ifdef __WIN32__
	if(hdc)
		::DeleteDC(hdc);
	hdc=NULL;
#endif
	_bit=NULL;
	return 0;
}

int GPU::Init(int w,int h,int f,u32 ex){
	RECT rc;
	int pxf;

	_hblank=_width=w;
	_vblank=_height=h;
	_format=f;
	pxf=sizeof(u16);
	if(!(_screen = malloc(_width*_height*pxf*2 + ex)))
		return -1;
	_screen0 = (u8 *)_screen + _width*_height*pxf + ex;
	_win=gui._getDisplay();

	rc.right=rc.bottom=-1;
	gui.GetClientSize(&rc);
	int ww=rc.right-rc.left;
	int hh=rc.bottom-rc.top;
	CreateBitmap(ww,hh);

//	if(stoi(gui["antialias"]))
	//	_gpu_status |= 1LL << 63;
	memset(_mtxView,0,sizeof(_mtxView));
	_mtxView[0]=4096;
	_mtxView[3]=4096;
	return 0;
}

int GPU::Reset(){
	__line=0;
	return 0;
}

int GPU::Destroy(){
	DestroyBitmap();
	if(_screen)
		free(_screen);
	_screen=NULL;
#ifndef __WIN32__
	if(_cairoRegion)
		cairo_region_destroy(_cairoRegion);
	_cairoRegion=NULL;
	_window=NULL;
#endif
	return 0;
}

int GPU::LoadSettings(void * &v){
	map<string,string> &m=(map<string,string> &)v;
	//_pcm_sync=stoi(m["pcm_sync"]);
	_gpu_status |= GPU_STATUS_MULTISAMPLE_BILINEAR;
	return 0;
}

int GPU::MoveResize(int x,int y,int w,int h){
	//CreateBitmap(w,h);
	return 0;
}

int GPU::InitGM(GM *p){
	p->_dst=(u16 *)_screen;
	p->_regs=(u32 *)_gpu_regs;
	p->_screen_width=_width;
	p->_screen_height=_height;
	return 0;
}

int GPU::_viewProjection(float *fv){
	for(int i=0;i<6;i++)
		_mtxView[i]=fv[i]*4096.0f;
	_gpu_status |= GPU_STATUS_TRANSFORM_MATRIX;
	return 0;
}

int GPU::_setBlankArea(int vs,int ve,int hs,int he,float vhz,float chz){
	_vblank_end=ve;
	_vblank=vs;
	_scanline_cycles= ceil(chz/(vhz*ve));
	return  0;
}