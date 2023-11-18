#include "game.h"
#include  <iostream>
#include "1943.h"
#include "tehkanwc.h"

void *GameManager::hInstance=NULL;

Game::Game(){
	_status=0;
	_data=NULL;
	_pos=0;
}

Game::~Game(){
}

int Game::_findMatch(__item &item,string &s){
	int len,res=-1;

	if(BVT(item._attr,2)) return 0;
#ifdef __WIN32__
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;

	s+=DPS_PATH;
	s+="*";
	if((hFind = FindFirstFile(s.c_str(), &FindFileData)) == INVALID_HANDLE_VALUE)
		return -1;
	do{
			if(string::npos != string(FindFileData.cFileName).find(item._name) &&
				string::npos != string(FindFileData.cFileName).find(_name)){
			s=FindFileData.cFileName;
		////	cout << s << endl;
			res=0;
			break;
		}
	} while(FindNextFile(hFind, &FindFileData));
A:
	FindClose(hFind);
#else
	struct dirent *dp;
	DIR *dirp = opendir(s.c_str());
	if (dirp == NULL)
		return -1;

	while ((dp = readdir(dirp)) != NULL) {
		if(string::npos == string(dp->d_name).find(item._name))
			continue;
		//cout << dp->d_name << " "  << item._name << endl;cout << dp->d_name << " "  << item._name << endl;
		if(string::npos == string(dp->d_name).find(_name))
			continue;
		//cout << dp->d_name << endl;
		s=dp->d_name;
		res=0;
		goto A;
	}
A:
	closedir(dirp);
#endif
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
	u32 i,m;

	push_back(new sfiii3n());
	push_back(new redearthr1());
	push_back(new redearthn());
	push_back(new blacktiger());
	push_back(new c1944j());

	GameManager::RegisterGame(new M1943::M1943Game());
	GameManager::RegisterGame(new tehkanwc::TWCPGame());

	return 0;
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

	if(_game < 1 || _game>size())
		goto Z;
	g=at(_game-1);
	if(!g) goto Z;
	if(!(res = new char[1025])) goto Z;
	*((u64 *)res)=0;
	if(g->Query(IGAME_GET_FILENAME,res)){
		delete []res;
		res=NULL;
		goto Z;
	}
Z:
	return res;
}

FileGame::FileGame() : Game(){
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
	return 0;
}

int FileGame::Write(void *dst,u32 len,u32 *plen){
	return -1;
}

int FileGame::Read(void *dst,u32 len,u32 *plen){
	auto it=_files.begin();
	u32 sz,pos,rd;
	int res;

	res-1;
	for(;it != _files.end();it++){
		if((*it)._pos + (*it)._size > _pos)
			break;
	}
	rd=0;
	pos=_pos-(*it)._pos;
	for(;it!=_files.end() && len;++it){
		FILE *fp;

		string s=_folder+DPS_PATH+(*it)._realname;
		if(!(fp=fopen(s.c_str(),"rb")))
			continue;
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

sfiii3n::sfiii3n() : FileGame(){
	_machine="cps3";
	_name="sfiii3";
	_files.push_back({"sfiii3_japan_nocd.29f400.u2",1});
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
	FILE *fp;

	char c[][64]={"nffj.03","nffj.04","nffj.05"};

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
	printf("key %x %x %x %x\n",_key[0],_key[1],_key[2],_key[3]);
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