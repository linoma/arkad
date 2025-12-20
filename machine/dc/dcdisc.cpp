#include "dcdisc.h"
#include "elf.h"

namespace dc{

class ElFStream : public LElfFile,IGameStreamer{
public:
	ElFStream(char *p) : LElfFile(this){
		_e_machine=0x2a;
		_streamer=NULL;
		_filename=p;
		_folder=GameManager::getBasePath(p);
	};

	virtual ~ElFStream(){
	};

	virtual int Read(void *buf,u32 sz,u32 *po){
		if(!_streamer)
			return -1;
		return _streamer->Read(buf,sz,po);
	}

	virtual int Write(void *buf,u32 sz,u32 *po){
		if(!_streamer)
			return -1;
		return _streamer->Write(buf,sz,po);
	}

	virtual int Tell(u64 *a){
		if(!_streamer)
			return -1;
		return _streamer->Tell(a);
	}

	virtual int Seek(s64 a,u32 b){
		if(!_streamer)
			return -1;
		return _streamer->Seek(a,b);
	}

	virtual int Open(char *p,u32 a=0){
		if(!_streamer)
			if(!(_streamer=new FileStream())) return -1;
		return _streamer->Open(p);
	}

	virtual int Close(){
		if(!_streamer)
			return -1;
		return _streamer->Close();
	};

	virtual int Parse(IGame *,void *h){
		if(Open((char *)_filename.c_str()))
			return -1;
		if(LElfFile::Open())
			return -2;
		if(ProgramHeader == NULL) return -3;

		((DCEXEHEADER *)h)->_exe_pos=ProgramHeader[1].offset;
		((DCEXEHEADER *)h)->_exe_size=ProgramHeader[1].filesz;
		((DCEXEHEADER *)h)->_boot_pos=ProgramHeader[0].offset;
		((DCEXEHEADER *)h)->_boot_size=ProgramHeader[0].filesz;
		((DCEXEHEADER *)h)->_boot_addr=ProgramHeader[0].vaddr;
		((DCEXEHEADER *)h)->_exe_addr=ProgramHeader[1].vaddr;
		((DCEXEHEADER *)h)->_entry_addr=header.e_entry;
		return 0;
	};

	virtual int _getFilePosition(char *a,u64 **b){return -1;};

	virtual int _getInfo(u32 *psz,void **o){return -1;};
protected:
	string _folder,_filename;
	FileStream *_streamer;
};

DcGame::DcGame() : DiscGame(){
}

DcGame::~DcGame(){
}

int DcGame::Open(char *path,u32){
	int res;
	DCEXEHEADER p;

	_files.clear();
	if(!(_data = (IGameStreamer *)new ElFStream(path)))
		goto H;
	_type=1;
	if(!((IGameStreamer *)_data)->Parse(this,&p))
		goto Y;
	delete (ElFStream *)_data;
H:
	if(!(_data = (IGameStreamer *)new CDIStream(path)))
		goto Y;
	_type=2;
	if(!((CDIStream *)_data)->Parse(this,&p))
		goto Y;
	delete (CDIStream *)_data;
	_data=NULL;
	res=-10;
	_type=-1;
	goto Z;
Y:
	res=-20;
	if(NewHeader(&_header,0))
		goto Z;
	res=0;
	_machine="dc";
	memcpy(_header,&p,sizeof(DCEXEHEADER));
Z:
	if(res) Close();
	return res;
}

int DcGame::NewHeader(void **o,u32){
	if(!o)
		return -1;
	if(!(*o=new DCEXEHEADER[1]))
		return -2;
	return 0;
}

int DcGame::CopyHeader(void *pv,void *s,u32 f){
	*((u32 *)pv + 0) =	((DCEXEHEADER *)s)->_exe_pos;
	*((u32 *)pv + 1) =	((DCEXEHEADER *)s)->_exe_size;
	*((u32 *)pv + 2) = 	((DCEXEHEADER *)s)->_boot_pos;
	*((u32 *)pv + 3) = 	((DCEXEHEADER *)s)->_boot_size;
	*((u32 *)pv + 4) = 	((DCEXEHEADER *)s)->_exe_addr;
	*((u32 *)pv + 5) = 	((DCEXEHEADER *)s)->_boot_addr;
	*((u32 *)pv + 6) = 	((DCEXEHEADER *)s)->_entry_addr;
	return 0;
}

int DcGame::_getSectorPosition(u32 sec,void *out){
	DiscStream *disc;
	int res;
	u32 pos;

	disc=(DiscStream *)_data;
	res=-1;pos=0;
	for(auto it=disc->_tracks.begin();it!=disc->_tracks.end();it++){
		if(sec >= (*it)._lba && sec < ((*it)._lba + ((*it)._size / (*it)._sector_size))){
			u32 p=((sec - (*it)._lba)*(*it)._sector_size) + pos;
			if(out){
				((u32 *)out)[0]=p;
				((u32 *)out)[1]=(*it)._sector_size;
				((u32 *)out)[2]=(*it)._sync_size;
			}
			res=0;
			break;
		}
		pos += (*it)._size;
	}
	return res;
}

CDIStream::CDIStream(char *p):DiscStream(p),ISOStream(p) {
	_streamer=(ISOStream *)this;
}

CDIStream::~CDIStream(){
}

int CDIStream::_getFilePosition(char *a,u64 **b){
	return _streamer->ISOStream::_getFilePosition(a,b);
}

int CDIStream::Read(void *buf,u32 sz,u32 *po){
	u32 n,r;

	n=0;
	//printf("cd r %lx %u %x \n",ftell(fp),_data_size,sz);
	for(u8*p=(u8 *)buf;sz;){
		if(FileStream::Read(p,sz > _data_size ? _data_size : sz,&r))
			break;
		sz -= r;
		p += r;
		n += r;
		FileStream::Seek( (_sector_size-_data_size),SEEK_CUR);
	}
	if(po) *po=n;
	return 0;
}

int CDIStream::Parse(IGame *,void *h){
	u32 v;

	if(_streamer->Open((char *) _filename.c_str()))
		return -1;
	ISOStream::Seek(-8,SEEK_END);

	ISOStream::Read(&_version,4);
	ISOStream::Read(&v,4);
	if(_version==0x80000006)//V3.5
		ISOStream::Seek(-(s32)v,SEEK_END);
	else
		ISOStream::Seek(v,SEEK_SET);
	ISOStream::Read(&_ns,2);
	if(_ns!=2)
		return -10;
	for(u32 n=_ns,ofs=0;n;n--){
		__session s;
		v=0;
		ISOStream::Read(&v,2);
		s.first_track=_tracks.size();
		for(u32 nn=v;nn;nn--){
			_parse_track(nn,&ofs);
			ISOStream::Seek(29,SEEK_CUR);
			if(_version !=0x80000004){//V2
				ISOStream::Seek(5,SEEK_CUR);
				ISOStream::Read(&v,4);
				if(v==0xffffffff)
					ISOStream::Seek(78,SEEK_CUR);
			}
		}
		s.last_track=_tracks.size()-1;
		s.leadin_fad=_tracks[s.first_track]._lba;
		s.leadout_fad=_tracks[s.last_track]._lba;
		_sessions.push_back(s);
		v = 4 + 8;
		if (_version != v)
			v += 1;
		ISOStream::Seek(v,SEEK_CUR);
	}
	u8 tmp[0x10000];

	ISOStream::Seek(_tracks[_sessions[1].first_track]._pos+0x10*0x920,SEEK_SET);
	ISOStream::Read(tmp,0x10000);

	struct iso_pvd *pvd =(struct iso_pvd *)&tmp[4];

	ISOStream::Seek(_tracks[_sessions[1].first_track]._pos+8,SEEK_SET);
	ISOStream::Read(&_meta,sizeof(_meta));
	_meta.bootnme[15]=0;
	_meta.hwareid[15]=0;
	_meta.makerid[15]=0;

	u64 *r;

	u32 m[]={0x920,4,0x800,_tracks[_sessions[1].first_track]._pos,_tracks[_sessions[1].first_track]._lba};
	_streamer->Query(ISTREAM_QUERY_SET_GEOMETRY,m);

	if(!_getFilePosition((char *)"cdrom:\\1ST_READ.BIN;1",&r)){
		((DCEXEHEADER *)h)->_exe_pos=r[2];//*(u32 *)p->extent;
		((DCEXEHEADER *)h)->_exe_size=r[1];//*(u32 *)p->extent;
		((DCEXEHEADER *)h)->_exe_addr=0xac010000;//*(u32 *)p->extent;
		((DCEXEHEADER *)h)->_boot_pos=_tracks[_sessions[1].first_track]._pos+8;
		((DCEXEHEADER *)h)->_boot_size=2336*16;
		((DCEXEHEADER *)h)->_boot_addr=0xac008000;
		((DCEXEHEADER *)h)->_entry_addr=0xac008300;
		delete []r;
		//_iso9660_read(pos-_tracks[_sessions[1].first_track]._lba,tmp);
		return 0;
	}
	return -10;
}

int CDIStream::_parse_track(int idx,u32 *pofs){
	u8 cdi_start_mark[] = {0, 0, 1, 0, 0, 0, 255, 255, 255, 255},m[10],len;
	u32 v,pregap_len, track_len, sector_mode, lba, total_len, sector_type;
	int data_offset,sector_size,cdi_sector_sizes[] = {2048, 2336, 2352};
	__tracks t;

	ISOStream::Read(&v,4);
	if(v)
		ISOStream::Seek(8,SEEK_CUR);
	ISOStream::Read(m,10);
	if(memcmp(m,cdi_start_mark,10))
		return -1;
	ISOStream::Read(m,10);
	if(memcmp(m,cdi_start_mark,10))
		return -2;
	ISOStream::Seek(4,SEEK_CUR);

	ISOStream::Read(&len,1);
	ISOStream::Seek(len + 11 + 4 + 4,SEEK_CUR);
	ISOStream::Read(&v,4);
	if(v==0x80000000)
		ISOStream::Seek(8,SEEK_CUR);
	ISOStream::Seek(2,SEEK_CUR);
	ISOStream::Read(&pregap_len,4);
	ISOStream::Read(&track_len,4);
	ISOStream::Seek(6,SEEK_CUR);
	ISOStream::Read(&sector_mode,4);
	ISOStream::Seek(12,SEEK_CUR);
	ISOStream::Read(&lba,4);
	ISOStream::Read(&total_len,4);
	ISOStream::Seek(16,SEEK_CUR);
	ISOStream::Read(&sector_type,4);
	if (total_len != (pregap_len + track_len))
		return -3;
	if(sector_type >= sizeof(cdi_sector_sizes)/sizeof(int))
		return -5;
	sector_size = cdi_sector_sizes[sector_type];
	data_offset = *pofs + pregap_len * sector_size;
	t._number=idx;
	t._sector_size=sector_size;
	t._pos=data_offset;
	t._lba=lba;
	t._lba_end=lba+track_len-1;
	t._size=total_len * sector_size;
	t._filename="";
	t._type="";
	if(sector_mode==2  && sector_size==2336)
		t._sync_size= 8;
	_tracks.push_back(t);
#ifdef _DEVELOPa
	printf("st %x  %u %x %u %x %u %x %x\n",lba,sector_size,data_offset,total_len,(int)t._pos,sector_mode,t._lba,t._lba_end);
#endif
	*pofs += total_len * sector_size;
	return 0;
}

};
