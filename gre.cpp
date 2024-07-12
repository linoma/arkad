#include "gre.h"
#include "gui.h"
#include "ogl.h"

extern GUI gui;

GRE::GRE() : GPU(){
	hRC = NULL;
	_outBuffer=NULL;
	_texture=NULL;
#ifdef __WIN32__
#else
	_display=0;
	_window=0;
#endif
	_sceneList=0;
	_mtx[0].identity();
	_mtx[1].identity();
	_mtx[2].identity();
}

GRE::~GRE(){
}

int GRE::Init(int w,int h,int f,u32 ex){
	HWND ww=gui._getDisplay();

	if(GPU::Init(w,h,f,ex))
		return -1;
	_engine_features=init_gl_extension();
#ifdef __WIN32__
	PIXELFORMATDESCRIPTOR pfd={0};
	int pixelformat;

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
	pfd.dwLayerMask = PFD_MAIN_PLANE;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 16;
	pfd.cStencilBits = 8;

    if((pixelformat = ChoosePixelFormat(gui._getDC(),&pfd)) == 0)
		return -1;
	if(!::SetPixelFormat(gui._getDC(),pixelformat,&pfd)){
		_engine_features &= ~GRE_MULTISAMPLE_MASK;
        MessageBox(NULL,"SetPixelFormat\r\nYour video card don't support this pixelformat : \r\nbpp: 24 alpha:8 z:16 stencil:8\r\nmultisample 2x\r\nCheck your video card settings or driver.","iDeaS Emulator",MB_OK|MB_ICONERROR);
		return -2;
	}
    if(!(hRC = wglCreateContext(gui._getDC())))
		return -3;
	wglMakeCurrent(gui._getDC(),hRC);
#else
	XVisualInfo *vi;
	GdkWindow *win;
	GdkVisual *vs;
	GdkDisplay *display;
	GdkScreen *screen;

	int cfg[] = {GLX_DOUBLEBUFFER,GLX_RGBA,GLX_ALPHA_SIZE,8,GLX_DEPTH_SIZE,24,GLX_STENCIL_SIZE,8,None };
	win = gtk_widget_get_window(GTK_WIDGET(ww));
	display = gdk_window_get_display(win);
	screen = gdk_window_get_screen(win);
	_display = gdk_x11_display_get_xdisplay(display);
	vi = glXChooseVisual(_display,gdk_x11_screen_get_screen_number(screen),cfg);
	if(vi == NULL)
		return -2;
	vs = gdk_window_get_visual(win);
	if (vs == NULL || GDK_VISUAL_XVISUAL(vs)->visualid != vi->visualid){
		gtk_widget_set_visual(ww,gdk_x11_screen_lookup_visual(screen,vi->visualid));
		gtk_widget_realize(ww);
	}
	_window=gdk_x11_window_get_xid(win);
	hRC = glXCreateContext(_display,vi,NULL,GL_TRUE);
    if(hRC == NULL)
		return -3;
    if(!glXMakeCurrent(_display,_window,hRC))
		return -4;
#endif
	_outBuffer=(u8 *)_gpu_mem + MB(2);
	_texture=(u8 *)_outBuffer + MB(2);
	_palette=(u8 *)_texture + MB(2);
	return 0;
}

int GRE::Destroy(){
	_resetScene(0);
	if(hRC){
#ifdef __WIN32__
		wglMakeCurrent(NULL,NULL);
		wglDeleteContext(hRC);
#else
		glXMakeCurrent(_display,0,NULL);
	    glXDestroyContext(_display,hRC);
#endif
		hRC=NULL;
	}
	return GPU::Destroy();
}

int GRE::Reset(){
	_bindTexture();
	_dither(0);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_FASTEST);
	glFogi(GL_FOG_MODE,GL_LINEAR);
  	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_1D);
	glDisable(GL_FOG);
	glDisable(GL_POLYGON_SMOOTH);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);

	glMatrixMode(GL_PROJECTION);
  	glLoadMatrixf(_mtx[0].ptr());
	glMatrixMode(GL_MODELVIEW);
  	glLoadMatrixf(_mtx[1].ptr());
	glMatrixMode(GL_TEXTURE);
  	glLoadMatrixf(_mtx[2].ptr());
	_clearBits=GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;
	memset(_clear_color,0,sizeof(_clear_color));
	//_clear_color[3]=1.0f;
	for(int i=0;i<sizeof(_draw_color)/sizeof(float);i++)
		_draw_color[i]=1.0f;
	//_clear_color[2]=1;
	glClearColor(_clear_color[0],_clear_color[1],_clear_color[2],_clear_color[3]);
  	glClear(_clearBits);
  	//if(_outBuffer) memset(_outBuffer,0,_displayWidth*_displayHeight*sizeof(u32));
	_clearTextures();
	_resetScene(0);
	return GPU::Reset();
}

int GRE::Update(u32 flags){
	if(!_outBuffer)
		return -1;
	if(!(_gpu_status & (GRE_STATUS_SWAP_BUFFER|GRE_STATUS_VERTEX_PUSH)))
		return 1;
	_gpu_status &= ~(GRE_STATUS_SWAP_BUFFER|GRE_STATUS_VERTEX_PUSH);
	//flags=1;

	_endScene(0);
	if(flags & 1){
		glFlush();
        glFinish();
        glReadBuffer(GL_BACK);
		glReadPixels(0,0,_displayWidth,_displayHeight,GL_RGBA,GL_UNSIGNED_BYTE,_outBuffer);
	}
	_resetScene(0);
	glClear(_clearBits);
	_clearTextures(1);
	return 0;
}

int GRE::_bindTexture(void *p){
	float f[16];

	if(pfn_glActiveTexture)	pfn_glActiveTexture(GL_TEXTURE0);
	_cr_texture=(GRETEXTURE *)p;
	if(!p){
		glBindTexture(GL_TEXTURE_2D,0);
		return 0;
	}
	((GRETEXTURE*)p)->_bind();
	if(_cr_texture->_scaled || _cr_texture->_translated){
		glMatrixMode(GL_TEXTURE);

		memset(f,0,sizeof(f));
		f[0]=1.0f/_cr_texture->w;
		f[5]=1.0f/_cr_texture->h;
		f[10]=f[15]=1;
		f[12]=f[0]*-_cr_texture->x;
		f[13]=f[5]*-_cr_texture->y;

		glLoadMatrixf(f);
	}
	return 0;
}

int GRE::_enable(u32 a,u32 b){
	if(b)
		glEnable(a);
	else
		glDisable(a);
	return 0;
}

int GRE::_blend(u32 a,...){
	va_list arg;
	int c[4];
	va_start(arg,a);
	switch(a){
#ifdef __WIN32__
#else
		case 2:{
			u32 b=va_arg(arg,u32);
			glBlendEquation(b);
		}
		break;
		case 3:{
			for(int i=0;i<4;i++)
				c[i]=va_arg(arg,int);
			glBlendColor(c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,c[3]/255.0f);
		}
		break;
		default:{
			glEnable(GL_BLEND);
			for(int i=0;i<2;i++)
				c[i] = va_arg(arg,int);
			pfn_glBlendFuncSeparate(GL_ONE,GL_ZERO,c[0],c[1]);
			//glBlendFunc(c[0],c[1]);
		}
		break;
#endif
		case 0:
			glDisable(GL_BLEND);
		break;
	}
	va_end(arg);
	return 0;
}

int GRE::_alpha(u32 a,...){
	va_list arg;

	va_start(arg,a);
	if(a){
		glEnable(GL_ALPHA_TEST);
		u32 b=va_arg(arg,u32);
		u32 c=va_arg(arg,u32);
		glAlphaFunc(b,c/255.0f);
	}
	else
		glDisable(GL_ALPHA_TEST);
	va_end(arg);
	return 0;
}

int GRE::_texEnv(u32 a,...){
	va_list arg;

	va_start(arg,a);
	u32 b=va_arg(arg,u32);
	u32 c=va_arg(arg,u32);
	glTexEnvi(a,b,c);
	va_end(arg);
	return 0;
}

int GRE::_dither(u32 a,...){
	if(a)
		glEnable(GL_DITHER);
	else
		glDisable(GL_DITHER);
	return 0;
}

int GRE::_clear(u32 a){
	glClear(a);
	return 0;
}

int GRE::_clearColor(float* v){
	memcpy(_clear_color,v,sizeof(_clear_color));
	glClearColor(_clear_color[0],_clear_color[1],_clear_color[2],_clear_color[3]);
	return 0;
}

int GRE::_color(float* v){
	memcpy(_draw_color,v,4*sizeof(float));
	glColor4fv(_draw_color);
	return 0;
}

int GRE::_beginDraw(int m){
	glBegin(m);
	//glColor4fv(_draw_color);
	return 0;
}

int GRE::_endDraw(){
	glEnd();
	return 0;
}

int GRE::_drawVertex(float *v,float *t,float *n,float *c){
	if(c) _color(c);
	if(t) glTexCoord2f(t[0],t[1]);
	if(n) glNormal3fv(n);
#ifdef _DEVELOPa
	printf("%d %d\n",(s32)v[0],(s32)v[1]);
#endif
	glVertex3fv(v);
	_gpu_status |= GRE_STATUS_VERTEX_PUSH;
	return 0;
}

int GRE::MoveResize(int x,int y,int w,int h){
	if(GPU::MoveResize(x,y,w,h)) return -1;
#ifdef __WIN32__
	wglMakeCurrent(NULL,NULL);
	wglMakeCurrent(gui._getDC(),hRC);
#else
	glFinish();
	glXMakeCurrent(_display,0,0);
	glFinish();
	//XSync(_display,0);
	glXWaitX();
	glXWaitGL();
	glXMakeCurrent(_display,_window,hRC);
	//glFinish();
	//XSync(_display,0);
	//glXWaitX();
	//glXWaitGL();
#endif
	glViewport(0,0,w,h);
	return 0;
}

int GRE::_clearTextures(u32 flags){
	vector<GRETEXTURE> t;

	for(auto it=_textures.begin();it!=_textures.end();++it){
		if(flags & 1){
			if(++(*it)._used < 31){
				t.push_back(*it);
				continue;
			}
		}
		(*it)._delete();
	}
	_textures.clear();
	if(!(flags&1)) return 0;
	_textures.swap(t);
	return 0;
}

int GRE::_genList(u32 *a,int n){
	if(!a) return -1;
	*a=glGenLists(n);
	return 0;
}

int GRE::_newList(u32 a,u32 b){
	glNewList(a,b);
	return 0;
}

int GRE::_endList(u32 a){
	glEndList();
	return 0;
}

int GRE::_callList(u32 a,int n){
	while(--n) glCallList(a++);
	return 0;
}

int GRE::_delList(u32 i,int n){
	if(!i || !n)
		return -1;
	glDeleteLists(i,n);
	return 0;
}

int GRE::_beginScene(u32){
	if(_sceneList) return 0;
	_genList(&_sceneList);
	_newList(_sceneList,GL_COMPILE);
	return 0;
}

int GRE::_endScene(u32){
	_endList(_sceneList);
	return 0;
}

int GRE::_drawScene(u32){
	glPushAttrib(GL_ENABLE_BIT);
	_callList(_sceneList);
	glPopAttrib();
	return 0;
}

int GRE::_resetScene(u32){
	_endScene(_sceneList);
	_delList(_sceneList);
	_sceneList=0;
	return 0;
}

int GRE::__texture::_bind(){
	if(!index) return -1;
	glBindTexture(GL_TEXTURE_2D,index);
	//printf("bind %llx\n",id);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_CLAMP);
    _used=0;
	return 0;
}

int GRE::__texture::_delete(){
	if(index)
		glDeleteTextures(1,&index);
	index=0;
	id=0;
	_status=0;
	return 0;
}

int GRE::__texture::_create(void *buf,GRE &g){
	if(!buf)
		return -1;
	if(index)
		return 1;
	glGenTextures(1,&index);
	glBindTexture(GL_TEXTURE_2D,index);
	if(g._createTexture(buf,this))
		return -1;
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,(GLvoid *)g._texture);
	//printf("gentex %u %u\n",w,h);
	_used=0;
	return 0;
}