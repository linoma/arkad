#include "arkad.h"

#ifdef __WIN32__
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include <GL/wglext.h>
#else
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
	#include <X11/Xatom.h>
	#include <X11/keysym.h>
	#include <GL/gl.h>
	#include <GL/glx.h>
	#include <GL/glext.h>
	#include <gdk/gdkx.h>
#endif

#ifndef _OGLH__
#define _OGLH__

extern PFNGLBINDBUFFERPROC                    pfn_glBindBuffer;
extern PFNGLDELETEBUFFERSPROC                 pfn_glDeleteBuffers;
extern PFNGLGENBUFFERSPROC                    pfn_glGenBuffers;
extern PFNGLBUFFERDATAPROC                    pfn_glBufferData;
extern PFNGLMAPBUFFERPROC                     pfn_glMapBuffer;
extern PFNGLUNMAPBUFFERPROC                   pfn_glUnmapBuffer;
extern PFNGLISBUFFERPROC                      pfn_glIsBuffer;
extern PFNGLCREATESHADERPROC                  pfn_glCreateShader;
extern PFNGLCOMPILESHADERPROC                 pfn_glCompileShader;
extern PFNGLSHADERSOURCEPROC                  pfn_glShaderSource;
extern PFNGLATTACHSHADERPROC                  pfn_glAttachShader;
extern PFNGLCREATEPROGRAMPROC                 pfn_glCreateProgram;
extern PFNGLLINKPROGRAMPROC                   pfn_glLinkProgram;
extern PFNGLUSEPROGRAMPROC                    pfn_glUseProgram;
extern PFNGLDELETESHADERPROC                  pfn_glDeleteShader;
extern PFNGLDELETEPROGRAMPROC                 pfn_glDeleteProgram;
extern PFNGLUNIFORM1IVPROC                    pfn_glUniform1iv;
extern PFNGLGETUNIFORMLOCATIONPROC            pfn_glGetUniformLocation;
extern PFNGLUNIFORM1FVPROC                    pfn_glUniform1fv;
extern PFNGLUNIFORM1IPROC                     pfn_glUniform1i;
extern PFNGLACTIVETEXTUREPROC                 pfn_glActiveTexture;
extern PFNGLBINDFRAMEBUFFEREXTPROC            pfn_glBindFramebufferEXT;
extern PFNGLGENFRAMEBUFFERSEXTPROC            pfn_glGenFramebuffersEXT;
extern PFNGLDRAWBUFFERSPROC                   pfn_glDrawBuffers;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC    pfn_glFramebufferRenderbufferEXT;
extern PFNGLGENRENDERBUFFERSEXTPROC           pfn_glGenRenderbuffersEXT;
extern PFNGLBINDFRAMEBUFFEREXTPROC            pfn_glBindRenderbufferEXT;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC        pfn_glRenderbufferStorageEXT;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC       pfn_glFramebufferTexture2DEXT;
extern PFNGLGETSHADERIVPROC                   pfn_glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC              pfn_glGetShaderInfoLog;
extern PFNGLGETPROGRAMIVPROC                  pfn_glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC             pfn_glGetProgramInfoLog;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC     pfn_glCheckFramebufferStatusEXT;
extern PFNGLDELETERENDERBUFFERSEXTPROC        pfn_glDeleteRenderbuffersEXT;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC         pfn_glDeleteFramebuffersEXT;
extern PFNGLFOGCOORDFPROC 						pfn_glFogCoordf;
extern PFNGLBLENDFUNCSEPARATEPROC 				pfn_glBlendFuncSeparate;;

int IsExtensionSupported(const char *ext,char *supported);
int printShaderInfoLog(GLuint obj);
int checkFramebufferStatus();
u64 init_gl_extension();
int compileShader(char *v,char *f,GLuint *out);

#ifndef __WIN32__
	#define wglGetProcAddress(a) glXGetProcAddress((GLubyte *)a)
#endif

class OGL{
public:
	OGL();
	virtual ~OGL();
protected:
};

#endif