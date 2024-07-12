#include "arkad.h"
#include "ccore.h"

#ifndef __PS1GTEH__
#define __PS1GTEH__

namespace ps1 {

#define GTE_MAX_48BIT	(SL((s64)1,47)-1)
#define GTE_MIN_48BIT 	(-SL((s64)1,47))
#define GTE_MAX_44BIT	(SL((s64)1,43)-1)
#define GTE_MIN_44BIT 	(-SL((s64)1,43))
#define GTE_MAX_16BIT 	(SL((s64)1,15)-1)
#define GTE_MIN_16BIT	(-SL((s64)1,15))
#define GTE_MAX_10BIT 	(SL((s64)1,10)-1)
#define GTE_MIN_10BIT	(-SL((s64)1,10))
#define GTE_MAX_32BIT	(SL((s64)1,31)-1)
#define GTE_MIN_32BIT 	(-SL((s64)1,31))
#define GTE_MAX_12BIT 	(SL((s64)1,12)-1)
#define GTE_MIN_12BIT	(-SL((s64)1,12))

#define GTE_ADD44_OVR(a,...) GTE_ADD_<s64,GTE_MAX_44BIT,GTE_MIN_44BIT,GTE_MAX_44BIT,GTE_MIN_44BIT,0>(GTE_OVR44H(a),GTE_OVR44L(a),## __VA_ARGS__)


struct __gte{
	union{
		u8 _regs8[32][4];
		u16 _regs16[32][2];
		s16 _sregs16[32][2];
		u32 _regs[32];
		s32 _sregs[32];
	};
	union{
		u16 _regs16[32][2];
		s16 _sregs16[32][2];
		u32 _regs[32];
		s32 _sregs[32];
	} ctrl;
	union{
		struct{
			unsigned int _cmd:6;
			unsigned int _unmd0:4;
			unsigned int _lm:1;
			unsigned int _unmd1:2;
			unsigned int _tx:2;
			unsigned int _vx:2;
			unsigned int _mx:2;
			unsigned int _sh:1;
		};
		u32 _opcode;
	};
	__gte();
	int _op(u32,PS1M &);
	int _ctrl_mv_from(u32,u32 *);
	int _ctrl_mv_to(u32,u32 *);
	int _mv_from(u32,u32 *);
	int _mv_to(u32,u32 *);
	int _reset();
	protected:

	u32 _clz(u32);
	s64 _rtp(int,int);
	void _ncd(int,int);
	void _cdp(int);
	void _cv(int,int);
	void _bkcdp(int);
	void _fccdp(int);
	void _cinterp(int);
	void _cfifo();
	void _cmac_store();
	u32 _div(u16,u16);
	template <typename T, T M,T m,T sM, T sm,u32 sat> T GTE_OVR_(T a,T b,u32 fM=0,u32 fm=0);
	template <typename T, T M,T m,T sM, T sm,u32 sat,typename... Params> T GTE_ADD_(u32 fM,u32 fm,Params &&... args);
	template <typename T, T M,T m,u32 sat> T GTE_OVR(T a,u32 fM=0,u32 fm=0);

	u8 _div_table[257];
};
};

#endif