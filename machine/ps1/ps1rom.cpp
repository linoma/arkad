#include "ps1rom.h"
#include <string.h>
#include <stdlib.h>
#include <memory.h>

namespace ps1{

PS1ROM::PS1ROM() : Game(){
	_header=NULL;
	_type=-1;
}

PS1ROM::~PS1ROM(){
}

int PS1ROM::Open(char *path,u32){
	PS1EXE_HEADER p;
	int res=-1;

	_files.clear();
	if(!(_data = new RomStream(path)))
		goto Z;
	_type=1;
	if(!((RomStream *)_data)->Parse(this,&p))
		goto Y;
	delete (RomStream *)_data;
	if(!(_data = new PS1CUEROM(path)))
		goto Y;
	_type=2;
	if(!((PS1CUEROM *)_data)->Parse(this,&p))
		goto Y;
	delete (PS1CUEROM *)_data;
	_data=NULL;
	res=-10;
	_type=-1;
	goto Z;
Y:
	res=-20;
	if(!(_header = new PS1EXE_HEADER[1]))
		goto Z;
	res=0;
	_machine="ps1";
	memcpy(_header,&p,sizeof(PS1EXE_HEADER));
	((RomStream *)_data)->SeekToStart();
Z:
	if(res) Close();
	return res;
}

int PS1ROM::Read(void *buf,u32 sz,u32 *po){
	if(!_data)
		return -1;
	//fread(PSXM(SWAP32(tmpHead.t_addr)), SWAP32(tmpHead.t_size), 1, (FILE *)_data);
	return ((RomStream *)_data)->Read(buf,sz,po);
}

int PS1ROM::Write(void *buf,u32 sz,u32 *po){
	if(!_data)
		return -1;
	return -1;
}

int PS1ROM::Seek(s64 a,u32 b){
	if(!_data)
		return -1;
	//fread(PSXM(SWAP32(tmpHead.t_addr)), SWAP32(tmpHead.t_size), 1, (FILE *)_data);
	return ((RomStream *)_data)->Seek(a,b);
}

int PS1ROM::Tell(u64 *a){
	if(!_data)
		return -1;
	//fread(PSXM(SWAP32(tmpHead.t_addr)), SWAP32(tmpHead.t_size), 1, (FILE *)_data);
	return ((RomStream *)_data)->Tell(a);
}

int PS1ROM::Close(){
	if(_data)
		delete (RomStream *)_data;
	_data=NULL;
	_type=-1;
	if(_header)
		delete []((u8 *)_header);
	_header=NULL;
	return 0;
}

int PS1ROM::Query(u32 w,void *pv){
	switch(w){
		case IGAME_SET_ISO_MODE:{
			if(!_data || !_header || !pv)
				return -3;
			return ((PS1CUEROM *)_data)->Query(ISTREAM_QUERY_SET_BLOK_SIZE,pv);
		}
		case IGAME_GET_INFO:
			if(Game::Query(w,pv))
				return -1;
			if(_header){
				*((u32 *)pv + IGAME_GET_INFO_SLOT)=((PS1EXE_HEADER *)_header)->t_addr;
				*((u32 *)pv + IGAME_GET_INFO_SLOT+1)=((PS1EXE_HEADER *)_header)->t_size;
				*((u32 *)pv + IGAME_GET_INFO_SLOT+2)= ((PS1EXE_HEADER *)_header)->pc0;
				*((u32 *)pv + IGAME_GET_INFO_SLOT+3) = ((PS1EXE_HEADER *)_header)->gp0;
				*((u32 *)pv + IGAME_GET_INFO_SLOT+4) = ((PS1EXE_HEADER *)_header)->s_adr;
				if (*((u32 *)pv + IGAME_GET_INFO_SLOT+4) == 0)
					*((u32 *)pv + IGAME_GET_INFO_SLOT+4)= 0x801fff00;
			}
			return 0;
		case IGAME_GET_ISO_INFO:
			if(_type==2){
				u32 sz,d[10];
				char o[10000],*p,*pp;
				void *v;

				if(!_data || !_header || !pv)
					return -3;
				memcpy(d,pv,sizeof(d));
				if(!d[2]){
					v=o;
					sz=sizeof(o);
				}
				else{
					sz=d[1];
					v = *(void **)&d[2];
				}

				if(((PS1CUEROM *)_data)->_getInfo(&sz,&v))
					return -4;

				((u32 *)pv)[0]=sz;
				((u32 *)pv)[1]=1;
				((u32 *)pv)[2]=((u32  *)v)[0];
				((u32 *)pv)[3]=0;
				((u32 *)pv)[4]=0;
				((u32 *)pv)[5]=0;
				p =  &((char*)v)[4];

				for(u32 i=1,pos=0;*p;i++){
					pp=p;

					p+=strlen(p)+1;
					p+=strlen(p)+1;
					((u32 *)pv)[5] = (pos=((u32 *)p)[1])  + ((u32 *)p)[0];

					if(((d[3] & 2)==0 && i ==  d[0]) ||
						((d[3] & 2) && d[0] >=pos && d[0] <=((u32 *)pv)[5])
					){
						((u32 *)pv)[3]=((u32 *)p)[0];
						((u32 *)pv)[4]=((u32 *)p)[1];
						((u32 *)pv)[6]=i;
						if(d[3] & 1)
							strcpy((char *)&((u32 *)pv)[7],pp);
						//return 0;
					}
					p  += sizeof(u32)*2;
				}

				return 0;
			}
			return -2;
		case IGAME_GET_ISO_FILEI:
			if(!_data || !_header || !pv)
				return -3;
			switch(_type){
				case 2:{
					u64 *r;

					if(!((PS1CUEROM *)_data)->_getFilePosition(*(char **)pv,&r)){
						((u64 *)pv)[0]=r[0];
						((u64 *)pv)[1]=r[1];
						((u64 *)pv)[2]=r[2];
						((u64 *)pv)[3]=r[3];
						delete []r;
						return 0;
					}
				}
					break;
			}
			return -1;
		default:
			return Game::Query(w,pv);
	}
}

PS1CUEROM::PS1CUEROM() : RomStream(){
	_size=0;
	_start=0;
}

PS1CUEROM::PS1CUEROM(char *p) : RomStream(p){
	_size=0;
	_start=0;
}

PS1CUEROM::~PS1CUEROM(){
}

int PS1CUEROM::_getInfo(u32 *psz,void **o){
	char *p,*pp;
	u32 sz,pos;
	int res;

	if(!psz) return -1;
	sz=sizeof(u32)*2;
	res=-2;
	for(auto it=_tracks.begin();it!=_tracks.end();it++){
		sz += (*it)._filename.length() + 1;
		sz += (*it)._type.length() + 1;
		sz+=sizeof(u32)*2;
	}
	////cout << " size " << sz << " "<<_tracks.size() << endl;
	if(!o) {
		res=0;
		goto Z;
	}
	res--;
	if(sz > *psz)
		goto Z;
	p=(char *)*o;
	res--;
	*(u32 *)p=_tracks.size();
	pp=p+sizeof(u32);
	pos=0;
	for(auto it=_tracks.begin();it!=_tracks.end();it++){
		strcpy(pp,(*it)._filename.c_str());
		pp += (*it)._filename.length() + 1;
		strcpy(pp,(*it)._type.c_str());
		pp += (*it)._type.length() + 1;
		((u32 *)pp)[0]=(*it)._size;
		((u32 *)pp)[1]=pos;
		pp+=sizeof(u32)*2;
		pos += (*it)._size;
	}
	*(u32 *)pp = 0;

	res=0;
Z:
	*psz=sz;
	return res;
}

int PS1CUEROM::Parse(IGame *g,PS1EXE_HEADER *_header){
	__tracks *pt;
	int res=-1;

	_tracks.empty();
	if(FileStream::Open((char *)_filename.c_str()))
		return -1;
	FileStream::Seek(0,SEEK_SET);

	while(!feof(fp)){
		char c[256],cc[256],*tok;

		if(!fgets(c,sizeof(c),fp))
			break;
		strncpy(cc,c,sizeof(c));
		tok = strtok(cc, " ");
		if (tok == NULL)
			continue;
		if (!strcmp(tok, "TRACK")) {
			char *pp,*p=tok+6;
			for(int i=0;(pp=strtok(p," "));i++){
				switch(i){
					case 0:
						sscanf(pp,"%d",&pt->_number);
						break;
					case 1:
						for(p=pp;isalnum(*p);p++);
						*p=0;
						pt->_type=pp;
						break;
				}
			//	cout << pp;
				p=pp+strlen(pp)+1;
			}
		//	cout <<endl;
		}
		else if (!strcmp(tok, "INDEX")) {
		}
		else if (!strcmp(tok, "PREGAP")) {
		}
		else if (!strcmp(tok, "FILE")) {
			char tmpb[257],*p;

			pt=NULL;
			sscanf(c, "FILE %255c", tmpb);
			p=tmpb;
			while(!isalnum(*p)) p++;
			for(char *pp=p;*pp;pp++){
				if(!isprint(*pp) || *pp=='"'){
					*pp=0;
					break;
				}
			}
			string s = _folder +"/"+(char *)p;
			FILE *f=fopen(s.c_str(),"rb");
			if(f){
				g->AddFile(p,1);
				fseek(f,0,SEEK_END);
				u32 sz=(u32)ftell(f);
				_size += sz;
				_tracks.push_back({s,sz});
				fclose(f);
				pt=&_tracks.back();
			}
		}
	}
#ifdef _DEVELOPa
for(auto it=_tracks.begin();it!=_tracks.end();it++)
	cout << (*it)._filename << " " << (*it)._size << " " << (*it)._type << " " << (*it)._number<< endl;
#endif

	{
		struct iso_directory_record *p;
		char _buf[CD_FRAMESIZE_RAW];
		u64 *r;

		g->GetFile(0,(char *)_buf);
		string s = _folder +"/"+(char *)_buf;

		fclose(fp);
		fp=fopen(s.c_str(),"rb");
		if(!_getFilePosition((char *)"cdrom:\\SYSTEM.CNF;1",&r)){
			int i,ii;

			_iso9660_read(r[0],_buf);
			delete []r;
			char _fn[1024],*pp;
				if(!(pp=strstr((char *)_buf+12,"cdrom:"))) goto Z;
				res--;
				for(char *s=pp;*s;s++){
					if(*s=='\n' || *s=='\r'){
						*s=0;
						break;
					}
				}

				for(i=0;pp[6+i];i++) _fn[i]=toupper(pp[6+i]);
				*((u32 *)&_fn[i])=0;
				pp=_fn;
				if(_getFilePosition(pp,&r)) goto Z;

				res--;

				u32 pos=r[0];//*(u32 *)p->extent;

				delete []r;
				_iso9660_read(pos,_buf);
				memcpy(_header,&_buf[12],sizeof(PS1EXE_HEADER));
				_start=2048 + pos*CD_FRAMESIZE_RAW + (CD_FRAMESIZE_RAW-CD_ISO_BLOCK_SIZE) + CDIO_CD_CHUNK_SIZE;
				return 0;//2352+(2352-2038)+23
		}
	}

Z:
	if(fp) fclose(fp);
	fp=0;
	return res;
}

int PS1CUEROM::Read(void *buf,u32 sz,u32 *po){
	u32 n,r;

	n=0;
	//printf("cd r %lu\n",ftell(fp));
	for(u8*p=(u8 *)buf;sz;){
		if(FileStream::Read(p,sz > _szBlock ? _szBlock : sz,&r))
			break;
		sz -= r;
		p += r;
		n += r;
		FileStream::Seek( (CD_FRAMESIZE_RAW-_szBlock),SEEK_CUR);
	}
	if(po) *po=n;
	return 0;
}

PS1ECMROM::PS1ECMROM(): PS1CUEROM(){
}

PS1ECMROM::PS1ECMROM(char *p): PS1CUEROM(p){
}

PS1ECMROM::~PS1ECMROM(){
}

int PS1ECMROM::Parse(IGame *,PS1EXE_HEADER *h){
	_tracks.empty();
	if(FileStream::Open((char *)_filename.c_str()))
		return -1;
	FileStream::Seek(0,SEEK_SET);
	return -1;
}

RomStream::RomStream() : ISOStream(){
	_start=2048;
}

RomStream::RomStream(char *p) : ISOStream(p){
	_start=2048;
}

RomStream::~RomStream(){
}

int RomStream::Parse(IGame *game,PS1EXE_HEADER *p){
	if(ISOStream::Open((char *)_filename.c_str()))
		return -1;
	Read(p,sizeof(PS1EXE_HEADER));
	if(memcmp(p->id,"PS-X EXE", 8))
		return -2;
	Seek(0,SEEK_SET);
	Read(p,sizeof(PS1EXE_HEADER));
	game->AddFile((char *)_filename.c_str(),1);
	return 0;
}

int RomStream::SeekToStart(s64 a){
	return ISOStream::Seek(_start+a,SEEK_SET);
}

};