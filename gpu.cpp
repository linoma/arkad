#include "gpu.h"
#include "gui.h"
#include "utils.h"
#include "settings.h"

extern GUI gui;

GPU::GPU(){
	_bit=NULL;
	_screen=NULL;
	_line=0;
	_gpu_status=0;
	_x_inc=_y_inc=4096;
	_width=_height=0;
	InitializeCriticalSection(&_cr);
}

GPU::~GPU(){
	Destroy();
}

int GPU::Draw(cairo_t* cr){
	cairo_t* _ca;
	GdkWindow *window;
	cairo_region_t *cairoRegion;
	GdkDrawingContext *drawingContext;
	int res;

	res=-1;
	EnterCriticalSection(&_cr);
	if(_bit == NULL || _win==NULL)
		goto A;
	//cr=NULL;
	drawingContext=NULL;
	cairoRegion=NULL;
	if(!(_ca=cr)){
		if(!(window = gtk_widget_get_window( GTK_WIDGET(_win) )))
			goto A;
		if(!(cairoRegion = cairo_region_create()))
			goto B;
		if(!(drawingContext = gdk_window_begin_draw_frame( window, cairoRegion )))
			goto B;
		_ca = gdk_drawing_context_get_cairo_context( drawingContext );
		if(!_ca)
			goto B;
	}
	gdk_cairo_set_source_pixbuf (_ca, _bit, 0,0);
	cairo_paint (_ca);
B:
	if(!cr){
		if(drawingContext)
			gdk_window_end_draw_frame(window, drawingContext );
		if(cairoRegion)
			cairo_region_destroy(cairoRegion );
	}
	res=0;
A:
	LeaveCriticalSection(&_cr);
	return res;
}

int GPU:: Update(){
	int bytes_perline,res,x,y;
	char *p,*p1,*po;
	u16 *ps;
	u32 xx,yy;

	res=-2;
	EnterCriticalSection(&_cr);
	if(!_bit || !_screen)
		goto A;
	res--;
	//g_object_ref(_bit);
	if((p = po=(char *)gdk_pixbuf_get_pixels(_bit)) == NULL)
		goto B;
	bytes_perline = gdk_pixbuf_get_rowstride(_bit);
	ps = (u16 *)_screen;
	y=x=xx=yy=0;
	if(!BT(_gpu_status,SL((u64)1,63))) goto C;

	ps = (u16 *)_screen;
	p1=p;
	for(x=xx=0,xx+=_x_inc;x<_displayWidth;x++,p1+=3,xx+=_x_inc){
		u32 c = ps[SR(xx,12)];

		p1[0]=SL(c&31,3);
		p1[1]=SR(c&0x3e0,2);
		p1[2]=SR(c&0x7c00,7);
	}

	y++;
	yy+=_y_inc;
	p+=bytes_perline;

	for(;y<_displayHeight-1;y++,yy+=_y_inc,p+=bytes_perline){
		p1=p;
		ps = (u16 *)_screen + SR(yy,12) * _width;
		u16 *psu = (u16 *)_screen + SR(yy-_y_inc,12) * _width;
		u16 *psd = (u16 *)_screen + SR(yy+_y_inc,12) * _width;

		xx=0;

		u32 c = ps[SR(xx,12)];

		int r=SL(c&31,3);
		int g=SR(c&0x3e0,2);
		int b=SR(c&0x7c00,7);

		p1[0]=r;
		p1[1]=g;
		p1[2]=b;
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

			if((r=SR(r,3))>255)  r=255;
			if((g=SR(g,3))>255)  g=255;
			if((b=SR(b,3))>255)  b=255;

			p1[0]=r;
			p1[1]=g;
			p1[2]=b;
		}
		c = ps[SR(xx,12)];

		r=SL(c&31,3);
		g=SR(c&0x3e0,2);
		b=SR(c&0x7c00,7);

		p1[0]=r;
		p1[1]=g;
		p1[2]=b;
	}

	ps = (u16 *)_screen + SR(yy,12) * _width;
	p1=p;
	for(x=xx=0,xx+=_x_inc;x<_displayWidth;x++,p1+=3,xx+=_x_inc){
		u32 c = ps[SR(xx,12)];

		p1[0]=SL(c&31,3);
		p1[1]=SR(c&0x3e0,2);
		p1[2]=SR(c&0x7c00,7);
	}
	goto B;
C:

	for(;y<_displayHeight;y++,yy+=_y_inc,p+=bytes_perline){
		p1=p;
		ps = (u16 *)_screen + SR(yy,12) * _width;
		xx=0;
		for(x=0;x<_displayWidth;x++,p1+=3,xx+=_x_inc){
			u32 c = ps[SR(xx,12)];

			p1[0]=SL(c&31,3);
			p1[1]=SR(c&0x3e0,2);
			p1[2]=SR(c&0x7c00,7);
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
//	printf("new size %d %d\n",w,h);
	res=-1;
	EnterCriticalSection(&_cr);
	bit=gdk_pixbuf_new(GDK_COLORSPACE_RGB,false,8,w,h);
	if(!bit)
		goto A;
	DestroyBitmap();
	_bit=bit;
	//printf("ook\n");
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
		g_object_unref(_bit);
	_bit=NULL;
	return 0;
}

int GPU::Init(int w,int h,int ww,int hh,int wb,int hb){
	_hblank=_width=w;
	_vblank=_height=h;
	_win=gui._getDisplay();
	if(ww==-1 || hh==-1){
		GtkAllocation al;

		gtk_widget_get_allocation(_win,&al);
		if(ww==-1)
			ww=al.width;
		if(hh==-1)
			hh=al.height;
	}

	CreateBitmap(ww,hh);
	if(wb!=-1)
	_hblank=wb-_hblank;
	if(hb!=-1)
		_vblank=hb-_vblank;

	if(stoi(gui["antialias"]))
		_gpu_status |= (u64)1 << 63;
	return 0;
}

int GPU::Reset(){
	_line=0;
	return 0;
}

int GPU::Destroy(){
	DestroyBitmap();
	if(_screen)
		free(_screen);
	_screen=NULL;
	return 0;
}