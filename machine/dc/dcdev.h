#include "ccore.h"
#include "pvr2.h"
#include "aica.h"
#include "dcdisc.h"
#include "cpu.h.inc"

#ifndef __DCDEVH__
#define __DCDEVH__

namespace dc{


#undef SHCORE
#undef MMUT

#define SHCORE 4

#define MMUT(a,b,c) 	_mmu.translate(a,c)
#define SYNCPC(a) 		RMAP_PC(MMUT(a,2,AM_READ|AM_WORD),_mem_pc,R);
#define ROP(a,b)		{b=*((u16 *)_mem_pc);_mem_pc = (u16 *)_mem_pc + 1;}

#include "sh.h.inc"


#define MI_BIOS		MB(16)
#define MS_BIOS		MB(2)
#define MI_FLASH	(MI_BIOS + MS_BIOS)
#define MS_FLASH	KB(128)
#define MI_RAM		(MI_FLASH + MS_FLASH)
#define MS_RAM		MB(16)
#define MI_VRAM		(MI_RAM + MS_RAM)
#define MS_VRAM		MB(8)
#define MI_TEXRAM	(MI_VRAM + MS_VRAM)
#define MS_TEXRAM	MB(8)

#define MI_IO		(MI_TEXRAM + MS_TEXRAM)
#define MS_IO		MB(4)
#define MI_DCACHE	(MI_RAM+(MS_RAM-KB(2)))
#define MI_SHREG	(MI_IO+MS_IO-0x10000-1)
#define MI_SPURAM	(MI_IO+MS_IO)
#define MS_SPURAM	MB(2)
#define MI_SQ		(MI_SPURAM+MS_SPURAM)
#define MS_SQ		64

#define SB_BASE  	0x005f6800

#undef IOREG__
#undef IOREG_
#undef IOREG

#define IOREG__(a,b) 		((u32 *) &(a)[(b) & (MS_IO-1)])
#define IOREG_(a,b) 		*IOREG__(a,b)
#define IOREG(b) 			IOREG_(_ioreg,b)

#define SH4REGI(a) 			(((a - 0xfe000000) >> 11) + ((a & 0xfc) >> 2))
#define SH4REG(a)			IOREG( (MS_IO-0x10000-1)+(a)*4)

#define ISIO(a) 			(__bus & MAIOMASK)
#define RMAP_IO(a) 			(SL(SR(a,31)+SR(a&0x10000000,28),20) | SR(a&0xfffff00,8))
#define ASIC_REG(a)			IOREG(SB_BASE + a)


#define SB_C2DSTAT  		((0x005f6800-0x005f6800))
#define SB_C2DLEN   		((0x005f6804-0x005f6800))
#define SB_C2DST    		((0x005f6808-0x005f6800))

#define SB_SDSTAW   		((0x005f6810-0x005f6800))
#define SB_SDBAAW   		((0x005f6814-0x005f6800))
#define SB_SDWLT    		((0x005f6818-0x005f6800))
#define SB_SDLAS    		((0x005f681c-0x005f6800))
#define SB_SDST     		((0x005f6820-0x005f6800))

#define SB_DBREQM   		((0x005f6840-0x005f6800))
#define SB_BAVLWC   		((0x005f6844-0x005f6800))
#define SB_C2DPRYC  		((0x005f6848-0x005f6800))
#define SB_C2DMAXL  		((0x005f684c-0x005f6800))
#define SB_TFREM    		((0x005f6880-0x005f6800))
#define SB_LMMODE0  		((0x005f6884-0x005f6800))
#define SB_LMMODE1  		((0x005f6888-0x005f6800))
#define SB_FFST     		((0x005f688c-0x005f6800))
#define SB_SFRES    		((0x005f6890-0x005f6800))
#define SB_SBREV    		((0x005f689c-0x005f6800))
#define SB_RBSPLT   		((0x005f68a0-0x005f6800))

#define SB_ISTNRM   		((0x005f6900-0x005f6800))
#define SB_ISTEXT   		((0x005f6904-0x005f6800))
#define SB_ISTERR   		((0x005f6908-0x005f6800))
#define SB_IML2NRM  		((0x005f6910-0x005f6800))
#define SB_IML2EXT  		((0x005f6914-0x005f6800))
#define SB_IML2ERR  		((0x005f6918-0x005f6800))
#define SB_IML4NRM  		((0x005f6920-0x005f6800))
#define SB_IML4EXT  		((0x005f6924-0x005f6800))
#define SB_IML4ERR  		((0x005f6928-0x005f6800))
#define SB_IML6NRM  		((0x005f6930-0x005f6800))
#define SB_IML6EXT  		((0x005f6934-0x005f6800))
#define SB_IML6ERR  		((0x005f6938-0x005f6800))
#define SB_PDTNRM   		((0x005f6940-0x005f6800))
#define SB_PDTEXT   		((0x005f6944-0x005f6800))
#define SB_G2DTNRM  		((0x005f6950-0x005f6800))
#define SB_G2DTEXT  		((0x005f6954-0x005f6800))

#define SB_MDSTAR   		((0x005f6c04-0x005f6800))
#define SB_MDTSEL   		((0x005f6c10-0x005f6800))
#define SB_MDEN     		((0x005f6c14-0x005f6800))
#define SB_MDST     		((0x005f6c18-0x005f6800))

#define SB_MSYS     		((0x005f6c80-0x005f6800))
#define SB_MST      		((0x005f6c84-0x005f6800))
#define SB_MSHTCL   		((0x005f6c88-0x005f6800))
#define SB_MDAPRO   		((0x005f6c8c-0x005f6800))
#define SB_MMSEL    		((0x005f6ce8-0x005f6800))

#define SB_MTXDAD   		((0x005f6cf4-0x005f6800))
#define SB_MRXDAD   		((0x005f6cf8-0x005f6800))
#define SB_MRXDBD   		((0x005f6cfc-0x005f6800))

#define SB_GDSTAR   		((0x005f7404-0x005f6800))
#define SB_GDLEN    		((0x005f7408-0x005f6800))
#define SB_GDDIR    		((0x005f740c-0x005f6800))
#define SB_GDEN     		((0x005f7414-0x005f6800))
#define SB_GDST     		((0x005f7418-0x005f6800))

#define SB_G1RRC    		((0x005f7480-0x005f6800))
#define SB_G1RWC    		((0x005f7484-0x005f6800))
#define SB_G1FRC    		((0x005f7488-0x005f6800))
#define SB_G1FWC    		((0x005f748c-0x005f6800))
#define SB_G1CRC    		((0x005f7490-0x005f6800))
#define SB_G1CWC    		((0x005f7494-0x005f6800))
#define SB_G1GDRC   		((0x005f74a0-0x005f6800))
#define SB_G1GDWC   		((0x005f74a4-0x005f6800))
#define SB_G1SYSM   		((0x005f74b0-0x005f6800))
#define SB_G1CRDYC  		((0x005f74b4-0x005f6800))
#define SB_GDAPRO   		((0x005f74b8-0x005f6800))

#define SB_SECUR_EADR  ((0x005f74e4-0x005f6800))
#define SB_SECUR_STATE ((0x005f74ec-0x005f6800))

#define SB_GDSTARD  		((0x005f74f4-0x005f6800))
#define SB_GDLEND   		((0x005f74f8-0x005f6800))

#define SB_ADSTAG   		((0x005f7800-0x005f6800))
#define SB_ADSTAR   		((0x005f7804-0x005f6800))
#define SB_ADLEN    		((0x005f7808-0x005f6800))
#define SB_ADDIR    		((0x005f780c-0x005f6800))
#define SB_ADTSEL   		((0x005f7810-0x005f6800))
#define SB_ADTRG    		SB_ADTSEL
#define SB_ADEN     		((0x005f7814-0x005f6800))
#define SB_ADST     		((0x005f7818-0x005f6800))
#define SB_ADSUSP   		((0x005f781c-0x005f6800))

#define SB_E1STAG   		((0x005f7820-0x005f6800))
#define SB_E1STAR   		((0x005f7824-0x005f6800))
#define SB_E1LEN    		((0x005f7828-0x005f6800))
#define SB_E1DIR    		((0x005f782c-0x005f6800))
#define SB_E1TSEL   		((0x005f7830-0x005f6800))
#define SB_E1TRG    		SB_E1TSEL
#define SB_E1EN     		((0x005f7834-0x005f6800))
#define SB_E1ST     		((0x005f7838-0x005f6800))
#define SB_E1SUSP   		((0x005f783c-0x005f6800))

#define SB_E2STAG   		((0x005f7840-0x005f6800))
#define SB_E2STAR   		((0x005f7844-0x005f6800))
#define SB_E2LEN    		((0x005f7848-0x005f6800))
#define SB_E2DIR    		((0x005f784c-0x005f6800))
#define SB_E2TSEL   		((0x005f7850-0x005f6800))
#define SB_E2TRG    		SB_E2TSEL
#define SB_E2EN     		((0x005f7854-0x005f6800))
#define SB_E2ST     		((0x005f7858-0x005f6800))
#define SB_E2SUSP   		((0x005f785c-0x005f6800))

#define SB_DDSTAG   		((0x005f7860-0x005f6800))
#define SB_DDSTAR   		((0x005f7864-0x005f6800))
#define SB_DDLEN    		((0x005f7868-0x005f6800))
#define SB_DDDIR    		((0x005f786c-0x005f6800))
#define SB_DDTSEL   		((0x005f7870-0x005f6800))
#define SB_DDTRG    		SB_DDTSEL
#define SB_DDEN     		((0x005f7874-0x005f6800))
#define SB_DDST     		((0x005f7878-0x005f6800))
#define SB_DDSUSP   		((0x005f787c-0x005f6800))

#define SB_G2ID     		((0x005f7880-0x005f6800))
#define SB_G2DSTO   		((0x005f7890-0x005f6800))
#define SB_G2TRTO   		((0x005f7894-0x005f6800))
#define SB_G2MDMTO  		((0x005f7898-0x005f6800))
#define SB_G2MDMW   		((0x005f789c-0x005f6800))
#define SB_G2APRO   		((0x005f78bc-0x005f6800))

#define SB_ADSTAGD  		((0x005f78c0-0x005f6800))
#define SB_ADSTARD  		((0x005f78c4-0x005f6800))
#define SB_ADLEND   		((0x005f78c8-0x005f6800))
#define SB_E1STAGD  		((0x005f78d0-0x005f6800))
#define SB_E1STARD  		((0x005f78d4-0x005f6800))
#define SB_E1LEND   		((0x005f78d8-0x005f6800))
#define SB_E2STAGD  		((0x005f78e0-0x005f6800))
#define SB_E2STARD  		((0x005f78e4-0x005f6800))
#define SB_E2LEND   		((0x005f78e8-0x005f6800))
#define SB_DDSTAGD  		((0x005f78f0-0x005f6800))
#define SB_DDSTARD  		((0x005f78f4-0x005f6800))
#define SB_DDLEND   		((0x005f78f8-0x005f6800))

#define SB_PDSTAP			((0x005F7C00-0x005f6800))
#define SB_PDSTAR			((0x005F7C04-0x005f6800))
#define SB_PDLEN			((0x005F7C08-0x005f6800))
#define SB_PDDIR			((0x005F7C0C-0x005f6800))
#define SB_PDTSEL			((0x005F7C10-0x005f6800))
#define SB_PDEN				((0x005F7C14-0x005f6800))
#define SB_PDST				((0x005F7C18-0x005f6800))
#define SB_PDAPRO			((0x005F7C80-0x005f6800))
#define SB_PDSTAPD			((0x005F7CF0-0x005f6800))
#define SB_PDSTARD			((0x005F7CF4-0x005f6800))
#define SB_PDLEND			((0x005F7CF8-0x005f6800))

#define RTC1        		((0x00710000-0x00710000))
#define RTC2        		((0x00710004-0x00710000))
#define RTC3        		((0x00710008-0x00710000))

#define IST_EOR_VIDEO    0x00000001
#define IST_EOR_ISP      0x00000002
#define IST_EOR_TSP      0x00000004
#define IST_VBL_IN       0x00000008
#define IST_VBL_OUT      0x00000010
#define IST_HBL_IN       0x00000020
#define IST_EOXFER_YUV   0x00000040
#define IST_EOXFER_OPLST 0x00000080
#define IST_EOXFER_OPMV  0x00000100
#define IST_EOXFER_TRLST 0x00000200
#define IST_EOXFER_TRMV  0x00000400
#define IST_DMA_PVR      0x00000800
#define IST_DMA_MAPLE    0x00001000
#define IST_DMA_MAPLEVB  0x00002000
#define IST_DMA_GDROM    0x00004000
#define IST_DMA_AICA     0x00008000
#define IST_DMA_EXT1     0x00010000
#define IST_DMA_EXT2     0x00020000
#define IST_DMA_DEV      0x00040000
#define IST_DMA_CH2      0x00080000
#define IST_DMA_SORT     0x00100000
#define IST_EOXFER_PTLST 0x00200000
#define IST_G1G2EXTSTAT  0x40000000
#define IST_ERROR        0x80000000

#define IST_EXT_EXTERNAL    0x00000008
#define IST_EXT_MODEM   0x00000004
#define IST_EXT_AICA    0x00000002
#define IST_EXT_GDROM   0x00000001

#define IST_ERR_ISP_LIMIT        0x00000004
#define IST_ERR_PVRIF_ILL_ADDR   0x00000040

class ASIC : public PVR2,public AICA,public SH4Cpu{
public:
	ASIC();
	virtual ~ASIC();
	virtual int Init(int,void *,void *,u32);
	virtual int Reset();
	virtual int Update();
	virtual int Destroy();
	int do_dma(void *);
protected:
	DcGame *_game;

	s32 fn_tafifo_poly_w(u32,pvoid,pvoid,u32);
	s32 fn_tafifo_yuv_w(u32,pvoid,pvoid,u32);
	s32 fn_aica_regs_w(u32,pvoid,pvoid,u32);
	s32 fn_asic_regs_w(u32,pvoid,pvoid,u32);
	s32 fn_sh4_regs_w(u32,pvoid,pvoid,u32);
	s32 fn_sh4_regs_r(u32,pvoid,pvoid,u32);

	virtual int _enterIRQ(int n,int v,u32 pc=0);

	virtual int write(u32,u32);
	virtual int read(u32,u32 *);
	int descrambl_buffer(const u8 *src, u8 *dst, u32 size);
private:
	struct __descrambler{
		u32 _seed;
		int *_idx;

		__descrambler(){_idx=NULL;};
		~__descrambler(){if(_idx) delete []_idx; _idx=NULL;};
		u32 rand();
		int load_chunk(const u8* &src, u8 *ptr, u32 sz);
		int descrambl(const u8 *src, u8 *dst, u32 size);
	} _descrambler;
	u8 *_ioreg;

};

};

#endif /* DCDEV_H */
