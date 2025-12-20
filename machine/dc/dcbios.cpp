#include "dcbios.h"

namespace dc{

extern "C" char _binary_biosfont_bin_start[];
extern "C" char _binary_biosfont_bin_end[];
extern "C"  volatile u8* _binary_biosfont_bin_size;

#define BIOS_PATCH(src,dst) *(u32 *)&_mem[MI_RAM|(u16)src]=dst;*(u32 *)&_mem[MI_RAM+((u16)dst)]=0x9000b;

#define FLASH_PT_FACTORY 0
#define FLASH_PT_RESERVED 1
#define FLASH_PT_USER 2
#define FLASH_PT_GAME 3
#define FLASH_PT_UNKNOWN 4
#define FLASH_PT_NUM 5

static struct __gdrom_hle : ICpuTimerObj{
	enum {
		GD_STAT_BUSY,GD_STAT_PAUSE,	GD_STAT_STANDBY,GD_STAT_PLAY,
		GD_STAT_SEEK,GD_STAT_SCAN,	GD_STAT_OPEN,GD_STAT_NODISC,
		GD_STAT_RETRY,GD_STAT_ERROR
	};

	enum {
		CdDA=0x00,CdRom=0x10,CdRom_XA=0x20,CdRom_Extra=0x30,CdRom_CDI=0x40,
		GdRom=0x80,	NoDisk=0x1,Open=0x2,
	};

	enum : s32 {
		GDC_ERR = -1, GDC_OK, GDC_BUSY, GDC_COMPLETE, GDC_CONTINUE, GDC_SMPHR_BUSY, GDC_RET1, GDC_RET2
	};

	enum  {
		GDROM_REQ_CMD,GDROM_GET_CMD_STAT,GDROM_EXEC_SERVER,
		GDROM_INIT_SYSTEM, GDROM_GET_DRV_STAT,
		GDROM_G1_DMA_END,GDROM_REQ_DMA_TRANS,
		GDROM_CHECK_DMA_TRANS, GDROM_READ_ABORT,GDROM_RESET,GDROM_CHANGE_DATA_TYPE,
		GDROM_SET_PIO_CALLBACK,	GDROM_REQ_PIO_TRANS, GDROM_CHECK_PIO_TRANS
	};

	enum : u32 {
		GDCC_NONE = 0,	GDCC_PIOREAD = 0x10,
		GDCC_DMAREAD,GDCC_GETTOC,GDCC_GETTOC2,GDCC_PLAY,GDCC_PLAY2,
		GDCC_PAUSE,	GDCC_RELEASE,GDCC_INIT,	GDCC_READABORT,	GDCC_OPEN,	GDCC_SEEK,
		GDCC_DMA_READ_REQ,	GDCC_GETQINFO,	GDCC_REQ_MODE,	GDCC_SET_MODE,	GDCC_SCAN,
		GDCC_STOP,	GDCC_GETSCD,	GDCC_REQ_SES,	GDCC_REQ_STAT,
		GDCC_PIOREADREQ,	GDCC_MULTI_DMAREAD,	GDCC_MULTI_PIOREAD,
		GDCC_GET_VERSION,	GDCC_CMDA,	GDCC_CMDB,	GDCC_CMDC,	GDCC_CMDD,
		GDCC_CMDE,	GDCC_CMDF,	GDCC_CMDG, GDCC_REQ_DMA_TRANS = 0x106,	GDCC_REQ_PIO_TRANS = 0x10C
	};
	DcGame *_streamer;

	u8 *_mem;
	u32 status,command,_sector;
	u32 params[4],result[4];

	struct{
		u32 dst,count,total,sector;
		union{
			u32 d[4];
			struct{
				u32 pos;
				u32 sector_size;
				u32 sync_size;
			};
		};
	} _read;

	struct{
		u32 id,count;

		int add(){
			id=++count;
			return 0;
		};
		int reset(){
			id=count=0;
			return 0;
		}
	} _request;

	__gdrom_hle(){
		_streamer=NULL;
		reset();
	};

	int reset(){
		start_command(GDCC_NONE);
		memset(&_read,0,sizeof(_read));
		_request.reset();
		return 0;
	};

	int start_command(u32 cmd){
		status = GDC_OK;
		command = cmd;
		memset(params,0,sizeof(params));
		memset(result,0,sizeof(result));
		return 0;
	};

	int new_command(u32 cmd,u32 dst,u8 *_mem){
		start_command(cmd);
		_request.add();
		status=GDC_BUSY;
		if(dst){
			u32 *mem;

			mem = (u32 *)&_mem[MI_RAM + (dst & 0xffffff)];
			memcpy(params,mem,4*sizeof(u32));
		}
		return 0;
	}

	int execute(u8 *_mem){
		int res;

		res=0;
		switch(command){
			case GDCC_GET_VERSION:{
				char ver[] = "GDC Version 1.10 1999-03-31 ";
				u32 *mem,len = (u32)strlen(ver);

				mem = (u32 *)&_mem[MI_RAM + (params[0] & 0xffffff)];
				ver[len - 1] = 0x02;
				memcpy(mem,ver,len);
			}
			break;
			case GDCC_DMAREAD:{
				if(_read.count) return -1;
				_sector=_read.sector=params[0] & 0xffffff;
				_read.dst=params[2];
				_read.count=_read.total=params[1];
				_streamer->_getSectorPosition(_sector,_read.d);
				_streamer->Seek(_read.pos,SEEK_SET);
				return 1;
			}
			break;
			case GDCC_INIT:
			break;
			default:
				printf("gdrom command %x\n",command);
				EnterDebugMode();
			break;
		}
		status=GDC_COMPLETE;
		return res;
	};

	virtual int Run(u8 *,int,void *){
		int res;

		res = 0;
		//printf("Run %x %u\n",command,_read.count);
		switch(command){
			case GDCC_DMAREAD:{
				u32 n =_read.count;
				if(n > 5) n=5;
				//printf("read %d\n",n);
				for(u32 nn=0;nn<n;nn++){
					u32 *mem,buf[600];

					//_streamer->ReadSector(0,_sector,n,2048);
					_streamer->Read(buf,_read.sector_size,0);
					mem = (u32 *)&_mem[MI_RAM + (_read.dst & 0xffffff)];
					memcpy(mem,((u8 *)buf)+_read.sync_size,2048);
				}
				_read.count -= n;
				_read.dst += n*2048;
				_read.sector += n;
				if(_read.count==0){
					status=GDC_COMPLETE;
					result[3]=0;
					res=-1;
				}
				result[2]=(_read.total-_read.count)*2048;
			}
			break;
		}
		return res;
	};

	virtual int Query(u32,void *){return -1;};
} _gdrom_hle;

struct __flashrom : FileStream{
	u8 *_data;
	u32 _size,_mask,_state,_offset;

	enum {
		FS_Normal,FS_ReadAMDID1,FS_ReadAMDID2,FS_ByteProgram,
		FS_EraseAMD1,FS_EraseAMD2,FS_EraseAMD3,FS_SelectMode,
	};
	__flashrom(u32 sz = MS_FLASH) : FileStream() {_size=sz;_mask=sz-1;_state=0;_data=NULL;_offset=0;};

	int Init(int,void *a,void *b,u32){
		_data=(u8 *)a;
		return 0;
	};

	int reset(){
		_state=0;
		return 0;
	};

	int write(u32,u32){
		return -1;
	};

	int read(u32,u8 *){
		return -1;
	};

	int Read(void *buf,u32 sz,u32 *o=0){
		if(_offset+sz >= _size)
			sz=_size-_offset;
		memcpy(buf,&_data[_offset],sz);
		_offset += sz;
		if(o) *o=sz;
		return 0;
	}

	int Seek(s64 a,u32 b){
		switch(b){
			default:
				return -1;
			case SEEK_SET:
				_offset=b;
			break;
			case SEEK_CUR:
				_offset += (s32)b;
			break;
		}
		if(_offset >= _size)
			_offset=_size-1;
		return 0;
	};

	int Erase(u32 a,u32 sz){
		if(Seek(a,SEEK_SET)) return -1;
		if(_offset+sz>= _size)
			sz=_size-_offset;
		memset(&_data[_offset],0xff,sz);
		return 0;
	}

	int Write(void *buf,u32 sz,u32 *o=0){
		if(_offset+sz>=_size)
			sz=_size-_offset;
		memcpy(&_data[_offset],buf,sz);
		if(o) *o=sz;
		return 0;
	};

	void _getPartitionInfo(int part_id, u32 *offset, u32 *size){
		switch (part_id){
			case FLASH_PT_FACTORY:
				*offset = 0x1a000;
				*size = KB(8);
				break;
			case FLASH_PT_RESERVED:
				*offset = 0x18000;
				*size = KB(8);
				break;
			case FLASH_PT_USER:
				*offset = 0x1c000;
				*size = KB(8)*2;
				break;
			case FLASH_PT_GAME:
				*offset = 0x10000;
				*size = KB(8)*4;
				break;
			case FLASH_PT_UNKNOWN:
				*offset = 0x00000;
				*size = KB(8)*8;
				break;
			default:
				*offset = 0;
				*size = 0;
				break;
		}
	};
} _flashrom;

dcbios::dcbios() : ASIC(){
}

dcbios::~dcbios(){
}

int dcbios::Reset(){
	_flashrom.Init(0,&_mem[MI_FLASH],0,0);
	return ASIC::Reset();
}

int dcbios::Init(int n,void *a,void *b,u32 f){
	if(ASIC::Init(n,a,b,f)) return -1;
	return _flashrom.Init(0,&_mem[MI_FLASH],SHCORECPU::_ioreg,f);
}

int dcbios::gdrom_hle_proc(){
	DLOG("GDROM %x %x",REG_[7],_gdrom_hle.status);
	switch(REG_[7]){
		case _gdrom_hle.GDROM_REQ_CMD:{//0
			if(_gdrom_hle.status!=_gdrom_hle.GDC_OK)
				REG_[0]=0;
			else{
				_gdrom_hle.new_command(REG_[4],REG_[5],_mem);
				REG_[0]=_gdrom_hle._request.id;
			}
		}
		break;
		case _gdrom_hle.GDROM_GET_CMD_STAT:{//1
			u32 *mem;

			mem = (u32 *)&_mem[MI_RAM + (REG_[5] & 0xffffff)];
			memcpy(mem,_gdrom_hle.result,sizeof(_gdrom_hle.result));
			if(_gdrom_hle.status == _gdrom_hle.GDC_OK || _gdrom_hle.status == _gdrom_hle.GDC_BUSY)
				REG_[0]=_gdrom_hle.status;
			else if(REG_[4] != _gdrom_hle._request.id)
				REG_[0]=_gdrom_hle.GDC_OK;
			else{
				REG_[0]=_gdrom_hle.status;
				if(_gdrom_hle.status != _gdrom_hle.GDC_CONTINUE)
					_gdrom_hle.status=_gdrom_hle.GDC_OK;
			}
		}
		break;
		case _gdrom_hle.GDROM_EXEC_SERVER://2
			_gdrom_hle._streamer=_game;
			_gdrom_hle._mem=_mem;
			if(_gdrom_hle.status==_gdrom_hle.GDC_BUSY){
				//if(_gdrom_hle.command==0x11) EnterDebugMode();
				if(_gdrom_hle.execute(_mem)==1)
					AddTimerObj(&_gdrom_hle,_gdrom_hle._read.count*2048);//_gdrom_hle._read.count*2048
			}
		break;
		case _gdrom_hle.GDROM_INIT_SYSTEM://3
			_gdrom_hle={};
		break;
		case _gdrom_hle.GDROM_GET_DRV_STAT:{//4
			u32 *mem;

			mem = (u32 *)&_mem[MI_RAM + (REG_[4] & 0xffffff)];
			REG_[0]=_gdrom_hle.GDC_OK;
			mem[0]=_gdrom_hle.GD_STAT_PAUSE;
			mem[1]=0x80;
		}
		break;
		case _gdrom_hle.GDROM_RESET://9
			_gdrom_hle.reset();
		break;
		default:
			cout << __FUNCTION__<< " " << REG_[7] << endl;
			EnterDebugMode();
			break;
	}
	return 0;
}

int dcbios::flashrom_hle_proc(){
	DLOG("FLASH %x",REG_[7]);
	switch(REG_[7]){
		case 0:{
			u32 part = REG_[4];
			if (part < FLASH_PT_NUM){
				u32 offset, size;
				u32 *mem;

				GET_RAM_PTR(mem,REG_[5]);
				_flashrom._getPartitionInfo(part, &offset, &size);
				mem[0]=offset;
				mem[1]=size;
				REG_[0] = 0;
			}
			else
				REG_[0] = -1;
		}
		break;
		case 1:{	//FLASHROM_READ
			u32 *mem;
			_flashrom.Seek(REG_[4],SEEK_SET);
			GET_RAM_PTR(mem,REG_[5]);
			_flashrom.Read(mem,REG_[6]);
			REG_[0] = 0;
		}
		break;
		case 2:{	//FLASHROM_WRITE
			u32 *mem;

			GET_RAM_PTR(mem,REG_[5]);
			_flashrom.Seek(REG_[4],SEEK_SET);
			_flashrom.Write(mem,REG_[6]);
			REG_[0] = REG_[6];
		}
		break;
		case 3:{	//FLASHROM_DELETE
			REG_[0]=-1;
			for (int part = 0; part < FLASH_PT_NUM; part++){
				u32 part_offset,size;
				_flashrom._getPartitionInfo(part, &part_offset, &size);
				if (REG_[4] == (u32)part_offset){
					REG_[0] = 0;
					_flashrom.Erase(part_offset,size);
					break;
				}
			}
		}
		break;
		default:
			EnterDebugMode();
		break;
	}
	return 0;
}

int dcbios::system_hle_proc(){
	DLOG("SYS %x",REG_[7]);
	switch(REG_[7]){
		case 0:
			REG_[0]=0x300;
		break;
		default:
		cout << __FUNCTION__<< " " << REG_[7] << endl;
			EnterDebugMode();
		break;
	}
	return 0;
}

int dcbios::sys_font_hle_proc(){
	DLOG("SYSFONT %x",REG_[1]);
	switch(REG_[1]){
		case 0:
			REG_[0]=FONT_TABLE_ADDR;
		break;
		case 1:
		case 2:
			REG_[0]=0;
		break;
		default:
		cout << __FUNCTION__<< " " << REG_[1] << endl;
			EnterDebugMode();
		break;
	}
	return 0;
}

int dcbios::Load(IGame *pg,char *fn){
		//bios hack
	BIOS_PATCH(0xc0000E0,0xc000800);
	BIOS_PATCH(0xc0000B0,0xc001000);
	BIOS_PATCH(0xc0000B4,0xc001004);
	BIOS_PATCH(0xc0000B8,0xc001008);
	BIOS_PATCH(0xc0000BC,0xc00100C);
	BIOS_PATCH(0xc0000c0,0xc001010);
//printf("bios %x\n",_binary_biosfont_bin_end-_binary_biosfont_bin_start);
	memcpy(&_mem[MI_BIOS+(FONT_TABLE_ADDR & (MS_BIOS-1))],_binary_biosfont_bin_start,_binary_biosfont_bin_end-_binary_biosfont_bin_start);
	return 0;
}

};