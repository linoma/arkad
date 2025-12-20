#include "gre.h"
#include "general_device.h"

#ifndef __DCPVR2H__
#define __DCPVR2H__

namespace dc{

#define PVR2_BASE 				0x5f8000
#define PVR2_REG(a) 			IOREG(PVR2_BASE + a)

#define PVR2_ID							(0x00000000)
#define PVR2_REVISION					(0x00000004)
#define PVR2_SOFTRESET					(0x00000008)

#define PVR2_STARTRENDER				(0x00000014)
#define PVR2_TEST_SELECT				(0x00000018)

#define PVR2_PARAM_BASE					(0x00000020)

#define PVR2_REGION_BASE				(0x0000002C)
#define PVR2_SPAN_SORT_CFG				(0x00000030)

#define PVR2_VO_BORDER_COL				(0x00000040)
#define PVR2_FB_R_CTRL					(0x00000044)
#define PVR2_FB_W_CTRL					(0x00000048)
#define PVR2_FB_W_LINESTRIDE			(0x0000004C)
#define PVR2_FB_R_SOF1					(0x00000050)
#define PVR2_FB_R_SOF2					(0x00000054)

#define PVR2_FB_R_SIZE					(0x0000005C)
#define PVR2_FB_W_SOF1					(0x00000060)
#define PVR2_FB_W_SOF2					(0x00000064)
#define PVR2_FB_X_CLIP					(0x00000068)
#define PVR2_FB_Y_CLIP					(0x0000006C)

#define PVR2_FPU_SHAD_SCALE				(0x00000074)
#define PVR2_FPU_CULL_VAL				(0x00000078)
#define PVR2_FPU_PARAM_CFG				(0x0000007C)
#define PVR2_HALF_OFFSET				(0x00000080)
#define PVR2_FPU_PERP_VAL				(0x00000084)
#define PVR2_ISP_BACKGND_D				(0x00000088)
#define PVR2_ISP_BACKGND_T				(0x0000008C)

#define PVR2_ISP_FEED_CFG				(0x00000098)

#define PVR2_SDRAM_REFRESH				(0x000000A0)
#define PVR2_SDRAM_ARB_CFG				(0x000000A4)
#define PVR2_SDRAM_CFG					(0x000000A8)

#define PVR2_FOG_COL_RAM				(0x000000B0)
#define PVR2_FOG_COL_VERT				(0x000000B4)
#define PVR2_FOG_DENSITY				(0x000000B8)
#define PVR2_FOG_CLAMP_MAX				(0x000000BC)
#define PVR2_FOG_CLAMP_MIN				(0x000000C0)
#define PVR2_SPG_TRIGGER_POS			(0x000000C4)
#define PVR2_SPG_HBLANK_INT				(0x000000C8)
#define PVR2_SPG_VBLANK_INT				(0x000000CC)
#define PVR2_SPG_CONTROL				(0x000000D0)
#define PVR2_SPG_HBLANK					(0x000000D4)
#define PVR2_SPG_LOAD					(0x000000D8)
#define PVR2_SPG_VBLANK					(0x000000DC)
#define PVR2_SPG_WIDTH					(0x000000E0)
#define PVR2_TEXT_CONTROL				(0x000000E4)
#define PVR2_VO_CONTROL					(0x000000E8)
#define PVR2_VO_STARTX					(0x000000EC)
#define PVR2_VO_STARTY					(0x000000F0)
#define PVR2_SCALER_CTL					(0x000000F4)
#define PVR2_PAL_RAM_CTRL				(0x00000108)
#define PVR2_SPG_STATUS					(0x0000010C)
#define PVR2_FB_BURSTCTRL				(0x00000110)
#define PVR2_FB_C_SOF					(0x00000114)
#define PVR2_Y_COEFF					(0x00000118)
#define PVR2_PT_ALPHA_REF				(0x0000011C)

#define PVR2_TA_OL_BASE         		0x00000124
#define PVR2_TA_ISP_BASE        		0x00000128
#define PVR2_TA_OL_LIMIT        		0x0000012C
#define PVR2_TA_ISP_LIMIT       		0x00000130
#define PVR2_TA_NEXT_OPB        		0x00000134
#define PVR2_TA_ITP_CURRENT     		0x00000138
#define PVR2_TA_GLOB_TILE_CLIP  		0x0000013C
#define PVR2_TA_ALLOC_CTRL      		0x00000140
#define PVR2_TA_LIST_INIT       		0x00000144
#define PVR2_TA_YUV_TEX_BASE    		0x00000148
#define PVR2_TA_YUV_TEX_CTRL    		0x0000014C
#define PVR2_TA_YUV_TEX_CNT     		0x00000150
#define PVR2_TA_LIST_CONT       		0x00000160
#define PVR2_TA_NEXT_OPB_INIT   		0x00000164
#define PVR2_SIGNATURE1         		0x00000180
#define PVR2_SIGNATURE2         		0x00000184
#define PVR2_FOG_TABLE_START        	0x00000200
#define PVR2_FOG_TABLE_END          	0x000003FC
#define PVR2_TA_OL_POINTERS_START   	0x00000600
#define PVR2_TA_OL_POINTERS_END     	0x00000F5C
#define PVR2_PALETTE_RAM_START      	0x00001000
#define PVR2_PALETTE_RAM_END        	0x00001FFC

class PVR2 : public GRE{
public:
	PVR2();
	virtual ~PVR2();
	virtual int Init(int,void *,void *,u32);
	virtual int Reset();
	virtual int Run(u8 *,int,void *);
	virtual int Update(u32 flags=0);
protected:
	virtual int write(u32,u32);
	virtual int read(u32,u32 *);
	virtual int _createTexture(void *,void *,void *dst=0);
private:
	u8 *_ioreg;
};

};

#endif