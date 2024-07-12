#include "ps2bios.h"

namespace ps2{

PS2BIOS::PS2BIOS() : PS2DEV() {
}

PS2BIOS::~PS2BIOS(){
}

int PS2BIOS::Reset(){
	_threads.clear();
	return PS2DEV::Reset();
}

int PS2BIOS::Init(PS2M &g){
	return PS2DEV::Init(g);
}

PS2BIOS::Thread::Thread(): vector<WAIT_OBJECT>(){
}

PS2BIOS::Thread::~Thread(){
}

int PS2BIOS::Thread::_create(){
}

};