#include "dcdev.h"

namespace dc{

char _vertex_shader[]={"varying vec4 VertexColor,Vertex;\
	void main(){\nVertex = ftransform();\
	gl_Position=vec4(Vertex.xy,1,1);\
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\
	VertexColor = gl_Color;\
	gl_FrontColor = VertexColor;\
	}"};
char _fragment_shader[]={"varying vec4 VertexColor,Vertex;\
	void main(){\
	gl_FragDepth=Vertex.z;\
	gl_FragData[0] = VertexColor;\
	}"};

struct : vector<u32>{
	union{
		u32 _status;
		struct{
			unsigned int _changed:1;
			unsigned int _vblank:1;
			unsigned int _render:1;
		};
	};
	union{
		u32 _control;
		struct{
			unsigned int _enable:1;
			unsigned int _pf:2;
			unsigned int _pdb:1;
		};
	};
	u32 _cycles,_line_cycles,_lines,_stride,_width,_height;
	u32 _xstart,_ystart;
	float _zfar;
	struct{
		u32 _left,_right,_top,_bottom;
		float _depth,_color[4];
	} _bg;

	int reset(){
		_status=0;
		_cycles=0;
		_lines=512;
		_line_cycles=0;
		_control=0;
		_width=640;
		_height=480;
		_ta.reset();
		_zfar=0;
		clear();
		return 0;
	};

	struct : vector<u32>{
		u32 _len;

		enum {
			DISPLAY_LIST_OPAQUE,DISPLAY_LIST_OPAQUE_MOD,
			DISPLAY_LIST_TRANS,	DISPLAY_LIST_TRANS_MOD,
			DISPLAY_LIST_PUNCH_THROUGH,DISPLAY_LIST_LAST,DISPLAY_LIST_COUNT,
			DISPLAY_LIST_NONE = -1};

		struct : vector<u32>{
			int _type,_begin,_strip;

			int _new(int a,int b){
				_type=a;
				_strip=b;
				_begin=0;
				clear();
				return 0;
			};

			int _end(){
				printf("strip end %u\n",(u32)size());
				clear();
				return 0;
			};
		} _list;

		int reset(){
			clear();
			_len=7;
			_list._type=-1;
			_list.clear();
			return 0;
		};

		int parse(){
			HEADER cmd;
			int res;

			res=1;
			cmd._value=at(0);
			switch(cmd._type){
				case 0:
					if(_list._type >= 0){
						printf("list type %d\n",_list._type);
						machine->OnEvent(7+_list._type,1);
						_list._type=-1;
						_list._begin=0;
					}
					break;
				case 1: case 2: case 3:
				break;
				case 4: case 5: case 6:
					_list._new(cmd._list_type,cmd._type);
				/*	_list.push_back(at(0));
					_list.push_back(at(1));
					_list.push_back(at(2));
					_list.push_back(at(3));*/
					res=1;
				break;
				case 7:
					if(!_list._begin){
						if(!cmd._end_strip)
							_list._begin=1;
					}
					switch(_list._strip){
						case 4:
							_list.push_back(at(0));
							_list.push_back(at(1));
							_list.push_back(at(2));
							_list.push_back(at(3));
							_list.push_back(at(4));
							_list.push_back(at(5));
							_list.push_back(at(6));
							_list.push_back(at(7));
						break;
						case 5:
						break;
					}
					if(_list._begin){
						if(cmd._end_strip || _list.size()==32)
							res=0;
					}
				break;
				default:
				break;
			}
			Z:
			_len=7;
			clear();
			return res;
		};

		#pragma pack(push,1)
		typedef struct{
			union{
				u32 _value;
				struct{
					union{
						u16 _object;
						struct{
							unsigned int _uv:1;
							unsigned int _gouraud:1;
							unsigned int _color:1;
							unsigned int _texture:1;
							unsigned int _color_type:2;
							unsigned int _volume:1;
							unsigned int _shadow:1;
						};
					};
					union{
						u8 _group;
						struct{
							unsigned int _user_clip:2;
							unsigned int _strip_len:2;
							unsigned int _en:1;
						};
					};
					union{
						u8 _para;
						struct{
							unsigned int _list_type:3;
							unsigned int _para0:1;
							unsigned int _end_strip:1;
							unsigned int _type:3;
						};
					};
				};
			};
		} HEADER;
		#pragma pack(pop)
	} _ta;

} _gpu;

PVR2::PVR2() : GRE(){
	_width=640;
	_height=480;
}//8c52f410

PVR2::~PVR2(){
}

int PVR2::Init(int,void *a,void *b,u32){
	_ioreg=(u8 *)b;
	//_mem=(u8 *)a;
	if(GRE::Init(_width,_height))
		return -1;
	compileShader(_vertex_shader,_fragment_shader,&_progShader);
	//printf("%p\n",&_binary_vtxshr_bin_size);
	pfn_glUseProgram(_progShader);
	return 0;
}

int PVR2::Reset(){
	PVR2_REG(PVR2_ID) = 0x17fd11db;
	PVR2_REG(PVR2_REVISION) = 0x11;
	PVR2_REG(PVR2_SPG_VBLANK_INT) = 0x01500104;
	PVR2_REG(PVR2_SPG_LOAD) = 0x01060359;
	_gpu.reset();
	if(GRE::Reset())
		return -1;
	_gpu._changed=1;

	_clear_color[0]=1;
	_clear_color[1]=1;
	_clear_color[2]=1;
	_clear_color[3]=1;
	_clearColor(_clear_color);
	//_clearBits=GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;

	_enable(GL_TEXTURE_2D,0);
	_enable(GL_LIGHTING,0);

	/*_env._reset(_width,_height,_displayWidth,_displayHeight);
	_env._display._fb=(u16*)_gpu_mem;
	_env._draw._fb=(u16*)_gpu_mem;

	if(_progShader){
		GLuint i1 = pfn_glGetUniformLocation(_progShader,"tex");
		pfn_glUniform1i(i1,0);
		_iublend = pfn_glGetUniformLocation(_progShader,"blend");
	}*/
	//glDisable(GL_TEXTURE_2D);
	_blend(0,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA);
	//_alpha(0,GL_GREATER,0);

	//glPolygonMode(GL_FRONT, GL_FILL);
	//glPolygonMode(GL_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0,0,_width,_height);

	return 0;
}

int PVR2::Run(u8 *,int cyc,void *){
	u32 r;

	if(_gpu._changed){
		r=PVR2_REG(PVR2_SPG_LOAD);
		_gpu._line_cycles=MHZ(200)*((r&0x3ff)+1)/MHZ(27);
		_gpu._lines=((r >> 16) & 0x3ff)+1;
		_gpu._pf=SR(PVR2_REG(PVR2_FB_R_CTRL),2);
		_gpu._stride=PVR2_REG(PVR2_FB_W_LINESTRIDE);
		_gpu._width =SL((PVR2_REG(PVR2_FB_R_SIZE) & 0x3ff)+1,1);
		_gpu._height = 480;//SL((SR(PVR2_REG(PVR2_FB_R_SIZE),10) & 0x3ff) + 1,1);
		_gpu._enable = PVR2_REG(PVR2_FB_R_SIZE);
		printf("PR2 %x %u %u %u %u %u %x\n",r,_gpu._lines,_gpu._line_cycles,_gpu._stride,
			_gpu._width,_gpu._height,PVR2_REG(PVR2_FB_R_SOF1));
		_gpu._changed=0;
	}
	_gpu._cycles += cyc;
	if(_gpu._cycles < _gpu._line_cycles || _gpu._line_cycles==0)
		return 0;
	_gpu._cycles -= cyc;
	if(++__line >=_gpu._lines){
		if(_gpu._render){
			machine->OnEvent(2,1);
			_gpu._render=0;
		}
		__line=0;
		machine->OnEvent(ME_ENDFRAME,0);
	}
	PVR2_REG(PVR2_SPG_STATUS)=__line;
	r=PVR2_REG(PVR2_SPG_VBLANK_INT);
	if(__line == (r&0x3ff))
		return 3;
	if(__line == ((r>>16)&0x3ff))
		return 4;
	return 0;
}

int PVR2::Update(u32 flags){
	u32 ofs,*s;
	u16 *dst,*src,*p;

	if(_gpu._changed){
		_gpu._pf=SR(PVR2_REG(PVR2_FB_R_CTRL),2);
		_gpu._stride=PVR2_REG(PVR2_FB_W_LINESTRIDE);
		_gpu._enable = PVR2_REG(PVR2_FB_R_SIZE);
		_gpu._width =SL((PVR2_REG(PVR2_FB_R_SIZE) & 0x3ff)+1,1);
		_gpu._height = SL((SR(PVR2_REG(PVR2_FB_R_SIZE),10) & 0x3ff) + 1,1);
		_gpu._xstart=PVR2_REG(PVR2_VO_STARTX) & 0x3ff;
		_gpu._changed=0;
	}
	if(!(dst=(u16 *)_screen))
		goto Z;
	ofs=PVR2_REG(PVR2_FB_R_SOF1);
	if(GRE::Update(1))
		goto Y;

	s=(u32 *)_outBuffer;
	dst=(u16 *)&((u8 *)_gpu_mem)[ofs];

	for(int y =0,yy=0;y<_displayHeight;y++,yy+=4096){
		int y0=(SR(yy,12)*_width);
		for(int x = 0,xx=0;x<_displayWidth;x++,xx+=4096){
			int r,g,b,a;
			u16 *p;

			u32 c = *s++;
			a=(u8)SR(c,24);
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
V:

Y:
	src=(u16 *)&((u8 *)_gpu_mem)[ofs];
	dst=(u16 *)_screen;
	for(int y=0;y<_height;y++){
		p=dst;
		for(int x=0;x<_width;x++){
			int c;

			c=src[x];
			u32 b = (c & 0x001f);
			u32 g = (c & 0x07e0) >> 6;
			u32 r = (c & 0xf800) >> 11;
			*p++=b|(g<<5)|(r<<10);
		}
		src += _width;
		dst += _width;
	}
Z:
	GPU::Update();
	Draw(NULL);
	//_gpu_status |= PS1_STATUS_MUSTCLEAR;
	return 0;
}

int PVR2::write(u32 a,u32 v){
	//printf("PVR2::write %x %x\n",a,v);
	switch((a-PVR2_BASE)){
		case PVR2_TA_LIST_INIT:
			PVR2_REG(PVR2_TA_ITP_CURRENT)=PVR2_REG(PVR2_TA_ISP_BASE);
			for(auto it=_gpu.begin();it!=_gpu.end();it++)
				_delList((*it),1);
			_gpu.clear();
		break;
		case PVR2_STARTRENDER:
			//printf("PVR2::swapbuffer %x %x %lu %x %.3f\n",a,v,_gpu.size(),_gpu.at(0),_gpu._zfar);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
		//	glViewport(0,0,640,480);
			glOrtho(_gpu._bg._left,_gpu._bg._right,_gpu._bg._top,_gpu._bg._bottom,-1,1);
			//glOrtho(-320,320,-240,240,-1,1);
		//	glScalef(3,3,0);
			//glTranslatef(320,0,0);
		//	glEnable(GL_CULL_FACE);
		//	glCullFace(GL_BACK);
			glFrontFace(GL_CW);
			for(auto it=_gpu.begin();it!=_gpu.end();it++)
				_callList((*it));

			/*glBegin(GL_QUADS);
			glColor3f(639.85 ,239.98, 0.03);
			glColor3f(639.85 ,239.98, 0.03);
			glColor3f(639.87 ,239.98, 0.04);
			glColor3f(639.87 ,239.98, 0.04);
			glEnd();
*/
			 /*639,85 239,98 0,03	639,85 239,98 0,03	639,87 239,98 0,04	639,87 239,98 0,04
			 639,85 239,98 0,03	639,85 239,98 0,03	639,87 239,98 0,04	639,87 239,98 0,04
			 639,85 239,98 0,03	639,85 239,98 0,03	639,87 239,98 0,04	639,87 239,98 0,04
			 639,85 239,98 0,03	639,85 239,98 0,03	639,87 239,98 0,04	639,87 239,98 0,04
			 639,85 239,98 0,03	639,85 239,98 0,03	639,87 239,98 0,04	639,87 239,98 0,04

			/*glBegin(GL_QUADS);
			glColor3f(0,0,1);
			glVertex3f(0,0,0.5);
			glVertex3f(640,0,0.5);
			glVertex3f(640,480,0.5);
			glVertex3f(0,480,0.5);
			glEnd();
			glBegin(GL_QUADS);
			glColor3f(1,0,0);
			glVertex3f(0,0,0);
			glVertex3f(320,0,0);
			glVertex3f(320,240,0);
			glVertex3f(0,240,0);
			glEnd();
			glBegin(GL_QUADS);
			glColor3f(0,1,0);
			glVertex3f(0,0,0);
			glVertex3f(-160,0,0);
			glVertex3f(-160,-240,0);
			glVertex3f(0,-240,0);
			glEnd();*/
			_gpu._render=1;
		break;
		case PVR2_ISP_BACKGND_D:
			_gpu._bg._depth=*(float *)&v;
		break;
		case PVR2_FB_X_CLIP:
			_gpu._bg._left=(u16)v;
			_gpu._bg._right=(u16)SR(v,16);
		break;
		case PVR2_FB_Y_CLIP:
			_gpu._bg._top=(u16)v;
			_gpu._bg._bottom=(u16)SR(v,16);
		break;
		case PVR2_SPG_VBLANK_INT:
			printf("PVR2_SPG_VBLANK_INT::write %x %x\n",a,v);
		case PVR2_SPG_LOAD:
			_gpu._changed=1;
		break;
		case PVR2_FB_R_CTRL:
		case PVR2_FB_W_CTRL:
		case PVR2_FB_R_SIZE:
		case PVR2_FB_W_LINESTRIDE:
		case PVR2_FB_R_SOF1:
		case PVR2_FB_R_SOF2:
			_gpu._changed=1;
		//	printf("PVR2::write %x %x\n",a,v);
		break;
		case 0x10000000-PVR2_BASE:
			_gpu._ta.push_back(v);
			printf("%x ",v);
			fflush(stdout);

			if((_gpu._ta.size() & _gpu._ta._len)==0){
				switch(_gpu._ta.parse()){
					case 0:{

				/*		for(auto it=_gpu._ta._list.begin();it!=_gpu._ta._list.end();it++)
							printf("%x ",(*it));
						printf("\n %.2f %.2f %.2f\n",*(float *)&_gpu._ta._list.at(5),
							*(float *)&_gpu._ta._list.at(6),*(float *)&_gpu._ta._list.at(7));*/

						{
							float fv[3],cv[4],ccv[]={1,1,1,1};
							u32 c,v;

							if(_gpu._ta._list.size()==32){
								c=_gpu._ta._list.at(6);
								_genList(&v);
								_newList(v,GL_COMPILE);
//_enable(GL_DEPTH_TEST,1);
								_beginDraw(GL_TRIANGLE_STRIP);

								fv[0]=*(float *)&_gpu._ta._list.at(1);
								fv[1]=*(float *)&_gpu._ta._list.at(2);
								fv[2]=*(float *)&_gpu._ta._list.at(3);
								if(fv[2] > _gpu._zfar) _gpu._zfar=fv[2];

								cv[0]= (u8)SR(c,16) /255.0f;
								cv[1]= (u8)SR(c,8) /255.0f;
								cv[2]= (u8)c /255.0f;
								cv[3]= (u8)SR(c,24) /255.0f;
								_drawVertex(fv,0,0,cv);
								printf("%.2f %.2f %.2f\t",fv[0],fv[1],fv[2]);
								fv[0]=*(float *)&_gpu._ta._list.at(9);
								fv[1]=*(float *)&_gpu._ta._list.at(10);
								fv[2]=*(float *)&_gpu._ta._list.at(11);
								c=_gpu._ta._list.at(14);
								cv[0]= (u8)SR(c,16) /255.0f;
								cv[1]= (u8)SR(c,8) /255.0f;
								cv[2]= (u8)c /255.0f;
								cv[3]= (u8)SR(c,24) /255.0f;
								_drawVertex(fv,0,0,cv);
								printf("%.2f %.2f %.2f\t",fv[0],fv[1],fv[2]);
								fv[0]=*(float *)&_gpu._ta._list.at(17);
								fv[1]=*(float *)&_gpu._ta._list.at(18);
								fv[2]=*(float *)&_gpu._ta._list.at(19);
								c=_gpu._ta._list.at(22);
								cv[0]= (u8)SR(c,16) /255.0f;
								cv[1]= (u8)SR(c,8) /255.0f;
								cv[2]= (u8)c /255.0f;
								cv[3]= (u8)SR(c,24) /255.0f;
								_drawVertex(fv,0,0,cv);
								printf("%.2f %.2f %.2f\t",fv[0],fv[1],fv[2]);
								fv[0]=*(float *)&_gpu._ta._list.at(25);
								fv[1]=*(float *)&_gpu._ta._list.at(26);
								fv[2]=*(float *)&_gpu._ta._list.at(27);
								c=_gpu._ta._list.at(30);
								cv[0]= (u8)SR(c,16) /255.0f;
								cv[1]= (u8)SR(c,8) /255.0f;
								cv[2]= (u8)c /255.0f;
								cv[3]= (u8)SR(c,24) /255.0f;
								_drawVertex(fv,0,0,cv);
								printf("%.2f %.2f %.2f\n",fv[0],fv[1],fv[2]);
								_endDraw();

								_endList(v);
								_gpu.push_back(v);
							}
							_gpu._ta._list._end();
						}
					}
					break;
				}
			}
			if(_gpu._ta.size()==0)
				printf("\n");
		break;
		default:
		//printf("PVR2::write %x %x\n",a,v);
			break;
	}
	return 1;
}

int PVR2::read(u32,u32 *){
	return 0;
}

int PVR2::_createTexture(void *buf,void *p,void *dst){
	int res;
	GRETEXTURE *tex;
	u32 *pixels;

	if(!(tex = (GRETEXTURE *)p)) return -1;
	if(!dst)
		dst=_texture;
	pixels=(u32 *)dst;
	res=-2;
	return res;
}

};