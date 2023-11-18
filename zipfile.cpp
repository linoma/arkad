#include "zipfile.h"

static char m_gszSignature[] = {0x50, 0x4b, 0x01, 0x02};
static char m_gszLocalSignature[] = {0x50, 0x4b, 0x03, 0x04};
static char m_gszCentralDirSignature[] = {0x50, 0x4b, 0x05, 0x06};
/*
LZipFile::LZipFile() : vector<void *>(){
   pFile = NULL;
   bRewriteLocal = bInternal = FALSE;
   m_iBufferSize = 32768;
   ZeroMemory(&m_internalinfo,sizeof(ZIPINTERNALINFO));
   ZeroMemory(&m_info,sizeof(ZIPCENTRALDIR));
   m_curZipFileHeader = NULL;
   m_pBuffer = NULL;
   m_iFileOpened = nothing;
   bModified = FALSE;
   ZeroMemory(tempFileName,sizeof(tempFileName));
}

LZipFile::LZipFile(const char *name) : vector<void *>()
{
   pFile = NULL;
   bRewriteLocal = bInternal = FALSE;
   m_iBufferSize = 32768;
   ZeroMemory(&m_internalinfo,sizeof(ZIPINTERNALINFO));
   ZeroMemory(&m_info,sizeof(ZIPCENTRALDIR));
   m_curZipFileHeader = NULL;
   m_pBuffer = NULL;
   m_iFileOpened = nothing;
   bModified = FALSE;
   if(name != NULL)
       strcpy(fileName,name);
   ZeroMemory(tempFileName,sizeof(tempFileName));
}

LZipFile::~LZipFile(){
   Close();
   Clear();
}

LPZIPFILEHEADER LZipFile::newHeader(const char* lpFilename,int bLocal){
   LPZIPFILEHEADER p;
   int i;

   p = new ZIPFILEHEADER[1];
   if(p == NULL)
       return NULL;
   ZeroMemory(p,sizeof(ZIPFILEHEADER));
   if(bLocal)
       *((u32 *)p->m_szSignature) = *((u32 *)m_gszLocalSignature);
   else
       *((u32 *)p->m_szSignature) = *((u32 *)m_gszSignature);
   if(lpFilename != NULL){
       i = strlen(lpFilename) + 1;
       if((p->nameFile = new char[i]) == NULL){
           delete p;
           return NULL;
       }
       strcpy(p->nameFile,lpFilename);
       p->m_uFileNameSize = (u16)(i - 1);
   }
   return p;
}
//---------------------------------------------------------------------------
void LZipFile::Rebuild(){
   u32 nFile,dwPos;
   ZIPFILEHEADER p,*p1,*p2;
   LZipFile *pList;

   if(size() < 1 || m_curZipFileHeader != NULL)
       return;
   pFile->Seek(0,SEEK_SET);
   dwPos = 0;
   auto it = begin();
   for(nFile=1;nFile <= size() && it != end();nFile++){
       if(!ReadZipFileHeader(&p,TRUE))
           break;
       p1 = (LPZIPFILEHEADER)(*it);
       if(dwPos != p1->m_uOffset)
           break;
       dwPos += p.m_uExtraFieldSize + p.m_uCommentSize + p.m_uComprSize +
           LOCALFILEHEADERSIZE + p.m_uFileNameSize;
       pFile->Seek(dwPos,SEEK_SET);
       it++;
   }
   if(nFile > size())
       return;
   pList = new LZipFile();
   if(pList == NULL)
       return;
   pFile->Seek(0,SEEK_SET);
   dwPos = 0;
   nFile=1;
   for(auto it=begin();it!=end();nFile++,it++){
       if(!ReadZipFileHeader(&p,TRUE))
           break;
       p1 = (LPZIPFILEHEADER)(*it);
       if((p2 = newHeader(p1->nameFile,TRUE)) == NULL)
           break;
       memcpy(&p2->m_uVersionMadeBy,&p.m_uVersionMadeBy,LOCALFILEHEADERSIZE);
       p2->m_uVersionNeeded = 0x14;
       p2->m_uOffset = dwPos;
       push_back((void *)p2);
       dwPos += p2->m_uExtraFieldSize + p2->m_uCommentSize + p2->m_uComprSize + LOCALFILEHEADERSIZE + p2->m_uFileNameSize;
       pFile->Seek(dwPos,SEEK_SET);
   }
   //pFile->SetEndOfFile(dwPos);
   Clear();

   for(auto it=pList->begin();it!=end();it++){
	   p1= (LPZIPFILEHEADER)(*it);
       if((p2 = newHeader(p1->nameFile,FALSE)) == NULL)
           break;
       memcpy(p2,p1,((u8 *)&p1->nameFile - (u8 *)p1));
       push_back((void *)p2);
   }
   delete pList;
   bModified = TRUE;
   Close();
}

int LZipFile::Open(const char *lpFileName,int bOpenAlways){
   long uMaxRecordSize,uPosInFile;
	u32 uFileSize;
	int uRead,iToRead,iActuallyRead,i;
   int res;
   LPZIPFILEHEADER p;

   if(lpFileName == NULL && pFile == NULL)
       return FALSE;
   if(pFile != NULL && pFile->IsOpen() && m_curZipFileHeader != NULL)
		return TRUE;
   if(m_curZipFileHeader != NULL)
       CloseZipFile();
   m_curZipFileHeader = NULL;
   if(m_info.m_pszComment != NULL)
       delete m_info.m_pszComment;
   ZeroMemory(&m_info,sizeof(ZIPCENTRALDIR));
   if(pFile == NULL){
       if((pFile = new FileStream() == NULL)
           return FALSE;
       bInternal = TRUE;
   }
   if(!pFile->Open((char *)lpFileName,GENERIC_READ|GENERIC_WRITE|(bOpenAlways ? OPEN_ALWAYS : OPEN_EXISTING)))
       return FALSE;
   Clear();
   uMaxRecordSize = 0xffff + CENTRALDIRSIZE;
	uFileSize = pFile->Size();
	if((u32)uMaxRecordSize > uFileSize)
		uMaxRecordSize = uFileSize;
   m_pBuffer = (u8 *)GlobalAlloc(GPTR,m_iBufferSize);
   if(m_pBuffer == NULL)
       return FALSE;
   res = FALSE;
	uPosInFile = 0;
	uRead = 0;
   if(uMaxRecordSize == 0) res = TRUE;
	while(uPosInFile < uMaxRecordSize){
       uPosInFile = uRead + m_iBufferSize;
		if(uPosInFile > uMaxRecordSize)
			uPosInFile = uMaxRecordSize;
		iToRead = uPosInFile - uRead;
		pFile->Seek(-uPosInFile,FILE_END);
		pFile->Read(m_pBuffer,iToRead,&iActuallyRead);
		if(iActuallyRead != iToRead)
			goto ex_Open;
		for(i = iToRead - 4;i >=0 ;i--){
           if(*((u32 *)((char*)m_pBuffer + i)) == *((u32 *)m_gszCentralDirSignature)){
               m_info.m_uCentrDirPos = uFileSize - (uPosInFile - i);
               goto ex_Open;
           }
       }
		uRead += iToRead - 3;
	}
   goto ex_Open_1;
ex_Open:
   pFile->Seek(m_info.m_uCentrDirPos,SEEK_SET);
   pFile->Read(m_pBuffer,CENTRALDIRSIZE,&uRead);
   if(uRead != CENTRALDIRSIZE)
       goto ex_Open_1;
   *((u32 *)m_info.m_szSignature) = *((u32 *)m_pBuffer);
   m_info.m_uThisDisk = *((u16 *)(m_pBuffer + 4));
   m_info.m_uDiskWithCD = *((u16 *)(m_pBuffer + 6));
   m_info.m_uDiskEntriesNo = *((u16 *)(m_pBuffer + 8));
   m_info.m_uEntriesNumber = *((u16 *)(m_pBuffer + 10));
   m_info.m_uSize = *((u32 *)(m_pBuffer + 12));
   m_info.m_uOffset = *((u32 *)(m_pBuffer + 16));
   m_info.uCommentSize = *((u16 *)(m_pBuffer + 20));
   if(m_info.uCommentSize != 0){
       m_info.m_pszComment = new char[m_info.uCommentSize + 1];
       ZeroMemory(m_info.m_pszComment,m_info.uCommentSize + 1);
       pFile->Read(m_info.m_pszComment,m_info.uCommentSize);
   }
   pFile->Seek(m_info.m_uOffset + m_info.m_uBytesBeforeZip,SEEK_SET);
   for(i = 0;i < m_info.m_uEntriesNumber;i++){
		if((p = new ZIPFILEHEADER[1]) == NULL)
           break;
       if(!ReadZipFileHeader(p,FALSE))
           break;
       if(!push_back((void *)p))
           break;
	}
   if(i == m_info.m_uEntriesNumber)
       res = TRUE;
ex_Open_1:
   return res;
}
//---------------------------------------------------------------------------
int LZipFile::RewriteLocalHeader(){
   LPZIPFILEHEADER p1;

   if(pFile == NULL)
       return FALSE;
   for(auto it =begin();it != end();it++){
       p1 = ((LPZIPFILEHEADER)(*it));
       pFile->Seek(p1->m_uOffset,SEEK_SET);
       WriteZipFileHeader(p1);
   }
   return TRUE;
}
//---------------------------------------------------------------------------
void LZipFile::Clear(){
	for(auto it=begin();it!=end();it++){
		LPZIPFILEHEADER p;

		p = (LPZIPFILEHEADER)(*it);
		if(p->nameFile != NULL)
			delete []p->nameFile;
		delete p;
	}
	clear();
}

void LZipFile::Close(){
   CloseZipFile();
   if(bRewriteLocal)
       RewriteLocalHeader();
   bRewriteLocal = FALSE;
   if(bModified)
       WriteCentralDir();
   bModified = FALSE;
   if(m_pBuffer != NULL)
       GlobalFree((HGLOBAL)m_pBuffer);
   m_pBuffer = NULL;
   if(m_info.m_pszComment != NULL)
       delete []m_info.m_pszComment;
   if(pFile != NULL && bInternal){
       pFile->Close();
       delete pFile;
       pFile = NULL;
       bInternal = FALSE;
   }
   if(lstrlen(tempFileName) > 0){
       if(pFile != NULL){
           pFile->Close();
           delete pFile;
           pFile = NULL;
       }
       ::DeleteFile(tempFileName);
       ZeroMemory(tempFileName,sizeof(tempFileName));
   }
   ZeroMemory(&m_internalinfo,sizeof(ZIPINTERNALINFO));
   ZeroMemory(&m_info,sizeof(ZIPCENTRALDIR));
}
//---------------------------------------------------------------------------
int LZipFile::WriteCentralDir(){
   char m_gszSignature[] = {0x50, 0x4b, 0x05, 0x06};

   if(pFile == NULL || m_curZipFileHeader != NULL)
       return FALSE;
   pFile->Seek(0,SEEK_END);
   m_info.m_uOffset = pFile->GetCurrentPosition() - m_info.m_uBytesBeforeZip;

   m_info.m_uSize = 0;
   for(auto it=begin();it != end();it++){
       m_info.m_uSize += WriteZipFileHeader((LPZIPFILEHEADER)(*it),FALSE);
   }
   m_info.m_uDiskEntriesNo = m_info.m_uEntriesNumber = (u16)nCount;
   *((u32 *)m_pBuffer) = *((u32 *)m_gszSignature);
   *((u16 *)(m_pBuffer + 4)) = m_info.m_uThisDisk;
   *((u16 *)(m_pBuffer + 6)) = m_info.m_uDiskWithCD;
   *((u16 *)(m_pBuffer + 8)) = m_info.m_uDiskEntriesNo;
   *((u16 *)(m_pBuffer + 10)) = m_info.m_uEntriesNumber;
   *((u32 *)(m_pBuffer + 12)) = m_info.m_uSize;
   *((u32 *)(m_pBuffer + 16)) = m_info.m_uOffset;
   *((u16 *)(m_pBuffer + 20)) = m_info.uCommentSize;
   if(m_info.m_pszComment != NULL)
	    memcpy(m_pBuffer + 22, m_info.m_pszComment,m_info.uCommentSize);
   pFile->Write(m_pBuffer,CENTRALDIRSIZE + m_info.uCommentSize);
   return TRUE;
}
//---------------------------------------------------------------------------
void LZipFile::SlashBackslashChg(u8 * buffer, int bReplaceSlash)
{
	char t1 = '\\',t2 = '/', c1, c2;
	if (bReplaceSlash){
		c1 = t1;
		c2 = t2;
	}
	else{
		c1 = t2;
		c2 = t1;
	}
   while(*buffer != 0){
       if(*buffer == c2)
			*buffer = c1;
       buffer++;
	}
}
//---------------------------------------------------------------------------
int LZipFile::DeleteAllFiles(){
   if(pFile == NULL || !pFile->SetEndOfFile(m_info.m_uBytesBeforeZip))
       return FALSE;
   Clear();
   return (int)(bModified = TRUE);
}
//---------------------------------------------------------------------------
int LZipFile::DeleteZipFile(u32 index){
   if(!DeleteFile(index))
       return FALSE;
   if(size())
       pFile->SetEndOfFile(m_info.m_uBytesBeforeZip + m_info.m_uOffset);
   return TRUE;
}
//---------------------------------------------------------------------------
int LZipFile::DeleteFile(u32 index)
{
   LPZIPFILEHEADER p;
   u32 uTemp,uMoveBy,uOffsetStart,i;
   elem_list *tmp;

   if(pFile == NULL || !pFile->IsOpen())
       return FALSE;
   p = (LPZIPFILEHEADER)at(index);
   if(p == NULL)
       return FALSE;
   uMoveBy = 0;
   if(index != size()){
       uOffsetStart = 0;
       for(i=1,tmp = First;tmp != NULL;tmp = tmp->Next,i++){
           p = (LPZIPFILEHEADER)(tmp->Ele);
           if(i == index){
               uTemp = p->m_uOffset;
               if(uOffsetStart){
				    MovePackedFiles(uOffsetStart,uTemp,uMoveBy);
				    uOffsetStart = 0;
			    }
               if(i == nCount)
                   uTemp = pFile->Size() - m_info.m_uBytesBeforeZip - uTemp;
               else
                   uTemp = ((LPZIPFILEHEADER)at(index+1))->m_uOffset - uTemp;
               uMoveBy += uTemp;
           }
           else{
               if(uOffsetStart == 0)
				    uOffsetStart = p->m_uOffset;
			    p->m_uOffset -= uMoveBy;
               bRewriteLocal = TRUE;
           }
       }
       if(uOffsetStart)
		    MovePackedFiles(uOffsetStart,pFile->Size() - m_info.m_uBytesBeforeZip,uMoveBy);
   }
   else
       pFile->SetEndOfFile(p->m_uOffset);
   if(uMoveBy != 0){
       pFile->SetEndOfFile(pFile->Size() - uMoveBy);
       m_info.m_uOffset -= uMoveBy;
   }
   Delete(index);
   m_info.m_uEntriesNumber = (u16)nCount;
   return (int)(bModified = TRUE);
}
//---------------------------------------------------------------------------
void LZipFile::MovePackedFiles(u32 uStartOffset, u32 uEndOffset, u32 uMoveBy,int bForward)
{
   u32 uTotalToMove,uPack,size_read,uPosition;
	int bBreak;

	uStartOffset += m_info.m_uBytesBeforeZip;
	uEndOffset += m_info.m_uBytesBeforeZip;
	uTotalToMove = uEndOffset - uStartOffset;
	uPack = uTotalToMove > (u32)m_iBufferSize ? m_iBufferSize : uTotalToMove;
   bBreak = FALSE;
	do{
		if(uEndOffset - uStartOffset < uPack){
			uPack = uEndOffset - uStartOffset;
			if(!uPack)
				break;
			bBreak = TRUE;
		}
		uPosition = bForward ? uEndOffset - uPack : uStartOffset;
       pFile->Seek(uPosition,SEEK_SET);
		size_read = pFile->Read(m_pBuffer,uPack);
		if(!size_read)
			break;
		if(bForward)
           uPosition += uMoveBy;
		else
			uPosition -= uMoveBy;
       pFile->Seek(uPosition,SEEK_SET);
       pFile->Write(m_pBuffer,size_read);
		if(bForward)
           uEndOffset -= size_read;
		else
           uStartOffset += size_read;
	}
	while(!bBreak);
}
//---------------------------------------------------------------------------
int LZipFile::AddZipFile(const char *lpFileName,const int iLevel){
   u16 uIndex;
   u32 dw,dwPos;
   LPZIPFILEHEADER p,p1;
   SYSTEMTIME sysTime;
   FILETIME fileTime;

   if(lpFileName == NULL || pFile == NULL || !pFile->IsOpen() || m_curZipFileHeader != NULL)
       return FALSE;
   p1 = newHeader(lpFileName,FALSE);
   if(p1 == NULL)
       return FALSE;
   SlashBackslashChg((u8 *)p1->nameFile,FALSE);
   GetLocalTime(&sysTime);
   SystemTimeToFileTime(&sysTime,&fileTime);
   FileTimeToDosDateTime(&fileTime,&p1->m_uModDate,&p1->m_uModTime);
   p1->m_uVersionNeeded = 0x14;
   p1->m_uMethod = Z_DEFLATED;
//   p1->m_uExternalAttr = FILE_ATTRIBUTE_ARCHIVE;
   switch (iLevel){
		case 1:
			p1->m_uFlag  |= 6;
       break;
		case 2:
			p1->m_uFlag  |= 4;
       break;
		case 8:
		case 9:
			p1->m_uFlag  |= 2;
       break;
   }

   dw = 1;
   uIndex = -1;
   for(auto it=begin();it!=end();it++){
		p = (LPZIPFILEHEADER)(*it);
       if(strcmpi(lpFileName,p->nameFile) == 0){
           uIndex = (u16)dw;
           break;
       }
       dw++;
   }
   if(uIndex != u16(-1))
       DeleteFile(uIndex);
   if(size())
       pFile->SetEndOfFile(m_info.m_uBytesBeforeZip + m_info.m_uOffset);
   push_back((void *)p1);
   pFile->Seek(0,SEEK_END);
   WriteZipFileHeader(p1);
   ZeroMemory(&m_internalinfo,sizeof(ZIPINTERNALINFO));
   m_internalinfo.m_stream.avail_out = (uInt)m_iBufferSize;
   m_internalinfo.m_stream.next_out = (Bytef*)m_pBuffer;
   deflateInit2(&m_internalinfo.m_stream,iLevel,Z_DEFLATED,-MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
   m_curZipFileHeader = p1;
   m_iFileOpened = compress;
   extractMode = 0;
   return (int)(bModified = TRUE);
}
//---------------------------------------------------------------------------
int LZipFile::CloseZipFile(){
   int err;
   u32 uTotal;

   if(m_curZipFileHeader == NULL)
       return FALSE;
   if(m_iFileOpened == extract){
       if(m_curZipFileHeader->m_uMethod == Z_DEFLATED)
           inflateEnd(&m_internalinfo.m_stream);
   }
   else if(m_iFileOpened == compress){
       m_internalinfo.m_stream.avail_in = 0;
       err = Z_OK;
		if(m_curZipFileHeader->m_uMethod == Z_DEFLATED){
           while(err == Z_OK){
				if(m_internalinfo.m_stream.avail_out == 0){
                   if((m_curZipFileHeader->m_uFlag & 1) != 0){
                   }
                   pFile->Write(m_pBuffer,m_internalinfo.m_uComprLeft);
					m_internalinfo.m_uComprLeft = 0;
					m_internalinfo.m_stream.avail_out = m_iBufferSize;
					m_internalinfo.m_stream.next_out = (Bytef*)m_pBuffer;
				}
				uTotal = m_internalinfo.m_stream.total_out;
				err = deflate(&m_internalinfo.m_stream, Z_FINISH);
				m_internalinfo.m_uComprLeft += m_internalinfo.m_stream.total_out - uTotal;
			}
       }
       if(err == Z_STREAM_END)
           err = Z_OK;
       if((err != Z_OK)&& (err != Z_NEED_DICT))
           return FALSE;
		if(m_internalinfo.m_uComprLeft > 0){
           if((m_curZipFileHeader->m_uFlag & 1) != 0){
           }
           pFile->Write(m_pBuffer,m_internalinfo.m_uComprLeft);
		}
		if(m_curZipFileHeader->m_uMethod == Z_DEFLATED){
			err = deflateEnd(&m_internalinfo.m_stream);
           if((err != Z_OK)&& (err != Z_NEED_DICT))
               return FALSE;
		}
		m_curZipFileHeader->m_uComprSize += m_internalinfo.m_stream.total_out;
		m_curZipFileHeader->m_uUncomprSize = m_internalinfo.m_stream.total_in;
       pFile->Seek(m_curZipFileHeader->m_uOffset+14,SEEK_SET);
       *((u32 *)m_pBuffer) = m_curZipFileHeader->m_uCrc32;
       *((u32 *)(m_pBuffer + 4)) = m_curZipFileHeader->m_uComprSize;
       *((u32 *)(m_pBuffer + 8)) = m_curZipFileHeader->m_uUncomprSize;
       m_curZipFileHeader->m_uOffset -= m_info.m_uBytesBeforeZip;
       pFile->Write(m_pBuffer,ZIPARCHIVE_DATADESCRIPTOR_LEN);
       pFile->Seek(m_curZipFileHeader->m_uOffset,SEEK_SET);
   }
   m_curZipFileHeader = NULL;
   m_iFileOpened = nothing;
   return TRUE;
}
//---------------------------------------------------------------------------
int LZipFile::get_FileCompressedInfo(u32 index,LPCOMPRESSEDFILEINFO p){
   LPZIPFILEHEADER p1;

   if(index == 0 || index > (u16)size() || pFile == NULL || !pFile->IsOpen())
       return FALSE;
   p1 = (LPZIPFILEHEADER)GetItem(index);
   if(p1 == NULL)
       return FALSE;
   strcpy(p->fileName,p1->nameFile);
   p->dwSize = p1->m_uUncomprSize;
   p->dwSizeCompressed = p1->m_uComprSize;
   return TRUE;
}
//---------------------------------------------------------------------------
void * LZipFile::OpenZipFile(u16 uIndex,int mode){
   LPZIPFILEHEADER p;
   int err;
   ZIPFILEHEADER zfh;
   FileStream *pFileTmp;
   char *buf;
   u32 ci,si;

   if(uIndex == 0 || uIndex > (u16)size() || pFile == NULL || !pFile->IsOpen())
       return NULL;
   if(m_curZipFileHeader != NULL)
       CloseZipFile();
   p = (LPZIPFILEHEADER)GetItem(uIndex);
   if(p == NULL)
       return NULL;
   if(p->m_uMethod != 0 && (p->m_uMethod != Z_DEFLATED) || (p->m_uFlag & 1) != 0)
       return NULL;
   pFile->Seek(p->m_uOffset + m_info.m_uBytesBeforeZip,SEEK_SET);
   ReadZipFileHeader(&zfh);
   ZeroMemory(&m_internalinfo,sizeof(ZIPINTERNALINFO));
   if(p->m_uMethod == Z_DEFLATED){
		m_internalinfo.m_stream.opaque =  0;
       err = inflateInit2(&m_internalinfo.m_stream,-MAX_WBITS);
       if((err != Z_OK)&& (err != Z_NEED_DICT))
           return NULL;
	}
	m_internalinfo.m_uComprLeft = p->m_uComprSize;
	m_internalinfo.m_uUncomprLeft = p->m_uUncomprSize;
	m_internalinfo.m_uCrc32 = 0;
	m_internalinfo.m_stream.total_out = 0;
	m_internalinfo.m_stream.avail_in = 0;
   m_curZipFileHeader = p;
   m_iFileOpened = extract;
   extractMode = 0;
   if(mode == 0)
       return p;
   GetTempPath(MAX_PATH,tempFileName);
   GetTempFileName(tempFileName,"iDeaS",0,tempFileName);
   if((pFileTmp = new FileStream()) == NULL)
       goto ex_OpenZipFile;
   if((buf = (char *)calloc(m_iBufferSize,1)) == NULL){
       delete pFileTmp;
       goto ex_OpenZipFile;
   }
   if(!pFileTmp->Open(tempFileName,GENERIC_WRITE|CREATE_ALWAYS)){
       free(buf);
       delete pFileTmp;
       goto ex_OpenZipFile;
   }
   ci = p->m_uUncomprSize;
   while(ci > 0){
       si = (ci > m_iBufferSize) ? m_iBufferSize : ci;
       if(ReadCompressedFile(buf,si) != si)
           break;
       pFileTmp->Write(buf,si);
       ci -=si;
      // ds.ProcessMessages();
   }
   free(buf);
   delete pFileTmp;
   if(ci)
       mode = 0;
   else{
       pFileTmp = new FileStream();
       if(!pFileTmp->Open(tempFileName)){
           delete pFileTmp;
           goto ex_OpenZipFile;
       }
       pFile->Close();
       delete pFile;
       pFile = pFileTmp;
   }
   extractMode = mode;
ex_OpenZipFile:
   return p;
}
//---------------------------------------------------------------------------
u32 LZipFile::WriteZipFile(void * buf,u32 dwByte)
{
   u32 uTotal,uToCopy;
   int err;

   if(m_curZipFileHeader == NULL || pFile == NULL || !pFile->IsOpen() || m_iFileOpened != compress || extractMode != 0)
       return 0;
   m_internalinfo.m_stream.next_in = (Bytef*)buf;
   m_internalinfo.m_stream.avail_in = dwByte;
   m_curZipFileHeader->m_uCrc32 = crc32(m_curZipFileHeader->m_uCrc32, (Bytef*)buf,dwByte);
   while(m_internalinfo.m_stream.avail_in > 0){
       if(m_internalinfo.m_stream.avail_out == 0){
           if((m_curZipFileHeader->m_uFlag & 1) != 0){
           }
           pFile->Write(m_pBuffer, m_internalinfo.m_uComprLeft);
			m_internalinfo.m_uComprLeft = 0;
           m_internalinfo.m_stream.avail_out = m_iBufferSize;
           m_internalinfo.m_stream.next_out = (Bytef*)m_pBuffer;
       }
       if(m_curZipFileHeader->m_uMethod == Z_DEFLATED){
           uTotal = m_internalinfo.m_stream.total_out;
           err = deflate(&m_internalinfo.m_stream, Z_NO_FLUSH);
           if((err != Z_OK) && (err != Z_NEED_DICT))
              break;
           m_internalinfo.m_uComprLeft += m_internalinfo.m_stream.total_out - uTotal;
       }
       else{
           uToCopy = (m_internalinfo.m_stream.avail_in < m_internalinfo.m_stream.avail_out) ? m_internalinfo.m_stream.avail_in : m_internalinfo.m_stream.avail_out;
			memcpy(m_internalinfo.m_stream.next_out, m_internalinfo.m_stream.next_in, uToCopy);
           m_internalinfo.m_stream.avail_in    -= uToCopy;
           m_internalinfo.m_stream.avail_out   -= uToCopy;
           m_internalinfo.m_stream.next_in     += uToCopy;
           m_internalinfo.m_stream.next_out    += uToCopy;
           m_internalinfo.m_stream.total_in    += uToCopy;
           m_internalinfo.m_stream.total_out   += uToCopy;
           m_internalinfo.m_uComprLeft         += uToCopy;
       }
   }
   return (u32)(dwByte - m_internalinfo.m_stream.avail_in);
}
//---------------------------------------------------------------------------
u32 LZipFile::Seek(LONG dwDistanceToMove,u32 dwMoveMethod)
{
   s64 rel_ofs,cur_pos,read_size;
   u32 dw;
   char *buf;

   if(m_curZipFileHeader == NULL || pFile == NULL || !pFile->IsOpen() || m_iFileOpened != extract)
       return 0xFFFFFFFF;
   if(extractMode != 0)
       return pFile->Seek(dwDistanceToMove,dwMoveMethod);
   cur_pos = m_curZipFileHeader->m_uUncomprSize - m_internalinfo.m_uUncomprLeft;
   switch(dwMoveMethod){
       case SEEK_SET:
           rel_ofs = dwDistanceToMove - cur_pos;
       break;
       case SEEK_CUR:
           rel_ofs = dwDistanceToMove;
       break;
       case SEEK_END:
           rel_ofs = m_curZipFileHeader->m_uUncomprSize + dwDistanceToMove - cur_pos;
       break;
       default:
           return (u32)-1;
   }
   if(rel_ofs == 0)
       return (u32)cur_pos;
   if(rel_ofs < 0){
       dw = IndexFromEle((void *)m_curZipFileHeader);
       if(dw == (u32)-1)
           return (u32)-1;
       CloseZipFile();
       if(!OpenZipFile((u16)dw))
          return (u32)-1;
       read_size = cur_pos + rel_ofs;
       cur_pos = 0;
   }
   else
       read_size = rel_ofs;
   if(read_size < 0)
       return (u32)-1;
   if(read_size + cur_pos > m_curZipFileHeader->m_uUncomprSize)
       return (u32)-1;
   if(read_size == 0)
       return (u32)cur_pos;
   if((buf = new char[m_iBufferSize]) == NULL)
       return (u32)-1;
   while(read_size > 0){
       dw = (u32)(read_size < m_iBufferSize ? read_size : m_iBufferSize);
       if(ReadZipFile(buf,dw) != dw)
           return (u32)-1;
       read_size -= dw;
   }
   delete []buf;
   return m_curZipFileHeader->m_uUncomprSize - m_internalinfo.m_uUncomprLeft;
}
//---------------------------------------------------------------------------
u32 LZipFile::ReadZipFile(void * buf,u32 dwByte)
{
   u32 iRead,uToRead,uToCopy,uTotal;
   int bForce;
   int err;
   Bytef* pOldBuf;

   if(m_curZipFileHeader == NULL || pFile == NULL || !pFile->IsOpen() || m_iFileOpened != extract)
       return 0;
   if(extractMode != 0)
       return pFile->Read(buf,dwByte);
   m_internalinfo.m_stream.next_out = (Bytef*)buf;
	m_internalinfo.m_stream.avail_out = dwByte > m_internalinfo.m_uUncomprLeft ? m_internalinfo.m_uUncomprLeft : dwByte;
   iRead = 0;
	bForce = m_internalinfo.m_stream.avail_out == 0 && m_internalinfo.m_uComprLeft > 0;
	while(m_internalinfo.m_stream.avail_out > 0 || (bForce && m_internalinfo.m_uComprLeft > 0)){
		if((m_internalinfo.m_stream.avail_in == 0) && m_internalinfo.m_uComprLeft > 0){
			uToRead = m_iBufferSize;
			if(m_internalinfo.m_uComprLeft < (u32)m_iBufferSize)
				uToRead = m_internalinfo.m_uComprLeft;
			if(uToRead == 0)
               goto ex_ReadZipFile;
           pFile->Read(m_pBuffer,uToRead);
           if((m_curZipFileHeader->m_uFlag & 1) != 0){
           }
			m_internalinfo.m_uComprLeft -= uToRead;
			m_internalinfo.m_stream.next_in = (Bytef*)m_pBuffer;
			m_internalinfo.m_stream.avail_in = uToRead;
		}
		if(m_curZipFileHeader->m_uMethod == 0){
			uToCopy = m_internalinfo.m_stream.avail_out < m_internalinfo.m_stream.avail_in ? m_internalinfo.m_stream.avail_out : m_internalinfo.m_stream.avail_in;
			memcpy(m_internalinfo.m_stream.next_out,m_internalinfo.m_stream.next_in,uToCopy);
			m_internalinfo.m_uCrc32 = crc32(m_internalinfo.m_uCrc32,m_internalinfo.m_stream.next_out,uToCopy);
			m_internalinfo.m_uUncomprLeft       -= uToCopy;
			m_internalinfo.m_stream.avail_in    -= uToCopy;
			m_internalinfo.m_stream.avail_out   -= uToCopy;
			m_internalinfo.m_stream.next_out    += uToCopy;
			m_internalinfo.m_stream.next_in     += uToCopy;
           m_internalinfo.m_stream.total_out   += uToCopy;
			iRead += uToCopy;
		}
		else{
			uTotal = m_internalinfo.m_stream.total_out;
			pOldBuf =  m_internalinfo.m_stream.next_out;
			err = inflate(&m_internalinfo.m_stream,Z_SYNC_FLUSH);
			uToCopy = m_internalinfo.m_stream.total_out - uTotal;
			m_internalinfo.m_uCrc32 = crc32(m_internalinfo.m_uCrc32, pOldBuf, uToCopy);
			m_internalinfo.m_uUncomprLeft -= uToCopy;
			iRead += uToCopy;
			if(err == Z_STREAM_END)
               goto ex_ReadZipFile;
           if((err != Z_OK) && (err != Z_NEED_DICT))
               break;
		}
	}
ex_ReadZipFile:
	return iRead;
}
//---------------------------------------------------------------------------
u32 LZipFile::WriteZipFileHeader(LPZIPFILEHEADER p,int bLocal){
   u32 iSize;

   if(bLocal)
       iSize = LOCALFILEHEADERSIZE;
   else
       iSize = FILEHEADERSIZE + p->m_uCommentSize;
   iSize += p->m_uFileNameSize + p->m_uExtraFieldSize;
   if(!bLocal){
       *((u32 *)p->m_szSignature) = *((u32 *)m_gszSignature);
       *((u32 *)m_pBuffer) = *((u32 *)m_gszSignature);
       *((u16 *)(m_pBuffer + 4)) = p->m_uVersionMadeBy;
       *((u16 *)(m_pBuffer + 6)) = p->m_uVersionNeeded;
       *((u16 *)(m_pBuffer + 8)) = p->m_uFlag;
       *((u16 *)(m_pBuffer + 10)) = p->m_uMethod;
       *((u16 *)(m_pBuffer + 12)) = p->m_uModTime;
       *((u16 *)(m_pBuffer + 14)) = p->m_uModDate;
       *((u32 *)(m_pBuffer + 16)) = p->m_uCrc32;
       *((u32 *)(m_pBuffer + 20)) = p->m_uComprSize;
       *((u32 *)(m_pBuffer + 24)) = p->m_uUncomprSize;
       *((u16 *)(m_pBuffer + 28)) = p->m_uFileNameSize;
       *((u16 *)(m_pBuffer + 30)) = p->m_uExtraFieldSize;
       *((u16 *)(m_pBuffer + 32)) = p->m_uCommentSize;
       *((u16 *)(m_pBuffer + 34)) = p->m_uDiskStart;
       *((u16 *)(m_pBuffer + 36)) = p->m_uInternalAttr;
       *((u32 *)(m_pBuffer + 38)) = p->m_uExternalAttr;
       *((u32 *)(m_pBuffer + 42)) = p->m_uOffset;
       memcpy(m_pBuffer + 46,p->nameFile,p->m_uFileNameSize);
   }
   else{
       *((u32 *)p->m_szSignature) = *((u32 *)m_gszLocalSignature);
       *((u32 *)m_pBuffer) = *((u32 *)m_gszLocalSignature);
       *((u16 *)(m_pBuffer + 4)) = p->m_uVersionNeeded;
       *((u16 *)(m_pBuffer + 6)) = p->m_uFlag;
       *((u16 *)(m_pBuffer + 8)) = p->m_uMethod;
       *((u16 *)(m_pBuffer + 10)) = p->m_uModTime;
       *((u16 *)(m_pBuffer + 12)) = p->m_uModDate;
       *((u32 *)(m_pBuffer + 14)) = p->m_uCrc32;
       *((u32 *)(m_pBuffer + 18)) = p->m_uComprSize;
       *((u32 *)(m_pBuffer + 22)) = p->m_uUncomprSize;
       *((u16 *)(m_pBuffer + 26)) = p->m_uFileNameSize;
       *((u16 *)(m_pBuffer + 28)) = p->m_uExtraFieldSize;
       *((u16 *)(m_pBuffer + 30)) = p->m_uCommentSize;
       p->m_uOffset = pFile->GetCurrentPosition();
   }
   pFile->Write(m_pBuffer,iSize,&iSize);
   return iSize;
}
//---------------------------------------------------------------------------
void LZipFile::SetFileStream(IStreamer *p)
{
   if(bInternal && pFile != NULL){
       pFile->Close();
       delete pFile;
       pFile = NULL;
   }
   if(p == NULL){
   	pFile = NULL;
       bInternal = TRUE;
       return;
   }
   bInternal = FALSE;
   pFile = p;
}
//---------------------------------------------------------------------------
int LZipFile::ReadZipFileHeader(LPZIPFILEHEADER p,int bLocal)
{
   int iSize;

   if(!bLocal)
       iSize = FILEHEADERSIZE;
   else
       iSize = LOCALFILEHEADERSIZE;
   pFile->Read(m_pBuffer,iSize,0);
   ZeroMemory(p,sizeof(ZIPFILEHEADER));
   if(!bLocal){
       *((u32 *)p->m_szSignature) = *((u32 *)m_pBuffer);
       p->m_uVersionMadeBy = *((u16 *)(m_pBuffer + 4));
       p->m_uVersionNeeded = *((u16 *)(m_pBuffer + 6));
	    p->m_uFlag = *((u16 *)(m_pBuffer + 8));
	    p->m_uMethod = *((u16 *)(m_pBuffer + 10));
	    p->m_uModTime = *((u16 *)(m_pBuffer + 12));
	    p->m_uModDate = *((u16 *)(m_pBuffer + 14));
	    p->m_uCrc32 = *((u32 *)(m_pBuffer + 16));
	    p->m_uComprSize = *((u32 *)(m_pBuffer + 20));
	    p->m_uUncomprSize = *((u32 *)(m_pBuffer + 24));
	    p->m_uFileNameSize = *((u16 *)(m_pBuffer + 28));
	    p->m_uExtraFieldSize = *((u16 *)(m_pBuffer + 30));
	    p->m_uCommentSize = *((u16 *)(m_pBuffer + 32));
	    p->m_uDiskStart = *((u16 *)(m_pBuffer + 34));
	    p->m_uInternalAttr = *((u16 *)(m_pBuffer + 36));
	    p->m_uExternalAttr = *((u32 *)(m_pBuffer + 38));
	    p->m_uOffset = *((u32 *)(m_pBuffer + 42));
       if(*((u32 *)p->m_szSignature) != *((u32 *)m_gszSignature))
		    return FALSE;
   }
   else{
       *((u32 *)p->m_szSignature) = *((u32 *)m_pBuffer);
       p->m_uVersionNeeded = *((u16 *)(m_pBuffer + 4));
	    p->m_uFlag = *((u16 *)(m_pBuffer + 6));
	    p->m_uMethod = *((u16 *)(m_pBuffer + 8));
	    p->m_uModTime = *((u16 *)(m_pBuffer + 10));
	    p->m_uModDate = *((u16 *)(m_pBuffer + 12));
	    p->m_uCrc32 = *((u32 *)(m_pBuffer + 14));
	    p->m_uComprSize = *((u32 *)(m_pBuffer + 18));
	    p->m_uUncomprSize = *((u32 *)(m_pBuffer + 22));
	    p->m_uFileNameSize = *((u16 *)(m_pBuffer + 26));
	    p->m_uExtraFieldSize = *((u16 *)(m_pBuffer + 28));
       if(*((u32 *)p->m_szSignature) != *((u32 *)m_gszLocalSignature))
		    return FALSE;
   }
   if(p->m_uFileNameSize != 0){
       if(!bLocal){
           if((p->nameFile = new char[p->m_uFileNameSize+1]) == NULL)
               return FALSE;
           ZeroMemory(p->nameFile,p->m_uFileNameSize+1);
           pFile->Read(p->nameFile,p->m_uFileNameSize,0);
           SlashBackslashChg((u8 *)p->nameFile,TRUE);
       }
       else
           pFile->Seek(p->m_uFileNameSize,SEEK_CUR);
   }
   pFile->Seek(p->m_uExtraFieldSize + p->m_uCommentSize,SEEK_CUR);
   return TRUE;
}
//---------------------------------------------------------------------------
int LZipFile::Open(u32 dwStyle,u32 dwCreation,u32 dwFlags){
   return Open(fileName,(dwCreation == OPEN_ALWAYS ? TRUE : FALSE));
}
//---------------------------------------------------------------------------
u32 LZipFile::Size(u32 * lpHigh)
{
   if(m_curZipFileHeader == NULL || pFile == NULL || !pFile->IsOpen() || m_iFileOpened != extract)
       return 0;
   return m_curZipFileHeader->m_uUncomprSize;
}
//---------------------------------------------------------------------------
int LZipFile::SetEndOfFile(u32 dw)
{
   return FALSE;
}
//---------------------------------------------------------------------------
u32 LZipFile::GetCurrentPosition(){
   if(m_curZipFileHeader == NULL || pFile == NULL || !pFile->IsOpen() || m_iFileOpened != extract)
       return 0xFFFFFFFF;
   return m_curZipFileHeader->m_uUncomprSize - m_internalinfo.m_uUncomprLeft;
}
//---------------------------------------------------------------------------
int LZipFile::IsOpen(){
   if(pFile == NULL || !pFile->IsOpen())
       return FALSE;
   return TRUE;
}*/