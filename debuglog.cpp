#include "debuglog.h"
#include "resource.h"

DebugLog::DebugLog(){
	_tv=NULL;
	_level=0x3;
	_status=S_LOG;
	_tags=_ctags=NULL;
}

DebugLog::~DebugLog(){
	Save();
	_logs.clear();
	if(_ctags)
		delete []_ctags;
	_ctags=NULL;
}

int DebugLog::Reset(){
	_clear();
	return 0;
}

int DebugLog::Init(HWND w){
	_window=w;
#ifdef __WIN32__
#else
	_tv=GetDlgItem(_window,STR(IDC_DEBUG_TV_LOG));
	OnCommand(5609,0,GetDlgItem(w,STR(5609)));
#endif
	return _tv ? 0 : -1;
}

void DebugLog::OnButtonClicked(u32 id,HWND w){
#ifdef _DEBUG
	switch(id){
		case 5600:
			_clear();
			break;
#ifdef __WIN32__
#else
		case 5601:
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
				BS(_level,BV(2));
			else
				BC(_level,BV(2));
			Update();
		break;
		case 5603:
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
				BS(_level,BV(0));
			else
				BC(_level,BV(0));
			Update();
			break;
		case 5604:
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
				BS(_level,BV(3));
			else
				BC(_level,BV(3));
			Update();
			break;
		case 5602:
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
				BS(_level,BV(1));
			else
				BC(_level,BV(1));
			Update();
			break;
		case 5605:
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
				BS(_status,S_LOG);
			else
				BC(_status,S_LOG);
		break;
#endif
		case 5606:
		{
			FILE *fp;

			if((fp=fopen("log.log","wb"))){
				for (auto it = _logs.begin();it != _logs.end(); ++it){
					fprintf(fp,"%s",(*it).c_str());
				}
			//_logs.clear();
				fclose(fp);
			}
		}
		break;
		default:
			break;
	}
#endif
}

void DebugLog::_clear(){
	Save();
	_logs.clear();
	DebugLog::Update();
}

void DebugLog::Update(){
#ifdef _DEBUG

#ifdef __WIN32__
#else
	GtkTextIter e,s;
	GtkTextBuffer *b;
	char *c,*p;
	int level;

	if(!_tv || !BT(_status,S_PAUSE) || !GTK_IS_TEXT_VIEW(_tv))
		return;
	b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(_tv));
	gtk_text_buffer_get_start_iter(b,&s);
	gtk_text_buffer_get_end_iter(b,&e);
	gtk_text_buffer_delete(b,&s,&e);
	for (auto it = _logs.rbegin();it != _logs.rend(); ++it){
		c =  (char *)(*it).c_str();
		sscanf(c,"%02d",&level);
		if(level > _level)
			continue;
		c += 2;
		p=c;
		if(_tags && *_tags){
			char tag[20];
			int i;

			while(*c && *c++ != 9);
			for(i=0;c[i] && c[i] != 32;i++)
				tag[i]=c[i];
			tag[i]=0;
			if(_ctags == NULL)
				_ctags=new char[strlen(_tags)+10];
			i=1;
			if(_ctags){
				char *pp,*p;

				i=0;
				pp=_ctags;
				strcpy(pp,_tags);
				while((p=strtok(pp,","))){
					if(strcasecmp(p,tag)==0){
						i=1;
						break;
					}
					pp=p+strlen(p)+1;
				}
			}
			if(!i)
				continue;
		}
		//b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(_tv));
		gtk_text_buffer_get_end_iter(b,&e);
		gtk_text_buffer_insert(b,&e, p,-1);
	}
#endif

#endif
}

void DebugLog::OnCommand(u32 id,u32 c,HWND w){
#ifdef _DEBUG

#ifdef __WIN32__
#else
	switch(id){
		case 5609:
			_tags=(char *)gtk_entry_get_text(GTK_ENTRY(w));
			if(_ctags)
				delete []_ctags;
			_ctags=NULL;
			//printf("%s\n",_tags);
		break;
	}
#endif

#endif
}

int DebugLog::putf(int level,const char *fmt,...){
	va_list arg;

	va_start(arg, fmt);
	putf(level,fmt,arg);
    va_end(arg);
    return 0;
}

void DebugLog::Save(){
#ifdef _DEVELOPa
	FILE *fp=fopen("debuglog.log","ab+");
	if(fp){
		for (auto it = _logs.begin();it != _logs.end(); ++it){
			fprintf(fp,"%s",(*it).c_str());
		}
		fclose(fp);
	}
#endif
}

int DebugLog::putf(int level,const char *fmt,va_list ptr){
	char s[4096];
	if(!(_status&S_LOG))  return 1;
	//if(level > _level)
		//return 1;
	if(_logs.size() > DEBUGLOG_SIZE){
		//Save();
		_logs.clear();
	}
	sprintf(s,"%02d",level);
    vsprintf(&s[2], fmt, ptr);
    _logs.push_back(s);
   // printf("LOG %u\n",(u32)_logs.size());
	return 0;
}