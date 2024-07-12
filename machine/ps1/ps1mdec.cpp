#include "ps1m.h"

namespace ps1{

u8 __mdec::_zz[64]={0 ,1 ,5 ,6 ,14,15,27,28, 2 ,4 ,7 ,13,16,26,29,42, 3 ,8 ,12,17,25,30,41,43,
  9 ,11,18,24,31,40,44,53, 10,19,23,32,39,45,52,54, 20,22,33,38,46,51,55,60, 21,34,37,47,50,56,59,61,
 35,36,48,49,57,58,62,63};

#define RLEV(a) EX_SIGN(a,10)
#define MDEC_EOC(){_busy=0;_dir=_dma0;_dif=0;}
#define MDEC_EOB() MDEC_EOC()

__mdec::__mdec(){
	_izz=NULL;
	_iqy=NULL;
}

__mdec::~__mdec(){
	if(_izz) delete []_izz;
	_izz=NULL;
	_iqy=NULL;
}

int __mdec::Run(u8 *,int cyc,void *obj){
	u32 v;

	PS1M *p=(PS1M *)((LPCPUTIMEROBJ)obj)->param;
	v=1;
	if(_dma1 && !_dor)
		v|=0x200;
	if(_dma0)
		v|=0x100;
	((PS1DEV*)p)->do_sync(v);
	return -1;
}

int __mdec::read(u32 a,u32 *p,PS1DEV &g){
	switch((u8)a){
		case 0x20:
			*p=_out.p[_out.pr++];
			if(_out.end <= 4){
				_out.end=0;
				if(_decode((u16 *)&_fifo._data[1])) {
					_dof=1;
				//	if(_dma0)
					//	g.do_sync(0x0101);
				}
				//_md._out.pr=0;
				//memset(_out.p,0,768*4);
			//	printf("mdecode %x %u %u\n",_out.end,_out.current,_out.size);
			}
			else
				_out.end -= 4;
			if(_out.pr >= 768)
				_out.pr -= 768;
			PS1IOREG(0x1F801824)=_status;
		break;
		case 0x24:
			*p=_status;
			printf("%x\n",_status);
		break;
	}
	return 0;
}

int __mdec::write(u32 r,u32 v,PS1DEV &g){
	int res=1;

	switch((u8)r){
		case 0x20:
			if(!_fifo._command_mode){
				DLOG("MDEC W %x %x",0,v);
				_status8[4]=SR(v,24);
				//if(_busy) EnterDebugMode();
				switch(_command){
					case 2:
						_fifo._new(2,1+16+16*(v&1),v);
					break;
					case 3:
						_fifo._new(_command,33,v);
					break;
					case 1:
					//printf("mdecc %x %x %x\n",v,_command,_cdod);
						_fifo._new(_command,(u16)v + 1,v);
					break;
					default:
						_command=0;
						_fifo._new(0,(u16)v,v);
					break;
				}
				_busy=1;
				_dif=0;
				_dof=1;
				_param=_fifo._data[0];
			}
			else{
			//	printf("__mdec %x %x\n",_fifo._command,_fifo._command_len);
				_fifo._push(v);
				_param--;
				_status = (_status & ~0x07800000) | SR(_fifo._data[0] & 0x1e000000,2);

				if(--_fifo._command_len == 0){
					_fifo._command=0;
					_param=0xffff;
					_dif=1;
					_dir=0;
					_dor=0;
				}
				else if(_fifo._command_len==1){
					switch(_fifo._command){
						case 1:
							_out.size=SL((u16)_fifo._data[0],2);
							//printf("mdec %08x %08x %x %x %x\n",_out.size,_fifo._command,_dof,_dma1,_dma0);
							_out.pr=_out.end=_out.pw=0;
							_out.current=0;
							_decode((u16*)&_fifo._data[1]);
							_dof=0;
							_dor=_dma1;
							//res=0x30002;
						break;
						case 2:
							//_iqt_init((s16 *)&_fifo._data[1],_md._iquv);
							memcpy(_iqy,&_fifo._data[1],64);
							memcpy(_iquv,&_fifo._data[1+16],64);
							MDEC_EOC();
						break;
						case 3:
							//_iqt_init((s16 *)&_fifo._data[1],_md._iqy);
							memcpy(_iscale,&_fifo._data[1],64*sizeof(s16));
							MDEC_EOC();
						break;
					}
					_fifo._reset();
				}
			}
			PS1IOREG(0x1F801824)=_status;
		break;
		case 0x24:
			DLOG("MDEC W %x %x",4,v);
			_status8[5]=SR(v,24);
			if(_reset)
				reset_decoder();
			PS1IOREG(0x1F801824)=_status;// | 0x80000000;
			res=0;
		break;
	}
	//printf("__mdec %x %x\n",r,v);
	return res;
}

u32 __mdec::_unrle(u16 *in,u16 *out,u8 *iq){
	int q_scale,rl,i,k;
	u32 ret;

	for(u64 *p=(u64 *)out,i=0;i<16;i++)
		*p++=0;
	rl=0xfe00;
	for(ret=0;rl==0xfe00;ret++)
		rl=*in++;
	q_scale=SR(rl,10);
	k=RLEV(rl)*iq[0];
	for(i=0;i<64;){
		if(q_scale==0)
			k*=2;
		if(k>1023)
			k = 1023;
		else if(k < -1024)
			k=-1024;

		if(q_scale > 0)
			out[_izz[i]]=k;
		else if(q_scale == 0)
			out[i]=k;
		ret++;
		rl=*in++;
		if(rl==0xfe00){
			MDEC_EOB();
			break;
		}
		i+=(SR(rl,10))+1;
		k=(RLEV(rl)*(s32)(s8)iq[i]*q_scale+4)/8;
	}
	return ret;
}

void __mdec::_idct(s16 *src){
	for(int x=0;x<8;x++){
		for(int y=0;y<8;y++){
			s32 sum=0;
			for(int z=0;z<8;z++)
				sum += src[y + z * 8]*(_iscale[x + z * 8] / 8);
			_in[x + y * 8]=(sum+0xfff)/0x2000;
		}
	}

	for(int x=0;x<8;x++){
		for(int y=0;y<8;y++){
			s32 sum=0;
			for(int z=0;z<8;z++)
				sum += (s32)_in[y + z * 8]*(_iscale[x + z * 8] / 8);
			sum=(sum+0xfff)/0x2000;
			if(sum > 127)
				sum=127;
			else if(sum<-128)
				sum=-128;
			src[x + y * 8]=sum;
		}
	}
}

void __mdec::_yuv_rgb(int xx,int yy,s32 *in,s32 *out,s16 *cb,s16 *cr){
	for(int y=0;y<8;y++){
		for(int x=0;x<8;x++){

			int R=cr[((x+xx)/2) + ((y+yy)/2)*8];
			int B=cb[((x+xx)/2) + ((y+yy)/2)*8];

			int G=(-0.3437f*B)- 0.7143f*R;

			R=(1.402f*R);
			B=(1.772f*B);
			s16 Y=((s16 *)in)[x+y*8];

			R+=Y;
			G+=Y;
			B+=Y;

			if(R > 127)
				R=127;
			else if(R<-128)
				R=-128;

			if(G > 127)
				G=127;
			else if(G<-128)
				G=-128;

			if(B > 127)
				B=127;
			else if(B<-128)
				B=-128;

			//if unsigned then BGR=BGR xor 808080h  ;aka add 128 to the R,G,B values
			out[x+xx+ ((y+yy)*16) ]=RGB((u8)R,(u8)G,(u8)B);
		}
	}
}

void __mdec::_yuv_mono4(s32 *in,s32 *out){
	memset(out,0,8);
	for(int i=0,b=0;i<64;i++){
		s16 Y=EX_SIGN(((s16 *)in)[i],10);
		if(Y > 127)
			Y=127;
		else if(Y < -128)
			Y=-128;
		//Y+=128;
		out[i/8] |= SL(SR(Y^0x80,4)&15,b);
		//printf(" %x;%x",Y,SR(Y,4)&15);
		b=(b+4)&31;
	}
}

void __mdec::_yuv_mono8(s32 *in,s32 *out){
	memset(out,0,16);
	for(int i=0,b=0;i<64;i++){
		s16 Y=EX_SIGN(((s16 *)in)[i],10);
		if(Y > 127)
			Y=127;
		else if(Y < -128)
			Y=-128;
		Y+=128;
		out[i/4] |= SL((u8)Y,b);
		//printf(" %x;%x",Y,SR(Y,4)&15);
		b=(b+8)&31;
	}
}

int __mdec::_decode(u16 *p){
	int *blk,res;
	u32 n,u;

	res=-1;
	blk=(int *)&_tmp[768];

	switch(_cdod){
		case 0://4bits
			n=_unrle((u16 *)p,(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_mono4((s32 *)blk,(s32 *)_out.p);
			printf("mdec mono4\n");
		break;
		case 1://8bits
			n=_unrle((u16 *)p,(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_mono8((s32 *)blk,(s32 *)_out.p);
			printf("mdec mono8\n");
		break;
		case 3:{
			if((u=_out.current) >= _out.size)
				goto A;
			n=_unrle((u16 *)&p[u],(u16 *)_cr,_iquv);
			_idct((s16 *)_cr);
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)_cb,_iquv);
			_idct((s16 *)_cb);
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(0,0,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(8,0,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(0,8,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(8,8,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
			_out.end = 64*2*4;
			u+=n;

			_out.current=u;
			u16 *p=(u16 *)(((u8 *)_out.p) +_out.pw);
			for(int i=0;i<256;i++){
				int R,G,B;

				u32 c =_tmp[i];
				R=(u8)c;
				G=(u8)SR(c,8);
				B=(u8)SR(c,16);

				B=SR(B^0x80,3)&31;
				G=SR(G^0x80,3)&31;
				R=SR(R^0x80,3)&31;
			///	if(!(i&15)) R=G=B=31;
			//	R=0;
			//	G=31;
			//	B=0;
				*p++=RGB555(R,G,B);
			}

			if((_out.pw += 512) >= 3072)
				_out.pw -= 3072;
		}
		break;
		case 2:{
			if((u=_out.current) >= _out.size)
				goto A;
			n=_unrle((u16 *)&p[u],(u16 *)_cr,_iquv);
			_idct((s16 *)_cr);
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)_cb,_iquv);
			_idct((s16 *)_cb);
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(0,0,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
		//	_out.end=64*3;

			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(8,0,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
		//	_out.end += 64*3;
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(0,8,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
		//_out.end += 64*3;
			u+=n;

			n=_unrle((u16 *)&p[u],(u16 *)blk,_iqy);
			_idct((s16 *)blk);
			_yuv_rgb(8,8,(s32 *)blk,(s32 *)_tmp,_cb,_cr);
			_out.end = 64*3*4;
			u+=n;

			_out.current=u;
			u8 *p=((u8 *)_out.p) +_out.pw;
			for(int i=0;i<256;i++){
				int R,G,B;

				u32 c =_tmp[i];
				R=(u8)c;
				G=(u8)SR(c,8);
				B=(u8)SR(c,16);
//G=B=R=0;G=255;
				*p++=R^0x80;
				*p++=G^0x80;
				*p++=B^0x80;
			}
			if((_out.pw += 64*4*3) >= 3072)
				_out.pw -= 3072;
		}
		break;
	}
	res=0;
	_dor=1;
	goto B;
A:
	printf("mdecc eof data\n");
	MDEC_EOC();
B:
	PS1IOREG(0x1F801824)=_status;
	return res;
}

int __mdec::reset(){
	_status64=0;
	reset_decoder();
	_fifo._reset();
	return 0;
}

int __mdec::init(int n,void *m,void *mm){
	_io_regs=(u32 *)m;
	_mem=(u8 *)mm;
//	_idx=n;
	if(!(_izz = (u8 *)new u8[5000* sizeof(u32)]))
		return -1;
	_iqy= (u8 *)&_izz[64];
	_iquv = &_iqy[64];
	_iscale= (s16 *)&_iquv[64];
	_in=(u32 *)&_iscale[64];
	_out.p=&_in[768];
	_tmp=&_out.p[768];
	_cb=(s16 *)&_tmp[768*2];
	_cr=&_cb[768];
	for(int i=0;i<64;i++)
		_izz[_zz[i]]=i;
	return 0;
}

int __mdec::reset_decoder(){
	_status=0;
	_param=0xffff;
	_dof=0;

	if(_iqy)
		memset(_iqy,0,5000*sizeof(u32)-64);
	_out.pw=_out.pr=0;
	_out.current=0;
	_out.end=0;
	_out.size=0;
	return 0;
}

};