#include "aica.h"

namespace dc{

AICA::AICA() : PCMDAC(){

}

AICA::~AICA(){

}

int AICA::Init(int,void *a,void *b,u32){
	_spu_mem=a;
	_spu_regs=b;
	return 0;
}

int AICA::Reset(){
	return 0;
}

int AICA::Update(){
	return 0;
}

int AICA::Destroy(){
	return 0;
}

int AICA::write(u32,u32){
	return 0;
}

int AICA::read(u32,u32 *){
	return 0;
}

};