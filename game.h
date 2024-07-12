#include "arkad.h"
#include <vector>
#include <map>

#ifndef __GAMEH__
#define __GAMEH__

using namespace std;

#define IGAME_GET_NAME			0x20000001
#define IGAME_GET_FILENAME		0x20000002
#define IGAME_GET_MACHINE		0x20000003
#define IGAME_GET_ROMSIZE		0x20000004
#define IGAME_GET_INFO			0x20000005
#define IGAME_GET_FOLDER		0x20000006
#define IGAME_SEARCH_FILENAME	0x20000007
#define IGAME_GET_DISK_IMAGE	0x20000008

#define IGAME_GET_ISO_INFO		0x20001005
#define IGAME_GET_ISO_FILEI		0x20001006
#define IGAME_SET_ISO_MODE		0x20001007

#define IGAME_GET_INFO_SLOT		5
#define IGAME_GIS_ATTRIBUTE		2
#define IGAME_GIS_USER			IGAME_GET_INFO_SLOT

#define btoi(b)     		((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i)     		((i) / 10 * 16 + (i) % 10) /* u_char to BCD */
#define MSF2SECT(m,s,f)		(((m) * 60 + (s) - 2) * 75 + (f))
#define SECTMFS2(a) 		(SL((a)/(75*60),16)|SL(((a)/75)%60,8)|((a)%75))
#define SECTMSF2B(a,b) 		{b[0]=a/(75*60);b[1]=((a/75)%60)+2;b[2]=a%75;}


#define CD_FRAMESIZE_RAW		2352
#define CD_ISO_BLOCK_SIZE		2048
#define CDIO_CD_SYNC_SIZE         12   /**< 12 sync bytes per raw data frame */
#define CDIO_CD_CHUNK_SIZE        24
#define ISODCL(from, to) 		(to - from + 1)

#pragma pack(push)
#pragma pack(1)

struct iso_directory_record {
	u8 length					[ISODCL (1, 1)]; 		/* 711 */
	u8 ext_attr_length			[ISODCL (2, 2)]; 		/* 711 */
	u8 extent					[ISODCL (3, 10)]; 		/* 733 */
	u8 size						[ISODCL (11, 18)]; 		/* 733 */
	u8 date						[ISODCL (19, 25)]; 		/* 7 by 711 */
	u8 flags					[ISODCL (26, 26)];
	u8 file_unit_size			[ISODCL (27, 27)]; 		/* 711 */
	u8 interleave				[ISODCL (28, 28)]; 		/* 711 */
	u8 volume_sequence_number	[ISODCL (29, 32)]; 		/* 723 */
	u8 name_len					[ISODCL (33, 33)]; 		/* 711 */
	char name					[0];
} ;
#pragma pack(pop)

class Game : public IGame{
public:
	Game();
	virtual ~Game();
	virtual int Query(u32,void *);
	virtual int Open(char *,u32)=0;
	virtual int Close()=0;
	virtual int Read(void *,u32,u32 *)=0;
	virtual int Write(void *,u32,u32 *)=0;
	virtual int Seek(s64,u32)=0;
	virtual int Tell(u64 *)=0;
	virtual int AddFile(char *,u32 type=1);
	virtual int GetFile(u32,char *);
protected:
	struct __item{
		string _name,_realname;
		u32 _attr,_size,_pos;

		__item(char *s,u32 a=0){
			_name=_realname=s;
			_attr=a;
			_size=0;
			_pos=0;
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
	virtual int Open(char *,u32);
	virtual int Close();
	virtual int Read(void *,u32,u32 *);
	virtual int Write(void *,u32,u32 *);
	virtual int Seek(s64,u32);
	virtual int Tell(u64 *);
	virtual int Query(u32,void *);
protected:
	string _folder;
	u64 _pos;
};

class ISOStream : public FileStream{
public:
	ISOStream();
	ISOStream(char *);
	virtual ~ISOStream();
	virtual int _getFilePosition(char *,u64 **);
	virtual int Query(u32,void *);
protected:
	int _iso9660_read(u32 lsn,char *_buf);
	int _iso9660_scan_dir(char *_buf,char *name,struct iso_directory_record *o);

	string _folder,_filename;
	u32 _start,_szBlock;
};

class GameManager : public vector<IGame *>{
public:
	typedef struct __machine{
		u32 _index,_attr;
		string _name;
		__machine(u32 i,u32 g,const char *n=0){
			if(n) _name=n;
			_index=i;
			_attr=g;
		};
		__machine(){_index=0;};
	} MACHINE;

	GameManager();
	virtual ~GameManager();
	int Init();
	virtual int Load(char *,void *);
	virtual int Load(char *,IMachine *);
	virtual int Find(char *,u32 *,u32 *);
	virtual int MachineIndexFromGame(IGame *,u32 *);
	char *getCurrentGameFilename();
	MACHINE &MachineFromIndex(u32 n=-1);
	static string getBasePath(string path);
	static string getFilename(string path);
	static int RegisterGame(IGame *);
	static void *hInstance;
protected:
	u32 _game,_machine;

	std::map<string,__machine> _machines;
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

class blacktiger : public FileGame{
public:
	blacktiger();
	virtual ~blacktiger();
};


class c1944j : public FileGame{
public:
	c1944j();
	virtual ~c1944j();
	virtual int Query(u32,void *);
	virtual int Open(char *,u32);
protected:
	u32 _key[4];
};
#endif