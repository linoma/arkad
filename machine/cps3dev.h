#include "arkad.h"

#ifndef __CPS3DEVH__
#define __CPS3DEVH__

namespace cps3{

#define ICORE_QUERY_GIC_LEVEL		0x10fe17

#define IOREG__(a,b) 	*((u32 *)&a[SL((b),2)])
#define IOREG(a) 		IOREG__(_ioreg,a)

#define DMA_SRC(a) 		IOREG(0x60+4*a)
#define DMA_DST(a) 		IOREG(0x61+4*a)
#define DMA_COUNT(a) 	IOREG(0x62+4*a)
#define DMA_CR(a) 		IOREG(0x63+4*a)

#define TIMER_REG(a)	IOREG(0x4 + a)

#define PPU_REG_(b,a) 		((u8 *)b + (a))
#define PPU_REG(a) 			PPU_REG_(_gpu_regs,(a))
#define PPU_REG16_(b,a) 	*((u16 *)PPU_REG_(b,a))
#define PPU_REG16(a) 		*((u16 *)PPU_REG(a))
#define PPU_STATUS 			PPU_REG16(0xe)

#define SS_REG_(b,a) *((u16 *)((u8 *)b + a))

#pragma pack(push,4)

#pragma pack(1)

#pragma pack(pop)

class CPS3M;
class CPS3DEV;

class CPS3DEV : private Runnable{
public:
	CPS3DEV();
	virtual ~CPS3DEV();
	virtual int Reset();
	virtual int Destroy();
	virtual int Init(CPS3M &);
	void OnRun();
protected:
	struct __dma : ICpuTimerObj{
		union{
			struct{
				unsigned int _enabled:1;
			};
			u16 _value;
		};
		u32 _dst,_src,_len;
		void *_mem;

		virtual int Run(u8 *,int){return 0;};
		virtual int _transfer();
		virtual void _reset();
		virtual void _init(void *);
	};

	struct __char_dma : __dma{
		virtual int _transfer();
		virtual void _init(void *);
	private:
		u32 _table_src;
		u16 _lastb,_lastb2;
		u8 _last_byte;

		void _transfer6bpp();
		void _transfer8bpp();
		u32 _decode6bpp(u8,u32,u32);
		u32 _decode8bpp(u8,u32);
	} _char_dma;

	struct __palette_dma : __dma{
		virtual int _transfer();
		virtual void _init(void *);
	private:
		u32 _fade;
	} _palette_dma;

	__dma *_dmas[5]={0,0,0,&_char_dma,&_palette_dma};

	struct __timer : ICpuTimerObj{
		enum {
			ICF  = 0x80,
			OCFA = 0x08,
			OCFB = 0x04,
			OVF  = 0x02,
			CCLRA = 0x01
		};

		union{
			struct{
				u16 _count;
				u8 _sc;
				u8 _ic;
			};
			u32 _status;
		};
		u16 _prescaler,_step,_ocra,_ocrb;
		int Init(u8 *mem,int port);
		int Run(u8 *mem,int cyc);
		int Reset();
		__timer();
		virtual ~__timer();
	} _timer;

	struct __wdt{
		union{
			struct{
				unsigned int _enable:1;
			};
			u32 _status;
		};
	} _wdt;

	struct __gic{
		u8 _vectors[8];
		u8 _levels[8];
		u32 _irq_pending;

		void reset();
	} _gic;

	struct __flash{
		enum{
			FM_NORMAL,  // normal read/write
			FM_READID,  // read ID
			FM_READSTATUS,  // read status
			FM_WRITEPART1,  // first half of programming, awaiting second
			FM_CLEARPART1,  // first half of clear, awaiting second
			FM_SETMASTER,   // first half of set master lock, awaiting on/off
			FM_READAMDID1,  // part 1 of alt ID sequence
			FM_READAMDID2,  // part 2 of alt ID sequence
			FM_READAMDID3,  // part 3 of alt ID sequence
			FM_ERASEAMD1,   // part 1 of AMD erase sequence
			FM_ERASEAMD2,   // part 2 of AMD erase sequence
			FM_ERASEAMD3,   // part 3 of AMD erase sequence
			FM_ERASEAMD4,   // part 4 of AMD erase sequence
			FM_BYTEPROGRAM,
			FM_BANKSELECT,
			FM_WRITEPAGEATMEL
		};

		union{
			struct{
				unsigned int _mode:4;
				unsigned int _status:8;
				unsigned int _load:1;
				unsigned int _active:1;
				unsigned int _bus:2;
				unsigned int _idx:3;
				unsigned int _command:8;
				unsigned int _prev:8;
			};
			u64 _value;
		};
		u32 _data,_cycles,_timeout;
		u8 *_mem;

		__flash(int);
		virtual ~__flash();
		int read(u32 a,u32 v,u32 f=0);
		void write(u32 a,u32 v,u32 f=0);
		void reset();
		void _reset();
	} *_flashs[8]={0};

	struct __eeprom{
		u16 _data;
		u16 _adr;
		u16 _status;

		u8 *_mem;
		void init(CPS3M &);
		void reset();
		int read(u32 a,u32 v,u32 f=0);
		void write(u32 a,u32 v,u32 f=0);
	} _eeprom;

	struct __cdrom : ICpuTimerObj{
		protected:
			int _write_reg(u32);
			void _read_reg();
			void _write_fifo(u8);
			void _write_fifo(void *,u8);
			int _process_command(u8);
			union{
				struct{
					unsigned int _reg_idx:5;
					unsigned int _con_mode:2;
					unsigned int _command_len:4;
					unsigned int _state:3;
					unsigned int _count:24;
					unsigned int _cr:12;
					unsigned int _cw:12;
				};
				u64 _status;
			};
			u8 *_regs,*_buffer;
			u32 _cycles;
			u16 _blocks;
			const int _size_buffer=4096;
		public:
			u8 _data;

			__cdrom();
			virtual ~__cdrom();
		//	int init(CPS3M &);
			void reset();
			int init(CPS3M &);
			int read(u32 a,u32 v,u32 f=0);
			int write(u32 a,u32 v,u32 f=0);
			int Run(u8 *mem,int cyc);

			enum : u8 {
				R_OWN_ID=0,
				R_CONTROL,
				R_COMMAND_STEP=0x10,
				R_STATUS=0x17,
				R_COMMAND,
				R_DATA,
				R_AUX_STATUS=0x1f
			};

			enum : u8{
				C_STEP_ZERO                     = 0x00,
				C_STEP_SELECTED                 = 0x10,
			};

			enum : u16 {
				IDLE = 1,
				FINISHED,

				DISC_SEL_ARBITRATION,

				INIT_MSG_WAIT_REQ,
				INIT_XFR,
				INIT_XFR_SEND_PAD_WAIT_REQ,
				INIT_XFR_SEND_PAD,
				INIT_XFR_RECV_PAD_WAIT_REQ,
				INIT_XFR_RECV_PAD,
				INIT_XFR_RECV_BYTE_ACK,
				INIT_XFR_RECV_BYTE_NACK,
				INIT_XFR_FUNCTION_COMPLETE,
				INIT_XFR_BUS_COMPLETE,
				INIT_XFR_WAIT_REQ,
				INIT_CPT_RECV_BYTE_ACK,
				INIT_CPT_RECV_WAIT_REQ,
				INIT_CPT_RECV_BYTE_NACK
			};
	} _cdrom;

	friend class CPS3M;
};

}

#endif