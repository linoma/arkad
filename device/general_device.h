#include "arkad.h"

#ifndef __GENERALDEVICEH__
#define __GENERALDEVICEH__

template<typename T,int nelem> struct __latch{
	T _data[nelem];

	__latch(){
		reset();
	}

	int reset(){
		memset(_data,0,nelem*sizeof(T));
		return 0;
	}

	int write(int i,T v){
		_data[i]=v;
		return 0;
	}

	T read(int i=0){
		return _data[i];
	}
};

#endif