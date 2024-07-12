#include "ps1m.h"
#include "utils.h"


namespace ps1{

extern "C" char _binary_fragshr_bin_start[];
extern "C" char _binary_vtxshr_bin_start[];
extern "C"  volatile u8 _binary_vtxshr_bin_size;

#define PS1GPUREG(a) PS1IOREG_(0x1f801800|((u8)a),_gpu_regs)
//#define PS1VTX(a) ((int)((a) & 0x7ff))-SL((a)&0x400,1)
#define PS1VTX(a) EX_SIGN(a,11)

/*
 5 	tex-page
 14 clut
 3 	type
 10 width
 9 	height
 10	X
 9	Y
*/

#define PS1GPUOK(a,b) PS1GPUREG(0x1f801814) |= BV(26);
#define PS1TEXID(a,b,m,x,y,w,h) (SL((u64)(a&0x1f),58)|SL((u64)(b&0x7fff),41)|SL((u64)(m&7),38)|SL((u64)(x&0x3ff),28)|SL(y&0x1ff,19)|SL(w&0x3ff,9)|(h&0x1ff))

typedef union  __ps1texid{
	u64 id;
	struct{
		unsigned int h:9;
		unsigned int w:10;
		unsigned int y:9;
		unsigned int x:10;
		unsigned int mode:3;
		unsigned int clut:15;
		unsigned int o_:2;
		unsigned int page:5;
	};
	__ps1texid(u64 a=0){id=a;};
	__ps1texid(u8 a,u16 b){
		id=0;
		page=a;
		mode=b;
	};
} PS1TEXIDD;

#define PS1_SEMI_TRSPARENT	1
#define PS1_TEXTURED		2
#define PS1_BLEND			4
#define PS1_POLY_SHADED		8
#define PS1_POLY_THREE		0x10
#define PS1_RAW_TEXTURE		0x20

#define PS1_STATUS_MUSTCLEAR	SL((u64)1,24)

//640x480 60Hz
PS1GPU::PS1GPU() : GRE(){
	_width=320;
	_height=240;
}

PS1GPU::~PS1GPU(){
}

int PS1GPU::Init(){
	if(GRE::Init(_width,_height))
		return -1;
//	compileShader(_binary_vtxshr_bin_start,_binary_fragshr_bin_start,&_progShader);
	//printf("%p\n",&_binary_vtxshr_bin_size);
	//pfn_glUseProgram(_progShader);
	return 0;
}

int PS1GPU::Reset(){
	PS1GPUREG(0x14) = BV(28)|BV(26);
	//_env._draw._w=640;
	//_env._draw._h=480;
	//_env._display._w=_width;
	_fifo._reset();
	_cycles=0;

	if(GRE::Reset())
		return -1;

	_clear_color[0]=1;
	_clear_color[1]=1;
	_clear_color[2]=1;
	_clear_color[3]=.49f;
	_clearColor(_clear_color);
	//_clearBits=GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;

	_enable(GL_TEXTURE_2D,11);
	_enable(GL_LIGHTING,0);
	_env._reset(_width,_height,_displayWidth,_displayHeight);
	_env._display._fb=(u16*)_gpu_mem;
	_env._draw._fb=(u16*)_gpu_mem;
	if(_progShader){
		GLuint i1 = pfn_glGetUniformLocation(_progShader,"tex");
		pfn_glUniform1i(i1,0);
		_iublend = pfn_glGetUniformLocation(_progShader,"blend");
	}
	//glDisable(GL_TEXTURE_2D);
	//_blend(0,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA);
	//_alpha(0,GL_GREATER,0);
	glPolygonMode(GL_FRONT, GL_FILL);
	glPolygonMode(GL_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	return 0;
}

int PS1GPU::Run(u8 *,int cyc,void *obj){
	if((_cycles += cyc) < 2128)
		return 0;
	for(;_cycles>=2128;_cycles -= 2128) {
		if(++__line == 240){
			PS1GPUREG(0x14) &=~BV(31);
			return 1;
		}
	}
	if(__line >= 314){
		machine->OnEvent(ME_ENDFRAME,0);

PS1GPUREG(0x14) |= BV(31);
		__line=0;
		_cycles=0;
	}
	return 0;
}

int PS1GPU::Update(u32 flags){
	u16 *d;
	u32 *s;

	if(GRE::Update(flags)) goto Y;
V:
	if(!_screen)
		goto Z;
//	if(!(flags &1))
//goto Y;
	//_env._draw._fb=_env._display._fb=(u16 *)_gpu_mem;
	//DLOG("GPU update");
	d=(u16 *)_env._draw._fb;
	s=(u32 *)_outBuffer;

	//SaveTextureAsTGA((u8 *)_outBuffer,_displayWidth,_displayHeight,__frame);

	for(int y =0,yy=0;y<_displayHeight;y++,yy+=_env._draw._y_inc){
		int y0=(SR(yy,12)*1024);
		for(int x = 0,xx=0;x<_displayWidth;x++,xx+=_env._draw._x_inc){
			int r,g,b,a;
			u16 *p;

			u32 c = *s++;
			a=(u8)SR(c,24);
		//	if((a ==  0) continue;
			//printf("%x ",(u8)SR(c,24));
			if(a>127){
				b = (int)SR((u8)c* a,8);
				g = (int)SR((u8)SR(c,8)* a,8);
				r = (int)SR((u8)SR(c,16)* a,8);
			}
			else
				r=g=b=a=0;
AA:
			c=*(p=&d[SR(xx,12)+y0]);

			a=SL(255-a,3);
		//	a=0;
			b += (int)SR((c&31) * a,8);
			g += (int)SR((c&0x3e0)* a,8);
			r += (int)SR((c&0x7c00)* a,8);
			*p = SR(b,3)|SL(SR(g,3),5)|SL(SR(r,3),10);
		}
	}

Y:
	d=(u16 *)_screen;
	s=(u32 *)_env._display._fb;
	//_env._display._mode&=~0x10;
	switch(_env._display._mode&0x10){
		default:
			for(int y =0;y<_height;y++,s+=512){
				u8 *p=(u8 *)s;
				for(int x=0;x<_width;x++){
					u8 r=*p++;
					u8 g=*p++;
					u8 b=*p++;
					*d++=RGB555(SR(r,3),SR(g,3),SR(b,3));
				}
			}
		break;
		case 0:
			for(int y =0,yy=0;y<_height;y++,yy+=_env._display._y_inc){
				u16 *p= (u16 *)&s[SL(SR(yy,12),9)];
				for(int x=0,xx=0;x<_width;x++,xx+=_env._display._x_inc){
					u32 c = p[SR(xx,12)];
					*d++= c;
				}
			}
		break;
	}
Z:
	GPU::Update();
	Draw(NULL);
	//_gpu_status |= PS1_STATUS_MUSTCLEAR;
	return 0;
}

int PS1GPU::read(u32 a,u32 *b){
	switch((u8)a){
		case 0x14:
			*b = PS1IOREG_(0x1f801814,_gpu_regs);
		break;
	}
	return 0;
}

int PS1GPU::write(u32 a,u32 b){
	//DLOG("GPU W %x %08x",a,b);
	//printf("GPu %1x %08x\n",a,b);
	switch((u8)a){
		case 0x10://GP0 Graphics ontrol
			if(!_fifo._command){
				PS1GPUREG(0x14) &= ~BV(28);
				//if(b) printf("GPu %1x %08x\n",a,b);
				switch(SR(b,24)){
					case 1://reset
						_fifo._reset();
						PS1GPUOK(0,0);
					break;
					case 2:
						_fifo._new(2,2,b);
						PS1GPUOK(0,0);
					break;
					case 0x1f:
						PS1GPUREG(0x14) |= BV(24);
						PS1GPUOK(0,0);
					break;
					case 0x22:
					case 0x25:
					case 0x27:
					case 0x2d:
					case 0x2e:
					case 0x2f:
					case 0x32:
					case 0x36:
					case 0x3a:
					case 0x3e:
					case 0x42:
					case 0x48:
					case 0x4a:
					case 0x52:
					case 0x58:
					case 0x5a:
					case 0x68:
					case 0x6a:
					case 0x70:
					case 0x72:
					case 0x78:
					case 0x7a:
					case 0x65:
					case 0x6c:
					case 0x6d:
					case 0x6e:
					case 0x6f:
					case 0x75:
					case 0x76:
					case 0x77:
					case 0x7c:
					case 0x7d:
					case 0x7e:
					case 0x7f:
					case 0xc0:
						printf("GPU x %x\n",b);
						EnterDebugMode();
					break;
					case 0x24:
					case 0x26:
						_fifo._new(SR(b,24),6,b);
						PS1GPUOK(0,0);
					break;
					case 0x20:
						_fifo._new(SR(b,24),3,b);
						PS1GPUOK(0,0);
					break;
					case 0x28:
					case 0x29:
					case 0x2a:
					case 0x2b:
						_fifo._new(SR(b,24),4,b);
						PS1GPUOK(0,0);
					break;
					case 0x2c:
					case 0x34:
						_fifo._new(SR(b,24),8,b);
						PS1GPUOK(0,0);
					break;
					case 0x30:
					//	printf("0x38 %x\n",b);
						_fifo._new(0x30,5,b);
						PS1GPUOK(0,0);
					break;
					case 0x38:
						_fifo._new(0x38,7,b);
						PS1GPUOK(0,0);
					break;
					case 0x3c:
						_fifo._new(0x3c,11,b);
						PS1GPUOK(0,0);
					break;
					case 0x40:
						_fifo._new(0x40,2,b);
						PS1GPUOK(0,0);
					break;
					case 0x50:
						_fifo._new(0x50,3,b);
						PS1GPUOK(0,0);
					break;
					case 0x60:
					case 0x62:
						_fifo._new(SR(b,24),2,b);
						PS1GPUOK(0,0);
					//	printf("6x %x\n",b);
					break;
					case 0x64:
					case 0x66:
					case 0x67:
						_fifo._new(SR(b,24),3,b);
					//	printf("64 %x\n",b);
						PS1GPUOK(0,0);
					break;
					case 0x74:
						//printf("74 %x\n",b);
						_fifo._new(0x74,2,b);
						PS1GPUOK(0,0);
					break;
					case 0x80:
						//if((b&0xffffff)==0)
						_fifo._new(0x80,3);
						PS1GPUOK(0,0);
					break;
					case 0xa0://load texture
						_fifo._new(0xa0,2);
						PS1GPUOK(0,0);
					break;
					case 0xe1://drawing mode
						_fifo._reset();
						_env._draw._tex_id=SL(b&0xf,6)|SL(b&0x10,14);
						//_env._draw._tex_mode=SR(b,7);
						_env._draw._value=b;

						_dither(_env._draw._dither);
						PS1GPUOK(0,0);
					break;
					case 0xe2:
						_fifo._reset();
						_env._texture._value=b;
						PS1GPUOK(0,0);
					//	printf("E2 %x \n",b);
					break;
					case 0xe4:{
						int w=b&0x3ff;
						int h=SR(b,10)  & 0x1ff;

						if(_env._draw._h!=h || _env._draw._w!=w)
							_gpu_status |= PS1_STATUS_MUSTCLEAR;
						_env._draw._w=w;
						_env._draw._h=h;
						//printf("E4 %x \n",b);
						_fifo._reset();
						PS1GPUOK(0,0);
					}
					break;
					case 0xe3:{
						int x=b&0x3ff;
						int y=SR(b,10)&0x1ff;

						if(_env._draw._y != y || _env._draw._x!=x)
							_gpu_status |= PS1_STATUS_MUSTCLEAR;
						_env._draw._x=x;
						_env._draw._y=y;
						_fifo._reset();
						PS1GPUOK(0,0);
					}
					break;
					case 0xe5:
						_env._draw._ox=PS1VTX(b&0x3ff);
						_env._draw._oy=PS1VTX(SR(b,11)&0x3ff);
						_fifo._reset();
						PS1GPUOK(0,0);
					break;
					case 0xe6:
						_fifo._reset();
						PS1GPUOK(0,0);
					//	printf("%X %x \n",SR(b,24),b);
					break;
					default:
						printf("GP0 %08x\n",b);
					case 0:
						PS1GPUOK(0,0);
						_fifo._reset();
						PS1GPUREG(0x14) |= BV(28);
						if((b&0xffffff) == 0xffffff)
							return -2;//2023 cack
					break;
				}
				if(!_fifo._command_len)
					PS1GPUREG(0x14) |= BV(28);
			}
			else if(_fifo._command_len){
				u32 flags=0;

				switch(_fifo._command){
					case 2:
						_fifo._push(b);
						if(_fifo._command_len==1){
							u32 c=_fifo._data[0];
							{
								int b = (int)(u8)c;
								int g = (int)(u8)SR(c,8);
								int r = (int)(u8)SR(c,16);
								c = SR(b,3)|SL(SR(g,3),5)|SL(SR(r,3),10);
								_env._dma._value=c;
							}
							_env._dma._transfer(2,_fifo._data,_gpu_mem);
							//_clear((u32)_clearBits);
						}
						break;
					case 0x20:
						flags |= PS1_POLY_THREE;
					case 0x2a:
					case 0x2b:
						flags |= PS1_SEMI_TRSPARENT;
					case 0x28:
					case 0x29:
						_fifo._push(b);
						if(_fifo._command_len==1)
							_drawPolygon(_fifo._data,0,flags);
					break;
					case 0x26:
						flags|=PS1_SEMI_TRSPARENT;
					case 0x24:
						flags|= PS1_POLY_THREE;
					case 0x2c:
						_fifo._push(b);
						if(_fifo._command_len==1){
							PS1TEXIDD o(_env._draw._tex_page,_env._draw._tex_mode);

							o.clut=SR(_fifo._data[2],16);
							o.page=SR(_fifo._data[4],16);
							o.mode=SR(_fifo._data[4],23);
							o.y=(u8)SR(_fifo._data[2],8);
							o.x=(u8)_fifo._data[2];
							o.w=(u8)_fifo._data[8];
							o.h=(u8)SR(_fifo._data[8],8);
							flags |=PS1_TEXTURED|PS1_BLEND;
							_drawPolygon(_fifo._data,o.id,flags);
						}
					break;
					case 0x30:
						flags|=PS1_POLY_THREE;
					case 0x38:
						_fifo._push(b);
						if(_fifo._command_len==1){
							_drawPolygon(_fifo._data,0,PS1_POLY_SHADED|flags);
						//	EnterDebugMode();
						}
					break;
					case 0x34:
						flags |= PS1_POLY_THREE;
					case 0x3c:
						_fifo._push(b);
						if(_fifo._command_len==1){
							PS1TEXIDD o(_env._draw._tex_page,_env._draw._tex_mode);

							o.clut=SR(_fifo._data[2],16);
							o.page=SR(_fifo._data[5],16);
							o.mode=SR(_fifo._data[5],23);
							o.y=(u8)SR(_fifo._data[2],8);
							o.x=(u8)_fifo._data[2];
							o.w=(u8)_fifo._data[11];
							o.h=(u8)SR(_fifo._data[11],8);

							flags |= PS1_TEXTURED|PS1_POLY_SHADED|PS1_BLEND;
							_drawPolygon(_fifo._data,o.id,flags);
						}
					break;
					case 0x50:
						flags |=PS1_POLY_SHADED;
					case 0x40:
						_fifo._push(b);
						if(_fifo._command_len==1){
							_drawLine(_fifo._data,2,flags);
						}
					break;
					case 0x62:
						flags|=PS1_SEMI_TRSPARENT;
					case 0x60:
						_fifo._push(b);
						if(_fifo._command_len==1)
							_drawQuad(_fifo._data[0],(u16)_fifo._data[1],SR(_fifo._data[1],16),(u16)_fifo._data[2],SR(_fifo._data[2],16),0,0,0,flags);
					break;
					case 0x67:
						flags|=PS1_RAW_TEXTURE;
					case 0x66:
						flags|=PS1_SEMI_TRSPARENT;
					case 0x64:
						_fifo._push(b);
						if(_fifo._command_len==1){
							flags|=PS1_TEXTURED;
;
							PS1TEXIDD o(_env._draw._tex_page,_env._draw._tex_mode);

							o.clut=SR(_fifo._data[2],16);
						//	o.page=SR(_fifo._data[5],16);
						//	o.mode=SR(_fifo._data[5],23) &3;
							o.y=(u8)SR(_fifo._data[2],8);
							o.x=(u8)_fifo._data[2];
							o.w=(u16)_fifo._data[3];
							o.h=(u16)SR(_fifo._data[3],16);
							_drawQuad(_fifo._data[0],(u16)_fifo._data[1],SR(_fifo._data[1],16),
								(u16)_fifo._data[3],SR(_fifo._data[3],16),(u8)_fifo._data[2],(u8)SR(_fifo._data[2],8),o.id,flags);
						}
					break;
					case 0x74:
						_fifo._push(b);
						if(_fifo._command_len==1){
							flags|=PS1_TEXTURED|PS1_SEMI_TRSPARENT;

							PS1TEXIDD o(_env._draw._tex_page,_env._draw._tex_mode);

							o.clut=SR(_fifo._data[2],16);
						//	o.page=SR(_fifo._data[5],16);
						//	o.mode=SR(_fifo._data[5],23) &3;
							o.y=(u8)SR(_fifo._data[2],8);
							o.x=(u8)_fifo._data[2];
							o.w=8;
							o.h=8;
							_drawQuad(_fifo._data[0],(u16)_fifo._data[1],SR(_fifo._data[1],16),8,8,(u8)_fifo._data[2],(u8)SR(_fifo._data[2],8),o.id,flags);
						}
					break;
					case 0x80:
						_fifo._push(b);
						if(_fifo._command_len == 1){
							_env._dma._transfer(0x80,_fifo._data,_gpu_mem);
							//printf("80\n");
							//_fifo._command_mode++;
						}
					break;
					case 0xa0:
						//printf("a0 %u %u\n",_fifo._command_mode,_fifo._command_len);
						if(_fifo._command_mode==1){
							_fifo._push(b);

							if(_fifo._command_len == 1){
								u32 pos;

								_env._dma._transfer(0xa0,_fifo._data,_gpu_mem);
								_fifo._command_len=SR(pos=_env._dma.w*_env._dma.h,1);
								//printf("A0 %x %u %u ",_env._dma._pos,_env._dma.x,_env._dma.y);

								if(SL(_fifo._command_len,1) != pos)
									_fifo._command_len++;
								//_fifo._command_len++;

							//	if(_env._dma._value & 2)
								//	_fifo._command_mode++;
								_fifo._command_mode++;
								//printf(" %u %u %u\n",_fifo._command_len,_env._dma.w,_env._dma.h);
								_fifo._command_len++;
								//EnterDebugMode();
								PS1GPUREG(0x14) |= BV(28);
								_clearTextures();
							//	DLOG("GPU a0 %x %u %ux%u %u %u",_env._dma._pos,_fifo._command_len,_env._dma.x,_env._dma.y,_env._dma.w,_env._dma.h);
							}
						}
						else{
							for(int i=0;i<2;i++,b >>= 16){
								*_env._dma.dst++ = (u16)b;
								if(++_env._dma._x >= _env._dma.w){
									_env._dma._x=0;
									if(++_env._dma._y >= _env._dma.h){
										break;
									}
									_env._dma.dst=&((u16 *)_gpu_mem)[(_env._dma.y+_env._dma._y)*1024+
										(_env._dma.x+_env._dma._x)];
								}
							}
						}
					break;
				}

				if(--_fifo._command_len == 0){
				///	printf("GP0 E %x \n",_fifo._command);
					_fifo._command=0;
					PS1GPUREG(0x14) |= BV(28);
					PS1GPUOK(0,0);
				}
			}
		break;
		case 0x14://GP1 Display control
			switch(SR(b,24) &0x3f){
				case 1:
					_fifo._reset();
					PS1GPUOK(0,0);
				break;
				case 2:
					PS1GPUREG(0x14) &= ~BV(24);
					PS1GPUOK(0,0);
				break;
				case 4:
					_env._dma._value=b;
					PS1GPUOK(0,0);
				break;
				case 5:
					_env._display._x = (b&0x3ff);
					_env._display._y = SR(b,10) & 0x1ff;
					DLOG("GPU 8 %d %d",_env._display._x,_env._display._y);
				break;
				case 6:
				case 7:
					//printf("GP1 %08x\n",b);
					PS1GPUOK(0,0);
				//	EnterDebugMode();
				break;
				case 8:{
					int w,h;

					_env._display._mode=b;
					PS1GPUOK(0,0);
					//	EnterDebugMode();
					switch(b&0x47){
						case 0:
							w=256;
						break;
						case 1:
							w=320;
						break;
						case 2:
							w=512;
						break;
						case 3:
							w=640;
						break;
						default:
							w=368;
						break;
					}
					h=240;
					_env._display._w=w;
					_env._display._h=h;
					_env._change(_width,_height,_displayWidth,_displayHeight);
					if(b&8)
						PS1GPUREG(0x14) |=0x80000;
					else
						PS1GPUREG(0x14) &=~0x80000;
				}
				break;
				case 9:
					printf("09\n");PS1GPUOK(0,0);
				break;
				case 0:
				default:
					PS1GPUOK(0,0);
				break;
			}
			PS1GPUREG(0x14) |= BV(28);
		break;
	}
	return 0;
}

int PS1GPU::_bindTexture(u64 id,int w,int h){
	if(!id) goto Z;
A:
		//printf("ex:%llx %u\n",id,_env._draw._tex_mode);
	for(auto it=_textures.begin();it != _textures.end();it++){
		if((*it).id==id){
			GRE::_bindTexture(&(*it));
			return 0;
		}
	}
	{
		GRETEXTURE tex;
		PS1TEXIDD itex(id);
		u32 b;

		//printf("tex; %llx %u %u %u %u %u\n",id,itex.w,itex.h,itex.mode,itex.x,itex.y);

		tex.id=id;
		tex.w=w;
		tex.h=h;
		tex._stride=1024;
		tex._mode=itex.mode;
		tex.x=itex.x;
		tex.y=itex.y;
		tex._scaled=1;
		tex._translated=1;
		int ty=tex.y;
		int tx=tex.x;

		b=itex.page;
		b=SL(b & 0xf,6) | SL(b & 0x10,14);
		//printf("ct %x %u %u W:%u H:%u %x",b*2,tx,ty,tex.w,tex.h,tex._mode);
		ty=SL(ty,10);
		switch(tex._mode){
			case 1://256
				tx=SR(tx,1);
				tex._stride*=2;
			break;
			case 0://16
				tx=SR(tx,2);
				tex._stride*=2;
			break;
		}
		//printf("%x %x %u %u %u %u\n",b,itex.page,tex.x,tex.y,tex.w,tex.h);
		b += ty+tx;
		//printf(" %x\n",b*2);
		tex._create((u16 *)_gpu_mem + b,*this);
		_textures.push_back(tex);
		goto A;
	}
Z:
	GRE::_bindTexture();
	return 1;
}

int PS1GPU::_drawLine(u32 *data,u32 nv,u32 f){
	float v[10],*c;
	int i,n;

	v[2]=0;
	v[3]=1;
	c=&v[6];
	_createColor(data[0],f,c);

	_beginDraw(GL_LINES);
	for(n=i=0;i<nv;i++,n++){
		u32 v_=data[1+n];
		v[0]=PS1VTX((u16)v_);
		v[1]=PS1VTX(SR(v_,16));

		_drawVertex(v,0,0,c);
		if(f & PS1_POLY_SHADED)
			_createColor(data[2+n++],f,c);
		else
			c=NULL;
	}
	_endDraw();
	return 0;
}

int PS1GPU::_drawPolygon(u32 *data,u64 id,u32 f){
	float v[10],*c,*tc;
	int i,n,nv;

	v[2]=0;
	v[3]=1;
	tc=NULL;
	c=&v[6];
	nv= f & PS1_POLY_THREE ? 3 : 4;

	if(!_bindTexture(id,PS1TEXIDD(id).w,PS1TEXIDD(id).h)){
		tc=&v[4];
	}
	//printf("%08X\n",data[0]);
	_createColor(data[0],f,c);
	_beginDraw(f & PS1_POLY_THREE ? GL_TRIANGLE_STRIP : GL_QUAD_STRIP);
#ifdef _DEVELOPa
	if(SR(data[0],24)==0x29)
		printf("DP %08X ",data[0]);
#endif
	for(n=i=0;i<nv;i++,n++){
		u32 v_=data[1+n];
		v[0]=PS1VTX((u16)v_);
		v[1]=PS1VTX(SR(v_,16));
		if(tc){
			v_=data[1 + ++n];
			tc[0]=(u8)v_;
			tc[1]=(u8)SR(v_,8);
		}
#ifdef _DEVELOPaa
if(SR(data[0],24)==0x29){
	printf("\t %f %f %f ",v[0],v[1],v[2]);
	if(c) printf(" %f %f %f",c[0],c[1],c[2]);
}
#endif
		_drawVertex(v,tc,0,c);
		if(f & PS1_POLY_SHADED) {
			_createColor(data[2+n++],f,c);
		}
		else
			c=NULL;
	}
	_endDraw();
#ifdef _DEVELOPa
if(SR(data[0],24)==0x29)
	printf("\n");
#endif
	return 0;
}

int PS1GPU::_drawQuad(u32 col,u16 x,u16 y,u16 w,u16 h,u16 s,u16 t,u64 id,u32 f){
	float v[10],*c,*tc;

	v[2]=0;
	v[3]=1;
	tc=NULL;
	c=&v[6];
	if(!_bindTexture(id,w,h)){
		tc=&v[4];
		//printf("tes %llx\n",id);
		tc[0]=_cr_texture->x;
		tc[1]=_cr_texture->y;
	}
	//sprintf("%08X\n",col);
	_createColor(col,f,c);
	v[0]=PS1VTX(x);
	v[1]=PS1VTX(y);

#ifdef _DEVELOPa
	printf("DQ %06X %u %u %u %u\n",col,x,y,w,h);
#endif
	_beginDraw(GL_QUADS);
	_drawVertex(v,tc,0,c);
	v[0]=PS1VTX(x + w);
	if(tc)
		tc[0] +=w;
	_drawVertex(v,tc);
	v[1]=PS1VTX(y + h);
	if(tc)
		tc[1]+=h;
	_drawVertex(v,tc);
	v[0]=PS1VTX(x);
	if(tc)
		tc[0]-=w;
	_drawVertex(v,tc);
	_endDraw();
	return 0;
}

int PS1GPU::_createTexture(void *buf,void *p,void *dst){
	int res;
	GRETEXTURE *tex;
	u32 *pixels;

	if(!(tex = (GRETEXTURE *)p)) return -1;
	if(!dst)
		dst=_texture;
	pixels=(u32 *)dst;
	res=-2;
	switch(tex->_mode){
		case 2:{
			u16 *s;

			s=(u16 *)buf;
			for(int y=0;y<tex->h;y++){
				for(int x = 0;x<tex->w;x++){
					u32 col=s[x];
					u32 r = SL(col & 0x7c00,9)|SL(col&0x3e0,6)|SL(col&0x1f,3);
				//printf(" %x ",col);
					if((col & 0x8000))
						r |= 0x80000000;
					else if(col)
						r |= 0xff000000;
					*pixels++ = r;
				}
				s += tex->_stride;
			}
		//	printf("\n");
			res=0;
		}
		break;
		case 0:{
				u16 *pal;
				u32 i = SR(tex->id,41) & 0x7fff;
				int cx,cy;

				cx=SL(i & 0x3f,4);
				cy=SR(i,6) & 0x1ff;
				//printf("clut %d %d %x\n",cx,cy,cy*2048 + cx*2);
				pal = (u16 *)&((u8 *)_gpu_mem)[(cy*2048 + cx*2)];
			//	printf("\ncol %x %u %u %u %x",(u32)(SR(tex->id,41)&0x7fff),tex->w,tex->h,tex->_stride,(u32)SR(tex->id,32));
				for(int i=0;i<16;i++){
					u32 col=pal[i];
					u32 r = SL(col&0x7c00,9)|SL(col&0x3e0,6)|SL(col&0x1f,3);
					if((col & 0x8000))
						r |= 0x80000000;
					else if(col)
						r |= 0xff000000;
					((u32 *)_palette)[i]=r;
				//	printf(" %x ",col);
				}
			//	printf("\n");
				u8 *s=(u8 *)buf;
				for(int y=0;y<tex->h;y++){
					for(int x = 0;x<tex->w;x++){
						u8 px=SR(s[x/2],4*(x&1)) & 15;
					//	printf(" %x ",px);
						u32 col=((u32 *)_palette)[px];
						//col =px ?0xff:0;
						*pixels++ = col;
					}
					//printf("\n");
					s+=tex->_stride;
				}
			//}
			}
			res=0;
		break;
		case 1:{
				u16 *pal;
				u32 i = SR(tex->id,41) & 0x7fff;
				int cx,cy;

				cx=SL(i & 0x3f,4);
				cy=SR(i,6) & 0x1ff;
			//	printf("clut 256 %d %d %x\n",cx,cy,cy*2048 + cx*2);
				pal = (u16 *)&((u8 *)_gpu_mem)[cy*2048 + cx*2];
			//	printf("\ncol %x %u %u %u %x",(u32)(SR(tex->id,41)&0x7fff),tex->w,tex->h,tex->_stride,(u32)SR(tex->id,32));
				for(int i=0;i<256;i++){
					u32 col=pal[i];
					u32 r = SL(col&0x7c00,9)|SL(col&0x3e0,6)|SL(col&0x1f,3);
					if((col & 0x8000))
						r |= 0x80000000;
					else if(col)
						r|=0xff000000;
					((u32 *)_palette)[i]=r;
				//	printf(" %x ",col);
				}
			//	printf("\n");
				u8 *s=(u8 *)buf;
				for(int y=0;y<tex->h;y++){
					for(int x = 0;x<tex->w;x++){
						u8 px=s[x];
					//	printf(" %x ",px);
						u32 col=((u32 *)_palette)[px];
						//col =px ?0xff:0;
						*pixels++ = col;
					}
					//printf("\n");
					s +=tex->_stride;
				}
			//}
			}
			res=0;
		break;
	}
#ifdef _DEVELOPa
	if(res==0)
		SaveTextureAsTGA((u8 *)dst,tex->w,tex->h,(u32)SR(tex->id,32));
#endif
	return res;
}

int PS1GPU::_beginDraw(int m){
	int b[4]={255,255,255,255};

	glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);

    glTexEnvf(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_MODULATE);

    glTexEnvf(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);
    glTexEnvf(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
    glTexEnvf(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_PRIMARY_COLOR);
    glTexEnvf(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_COLOR);

    glTexEnvf(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_INTERPOLATE);

    glTexEnvf(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_TEXTURE);
    glTexEnvf(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV,GL_SOURCE1_ALPHA,GL_PRIMARY_COLOR);
    glTexEnvf(GL_TEXTURE_ENV,GL_OPERAND1_ALPHA,GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE2_ALPHA,GL_CONSTANT);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND2_ALPHA,GL_SRC_ALPHA);

    float f[4]={0,0,0,0.5f};

    switch(_env._draw._blend){
		case 0:
	//	case 3:
			_blend(2,GL_FUNC_ADD);
			f[3]=0.5f;
			b[3]=128;
		break;
		case 1:
			_blend(2,GL_FUNC_ADD);
			f[3]=0.5f;
			b[3]=255;
		break;
		default:
			printf("unk blend %x\n",_env._draw._blend);
		break;
	}
    glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,f);
    _blend(1,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA);
    _blend(3,b[0],b[1],b[2],b[3]);
	_alpha(1,GL_GREATER,128);

//	if((_gpu_status & GRE_STATUS_VERTEX_PUSH) == 0)
//		_gpu_status|=PS1_STATUS_MUSTCLEAR;
	if((_gpu_status & (GRE_STATUS_VERTEX_PUSH|PS1_STATUS_MUSTCLEAR)) == PS1_STATUS_MUSTCLEAR){
		_env._display._fb=(u16 *)_gpu_mem + _env._display._y*1024;
		_env._draw._fb= (u16 *)_gpu_mem + _env._draw._y*1024;
		_gpu_status &= ~PS1_STATUS_MUSTCLEAR;
		//_blend(0);
		for(int y=_env._draw._y,xx=min(_env._draw._w,_env._display._w);y<_env._draw._h;y++){
			u16 *p= (u16 *)_gpu_mem + y*1024 +_env._draw._x;
			for(int x =_env._draw._x;x<xx;x++)
				*p++=0;
		}
		//DLOG("GPU BD %d %d %d %d %u %u %u",_env._draw._x,_env._draw._y,_env._draw._w,_env._draw._h,(u32)((u64)_outBuffer - (u64)_env._draw._fb),b[3],_env._draw._y*2048);
		//_beginScene();
	}
    return GRE::_beginDraw(m);
}

void PS1GPU::_createColor(u32 col,u32 f,float *c){
	int r,g,b;

	b=(int)(u8)col;
	g=(int)(u8)SR(col,8);
	r=(int)(u8)SR(col,16);
	if(f & PS1_TEXTURED){
		if((r*=2)>255) r=255;
		if((g*=2)>255) g=255;
		if((b*=2)>255) b=255;
	}
	c[0]=b / 255.0f;
	c[1]=g / 255.0f;
	c[2]=r / 255.0f;
	c[3]=f & PS1_SEMI_TRSPARENT ? 0.5:1.0;
}

int PS1GPU::__env::__dma::_transfer(int m,u32 *_data,void *_gpu_mem){
	u16 *p;

	p=(u16 *)&_data[1];
	if(m==0x80){
		xs = *p++;
		ys = *p++;
		_pos=ys*2048+(xs*2);
		src = (u16 *)&((u8 *)_gpu_mem)[_pos];
	}
	x = *p++;
	y = *p++;
	w =	*p++;
	h = *p++;
	_pos=y*2048+(x*2);
#ifdef _DEVELOPa
	printf("FR %02x %x %x %x %x %x %x\n",m,x,y,w,h,_value,_pos);
#endif
	dst = (u16 *)&((u8 *)_gpu_mem)[_pos];
	_x =_y=0;
	switch(m){
		default:
			return -1;
		case 2:
			for(;;){
				*dst++ = (u16)_value;
				if(++_x >= w){
					_x=0;
					if(++_y >= h){
						break;
					}
					dst=&((u16 *)_gpu_mem)[(y+_y)*1024+(x+_x)];
				}
			}

		break;
		case 0x80:
			for(;;){
				*dst++ = *src++;
				if(++_x >= w){
					_x=0;
					if(++_y >= h){
						break;
					}
					dst=&((u16 *)_gpu_mem)[(y+_y) * 1024+(x+_x)];
					src=&((u16 *)_gpu_mem)[(ys+_y) * 1024+(xs+_x)];
				}
			}
		break;
	}
	return 0;
}

int PS1GPU::__env::_reset(int w,int h,int ww,int hh){
	_status=0;
	memset(&_display,0,sizeof(__display));
	memset(&_draw,0,sizeof(__draw));
	memset(&_dma,0,sizeof(__dma));
	memset(&_texture,0,sizeof(__texture));
	_display._w=w;
	_display._h=h;
	return _change(w,h,ww,hh);
}

int PS1GPU::__env::_change(int w,int h,int ww,int hh){
	_display._x_inc=_display._w*4096/w;
	_display._y_inc=_display._h*4096/h;
	_draw._x_inc=_display._w*4096/ww;
	_draw._y_inc=_display._h*4096/hh;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,_display._w,0,_display._h,-1,1);
	return 0;
}

};