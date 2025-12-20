#include "gbabios.h"
#include "bios7_gba.h"
#include <math.h>

namespace gba{

#define FREQ_MIDI  1.0594630944
#define OCT_MIDI   32.70319566

gbabios::gbabios() : gbadev(){

}

gbabios::~gbabios(){

}

int gbabios::Reset(){
	memcpy(M_BIOS,my_bios7_gba,sizeof(my_bios7_gba));
	return gbadev::Reset();
}

int gbabios::_enterIRQ(int n,u32 pc){
	if(gbadev::_enterIRQ(n,pc)) return 10;
	EnterDebugMode(DEBUG_BREAK_IRQ);
	/*REG_(13) -=4;
	WL(REG_(13),REG_(14));
	REG_(14)=_pc;
	RL(0x3007ffc,_pc);*/
	return 0;
}

int gbabios::Midi2Key(){
	float freq,oct,nota;
	u8 f,o;
	u32 a;

	o = (u8)REG_(1);
	f = (u8)(o & 0xF);
	o >>= 4;
	freq = 1.0;
	for(u8 i=0;i<=f;i++)
		freq = freq * FREQ_MIDI;
	oct = OCT_MIDI;
	for(u8 i=0;i<=o;i++)
		oct = oct * 2.0;
	nota = freq * oct;
	if(f > 0xE){
		freq = 1;
		oct *= 2.0;
	}
	else
		freq *= FREQ_MIDI;
	nota = (oct * freq) - nota;
	if(REG_(2) != 0)
		nota /= (float)REG_(2);
	RL_(REG_(0)+4,a,R, );
	REG_(0) = (u32)((a >> 16) * nota);
	return 0;
}

int gbabios::biosCall(u32 num){
	switch(num){
		default:
			printf("bios %x\n",num);
			EnterDebugMode();
			break;
/*		case 0:
           return SoftReset();
  */
		case 1:
           return RegisterRamReset(REG_(0));
		case 2:
			return stop();
		case 6:
			if(REG_(1) != 0){
				s32 i = (int)REG_(0) / (int)REG_(1);
				REG_(1) = (int)REG_(0) % (int)REG_(1);
				REG_(0) = i;
				REG_(3) = abs(i);
			}
			return 1;
		case 0xa:{
			float theta;
			s16 x,y;

			y = (s16)REG_(1);
			x = (s16)REG_(0);
			if(x != 0)
				theta = atan2(y,x);
			else if(y != 0)
				theta = atan(y);
			else
				theta = 0;
			REG_(0) = (u32)((s16)(theta * 65535.0 / 2.0 / M_PI));
		}
			return 1;
		case 0xb:
			return set(REG_(1),REG_(0),REG_(2));
		case 0xc:
			return fastset(REG_(1),REG_(0),REG_(2));
		case 0xe:
			return BgAffineSet();
		case 0xf:
			return ObjAffineSet();
		case 0x11:
		case 0x12:
			return LZ77UnComp(REG_(0),REG_(1));
		case 0x1f:
			return Midi2Key();
	}
	return 1;
}

int gbabios::stop(){
	WB(0x4000301,0x80);
	return 0;
}

int gbabios::set(u32 dst,u32 src,u32 flags){
	u8 DataSize;
	int count;
	u32 i;

	count = (int)(flags&0xfffff);
   if((DataSize = (u8)((flags >> 26) & 1)) != 0){
       dst &= ~3;
       src &= ~3;
   }
   else{
       dst &= ~1;
       src &= ~1;
   }
   switch((flags >> 24) & 1){
       case 0:
           if(DataSize != 0){
               for(;count>0;count--){
				   u32 a;

                   RLPC(src,a);
                   WL_(dst,a,W, );
                   dst += 4;
                   src += 4;
               }
           }
           else{
               for(;count>0;count--){
                   u16 a;

                   RWPC(src,a);
                   WW_(dst,a,W, );
                   dst += 2;
                   src += 2;
               }
           }
       break;
       case 1:
           if(DataSize != 0){
               RLPC(src,i);
               for(;count>0;count--){
					WL_(dst,i,W, );
					dst += 4;
               }
           }
           else{
               RWPC(src,i);
               for(;count>0;count--){
                   WW_(dst,i,W, );
                   dst += 2;
               }
           }
       break;
   }
   return 0;
}

int gbabios::fastset(u32 dst,u32 src,u32 flags){
	int count;
	u32 i;

	count = (int)(flags&0xfffff);
	switch((flags >> 24) & 1){
		case 0:
			for(;count>0;count--){
				RLPC(src,i);
				WL_(dst,i,W, );
				dst += 4;
				src += 4;
			}
		break;
		case 1:
			RLPC(src,i);
			for(;count>0;count--){
				WL_(dst,i,W, );
				dst += 4;
			}
		break;
	}
	return 0;
}

int gbabios::LZ77UnComp(u32 src,u32 dst){
	u32 DataHeader,i1,i3,b;
	int DataSize;
	u8 i,i4;

	RLPC(src,DataHeader);
	src += 4;
	DataSize = DataHeader >> 8;
	while(DataSize > 0){
		b=0;
		RBPC(src,b);
		src++;
		for(i=8;i > 0 && DataSize > 0;i--){
			u8 a;

			if((b & 0x80) == 0){
				RBPC(src,a);
				WB_(dst,a,W, );
				src++;
				dst++;
				DataSize--;
			}
			else{
				i4=0;
				RBPC(src,i4);
				src++;
				i1 = 3 + (i4 >> 4);
				RBPC(src,a);
				src++;
				i3 = (((i4 & 0xF) << 8) | a) + 1;
				DataSize -= i1;
				for(;i1 > 0;i1--){
					RBPC(dst-i3,a);
					WB_(dst,a,W, );
					dst++;
				}
			}
			b <<= 1;
		}
	}
	return 0;
}

int gbabios::RegisterRamReset(u8 value){
   WLPC(0x04000000,0x80);
   if((value & 0x80)){
       _resetMem(0x04000200,8);
       WWPC(0x04000202,0xFFFF);
       _resetMem(0x04000004,8);
       _resetMem(0x04000020,16);
       _resetMem(0x040000B0,24);
       WLPC(0x04000130,0x0000FFFF);
       WWPC(0x04000020,0x0100);
       WWPC(0x04000026,0x0100);
       WWPC(0x04000030,0x0100);
       WWPC(0x04000036,0x0100);
   }
   if((value & 0x40)){
       WBPC(0x04000084,0x80);
       WLPC(0x04000080,0);
       WBPC(0x04000070,0);
       _resetMem(0x04000090,8);
   }
   if((value & 0x20)){
       _resetMem(0x04000110,8);
       WWPC(0x04000130,0xFFFF);
       WBPC(0x04000140,0x7);
       _resetMem(0x04000140,7);
   }
   if((value & 0x10))
       _resetMem(0x07000000,0x0100);
   if((value & 0x8))
       _resetMem(0x06000000,0x6000);
   if((value & 0x4))
       _resetMem(0x05000000,0x0100);
   if((value & 0x2))
       _resetMem(0x03000000,0x1F7F);
   if((value & 0x1))
       _resetMem(0x02000000,0x10000);
	return 0;
}

void gbabios::_resetMem(u32 dst,u32 count){
   for(;count > 0;count--){
       WLPC(dst,0);
       dst += 4;
   }
}

int gbabios::BgAffineSet(){
   int i,sn,cs,i1,i2,i3,i4,i5,i6,i9,i10,i12,i11;
   u16 w;
   u32 dst,src;

   dst = REG_(1);
   src = REG_(0);
   for(i=REG_(2);i>0;i--){
	   RWPC(src+0x10,w);
       w = (u16)(w >> 8);
       sn = (int)(sin(((u8)(w + 0x40)) * M_PI / 128.0) * 16384.0);
       cs = (int)(sin(w * M_PI / 128.0) * 16384.0);
       RWPC(src+12,w);
       i1 = (int)(s16)w;
       RWPC(src+14,w);
       i2 = (int)(s16)w;
       i3 = (sn * i1) >> 14;
       i4 = (cs * i1) >> 14;
       i5 = (cs * i2) >> 14;
       i6 = (sn * i2) >> 14;
       RLPC(src,i9);
       RLPC(src + 4,i10);
       RLPC(src + 8,i12);
       i11 = (int)((u16)i12);
       i12 >>= 16;
       i9 += i3 * -i11;
       WLPC(dst + 8,i4 * i12 + i9);
       i10 += i5 * -i11;
       WLPC(dst+12,i6 * -i12 + i10);
       WWPC(dst,(u16)i3);
       WWPC(dst+2,(u16)(0-i4));
       WWPC(dst+4,(u16)i5);
       WWPC(dst+6,(u16)i6);
       src += 20;
       dst += 16;
   }
   return 0;
}

int gbabios::ObjAffineSet(){
   int i,sn,cs,i1,i2,i3,offset;
   u16 w;
   u32 dst,src;

   dst = REG_(1);
   src = REG_(0);
   offset = REG_(3);
   for(i=REG_(2);i>0;i--){
	   RWPC(src+4,w);
       w = (u16)(w >> 8);
       sn = (int)(sin(((u8)(w + 0x40)) * M_PI / 128.0) * 16384.0);
       cs = (int)(sin(w * M_PI / 128.0) * 16384.0);
       RWPC(src,w);
       i1 = (int)((s16)w);
       RWPC(src+2,w);
       i2 = (int)((s16)w);
       i3 = (sn * i1) >> 14;
       WWPC(dst,(u16)i3);
       dst += offset;
       i3 = 0 - ((cs * i1) >> 14);
       WWPC(dst,(u16)i3);
       dst += offset;
       i3 = (cs * i2) >> 14;
       WWPC(dst,(u16)i3);
       dst += offset;
       i3 = (sn * i2) >> 14;
       WWPC(dst,(u16)i3);
       dst += offset;
       src += 8;
   }
   return 0;
}

};