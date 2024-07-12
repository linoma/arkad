#include "game.h"
#include  <iostream>
#include "1943.h"
#include "tehkanwc.h"
#include  "popeye.h"

void *GameManager::hInstance=NULL;

Game::Game(){
	_status=0;
	_data=NULL;
	_pos=0;
}

Game::~Game(){
}

int Game::_findMatch(__item &item,string &s){
	int len,res=-1,lx,px;
	char *ps,*pd,*cpd,*cps;

	if(BVT(item._attr,2))
		return 0;
	string name=item._name;
	len = name.length();
	lx=0;
#ifdef __WIN32__
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;

	s+=DPS_PATH;
	s+="*";

	if((hFind = FindFirstFile(s.c_str(), &FindFileData)) == INVALID_HANDLE_VALUE)
		return -1;
	do{
		pd=FindFileData.cFileName;
#else
	struct dirent *dp;

	DIR *dirp = opendir(s.c_str());
	if (dirp == NULL)
		return -1;
	while ((dp = readdir(dirp)) != NULL) {
		pd=dp->d_name;
#endif
		int m,mx,i,l,p,pc,ln = strlen(pd);
		cps=ps=(char *)name.c_str();
		cpd=pd;

		for(m=mx=l=i=p=pc=0;*ps && *pd;){
			if(*ps == *pd){
				if(m++==0)
					p=ps-cps;
				if(m > mx) mx=m;
				pc++;
				l++;
				ps++;
				pd++;
			}
			else{
				m=0;
				i++;
				if(ln>len)
					pd++;
				else
					ps++;
			}
		}
#ifdef _DEVELOPa
		cout << dp->d_name << " len " << len << " " << mx << " "<<  p<<  " " << i << "  " << l<< " " << name << endl;
#endif
		if(l >=lx){
			if(p >=px || l>lx){
				s=cpd;
				px=p;
			}
			lx=l;
			if(l==len){
				res=0;
				goto A;
			}
		}
	//
	}
#ifdef __WIN32__
	while(FindNextFile(hFind, &FindFileData));
A:
	FindClose(hFind);
#else
A:
	closedir(dirp);
#endif
	cout<< name << " " << s  << " " << lx << " "<< len << endl;
	return res;
}

int Game::Query(u32 what,void *pv){
	switch(what){
		case IGAME_GET_INFO:
			*((u32 *)pv +  0) = _files.size();
			*((u32 *)pv +  1) = (u32)_status;
			return 0;
		case IGAME_GET_MACHINE:
			strcpy((char *)pv,_machine.c_str());
			return 0;
		case IGAME_GET_NAME:
			strcpy((char *)pv,_name.c_str());
			return 0;
		case IGAME_GET_FILENAME:
			if(_files.size() == 0 || !pv)
				return -2;
		{
			__item i = _files[0];
			strcpy((char *)pv,i._name.c_str());
		}
			return 0;
		case IGAME_GET_DISK_IMAGE:{
			for(auto it=_files.begin();it!=_files.end();it++){
				if((*it)._attr & BV(2)){
					strcpy((char *)pv,(*it)._name.c_str());
					return 0;
				}
			}
		}
			return -1;
	}
	return -1;
}

int Game::AddFile(char *p,u32 type){
	_files.push_back({p,(u32)type});
	return 0;
}

int Game::GetFile(u32 n,char *o){
	if(n>=_files.size())
		return -1;
	if(!o) return _files[n]._name.length();
	*o=0;
	strcpy(o,_files[n]._name.c_str());
	return 0;
}

GameManager::GameManager() : vector<IGame *>(){
	_game=_machine=0;
	GameManager::hInstance=this;
}

GameManager::~GameManager(){
	for(auto it=rbegin();it != rend();it++)
		delete (Game *)(*it);
}

string GameManager::getFilename(string path){
	string filename;

	filename=path;
	{
		size_t last_slash_idx;

		last_slash_idx = filename.find_last_of("\\/");
		if (std::string::npos != last_slash_idx){
			filename.erase(0,last_slash_idx+1);
		}
	}
	return filename;
}

string GameManager::getBasePath(string path){
	string filename,basepath;

	filename=path;
	{
		size_t last_slash_idx;

		last_slash_idx = filename.find_last_of("\\/");
		if (std::string::npos != last_slash_idx){
			basepath=filename;
			basepath.erase(last_slash_idx);
			//filename.erase(0,last_slash_idx+1);
		}
	}
	return basepath;
}

int GameManager::Init(){
	_machines.insert(std::make_pair("cps3",__machine(0,0,"Capcom System III")));
	_machines.insert(std::make_pair("blktiger",__machine(1,0)));
	_machines.insert(std::make_pair("cps2",__machine(2,0)));
	_machines.insert(std::make_pair("1943",__machine(3,0)));
	_machines.insert(std::make_pair("twcp",__machine(4,0)));
	_machines.insert(std::make_pair("ps1",__machine(5,1,"Playstation 1")));
	_machines.insert(std::make_pair("popeye",__machine(6,0)));
#ifdef _DEVELOP
	_machines.insert(std::make_pair("amiga",__machine(7,1)));
	_machines.insert(std::make_pair("ps2",__machine(8,1,"Playstation 2")));
#endif
	GameManager::RegisterGame(new sfiii3n());
	GameManager::RegisterGame(new redearthr1());
	GameManager::RegisterGame(new redearthn());
	GameManager::RegisterGame(new blacktiger());
	GameManager::RegisterGame(new c1944j());
	GameManager::RegisterGame(new M1943::M1943Game());
	GameManager::RegisterGame(new tehkanwc::TWCPGame());
	GameManager::RegisterGame(new POPEYE::PopeyeGame());

	return 0;
}

GameManager::MACHINE &GameManager::MachineFromIndex(u32 n){
	MACHINE *a=0;
	u32 i=0;

	if(n==-1)
		n=_machine-1;
	for(auto it=_machines.begin();it!=_machines.end();it++){
		if(i++==n){
			cout << (*it).first;
			return (*it).second;
		}
	}
	return *a;
}


int GameManager::Find(char *p,u32 *pg,u32 *pm){
	u32 i;
	int res;

	if(!p || !*p)
		return -1;

	string filename = getFilename(p);
	i=0;
	res=-2;
	for(auto it=begin();it!=end();it++,i++){
		char c[1024];

		*((u64 *)c)=0;
		if(!(*it)->Query(IGAME_GET_NAME,c)){
			if(filename==c){
				res=0;
				goto A;
			}
		}
		*((u64 *)c)=0;
		if(!(*it)->Query(IGAME_GET_FILENAME,c)){
			if(filename==c){
				res=0;
				goto A;
			}
		}
		strcpy(c,filename.c_str());
		u64 u[2];

		u[0]=(u64)c;
		u[1]=0;
		if(!(*it)->Query(IGAME_SEARCH_FILENAME,u)){
			res=0;
			goto A;
		}
	}
A:
	if(pg)
		*pg=i+1;
	if(!pm || res)
		goto B;

	{
		u32 m;

		IGame *p = (*this)[i];
		*pm=0;
		if(!MachineIndexFromGame((IGame *)p,&m))
			*pm=m;
	}
B:
	return res;
}

int GameManager::Load(char *path,void *dst){
	u32 i;

	i=0;
	if(Find(path,&i,NULL))
		return -1;
	return -1;
}

int GameManager::Load(char *,IMachine *){
	return -1;
}

int GameManager::MachineIndexFromGame(IGame *p,u32 *i){
	char c[100];
	u32 mi;
	int res=-2;

	*((u64 *)c)=0;
	res=-1;
	mi=0;
	if(p->Query(IGAME_GET_MACHINE,c))
		return res;
	res--;
	for(int i=0;c[i];i++)
		c[i]=tolower(c[i]);
	auto it=_machines.find(c);
	if(it==_machines.end()) goto A;

	if(!strcmp(c,"cps3")){
		mi=0;
		res=0;
		goto A;
	}
	if(!strcmp(c,"blktiger")){
		mi=1;
		res=0;
		goto A;
	}
	if(!strcmp(c,"cps2")){
		mi=2;
		res=0;
		goto A;
	}
	if(!strcmp(c,"1943")){
		mi=3;
		res=0;
		goto A;
	}
	if(!strcmp(c,"twcp")){
		mi=4;
		res=0;
		goto A;
	}
	if(!strcmp(c,"ps1")){
		mi=5;
		res=0;
		goto A;
	}
	if(!strcmp(c,"popeye")){
		mi=6;
		res=0;
		goto A;
	}
A:
	if(i) *i=mi;
	return res;
}

int GameManager::RegisterGame(IGame *g){
	if(!g) return -1;
	if(GameManager::hInstance==NULL) return -2;
	((GameManager *)GameManager::hInstance)->push_back(g);
	return 0;
}

char *GameManager::getCurrentGameFilename(){
	char *res=NULL;
	IGame *g;

	if(!(res = new char[1025])) goto Z1;
	*((u64 *)res)=0;
	if(_game < 1 || _game>size())
		goto Z;
	g=at(_game-1);
	if(!g)
		goto Z;

	if(!g->Query(IGAME_GET_FILENAME,res)){
		goto Z1;
	}
Z:
	if(!cpu || cpu->Query(ICORE_QUERY_FILENAME,res)){
		delete []res;
		res=NULL;
	}
Z1:
	return res;
}

FileGame::FileGame() : Game(){
	_pos=0;
}

FileGame::~FileGame(){
}

int FileGame::Query(u32 what,void *pv){
	switch(what){
		case IGAME_GET_FOLDER:
			strcpy((char *)pv,_folder.c_str());
			return 0;
		case IGAME_GET_FILENAME:
			{
				string s;

				if(_folder.length())
					s=_folder + DPS_PATH;
				if(_files.size()){
					__item i = _files[0];
					s += i._name;
				}
				//cout << s << endl;
				strcpy((char *)pv,s.c_str());
			}
			return 0;
		case IGAME_SEARCH_FILENAME:{
			string s;
			int n;

			n=0;
			s=(char *)*((u64 *)pv);
			for(auto it=_files.begin();it!=_files.end();it++,n++){
				__item i = *it;
				if(i._name == s) {
					((u64 *)pv)[1]=n;
					return 0;
				}
			}
		}
			return -1;
		default:
			return Game::Query(what,pv);
	}
	return -1;
}

int FileGame::Open(char *path,u32){
	string s;
	u32 pos;

	//printf("dame load %s\n",path);
	Close();
	if(!path)
		return -1;
	_folder=GameManager::getBasePath(path);
	_pos=pos=0;
	for(auto it=_files.begin();it!=_files.end();it++){
		FILE *fp;
		int  i=0;
A:
		string s = _folder+DPS_PATH+(*it)._realname;

		if(!(fp=fopen(s.c_str(),"rb"))){
			s=_folder;
			if(!_findMatch(*it,s)){
				BS((*it)._attr,BV(1));
				(*it)._realname=s;
				if(++i < 2)	goto A;
			}
			continue;
		}

		fseek(fp,0,SEEK_END);
		(*it)._size=ftell(fp);
		(*it)._pos=pos;
		fseek(fp,0,SEEK_SET);
		pos+=(*it)._size;
		fclose(fp);
	}
	return 0;
}

int FileGame::Close(){
	if(_data)
		fclose((FILE *)_data);
	_data=NULL;
	_pos=0;
	return 0;
}

int FileGame::Write(void *dst,u32 len,u32 *plen){
	return -1;
}

int FileGame::Tell(u64 *o){
	if(!_data || !o) return -1;
	*o=ftell((FILE *)_data);
	return 0;
}

int FileGame::Read(void *dst,u32 len,u32 *plen){
	u32 sz,pos,rd;
	int res;

	res=-1;
	auto it=_files.begin();
	for(;it != _files.end();it++){
		if((*it)._pos + (*it)._size > _pos)
			break;
	}
#ifdef _DEVELOPa
	cout<<(*it)._realname<<" "<<_pos<<endl;
#endif
	rd=0;
	pos=_pos-(*it)._pos;
	for(;it!=_files.end() && len;++it){
		FILE *fp;

		string s=_folder+DPS_PATH+(*it)._realname;

		for(int i=0;i<2;i++){
			if(!(fp=fopen(s.c_str(),"rb"))){
#ifdef _DEVELOP
				cout << "Error opening " << s << endl;
#endif
				s=_folder;
				if(!_findMatch(*it,s)){

				}
				continue;
			}
#ifdef _DEVELOPa
			printf("reading %s %llu %llx\n",s.c_str(),(_pos + rd),(_pos + rd));
#endif
			goto A;
		}
		continue;
A:
		fseek(fp,pos,SEEK_SET);
		sz = (*it)._size - pos;
		if(sz>len)
			sz=len;
		fread((u8 *)dst +rd,1,sz,fp);
		len-=sz;
		rd+=sz;
		fclose(fp);
		pos=0;
	}

	if(!len)
		res=0;
	_pos+=rd;
	if(plen)
		*plen=rd;
	return res;
}

int FileGame::Seek(s64 p,u32){
	Close();
	_pos=(u32)p;
	return 0;
}

ISOStream::ISOStream() : FileStream(){
	_start=0;
	_szBlock=CD_ISO_BLOCK_SIZE;
}

ISOStream::ISOStream(char *p) : FileStream(){
	_szBlock=CD_ISO_BLOCK_SIZE;
	_start=0;
	_filename=p;
	_folder=GameManager::getBasePath(p);
}

ISOStream::~ISOStream(){
}

int ISOStream::_iso9660_read(u32 lsn,char *_buf){
	//printf("%s %x\n",__FUNCTION__,lsn*CD_FRAMESIZE_RAW+CDIO_CD_SYNC_SIZE);
	FileStream::Seek(lsn*CD_FRAMESIZE_RAW+CDIO_CD_SYNC_SIZE,SEEK_SET);
	return FileStream::Read(_buf,CD_ISO_BLOCK_SIZE);//pvd
}

int ISOStream::_iso9660_scan_dir(char *_buf,char *name,struct iso_directory_record *o){
	struct iso_directory_record *p;
	u32 ln,sz=*(u32 *)o->size;

	ln=name ? strlen(name):0;
	for(int i=0;i<sz;){
		p=(struct iso_directory_record *)&_buf[12+i];
		//printf("len: %u %u %x %s %u\n",p->length[0],p->name_len[0],*(u32 *)p->extent,p->name,ln);
		if(!p->length[0])
			break;
		if(strncmp(name,p->name,p->name_len[0])==0){
			if(o) memcpy(o,p,sizeof(struct iso_directory_record));
			return 0;
		}
		i+=p->length[0];
	}
	return -1;
}

int ISOStream::_getFilePosition(char *fn,u64 **ret){
	struct iso_directory_record *dir;
	u64 p_;
	int res,i;
	char *p,*pp,*_buf,*p0;
	u32 sc;

	if(!fp) return -1;
	res=-2;
	if(!(p=fn))  goto Z;
	res--;
	if(!(_buf=new char[CD_FRAMESIZE_RAW+1024+10])) goto Z;
	p0=&_buf[CD_FRAMESIZE_RAW];

	p+=(*(u64 *)fn & 0x0000FFFFFFFFFFFF) == 0x3a6d6f726463 ? 6 : 0;//cdrom:
	for(i=0;*p;i++)
		p0[i]=toupper(*p++);
	*(u32 *)&p0[i]=0;
//	printf("%s\n",p0);
	p_=ftell(fp);
	res--;
	p=p0;

	sc=0;
	_iso9660_read(0x10,_buf);
	dir=(struct iso_directory_record *)&_buf[12+156];//root
	for(i=0;pp=strtok(p,"\\");i++){
		//printf("%s\n",pp);

		_iso9660_read(*(u32 *)dir->extent,_buf);
		sc=0;
		if(_iso9660_scan_dir(_buf,(char *)pp,(struct iso_directory_record *)_buf))
			goto V;
		dir=(struct iso_directory_record *)_buf;
		sc=*(u32 *)dir->extent;
AA:
		p=pp+strlen(pp)+1;
	}
	//printf("sc %u %x\n",sc,*(u32*)dir->size);
	if(sc){
		res=0;
		if(ret){
			u64 *s;

			res++;
			if(s=new u64[10]){
				res--;
				*ret=s;

				s[0]=sc;
				s[1]=*(u32*)dir->size;
				s[2]=(sc*CD_FRAMESIZE_RAW)+CDIO_CD_SYNC_SIZE*2;
				s[3]=CD_FRAMESIZE_RAW;
				s[4]=CDIO_CD_SYNC_SIZE;
			}
		}
	}
V:
	FileStream::Seek(p_,SEEK_SET);
	delete []_buf;
Z:
	return res;
}

int ISOStream::Query(u32 w,void *pv){
	switch(w){
		default:
			return FileStream::Query(w,pv);
		case ISTREAM_QUERY_SET_BLOK_SIZE:
			_szBlock=*(u32 *)pv;
			return 0;
	}
}

sfiii3n::sfiii3n() : FileGame(){
	_machine="cps3";
	_name="sfiii3";
	_files.push_back({"sfiii3_japan_nocd.29f400.u2",1});
	//_files.push_back({"sfiii3(__990512)-simm1.0",0});
	//_files.push_back({"sfiii3(__990512)-simm1.1",0});
	//_files.push_back({"sfiii3(__990512)-simm1.2",0});
	//_files.push_back({"sfiii3(__990512)-simm1.3",0});
	for(int i=1;i<7;i++){
		for(int ii=0;ii<8;ii++){
			char c[40];

			sprintf(c,"sfiii3-simm%d.%d",i,ii);
			if(i<3 && ii >3)
				break;
			_files.push_back({c,0});
		}
	}
}

sfiii3n::~sfiii3n(){
}

int sfiii3n::Query(u32 w,void *pv){
	switch(w){
		case IGAME_GET_INFO:
			if(FileGame::Query(w,((u32 *)pv + 0)))
				return -1;
			*((u32 *)pv + IGAME_GET_INFO_SLOT)=0xa55432b4;
			*((u32 *)pv + IGAME_GET_INFO_SLOT+1)=0x0c129981;
			return 0;
		default:
			return FileGame::Query(w,pv);
	}
}

redearthr1::redearthr1() : FileGame(){
	_machine="cps3";
	_name="redearth";
	_status |= BV(1);
	_files.push_back({"redearth_euro.29f400.u2",1});
	_files.push_back({"cap-wzd-5.chd",4});//cdrom
}

redearthr1::~redearthr1(){
}

int redearthr1::Query(u32 w,void *pv){
	switch(w){
		case IGAME_GET_INFO:
			if(FileGame::Query(w,((u32 *)pv + 2)))
				return -1;
			*((u32 *)pv + IGAME_GET_INFO_SLOT)=0x9e300ab1;
			*((u32 *)pv + IGAME_GET_INFO_SLOT+1)=0xa175b82c;
			return 0;
		default:
			return FileGame::Query(w,pv);
	}
}

redearthn::redearthn() : redearthr1(){
	_machine="cps3";
	_name="redeartn";
	//_files.clear();
	//_files.push_back({"redearthn_asia_nocd.29f400.u2",1});
	_files[0]._name=_files[0]._realname="redearthn_asia_nocd.29f400.u2";
	for(int i=1;i<7;i++){
		for(int ii=0;ii<8;ii++){
			char c[40];

			sprintf(c,"simm%d.%d",i,ii);
			if(i<3 && ii >3)
				break;
			_files.push_back({c,0});
		}
	}
}

redearthn::~redearthn(){
}

blacktiger::blacktiger() : FileGame(){
	char c[][32]={
		"bdu-01a.5e","bdu-02a.6e","bdu-03a.8e","bd-04.9e","bd-05.10e",
		"bd-06.1l","bd.6k","bd-15.2n","bd-12.5b","bd-11.4b","bd-14.9b",
		"bd-13.8b","bd-08.5a","bd-07.4a","bd-10.9a","bd-09.8a","bd01.8j",
		"bd02.9j","bd03.11k","bd04.11l"
	};
	_machine="blktiger";
	_name="blktiger";
	for(int i =0;i<sizeof(c)/sizeof(c[0]);i++)
		_files.push_back({c[i],0});
}

blacktiger::~blacktiger(){
}

c1944j::c1944j() : FileGame(){
	char c[][64]={"nffj.03","nffj.04","nffj.05",
		"nff.13m","nff.15m","nff.17m","nff.19m","nff.14m","nff.16m","nff.18m","nff.20m",
		"nff.01","nff.11m","nff.12m"};

	_machine="cps2";
	_name="1944j";
	for(int i =0;i<sizeof(c)/64;i++)
		_files.push_back({c[i],0});
	memset(_key,0,sizeof(_key));
}

c1944j::~c1944j(){
}

int c1944j::Open(char *path,u32){
	FILE *fp;
	int b;
	unsigned short decoded[10] = { 0 };
	u8 data[20];

	if(FileGame::Open(path,0)) return -1;
	string s=_folder+DPS_PATH+_name+".key";
	memset(_key,0,sizeof(_key));
	if(!(fp=fopen(s.c_str(),"rb")))
		goto A;
	fread(data,1,sizeof(data),fp);
	fclose(fp);

	for (b = 0; b < 10 * 16; b++){
		int bit = (317 - b) % 160;
		if ((data[bit / 8] >> ((bit ^ 7) % 8)) & 1)
			decoded[b / 16] |= (0x8000 >> (b % 16));
	}
	_key[0]=((uint32_t)decoded[0] << 16) | decoded[1];
	_key[1]=((uint32_t)decoded[2] << 16) | decoded[3];

	if (decoded[9] == 0xffff){
		_key[3] = 0xffffff;
		_key[2] = 0xff0000;
	}
	else{
		_key[3] = (((~decoded[9] & 0x3ff) << 14) | 0x3fff) + 1;
		_key[2] = 0;
	}
	//printf("key %x %x %x %x\n",_key[0],_key[1],_key[2],_key[3]);
A:
	return 0;
}

int c1944j::Query(u32 w,void *pv){
	switch(w){
		case IGAME_GET_INFO:
			if(FileGame::Query(w,((u32 *)pv))) return -1;
			memcpy(((u32 *)pv + IGAME_GET_INFO_SLOT),_key,sizeof(_key));
			return 0;
		default:
			return FileGame::Query(w,pv);
	}
}