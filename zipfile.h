#include <zlib.h>
#include "arkad.h"
#include <vector>

using namespace std;

//---------------------------------------------------------------------------
#ifndef __ZIPFILEH__
#define __ZIPFILEH__

//---------------------------------------------------------------------------
#define CENTRALDIRSIZE	                22
#define FILEHEADERSIZE	                46
#define LOCALFILEHEADERSIZE	        	30
#define ZIPARCHIVE_ENCR_HEADER_LEN     	12
#define DEF_MEM_LEVEL                  	8
#define ZIPARCHIVE_DATADESCRIPTOR_LEN  	12
//---------------------------------------------------------------------------
typedef struct {
   char  m_szSignature[4];
	u16  m_uVersionMadeBy;
	u16  m_uVersionNeeded;
	u16  m_uFlag;
	u16  m_uMethod;
	u16  m_uModTime;
	u16  m_uModDate;
	u32 m_uCrc32;
	u32 m_uComprSize;
	u32 m_uUncomprSize;
 	u16  m_uFileNameSize;
 	u16  m_uExtraFieldSize;
 	u16  m_uCommentSize;
	u16  m_uDiskStart;
	u16  m_uInternalAttr;
	u32 m_uExternalAttr;
	u32 m_uOffset;
   char *nameFile;
} ZIPFILEHEADER,*LPZIPFILEHEADER;

typedef struct {
   char  m_szSignature[4];
   u32 m_uCentrDirPos;
	u32 m_uBytesBeforeZip;
	u16  m_uThisDisk;
	u16  m_uDiskWithCD;
	u16  m_uDiskEntriesNo;
	u16  m_uEntriesNumber;
	u32 m_uSize;
	u32 m_uOffset;
   u16  uCommentSize;
   char *m_pszComment;
} ZIPCENTRALDIR, *LPZIPCENTRALDIR;

typedef struct {
   z_stream    m_stream;
   u32       m_uUncomprLeft;
   u32       m_uComprLeft;
	u32       m_uCrc32;
   u32       m_iBufferSize;
} ZIPINTERNALINFO,*LPZIPINTERNALINFO;

class LZipFile : public ICompressedFile,public vector<void *>{
public:
   LZipFile();
   LZipFile(const char *name);
   virtual ~LZipFile();
   void Release(){delete this;};
   int Open(const char *lpFileName,int bOpenAlways = FALSE);
   int Close();
   void Clear();
   LPVOID OpenZipFile(u16 uIndex,int mode = 0);
   int CloseZipFile();
   u32 ReadZipFile(LPVOID buf,u32 dwByte);
   LPZIPFILEHEADER GetCurrentZipFileHeader(){return m_curZipFileHeader;};
   LPZIPFILEHEADER GetZipFileHeader(u32 i){return (LPZIPFILEHEADER)at(i);};
   int AddZipFile(const char *lpFileName,const int iLevel = 9);
   int DeleteZipFile(u32 index);
   int DeleteAllFiles();
   u32 WriteZipFile(LPVOID buf,u32 dwByte);
	int Open(u32 dwStyle = 0,u32 dwCreation = 0,u32 dwFlags = 0);
	inline u32 Read(LPVOID lpBuffer,u32 dwBytes){return ReadZipFile(lpBuffer,dwBytes);};
	u32 Write(LPVOID lpBuffer,u32 dwBytes){return WriteZipFile(lpBuffer,dwBytes);};
	inline int SeekToBegin(){if(Seek() != 0xFFFFFFFF) return TRUE; return FALSE;};
	inline int SeekToEnd(){if(Seek(0,SEEK_END) != 0xFFFFFFFF) return TRUE; return FALSE;};
	u32 Size(u32 *lpHigh = NULL);
	int SetEndOfFile(u32 dw);
	u32 GetCurrentPosition();
	int IsOpen();
	int get_FileCompressedInfo(u32 index,LPCOMPRESSEDFILEINFO p);
	int AddCompressedFile(const char *lpFileName,const int iLevel = 9){return AddZipFile(lpFileName,iLevel);};
	int DeleteCompressedFile(u32 index){return DeleteZipFile(index);};
	u32 ReadCompressedFile(LPVOID buf,u32 dwByte){return ReadZipFile(buf,dwByte);};
	int OpenCompressedFile(u16 uIndex,int mode = 0){return OpenZipFile(uIndex,mode) != NULL ? TRUE : FALSE;};
	u32 WriteCompressedFile(LPVOID buf,u32 dwByte){return WriteZipFile(buf,dwByte);};
	void SetFileStream(IStreamer *pStream);
	void Rebuild();
	u32 Seek(LONG dwDistanceToMove = 0,u32 dwMoveMethod = SEEK_SET);
	u32 Count(){return size();};
protected:
   IStreamer *pFile;
   char fileName[1024+1],tempFileName[1024+1];
   int m_iBufferSize;
   enum OpenFileType{
       extract = -1,
	    nothing,
	    compress
   };
   OpenFileType m_iFileOpened;
   u8 *m_pBuffer;
   ZIPCENTRALDIR m_info;
   ZIPINTERNALINFO m_internalinfo;
   LPZIPFILEHEADER m_curZipFileHeader;
   int bModified,bInternal,bRewriteLocal,extractMode;
//---------------------------------------------------------------------------
   int DeleteFile(u32 index);
   int RewriteLocalHeader();
   void DeleteElem(LPVOID ele);
   int ReadZipFileHeader(LPZIPFILEHEADER p,int bLocal = TRUE);
   u32 WriteZipFileHeader(LPZIPFILEHEADER p,int bLocal = TRUE);
   void MovePackedFiles(u32 uStartOffset, u32 uEndOffset, u32 uMoveBy,int bForward=FALSE);
   int WriteCentralDir();
   void SlashBackslashChg(u8 *buffer,int bReplaceSlash);
   LPZIPFILEHEADER newHeader(const char *lpFilename = NULL,int bLocal = TRUE);
};


#endif