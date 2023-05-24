#include "arkad.h"
#include <vector>

#ifndef __GAMEH__
#define __GAMEH__

using namespace std;

#define IGAME_GET_NAME	0x20000001
#define IGAME_GET_FILENAME	0x20000002
#define IGAME_GET_MACHINE	0x20000003
#define IGAME_GET_ROMSIZE	0x20000004
#define IGAME_GET_INFO		0x20000005
#define IGAME_GET_FOLDER	0x20000006

class Game : public IGame{
public:
	Game();
	virtual ~Game();
	virtual int Query(u32,void *);
	virtual int Open(char *)=0;
	virtual int Close()=0;
	virtual int Read(void *,u32,u32 *)=0;
	virtual int Seek(s64,u32)=0;
protected:
	struct __item{
		string _name,_realname;
		u32 _attr,_size,_pos;

		__item(string s,u32 a){
			_name=_realname=s;
			_attr=a;
		}
	};

	virtual int _findMatch(__item &,string &);

	vector<__item> _files;
	string _machine,_name;
	void *_data;
	u64 _status;
	u32 _pos;
};

class FileGame : public Game{
public:
	FileGame();
	virtual ~FileGame();
	virtual int Open(char *);
	virtual int Close();
	virtual int Read(void *,u32,u32 *);
	virtual int Seek(s64,u32);
	virtual int Query(u32,void *);
protected:
	string _folder;
	u64 _pos;
};

class CdRomGame : public FileGame{
public:
	CdRomGame();
	virtual ~CdRomGame();
protected:
};

class GameManager : public vector<IGame *>{
public:
	GameManager();
	virtual ~GameManager();
	int Init();
	virtual int Load(char *,void *);
	virtual int Load(char *,IMachine *);
	virtual int Find(char *,u32 *,u32 *);
	virtual int MachineIndexFromGame(IGame *,u32 *);
	static string getBasePath(string path);
	static string getFilename(string path);
protected:
	u32 _game,_machine;
};


class sfiii3n : public FileGame{
public:
	sfiii3n();
	virtual ~sfiii3n();
	virtual int Query(u32,void *);
};

class redearthr1 : public FileGame{
public:
	redearthr1();
	virtual ~redearthr1();
	virtual int Query(u32,void *);
};

class redearthn : public redearthr1{
public:
	redearthn();
	virtual ~redearthn();

};
#endif