#include "cps2gpu.h"

extern IMachine *machine;

namespace cps2{

CPS2GPU::CPS2GPU() : GPU(){
	_width=384;
	_height=224;
}

CPS2GPU::~CPS2GPU(){
}

int CPS2GPU::Init(){
	if(GPU::Init(_width,_height))
		return -1;//59.59Hz
	return 0;
}

int CPS2GPU::Reset(){
	GPU::Reset();
	__line=0;
	_cycles=0;
	return 0;
}

int CPS2GPU::Update(u32 flags){
	u32 *pp=(u32 *)_gpu_mem;

	if(GPU::Clear()) return -11;
Z:
	GPU::Update();
	Draw(NULL);
	return 0;
}

int CPS2GPU::Run(u8 *,int cyc,void *obj){
	if((_cycles+=cyc)<1589)
		return 0;
	_cycles-=1589;
	if(++__line==224)
		return 12;
	if(__line==264){
		__line=0;
		machine->OnEvent((u32)-1,0);
	}
	return 0;
}
}