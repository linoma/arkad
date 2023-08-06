#include "game.h"
#include  <iostream>

Game::Game(){
	_status=0;
	_data=NULL;
	_pos=0;
}

Game::~Game(){
}

int Game::_findMatch(__item &item,string &s){
	struct dirent *dp;
	int len,res=-1;

	if(BVT(item._attr,2)) return 0;

	DIR *dirp = opendir(s.c_str());
	if (dirp == NULL)
		return -1;

	while ((dp = readdir(dirp)) != NULL) {
		if(string::npos == string(dp->d_name).find(item._name))
			continue;
		//cout << dp->d_name << " "  << item._name << endl;cout << dp->d_name << " "  << item._name << endl;
		if(string::npos == string(dp->d_name).find(_name))
			continue;
		s=dp->d_name;
		res=0;
		goto A;
	}
A:
	closedir(dirp);
	return res;
}

int Game::Query(u32 what,void *pv){
	switch(what){
		case IGAME_GET_INFO:
			*((u32 *)pv +  0) =_files.size();
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
	}
	return -1;
}

GameManager::GameManager() : vector<IGame *>(){
	_game=_machine=0;
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
		char c[0124];

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
A:
	if(i) *i=mi;
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
		default:
			return Game::Query(what,pv);
	}
	return -1;
}

int FileGame::Open(char *path){
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
A:
		string s = _folder+DPS_PATH+(*it)._realname;
		if(!(fp=fopen(s.c_str(),"rb"))){
			s=_folder;
			if(!_findMatch(*it,s)){
				BS((*it)._attr,BV(1));
				(*it)._realname=s;
				goto A;
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
	for(;it!=_files.end() && len;it++){
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
			if(FileGame::Query(w,((u32 *)pv + 2))) return -1;
			*((u32 *)pv + 0)=0xa55432b4;
			*((u32 *)pv + 1)=0x0c129981;
			return 0;
		default:
			return FileGame::Query(w,pv);
	}
}

redearthr1::redearthr1() : FileGame(){
	_machine="cps3";
	_name="redearth";
	_files.push_back({"redearth_euro.29f400.u2",1});
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

redearthr1::~redearthr1(){
}

int redearthr1::Query(u32 w,void *pv){
	switch(w){
		case IGAME_GET_INFO:
			if(FileGame::Query(w,((u32 *)pv + 2))) return -1;
			*((u32 *)pv + 0)=0x9e300ab1;
			*((u32 *)pv + 1)=0xa175b82c;
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
	/*for(int i=1;i<7;i++){
		for(int ii=0;ii<8;ii++){
			char c[40];

			sprintf(c,"simm%d.%d",i,ii);
			if(i<3 && ii >3)
				break;
			_files.push_back({c,0});
		}
	}*/
}

redearthn::~redearthn(){
}

blacktiger::blacktiger() : FileGame(){
	char c[][64]={
"bdu-01a.5e","bdu-02a.6e","bdu-03a.8e","bd-04.9e","bd-05.10e",
"bd-06.1l","bd.6k","bd-15.2n","bd-12.5b","bd-11.4b","bd-14.9b",
"bd-13.8b","bd-08.5a","bd-07.4a","bd-10.9a","bd-09.8a","bd01.8j",
"bd02.9j","bd03.11k","bd04.11l",};

	_machine="blktiger";
	_name="blktiger";

	for(int i =0;i<sizeof(c)/64;i++)
		_files.push_back({c[i],0});
}

blacktiger::~blacktiger(){
}