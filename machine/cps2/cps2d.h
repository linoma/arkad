#include "arkad.h"

#ifndef __CPS2DH__
#define __CPS2DH__

namespace cps2{

class CPS2D{
public:
	CPS2D();
	virtual ~CPS2D();

	int _decrypt(u16 *,u16 *,u32 *,int);

	struct sbox{
		int extract_inputs(uint32_t val) const;

		const u8 table[64];
		const int inputs[6],outputs[2];
	};

	struct optimised_sbox{
		void optimise(sbox const &in);
		u8 fn(u8 in, u32 key) const;

		protected:
		u8 input_lookup[256],output[64];
	};
protected:
	void optimise_sboxes(optimised_sbox* out, const sbox* in);
	void expand_2nd_key(u32 *dstkey, const u32 *srckey);
	void expand_1st_key(u32 *dstkey, const u32 *srckey);
	void expand_subkey(u32* subkey, u16 seed);
	u8 fn(u8 in, const optimised_sbox *sboxes, u32 key);
	u16 feistel(u16 val, const int *bitsA, const int *bitsB,const optimised_sbox* boxes1, const optimised_sbox* boxes2, const optimised_sbox* boxes3, const optimised_sbox* boxes4,u32 key1, u32 key2, u32 key3, u32 key4);

	static sbox fn1_r1_boxes[4],fn1_r2_boxes[4],fn1_r3_boxes[4],fn1_r4_boxes[4],fn2_r4_boxes[4],fn2_r3_boxes[4],fn2_r1_boxes[4],fn2_r2_boxes[4] ;
	static int fn1_groupA[8],fn1_groupB[8],fn2_groupA[8],fn2_groupB[8];
};

};

#endif