#include "gpu.h"
#include "gui.h"
#include "utils.h"

extern GUI gui;

GPU::GPU(){
	_bit=NULL;
	_screen=NULL;
	_line=0;
	_gpu_status=0;
	_x_inc=_y_inc=4096;
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
	if(!(_ca=cr)){
		window = gtk_widget_get_window( GTK_WIDGET(_win) );
		cairoRegion = cairo_region_create();
		drawingContext = gdk_window_begin_draw_frame( window, cairoRegion );
		_ca = gdk_drawing_context_get_cairo_context( drawingContext );
	}
	gdk_cairo_set_source_pixbuf (_ca, _bit, 0,0);
	cairo_paint (_ca);
	if(!cr){
		gdk_window_end_draw_frame( window, drawingContext );
		cairo_region_destroy( cairoRegion );
	}
	res=0;
A:
	LeaveCriticalSection(&_cr);
	return res;
}

int GPU:: Update(){
	int bytes_perline,res,x,y;
	char *p,*p1;
	u16 *ps;
	u32 xx,yy;

	res=-2;
	EnterCriticalSection(&_cr);
	if(!_bit || !_screen)
		goto A;
	res--;
	//g_object_ref(_bit);
	if((p = (char *)gdk_pixbuf_get_pixels(_bit)) == NULL)
		goto B;
	bytes_perline = gdk_pixbuf_get_rowstride(_bit);
	ps = (u16 *)_screen;
	y=x=xx=yy=0;

	for(;y<_displayHeight;y++,yy+=_y_inc,p+=bytes_perline){
		p1=p;
		ps = (u16 *)_screen + SR(yy,12) * _width;
		for(int x=0,xx=0;x<_displayWidth;x++,p1+=3,xx+=_x_inc){
			u32 c = ps[SR(xx,12)];

			p1[0]=SL(c&31,3);
			p1[1]=SR(c&0x3e0,2);
			p1[2]=SR(c&0x7c00,7);
		}
	}


	BC(_gpu_status,0xF);
	res=0;
B:
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

int GPU::Init(int w,int h,int ww,int hh){
	_width=w;
	_height=h;
	CreateBitmap(w,h);
	_win=gui._getDisplay();
	_hblank=ww-w;
	_vblank=hh-h;
	return 0;
}

int GPU::Reset(){
	_line=0;
	return 0;
}

int GPU::Destroy(){
	printf("%s destroy\n",__FILE__);
	DestroyBitmap();
	if(_screen)
		free(_screen);
	_screen=NULL;
	return 0;
}