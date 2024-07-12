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

template<typename T> struct __command{
	u32 _command_len,_command_mode,_data_idx,_data_size;
	T *_data,_command;

	__command(){_data=NULL;_data_size=0;_data_idx=0;};
	~__command(){if(_data) delete []_data;};

	int _new(T a,u32 b=0,T c=0){
		_command=a;
		_command_len=b;
		_data_idx=0;
		_push(c);
		_command_mode=1;
		return 0;
	}

	int _push(T a){
		if(!_data || _data_size <= _data_idx){
			T *p = new T[_data_size+10];
			if(!p)
				return -1;
			if(_data) {
				memcpy(p,_data,_data_size*sizeof(T));
				delete []_data;
			}
			_data_size+=10;
			_data=p;
		}
		_data[_data_idx++]=a;
		return 0;
	}

	int _reset(){
		_command=0;
		_command_len=0;
		_command_mode=0;
		_data_idx=0;
		return 0;
	}

	int _clear(){
		if(_data) delete []_data;
		_data=NULL;
		_data_size=0;
		return _reset();
	}
};

#endif