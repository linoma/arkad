#include "gpu.h"
#include <vector>
#include "ogl.h"

#ifndef __GREH__
#define __GREH__


#define GRE_DEPTH_TEST_OPTION	    0x00000100
#define GRE_KEEP_TRANSLUCENT		0x00000200
#define GRE_LIGHT0_OPTION		    0x00000400
#define GRE_LIGHT1_OPTION		    0x00000800
#define GRE_LIGHT2_OPTION		    0x00001000
#define GRE_LIGHT3_OPTION		    0x00002000
#define GRE_TEXTURE_OPTION		    0x00004000
#define GRE_BLEND_OPTION		    0x00008000
#define GRE_ALPHA_TEST_OPTION	    0x00010000
#define GRE_FOG_OPTION			    0x00020000
#define GRE_MULTISAMPLE_SUPPORTED	0x00040000
#define GRE_MULTISAMPLE_MASK       	0x000C0000
#define GRE_MULTISAMPLE_SHIFT      	18
#define GRE_ASYNC_READ_FLAG	    	0x00100000
#define GRE_ASYNC_READ_OPTION	    0x00200000
#define GRE_SYNC_SWAP_BUFFER		0x00400000
#define GRE_TEX_COMBINE_FLAG	    0x00800000
#define GRE_VERTEX_SHADER_FLAG     0x01000000
#define GRE_FRAGMENT_SHADER_FLAG   0x02000000
#define GRE_FOG_ALPHA_OPTION       0x04000000
#define GRE_EDGE_MARKING_OPTION    0x08000000
#define GRE_SHADERS_OPTION         0x10000000
#define GRE_FRAMEBUFFER_OBJ_FLAG   0x20000000
#define GRE_FRAMEBUFFER_OBJ_OPTION 0x40000000
#define GRE_FOGCOORD_FLAG			0x80000000
#define GRE_RENDER_WIREFRAME		0x100000000LLU
#define GRE_DONT_USE_LISTS			0x200000000LLU
#define GRE_RENDER_ONLY_POLYGONS	0x400000000LLU
#define GRE_DONT_USE_LIGHT			0x800000000LLU
#define GRE_USE_SOFT_LIGHT			0x1000000000LLU
#define GRE_USE_DIRECT_COMMAND		0x2000000000LLU

#define GRE_STATUS_SWAP_BUFFER		SL((u64)1,32)
#define GRE_STATUS_VERTEX_PUSH		SL((u64)1,31)
#define GRE_STATUS_TEXTURE_CREATE	SL((u64)1,30)

using namespace std;
struct __texture;

class GRE : public GPU{
public:
	GRE();
	virtual ~GRE();
	virtual int Init(int,int,int f=1,u32 ex=0);
	virtual int Destroy();
	virtual int Reset();
	virtual int Update(u32 flags=0);
	virtual int _createTexture(void *,void *,void *dst=0)=0;
	virtual int _bindTexture(void *p=NULL);
	virtual int _beginDraw(int);
	virtual int _endDraw();
	virtual int _drawVertex(float *v,float *t=NULL,float *n=NULL,float *c=NULL);
	virtual int _enable(u32 a,u32 b=1);
	virtual int _blend(u32 a=0,...);
	virtual int _alpha(u32 a=0,...);
	virtual int _texEnv(u32 a,...);
	virtual int _dither(u32 a =0,...);
	virtual int _clear(u32 a);
	virtual int _clearColor(float *);
	virtual int _color(float *);
	virtual int _genList(u32 *,int n=1);
	virtual int _newList(u32,u32);
	virtual int _endList(u32);
	virtual int _callList(u32,int n=1);
	virtual int _delList(u32,int n=1);

protected:
	virtual int _clearTextures(u32 flags=0);
	virtual int MoveResize(int,int,int,int);
	virtual int _beginScene(u32);
	virtual int _endScene(u32);
	virtual int _drawScene(u32);
	virtual int _resetScene(u32);

	float _clear_color[4],_draw_color[4];
	u8 *_outBuffer,*_texture;

	typedef struct __texture{
		u64 id;
		GLuint index;
		u16 w,h,_mode,_stride,x,y;
		union{
			struct{
				unsigned int _used:5;
				unsigned int _scaled:1;
				unsigned int _translated:1;
			};
			u32 _status;
		};

		__texture(){id=0;index=0;_status=0;};
		int _bind();
		int _delete();
		int _create(void *,GRE &);
	} GRETEXTURE;

	template<typename T,int c> struct __vector{
		T _data[c];

		__vector(){
			reset();
		}

		void reset(){
			memset(_data,0,c*sizeof(T));
		}

		T* ptr(){return _data;};
		T &dot(struct __vector<T,c> v) const{
			T r=_data[0]*v[0];
			r+=_data[1]*v[1];
			r+=_data[2]*v[2];
			return r;
		}

		struct __vector<T,c>&cross(struct __vector<T,c> v) const{
			return *this;
		}
		struct __vector<T,c>& operator+=(struct __vector<T,c> v) const{
			return *this;
		}
		struct __vector<T,c>& operator+(struct __vector<T,c> v) const{
			return *this;
		}
		struct __vector<T,c>& operator-=(struct __vector<T,c> v) const{
			return *this;
		}
		struct __vector<T,c>& operator-(struct __vector<T,c> v) const{
			return *this;
		}
		const T& operator[](const u32 k) const{ return _data[k]; };
		T& operator=(const T& other){
			if (this == &other)
				return *this;

			return *this;
		}
	};

	template<typename T,int r,int c> struct __matrix{
		int _n;
		T _data[r*c];

		__matrix(){
			_n=min(r,c);
		}

		void reset(){
			memset(_data,0,r*c*sizeof(T));
		}

		void identity(){
			reset();
			for(int i=0,ii=0;i<_n;i++,ii+=c)
				_data[ii+i]=1.0f;
		}

		__matrix<T,r,c> & load(T *p){
			memcpy(_data,p,r*c*sizeof(T));
			return *this;
		}

		__matrix<T,r,c> & multiply(T *m1){
			T m2[r*c];

			memcpy(m2,_data,sizeof(T)*r*c);


 /*  m[0] = m1[0]*m2[0] + m1[1]*m2[4] + m1[2]*m2[8] + m1[3]*m2[12];
   m[1] = m1[0]*m2[1] + m1[1]*m2[5] + m1[2]*m2[9] + m1[3]*m2[13];
   m[2] = m1[0]*m2[2] + m1[1]*m2[6] + m1[2]*m2[10] + m1[3]*m2[14];
   m[3] = m1[0]*m2[3] + m1[1]*m2[7] + m1[2]*m2[11] + m1[3]*m2[15];

   m[4] = m1[4]*m2[0] + m1[5]*m2[4] + m1[6]*m2[8] + m1[7]*m2[12];
   m[5] = m1[4]*m2[1] + m1[5]*m2[5] + m1[6]*m2[9] + m1[7]*m2[13];
   m[6] = m1[4]*m2[2] + m1[5]*m2[6] + m1[6]*m2[10] + m1[7]*m2[14];
   m[7] = m1[4]*m2[3] + m1[5]*m2[7] + m1[6]*m2[11] + m1[7]*m2[15];

   m[8] = m1[8]*m2[0] + m1[9]*m2[4] + m1[10]*m2[8] + m1[11]*m2[12];
   m[9] = m1[8]*m2[1] + m1[9]*m2[5] + m1[10]*m2[9] + m1[11]*m2[13];
   m[10] = m1[8]*m2[2] + m1[9]*m2[6] + m1[10]*m2[10] + m1[11]*m2[14];
   m[11] = m1[8]*m2[3] + m1[9]*m2[7] + m1[10]*m2[11] + m1[11]*m2[15];

   m[12] = m1[12]*m2[0] + m1[13]*m2[4] + m1[14]*m2[8] + m1[15]*m2[12];
   m[13] = m1[12]*m2[1] + m1[13]*m2[5] + m1[14]*m2[9] + m1[15]*m2[13];
   m[14] = m1[12]*m2[2] + m1[13]*m2[6] + m1[14]*m2[10] + m1[15]*m2[14];
   m[15] = m1[12]*m2[3] + m1[13]*m2[7] + m1[14]*m2[11] + m1[15]*m2[15];*/
			return *this;
		}

		T* ptr(){return _data;};
	};

	struct __matrix<float,4,4> _mtx[3];
	GRETEXTURE *_cr_texture;
	vector<GRETEXTURE> _textures;

	u64 _engine_features;
	GLuint _progShader,_clearBits,_sceneList;
private:
#ifdef __WIN32__
	HGLRC hRC;
#else
	GLXContext hRC;
	Display *_display;
	Window _window;
#endif

	friend class GPU;
};

#endif