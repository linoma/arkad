#include "ps2gpu.h"
#include "ps2dev.h"
#include "machine.h"

namespace ps2{

static u32 *_ioreg;

#pragma pack(push,1)
typedef struct __ps2texid{
	union{
		u64 _value;
		struct{
			u64 _src:14;
			u64 _swidth:6;
			u64 _sfmt:6;
			u64 _width:4;
			u64 _height:4;
			u64 _alpha:1;
			u64 _env:2;
			u64 _clsrc:14;
			u64 _clfmt:4;
		};
	};
	__ps2texid(u64 a=0){_value=a;};
} PS2TEXID;
#pragma pack(pop)

struct __enviroment{
	union{
		u32 _control;
		struct{
			u32 _changed:1;
		};
	};
	struct{
		u32 _width,_fmt,_mask,_dst;
	} _fb;
	struct{
		u32 x,y,w,h,_x,_y,_value,_spos,_dpos,_dir,_bpp;

		union{
			u64 _control;
			struct{
				u64 _src:14;
				u64 _dummy0:2;
				u64 _swidth:6;
				u64 _dummy1:2;
				u64 _sfmt:6;
				u64 _dummy2:2;
				u64 _dst:14;
				u64 _dummy3:2;
				u64 _dwidth:6;
				u64 _dummy4:2;
				u64 _dfmt:6;
			};
		};

		union{
			u64 _size;
		};

		union{
			u32 _state;
			struct{
				u32 _init:1;
			};
		};

		int control(u64 v){
			_init=0;
			_control=v;
			return 0;
		};

		int start(u64 v){
			_init=0;
			_dir=v&3;
			return 0;
		}
		int pos(u64 v){
			_init=0;
			return 0;
		}

		int size(u64 v){
			_init=0;
			w=v&0x7ff;
			h=SR(v,32)&0x7ff;
			_size=v;
			return 0;
		};

		int write(u8 *m,u64 v){
			if(!_init){
				_x=x;_y=y;
				_dpos=SL(_dst+_x+(_y*_dwidth),6)*3;
				_spos=0;//SL(_src+_x+(_y*_swidth),6);
				switch(_dfmt){
					case 1://GS_PSM_24
						_bpp=24;
					break;
				}
				_init=1;
			}

			switch(_dir){
				case 0:
					for(int i=0;i<64;i+=8,v >>= 8){
						m[_dpos++]=(u8)v;
						if((_dpos & 3)==3) x++;
					}
					if(_x >= w){
						_x=x;
						_y++;
						if(_y>=h){
							printf("bitblt end\n");
						}
						_dpos=SL(_dst+_x+(_y*_dwidth),6)*3;
						//_spos=SL(_src+_x+(_y*_swidth),6);
					}
				break;
			}
			return 0;
		};

	} _blit;
} _env;

struct __vertex{
	float _f[11],q;

	union{
		s32 pos[3];
		struct{
			s32 x,y,z;
		};
	};
	union{
		u8 color[4];
		struct{
			u8 r,g,b,a;
		};
	};
	union{
		float st[2];
		struct{
			float u,v;
		};
	};

	__vertex(){
		memset(pos,0,sizeof(pos));
		memset(color,0,sizeof(color));
		memset(st,0,sizeof(st));
		q=1;
	};

	void reset(int v=0){
		memset(pos,v,sizeof(pos));
		memset(color,v,sizeof(color));
		memset(st,v,sizeof(st));
	};

	float *colorf(){
		return &_f[3];
	};

	float *posf(){
		return _f;
	};

	float *texf(){
		return &_f[7];
	};
};

struct __polygon : vector<__vertex>{
protected:
	__vertex _cr;
	float _q;
public:
	#pragma pack(push,1)
	union{
		u16 _value;
		struct{
			u32 _type:3;
			u32 _gourand:1;
			u32 _texture:1;
			u32 _fog:1;
			u32 _blend:1;
		};
	} prim;
	PS2TEXID tex0;
	union{
		u64 _value;
		struct{
			u64 _base:14;
		};
	} tex1;
	union{
		u64 _value;
		struct{
			u64 _base:14;
		};
	} tex2;
	#pragma pack(pop)

	int _dither,_prim_mode,_prim_type;
	s16 _xofs,_yofs;
	struct{
		int l,b,w,h;
	} _scissor;

	struct{
		u32 func,mask;
	} _z_test;

	struct{
		u32 func;
	} _alpha_test;

	union{
		u32 _control;
		struct{
			u32 _vtx:1;
			u32 _color:1;
			u32 _st:1;
		};
	};

	__polygon &operator = (u16 v){
		prim._value=v;
		clear();
		_cr.reset();
		return *this;
	};

	__vertex &xyz(u32 *f,int n=3){
		memcpy(_cr.pos,f,n*sizeof(s32));
		push_back(_cr);
		_vtx=1;
		return _cr;
	};

	__vertex &color(u8 *f,int n=4){
		memcpy(_cr.color,f,n*sizeof(u8));
		_color=1;
		return _cr;
	};

	__vertex &st(float *f,int n=2){
		memcpy(_cr.st,f,n*sizeof(float));
		_st=1;
		return _cr;
	};

	void q(float f){
		_cr.q=f;
	};

	int close(){
		u32 res=0;
		if(size()==0)
			return 0;
		switch(prim._type){
			case 3:
				res=GL_TRIANGLES;
			break;
			case 6:{
				vector<__vertex> vl=*this;
				clear();
				for(auto it=vl.begin();it!=vl.end();){
					__vertex v0 = (*it++);
					__vertex v1 = (*it++);

					push_back(v0);
					__vertex v=v0;
					v.x=v1.x;
					push_back(v);
					v=v1;
					push_back(v);
					v.x=v0.x;
					push_back(v);
				}
				res=GL_QUADS;
			}
			break;
			case 0:
			default:
			//	printf("prim unk %u\n",prim._type);
			break;
		}
		if(!res) return 0;
		for(auto it=begin();it!=end();it++){
			__vertex &v = *it;
			v._f[0]=(v.x-(u16)_xofs)/16.0f/320.0f - 1;
			v._f[1]=(v.y-(u16)_yofs)/16.0f/256.0f - 1;
			v._f[2]=(v.z/16.0f/128.0);
			v._f[3]=v.r/255.0f;
			v._f[4]=v.g/255.0f;
			v._f[5]=v.b/255.0f;
			v._f[6]=v.a/255.0f;
			if(_st){
				v._f[7]=v.st[0]/v.q;
				v._f[8]=v.st[1]/v.q;
			}
		}
		return res;
	};
} _polygon;

static vector<__polygon> _polygons;

ps2gpu::ps2gpu() : GRE(){
	_width=640;
	_height=480;
}

ps2gpu::~ps2gpu(){
}

int ps2gpu::Init(int,void *io,void *m,u32){
	if(GRE::Init(_width,_height))
		return -1;
	_ioreg=(u32 *)io;
	return 0;
}

int ps2gpu::Reset(){
	if(GRE::Reset())
		return -1;
	_clear_color[0]=0;
	_clear_color[1]=0;
	_clear_color[2]=0;
	_clear_color[3]=1;
	_clearColor(_clear_color);
	//_clearBits=GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;

	_enable(GL_TEXTURE_2D,0);
	_enable(GL_LIGHTING,0);
	_alphaTest(0);

	//glShadeModel(GL_SMOOTH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0,0,_width,_height);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	_polygons.clear();
	return 0;
}

int ps2gpu::Run(u8 *,int,void *){
	if(++__line == 312){
		__line=0;
		machine->OnEvent(ME_ENDFRAME,0);
	}
	return 0;
}

int ps2gpu::Update(u32 flags){
	u16 *dst;
	u32 *s;

	if(!(dst=(u16 *)_screen))
		goto Z;
	//PS2IOREG(0x12001000) |= 2;
	if(flags &1){
	for(auto it=_polygons.begin();it!=_polygons.end();it++){
		struct __polygon &poly = (*it);
		u32 m=poly.close();
		if(!m)
			continue;
		glShadeModel(poly.prim._gourand ? GL_SMOOTH : GL_FLAT);
		glDepthMask(poly._z_test.mask ? GL_TRUE:GL_FALSE);
		_depthTest(poly._z_test.func);
	//	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
		if(poly.prim._texture){
			_enable(GL_TEXTURE_2D,1);
			_bindTexture(poly.tex0._value,poly.tex1._value,poly.tex2._value);
			glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);
		}
		else
			_enable(GL_TEXTURE_2D,0);
		_beginDraw(m);
		//printf("%x %u %x %.3f %.3f\n",m,(u32)poly.size(),poly._z_test.func,poly._xofs/16.0f,poly._yofs/16.0f);
		m=0;
		for(auto i=poly.begin();i!=poly.end();i++){
			float *pf=(*i).posf();
			//float *pf=(*i).posf(0,0);
/*
			if(m<10){
				printf("%u %.3f %.3f %.3f\t\t%.3f %.3f %.3f %.3f\n",m++,pf[0],pf[1],pf[2],
				(*i).colorf()[0],(*i).colorf()[1],(*i).colorf()[2],(*i).colorf()[3]);
			}*/
			_drawVertex(pf,poly.prim._texture ? (*i).texf(): 0
				,0,(*i).colorf());
		}

		_endDraw();
	}
}
	if(GRE::Update(((flags & 1) ? GREUF_FLUSH : 0)|GREUF_NOTCLEAR))
		goto Y;

	s=(u32 *)_outBuffer;
	//dst=(u16 *)&((u8 *)_gpu_mem)[ofs];

	for(int y =0,yy=0;y<_displayHeight;y++,yy+=4096){
		int y0=(SR(yy,12)*_width);
		for(int x = 0,xx=0;x<_displayWidth;x++,xx+=4096){
			int r,g,b,a;
			u16 *p;

			u32 c = *s++;
			a=(u8)SR(c,24);
			a=255;
			if(a>0){
				b = (int)SR((u8)c* a,8);
				g = (int)SR((u8)SR(c,8)* a,8);
				r = (int)SR((u8)SR(c,16)* a,8);
			}
			else
				r=g=b=a=0;
			c=*(p=&dst[SR(xx,12)+y0]);
			/*a=SL(255-a,3);
		//	a=0;
			b += (int)SR((c&31) * a,8);
			g += (int)SR((c&0x3e0)* a,8);
			r += (int)SR((c&0x7c00)* a,8);
			*/
			*p = SR(b,3)|SL(SR(g,2),5)|SL(SR(r,3),11);
		}
	}
Y:
Z:
	GPU::Update();
	Draw(NULL);
	_polygons.clear();
	_polygon.clear();
	return 0;
}

int ps2gpu::write(u32 a,u64 v){
	switch((u8)a){
		case 1:{
			u8 f[5];

			f[0]=(u8)v;
			f[1]=(u8)(v>>8);
			f[2]=(u8)(v>>16);
			f[3]=(u8)(v>>24);
			_polygon.color(f);
			_polygon.q(*(float *)((s32 *)&v + 1));
			//printf("RGBAQ %llx\n",v);
		}
		break;
		case 2:{
			float f[2];

			*(s32 *)f=(u32)v;
			*(s32 *)&f[1]=(u32)SR(v,32);
			_polygon.st(f);
		}
		break;
		case 3:{

		}
		break;
		case 5:{
			u32 f[3];

			f[0]=(u16)v;
			f[1]=(u16)(v>>16);
			f[2]=(u32)(v>>32) / (float)BV(31-12-4);
		//	if(_polygon.prim._type==3)
		//	printf("X:%d Y:%d z:%u\tXYZ2 %016llx %.3f\n",f[0],f[1],f[2],v,(v>>32)/(float)BV(31-12-4));
			_polygon.xyz(f);
		}
		break;
		case 0x6:
		case 0x7:
			_polygon.tex0._value=v;
		//	printf("text0 %u %u %u %u\n",(v>>26)&15,(v>>30)&15,(v>>35)&3,(v>>14)&0x3f);
		break;
		case 0x14:
			_polygon.tex1._value=v;
		break;
		case 0x15:
			_polygon.tex2._value=v;
		break;
		case 0x18:
		case 0x19: {
			_polygon._xofs=(s16)v;
			_polygon._yofs=(s16)(v>>32);
			printf("os %08x %x %x\n",a,(s16)v,(s16)(v>>32));
		}
		break;
		case 0x1a:
			_polygon._prim_mode=(int)v;
		break;
		case 0x40:
		case 0x41:
		//	printf("%x %x %x %x\n",(v & 0x7ff),(v >> 16) & 0x7ff,(v>>32) & 0x7ff,(v>>48) & 0x7ff);
		break;
		case 0x4c:
		case 0x4d:{
			_env._changed=1;
			_env._fb._width=SL((v>>16)&63,6);
			printf("%x %08x %08x\n",a,(u32)(v>>16)&63,(u32)(v>>32));
		}
		break;
		case 0x1b:
			v=(v & ~7)|_polygon.prim._type;
		case 0:
		//	printf("GS PRIM 0%x %016llx %x\n",a,v,_polygon._z_test.func);
			if(_polygon.size()){
				_polygons.push_back(_polygon);
				_polygon.clear();
			}
			_polygon=(u16)v;
		break;
		case 0x45:
			_polygon._dither=(int)v;
		break;
		case 0x47:
		case 0x48:
			if(_polygon.size()){
				//memcpy(&_polygon._env,&_env,sizeof(__enviroment));
				_polygons.push_back(_polygon);
				_polygon.clear();
			}
			switch(SR(v,17) & 3){
				case 0:
					_polygon._z_test.func = GL_NEVER;
				break;
				case 1:
					_polygon._z_test.func = GL_ALWAYS;
				break;
				case 2:
					_polygon._z_test.func = GL_GEQUAL;
				break;
				case 3:
					_polygon._z_test.func = GL_GREATER;
				break;
			}
		break;
		case 0x4e:
		case 0x4f:
			_polygon._z_test.mask=(SR(v,32) ^ 1)&1;
		break;
		case 0x3b:
		break;
		case 0x3d://og color
		break;
		case 0x42://alpha
		break;
		case 0x44://dimx
		break;
		case 0x49:
		break;
		case 0x4a://fba
		break;
		case 0x50:
			_env._blit.control(v);
		break;
		case 0x51:
			_env._blit.pos(v);
		break;
		case 0x52:
			_env._blit.size(v);
		break;
		case 0x53:
			_env._blit.start(v);
		break;
		case 0x54:
			_env._blit.write((u8 *)_gpu_mem,v);
		break;
		default:
			printf("GS unk %x\n",a);
		break;
		case 0x60:
		case 0x62:
		printf("GS unk %x\n",a);
		break;
		case 0x61:
			PS2IOREG(0x12001000) |= 2;
			if(_polygon.size()){
				//memcpy(&_polygon._env,&_env,sizeof(__enviroment));
				_polygons.push_back(_polygon);
				_polygon.clear();
			}
		break;
	}
	return -1;
}

int ps2gpu::read(u32,u64 *){
	return -1;
}

int ps2gpu::_createTexture(void *buf,void *p,void *dst){
	int res;
	GRETEXTURE *tex;
	u32 *pixels;

	if(!(tex = (GRETEXTURE *)p)) return -1;
	if(!dst)
		dst=_texture;
	pixels=(u32 *)dst;
	res=-2;
	switch(tex->_mode){
		case 1:{
			u8 *s;

			s=(u8 *)buf;
			for(int y=0;y<tex->h;y++){
				for(int x = 0;x<tex->w;x++){
					u8 col[3];

					col[0]=s[x*3];col[1]=s[x*3+1];col[2]=s[x*3+2];
					*pixels++ = 0xff000000|RGB(col[0],col[1],col[2]);
				}
				s += tex->_stride*3;
			}
		//	printf("\n");
			res=0;
		}
		break;
	}
#ifdef _DEVELOP
	if(res==0)
		SaveTextureAsTGA((u8 *)dst,tex->w,tex->h,(u32)SR(tex->id,32));
#endif
	return res;
}

int ps2gpu::_bindTexture(u64 id,u64,u64){
A:
	for(auto it=_textures.begin();it != _textures.end();it++){
		if((*it).id==id){
			GRE::_bindTexture(&(*it));
			return 0;
		}
	}
	{
		GRETEXTURE tex;
		u32 b;
		PS2TEXID t(id);
		//printf("tex: %016llx %x %u %u %u %u %u\t",id,itex.page,itex.w,itex.h,itex.mode,itex.x,itex.y);

		tex.id=id;
		tex.w=BV(t._width);
		tex.h=BV(t._height);
		tex._stride=SL(t._swidth,6);
		tex._mode=t._sfmt;
		tex.x=0;
		tex.y=0;
		tex._scaled=0;
		tex._translated=0;

	//	printf(" %x\n",b*2);
		tex._create((u8 *)_gpu_mem + SL(id & 0x7fff,6)*3,*this);
		_textures.push_back(tex);
		goto A;
	}
Z:
	GRE::_bindTexture();
	return 1;
}

};