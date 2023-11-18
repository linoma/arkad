#include "savestate.h"


SaveState::SaveState(IMachine *m) : SaveState(){

}

SaveState::SaveState(){
	_version=0;
}

SaveState::~SaveState(){
}

int SaveState::Load(char *p){
	return -1;
}

int SaveState::Save(char *p){
	return -1;
}

int SaveState::GetVersion(u32 *p){
	return -1;
}