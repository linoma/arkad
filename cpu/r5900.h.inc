#undef MIPSCORE
#undef MIPSCORECPU
#undef CORE_REG_SIZE
#undef MIPS
#undef CORE_REG_STEP


#define MIPSCORE R5900
#define CORE_REG_SIZE	64
#define CORE_REG_STEP	4
#define MIPS	4

#define REG64_(a) 		(((u64 *)R5900Cpu::_regs)[(a)*2])
#define REG128_(a) 		(((u128 *)R5900Cpu::_regs)[(a)])

#include "r3000.h.inc"