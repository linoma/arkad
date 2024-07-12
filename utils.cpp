#include "utils.h"

#ifndef __WIN32__
#include <alsa/asoundlib.h>

extern "C" volatile char _binary_res_glade_start[];

void InitializeCriticalSection(u32 *cs){
	*cs=0;
}

void EnterCriticalSection(u32 *cs){
	while(!TryEnterCriticalSection(cs))
		pthread_yield();
}

int TryEnterCriticalSection(u32 *cs){
	u32 __new=1,ret=0;

	__asm__ __volatile__(
		"xorl %%eax,%%eax\n"
		"lock cmpxchg %2,%1\n"
		"setzb %0\n"
		: "=m"(ret) : "m" (*cs),"r"(__new) : "rax"
	);
	return ret;
}

void LeaveCriticalSection(u32 *cs){
	*cs=0;
}

u32 InterlockedExchange(u32 *Target,u32 Value){
    __asm__ __volatile__(
        "mov %0,%%eax\n"
        "lock xchg %%eax,%1\n"
        "mov %%eax,%0\n"
        : "=m" (Value) : "m" (Target) : "rax"
    );
    return Value;
}

DWORD GetTickCount(){
	struct timeval t;

	gettimeofday( &t, NULL );
	return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

void GetLocalTime(LPSYSTEMTIME lpSystemTime){
	struct tm *p;
	time_t t;

	if(lpSystemTime == NULL)
		return;
	t = time(NULL);
	p = localtime(&t);
	ZeroMemory(lpSystemTime,sizeof(SYSTEMTIME));
	if(p == NULL)
		return;
	lpSystemTime->wSecond = p->tm_sec;
	lpSystemTime->wMinute = p->tm_min;
	lpSystemTime->wHour = p->tm_hour;
	lpSystemTime->wDay = p->tm_mday;
	lpSystemTime->wMonth = p->tm_mon + 1;
	lpSystemTime->wYear = 1900 + p->tm_year;
	lpSystemTime->wDayOfWeek = p->tm_wday;
	lpSystemTime->wMilliseconds = 0;
}

int SystemTimeToFileTime(SYSTEMTIME *lpSystemTime,LPFILETIME lpFileTime){
	struct tm p;

	if(lpFileTime == NULL || lpSystemTime == NULL)
		return FALSE;
	p.tm_sec = lpSystemTime->wSecond;
	p.tm_min = lpSystemTime->wMinute;
	p.tm_hour = lpSystemTime->wHour;
	p.tm_mday = lpSystemTime->wDay;
	p.tm_mon = lpSystemTime->wMonth - 1;
	p.tm_year = lpSystemTime->wYear - 1900;
	p.tm_wday = lpSystemTime->wDayOfWeek;
	*lpFileTime = mktime(&p);
	return TRUE;
}

int DosDateTimeToFileTime(u16 wFatDate,u16 wFatTime,LPFILETIME lpFileTime){
	struct tm t={0};

	if(lpFileTime == NULL)
		return FALSE;
	t.tm_sec = (wFatTime & 0x1f) * 2;
	t.tm_min = (wFatTime >> 5) & 0x3f;
	t.tm_hour= (wFatTime >> 11) & 0x1f;
	t.tm_mday = (wFatDate & 31);
	t.tm_mon = ((wFatDate >> 5) & 0xF)-1;
	t.tm_year = 80 + (wFatDate >> 9);
	t.tm_isdst = -1;
	*lpFileTime = mktime(&t);
	return TRUE;
}

int FileTimeToDosDateTime(FILETIME *lpFileTime,u16 * lpFatDate,u16 * lpFatTime){
	struct tm *p;

	if(lpFileTime == NULL || lpFatDate  == NULL || lpFatTime == NULL)
		return FALSE;
	p = localtime((const time_t *)lpFileTime);
	if(p == NULL)
		return FALSE;
	*lpFatTime = p->tm_sec / 2;
	*lpFatTime |= p->tm_min << 5;
	*lpFatTime |= p->tm_hour << 11;
	*lpFatDate = p->tm_mday;
	*lpFatDate |= (p->tm_mon+1) << 5;
	*lpFatDate |= (p->tm_year - 80) << 9;
	return TRUE;
}

int FileTimeToLocalFileTime(FILETIME *lpFileTime,LPFILETIME lpLocalFileTime){
	struct tm *p;

	if(lpFileTime == NULL || lpLocalFileTime == NULL)
		return FALSE;
	/*p = localtime(lpFileTime);
	if(p == NULL)
		return FALSE;
	lpSystemTime->wSecond = p->tm_sec;
	lpSystemTime->wMinute = p->tm_min;
	lpSystemTime->wHour = p->tm_hour;
	lpSystemTime->wDay = p->tm_mday;
	lpSystemTime->wMonth = p->tm_mon + 1;
	lpSystemTime->wYear = 1900 + p->tm_year;
	lpSystemTime->wDayOfWeek = p->tm_wday;
	lpSystemTime->wMilliseconds = 0;*/
	return TRUE;
}

int FileTimeToSystemTime(FILETIME *lpFileTime,LPSYSTEMTIME lpSystemTime){
	struct tm *p;

	if(lpFileTime == NULL || lpSystemTime == NULL)
		return FALSE;
	p = localtime((const time_t *)lpFileTime);
	if(p == NULL)
		return FALSE;
	lpSystemTime->wSecond = p->tm_sec;
	lpSystemTime->wMinute = p->tm_min;
	lpSystemTime->wHour = p->tm_hour;
	lpSystemTime->wDay = p->tm_mday;
	lpSystemTime->wMonth = p->tm_mon + 1;
	lpSystemTime->wYear = 1900 + p->tm_year;
	lpSystemTime->wDayOfWeek = p->tm_wday;
	lpSystemTime->wMilliseconds = 0;
	return TRUE;
}

u32 GetTempFileName(char * lpPathName,char * lpPrefixString,u32 uUnique,char *lpTempFileName){
	char *p,s[20];

	if(lpPathName == NULL || lpTempFileName == NULL)
		return 0;
	p = (char *)calloc(1024+1,1);
	if(p == NULL)
		return 0;
	strcpy(p,lpPathName);
	if(p[strlen(p)] != '/')
		strcat(p,"/");
	if(lpPrefixString != NULL)
		strcat(p,lpPrefixString);
	if(uUnique == 0)
		uUnique = GetTickCount();
	sprintf(s,"%08X.tmp",uUnique);
	strcat(p,s);
	strcpy(lpTempFileName,p);
	free(p);
	return uUnique;
}

u32 GetTempPath(u32 nBufferLength,char *lpBuffer){
	char *p;
	u32 dwLen;

	p = getenv("HOME");
	if(p == NULL)
		return 0;
	dwLen = strlen(p);
	if(lpBuffer == NULL || nBufferLength < dwLen)
		return dwLen;
	strncpy(lpBuffer,p,dwLen+1);
	return dwLen;
}

GtkWidget* GetDlgItem(GtkWidget* parent, u32 id){
	char c[30];

	sprintf(c,"%u",id);
	return GetDlgItem(parent,c);
}

GtkWidget* GetDlgItem(GtkWidget* parent, const gchar* name){
	if(!parent || !GTK_IS_WIDGET(parent))
		return 0;
	gchar *s=(gchar *)gtk_widget_get_name((GtkWidget*)parent);
	//if(s) printf("gi: %s\n",s);
	if (s && strcasecmp(s, (gchar*)name) == 0)
		return parent;
	if (GTK_IS_BIN(parent)) {
		GtkWidget *child = gtk_bin_get_child(GTK_BIN(parent));
		if((child=GetDlgItem(child, name)))
			return child;
	}
	if (GTK_IS_CONTAINER(parent)) {
		GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
		for(;children;children = g_list_next(children)){
			GtkWidget* widget = GetDlgItem((GtkWidget *)children->data, name);
			if (widget != NULL)
				return widget;
		}
	}
	if(GTK_IS_MENU_ITEM(parent)){
		GtkWidget *child=gtk_menu_item_get_submenu(GTK_MENU_ITEM(parent));
		if((child=GetDlgItem(child, name)))
			return child;
	}
	return NULL;
}

u32 GetDlgItemId(GtkWidget *w){
	gchar *name = (gchar *)gtk_widget_get_name(GTK_WIDGET(w));
	if(!name) return (u32)-1;
	return atoi(name);
}

void gtk_widget_set_name(GtkWidget *w,u32 id){
	char c[20];

	sprintf(c,"%u",id);
	gtk_widget_set_name(w,c);
}

GtkWidget *CreateDialogFromResource(const char *id,void *h){
	GtkBuilder *_builder = gtk_builder_new();
		gtk_builder_add_from_string (_builder,(gchar *)_binary_res_glade_start,-1,NULL);
	GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(_builder, id));
	gtk_builder_connect_signals(_builder, h);
	//gtk_widget_show(w);
	g_object_unref(_builder);
	//g_object_ref(w);
	return w;
}

void disable_gobject_callback(GObject *obj, GCallback cb,gboolean _block){
	gulong hid;

	if (!obj || !cb) {
		return;
	}
	if((hid = g_signal_handler_find(obj, G_SIGNAL_MATCH_FUNC,0, 0, NULL,(gpointer) cb, NULL))){
		if(_block)
			g_signal_handler_block(obj, hid);
		else
			g_signal_handler_unblock(obj, hid);
	}
}

void FlashWindow(GtkWidget *hwnd,u32 uCount){
	u32 i;
	MSG m;

	for(i=0;i<uCount;i++){
		EnableWindow(hwnd,(i & 1) ? TRUE : FALSE);
		//gdk_window_beep(gtk_widget_get_window(hwnd));
		while(PeekMessage(&m))
			DispatchMessage(&m);
		Sleep(150);
	}
}

void Sleep(u32 dwMilliseconds){
	usleep(dwMilliseconds*1000);
}

void EnableWindow(GtkWidget *hwnd,int enable){
	gtk_widget_set_sensitive(hwnd,enable);
}

int beep(int freq,int duration,int vol){
	int err,res;
    unsigned int i;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;
	char *buf;

    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		return -1;
	res=-2;
	buf=NULL;
    if ((err = snd_pcm_set_params(handle,SND_PCM_FORMAT_U8,SND_PCM_ACCESS_RW_INTERLEAVED,1,11025,1, 500000)) < 0)
		goto A;
	i=11025.0f / freq;
	if(!(buf= new char[12025]))
		goto A;
	vol = std::max(255,vol);
	while(duration){
	   int x,n,m = std::min(duration,1000);
	   duration -= m;
		x = (11.025f*m)-i;
	   	for(n=0;n<=x;){
			int ii;

			for(ii=0;ii<i/2;ii++)
				buf[n++]=255;
			for(;ii<i;ii++)
				buf[n++]=0;
			//printf("\t %d %d\n",ii,n);
		}

		//printf("%d %d %d dur:%d\n",n,i,x,m);
		for(x=n;x<11025;x++)
			buf[x]=0;

        frames = snd_pcm_writei(handle, buf, n);
        if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
        if (frames < 0) {
           // printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
            break;
        }
        //if (frames > 0 && frames < (long)11025)
          //  printf("Short write (expected %li, wrote %li)\n", (long)11025, frames);
    }
	res=0;
A:
	//err = snd_pcm_drain(handle);
    snd_pcm_close(handle);
    if(buf)
		delete []buf;
    return res;
}

HBITMAP CreateBitmap(int width,int height,u32 nPlanes,u32 nBits,const void *lpBits){
	BOOL bAlpha;
	HBITMAP bit;

	bAlpha = nBits == 32 ? TRUE : FALSE;
	bit = gdk_pixbuf_new(GDK_COLORSPACE_RGB,bAlpha,8,width,abs(height));
	if(bit == NULL)
		return NULL;
	if(lpBits == NULL)
		return bit;
	return bit;
}

#else
int CGdiPlusLoadBitmapResource(LPCTSTR pName, LPCTSTR pType, HMODULE hInst, void **ret){
	int res=-1;
    HRSRC hResource = ::FindResource(hInst, pName, pType);
    if (!hResource)
        return -1;

    DWORD imageSize = ::SizeofResource(hInst, hResource);
    if (!imageSize)
        return -2;

    const void* pResourceData = ::LockResource(::LoadResource(hInst,hResource));
    if (!pResourceData)
        return -3;

    void *m_hBuffer  = ::GlobalAlloc(GMEM_MOVEABLE, imageSize);
    res--;
    if (m_hBuffer){
        void* pBuffer = ::GlobalLock(m_hBuffer);
        if (pBuffer){
            CopyMemory(pBuffer, pResourceData, imageSize);

            IStream* pStream = NULL;
            if (::CreateStreamOnHGlobal(m_hBuffer, FALSE, &pStream) == S_OK){
				void *image;

				if(!pfnGdipLoadImageFromStream(pStream,&image)){
					*ret=image;
					res=0;
				}
            }
            ::GlobalUnlock(m_hBuffer);
        }
        ::GlobalFree(m_hBuffer);
        m_hBuffer = NULL;
    }
    return res;
}
#endif

BOOL PeekMessage(LPMSG lpMsg){
#ifndef __WIN32__
	HWND hwnd;

	gtk_main_iteration_do (false);
	hwnd = gtk_grab_get_current();
	if(hwnd != NULL){
		if(GTK_IS_MENU_SHELL(hwnd))
			return TRUE;
	}
	return gtk_events_pending();
#else
   return ::PeekMessage(lpMsg,NULL,0,0,PM_REMOVE);
#endif
}

DWORD DispatchMessage(LPMSG lpMsg){
#ifndef __WIN32__
	//gtk_main_do_event(lpMsg);
	gtk_main_iteration_do (false);
	return 0;
#else
	return ::DispatchMessage(lpMsg);
#endif
}

u32 _log2(u32 val){
	u32 v;

	__asm__ __volatile__(
		"bsrl %1,%%eax\n"
		"mov %%eax,%0\n"
		: "=m" (v) : "m" (val) : "rax"
	);
	return v;
}

int SaveTextureAsTGA(u8 *image,u16 width,u16 height,u32 data){
   u8 *buffer,header[6],TGAheader[12]={0,0,2,0,0,0,0,0,0,0,0,0};
   FILE *fp;
   u32 i,res;
   char s[250];

   buffer = (u8 *)malloc(width*height*sizeof(u32));
   if(buffer == NULL)
       return -1;
	res=-2;
	sprintf(s,"tex_0x%08X_%08X.tga",data,GetTickCount());
	if(!(fp=fopen(s,"wb+"))) goto Z;

	fwrite(TGAheader,1,sizeof(TGAheader),fp);
	header[4] = (u8)32;
	header[0] = (u8)(width % 256);
	header[1] = (u8)(width / 256);
	header[2] = (u8)(height % 256);
	header[3] = (u8)(height / 256);
	header[5]=0;
	fwrite(header,sizeof(header),1,fp);

	for(i=0;i<width*height*4;i+=4){
		buffer[i] = image[i+2];
		buffer[i+1] = image[i+1];
		buffer[i+2] = image[i];
		buffer[i+3] = image[i+3];
	}
	fwrite(buffer,width*height*sizeof(u32),1,fp);
	res=0;
Z:
	if(fp) fclose(fp);
	free(buffer);
	return res;
}

int SaveBitmap(char *fn,int width,int height,int nc,int bp,u8 *data){
	u8 *lpBits,*p,*p1;
	BITMAPFILEHEADER hdr;
	PBITMAPINFO pbmi;
	PBITMAPINFOHEADER pbih;
	int   res,i,n;
	FILE *fp;

	nc=0;
	i=24;
	if (i && i <= 4)
		i = 4;
	else if (i <= 8)
		i = 8;
	else if (i <= 16)
		i = 16;
	else if (i <= 24)
		i = 24;
	else
		i = 32;

	int stride = ((((width * 24) + 31) & ~31) >> 3);
	n=(width + 7) / 8 * height * i;
	res=sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (2^i);
	if(!(pbmi=(PBITMAPINFO)malloc(res+n+sizeof(void *)*2)))
		return -1;
	lpBits=(u8 *) ((u64)pbmi + res);
	//printf("%lu %u %d %d %d %d\n",(u64)lpBits - (u64)pbmi,res,bp,stride,n,bp);
	memset(pbmi,0,res);
	fp=NULL;
	res=-2;
	pbih = &pbmi->bmiHeader;
	pbih->biSize = sizeof(BITMAPINFOHEADER);
	pbih->biWidth = width;
	pbih->biHeight = -height;
	pbih->biPlanes = 1;
	pbih->biBitCount = 24;
	pbih->biClrUsed = (i < 9 ? i : 0);
	pbih->biCompression = 0;//BI_RGB;
	pbih->biSizeImage = n;

	hdr.bfType = 0x4d42;
	hdr.bfSize = (u32) (sizeof(BITMAPFILEHEADER) + pbih->biSize + nc * sizeof(RGBQUAD) + pbih->biSizeImage);
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = (u32) sizeof(BITMAPFILEHEADER) + pbih->biSize + nc * sizeof(RGBQUAD);

	res--;
	if(!(fp=fopen(fn,"wb")))
		goto Z;
	p = lpBits;
	p1 = (u8 *)data;
	switch(bp){
		case 16:{
			u8 *p2,*p3;

			for(int y=0;y<height;y++){
				p2 = p;
				p3 = p1;
				for(int x=0;x<width;x++,p3+=2,p2+=3){
					int col = *((u16 *)p3);
					p2[2] = SL(col&0x1f,3);
					p2[1] = SR(col&0x3e0,2);
					p2[0] = SR(col&0x7c00,7);
				}
				p += stride;
				p1 += width*2;
  			}
		}
		break;
		case 24:{
			u8 *p2,*p3;

			for(int y=0;y<height;y++){
				p2 = p;
				p3 = p1;
				for(int x=0;x<width;x++){
					*p2++ = p3[2];
					*p2++ = p3[1];
					*p2++ = p3[0];
					p3 += 3;
				}
				p += stride;
				p1 += stride;
  			}
		}
		break;
	}
	fwrite(&hdr,sizeof(BITMAPFILEHEADER),1,fp);
	fwrite(pbih,sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof (RGBQUAD),1,fp);
	fwrite(lpBits,pbih->biSizeImage,1,fp);
	res=0;
Z:
	free(pbmi);
	if(fp)
		fclose(fp);
	return res;
}