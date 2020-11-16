/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/common/ABasetools.c,v $ -- some useful functions
 * $Id: ABasetools.c,v 1.3 2003/05/07 16:05:41 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jérémy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include "config.h"


#include "ABaselog.h"
#include "ABasetools.h"
#include "globalDef.h"

/*************************************************************************
 * GLOBAL VARIABLES
 */
// signals to reading function that they shouldn't wait for input and return.
int terminate=0;


/*************************************************************************
 * READ ONE TRIMED LINE FROM FH
 */
int freadtrimedline(FILE *fh, char *buf, int count)
{
	int	rc, c;

	buf[0]=0;
	c=0;

	do
	{
		rc=getc(fh);

		if(rc == EOF)
			return	-1;

		// CR/LF are read but ignored
		if(rc==0 || rc=='\r' || rc=='\n')
		{
			if(rc!=0)	// if we have CR or LF remove both
			{
				int	rc2;
			
				rc2=getc(fh);
	
				if(rc2==rc || (rc2!='\r' || rc2=='\n'))
					ungetc(rc2, fh);
			}
			
			break;
		}
		else if(c>0 || (rc!=' ' && rc!='\t'))	// ignore spaces at begin
		{
			buf[c++]=(char)rc;
		}
	}while(!terminate && c<count);
	buf[c]=0;

	// now trim spaces at the end
	while(c>1 && (buf[c-1]==' ' || buf[c-1]=='\t'))
	{
		buf[--c]=0;
	}
	
	return c;
}

/*************************************************************************
 * READ ONE LINE FROM FD
 */
int readline (int fd, char *buf, int count)
{
	int rc, c;

	buf[0] = 0;
	c = 0;
	do {
		rc = read(fd, buf+c, 1);
		if (rc <= 0)
			return rc-1;
		// CR/LF at beginning of line is ignored.
		if (!c && (*buf == '\r' || *buf == '\n')) {
			break;
		}
		// skip CR and/or LF at end of line
		if (buf[c] == 0 || buf[c] == '\r' || buf[c] == '\n')
			break;
		c++;
	} while (!terminate && c < count);
	buf[c] = 0;

	return c;
}


/*************************************************************************
 * SEND TEXT TO FD
 */
int sendtext (int fd, char *text, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, text);
	vsnprintf(buf, sizeof(buf)-1, text, ap);
	buf[sizeof(buf)-1] = 0;
	va_end(ap);

	return write(fd, buf, strlen(buf));
}


/*************************************************************************
 * SEND TEXT TO FD
 */
int sendtextWait (int fd, char *successMessage, char *text, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, text);
	vsnprintf(buf, sizeof(buf)-1, text, ap);
	buf[sizeof(buf)-1] = 0;
	va_end(ap);

	int Answer=write(fd, buf, strlen(buf));
	//Read all success-messages from LCDd
	do
	{
		readline(fd,buf,sizeof(buf));
	}
	while (!strcasecmp(buf,successMessage));
	return Answer;
}


/*************************************************************************
 * TRIM BLANKS
 */
char *trimleft (char *str)
{
	char *s = str;
	while (*s==' ' || *s=='\t')
		s++;
	strcpy(str, s);
	return str;
}

char *trimright (char *str)
{
	char *s = str + strlen(str) - 1;
	while (s>=str && (*s==' ' || *s=='\t'))
		*s-- = 0;
	return str;
}

char *trim (char *str)
{
	return trimleft(trimright(str));
}



//substitutes expression like %artist%, %title%,..
//returns	0 if the expression was "FALSE" or if the expression is not availible (e. g. %artist%)
//		1  	"	"	"TRUE"	"	"	"     is availible (e. g. %artist%)
int substitute_single_references(char *str, struct MPD_PLAYER_SONG *song, struct MPD_PLAYER_STATUS *status)
{
	int retbool=1;
	int abort=0;
	char buf[800];
	strcpy(buf,str);
	char *new;
	do
	{
		char *cf;	//cfound
		char subst[64]="%artist%";
		char ctmp[256];
		new=NULL;
		if (cf=strstr(buf,subst))
		{
			log_printf(LOG_DEBUG,"artist");
			//deletes "%blabla%"
			new=song->artist;
		}
		else if(strcpy(subst,"%title%") && (cf=strstr(buf,subst)))
		{
			new=song->title;
		}
		else if(strcpy(subst,"%name%") && (cf=strstr(buf,subst)))
		{
			new=song->name;
		}
		else if(strcpy(subst,"%track%") && (cf=strstr(buf,subst)))
		{
			new=song->track;
		}
		else if(strcpy(subst,"%album%") && (cf=strstr(buf,subst)))
		{
			new=song->album;
		}
		else if(strcpy(subst,"%filename%") && (cf=strstr(buf,subst)))
		{
			new=song->file;
		}
		else if(strcpy(subst,"%shortfilename%") && (cf=strstr(buf,subst)))
		{
			if (strrchr(song->file,'/'))
				strcpy(ctmp, strrchr(song->file,'/')+1);
			else 
				strcpy(ctmp, song->file);
			new=ctmp;
		}
		else if(strcpy(subst,"%plpos%") && (cf=strstr(buf,subst)))
		{
			sprintf(ctmp,"%d",song->pos);
			new=ctmp;
		}
		else if(strcpy(subst,"%duration%") && (cf=strstr(buf,subst)))
		{
			int hour=0,min=0,sec=0;
			hour=(song->time/3600);
			min=(song->time%3600)/60;
			sec=(song->time%60);
			strcpy(ctmp,"");
			if (hour)
				sprintf(ctmp,"%d:%02d:%02d",hour,min,sec);
			else
				sprintf(ctmp,"%02d:%02d",min,sec);
			new=ctmp;
		}
		else if (strcpy(subst,"%volume%") && (cf=strstr(buf,subst))) 
		{
			if (status->volume<0||status->volume>100)
				new=NULL;
			else
			{
				snprintf(ctmp, 9, "%d", status->volume);
				new=ctmp;
			}
		
		}
		else if (strcpy(subst,"%repeat%") && (cf=strstr(buf,subst))) 
		{
			if (status->repeat)
			{
				strcpy(ctmp,"");
				new=ctmp;
			}
			else
				new=NULL;
		}
			
		if (new)
		{
			memmove(cf,cf+strlen(subst),strlen(cf)-strlen(subst)+1);	//we need memmove because the dest and src overlap!
			char tmp[256];
			strcpy(tmp,cf);
			strcpy(cf,fromUtf8(new));
			strcat(cf,tmp);
			if (*new)
				retbool++;
		}
		else
			retbool=0;
			
		//////////////////////////////
		// haut so nicht hin!
		// ich muss den string gleich löschen wenn ein ausdruck ungültig o. n/a ist!
		// es muss aber die Mgl. bestehen mehrere %% zu übersetzen!
		//////////////////////////////
	} while (new);
	strcpy(str,buf);
	return retbool;
}



char* substituteString(struct MPD_PLAYER_SONG *song, struct MPD_PLAYER_STATUS *status, const char * format)
{
	char ret[512];
	
	strcpy(ret,format);

	char *nextIdx=ret;
	char *prevIdx=ret;
	
	do
	{
		do {	prevIdx=strpbrk(prevIdx, "[]");		} while (prevIdx && prevIdx>ret && *(prevIdx-1)=='#');	//# is escape char.
		if (!prevIdx)
			break;
		do {	nextIdx=strpbrk(prevIdx+1, "[]");	} while (nextIdx && nextIdx>ret && *(nextIdx-1)=='#');	//# is escape char.
		if (nextIdx==NULL && prevIdx != NULL)
			nextIdx = prevIdx + strlen(prevIdx);	//forgotten last bracket like " [[%blabla%]|%bla%"
		if(prevIdx && nextIdx && *prevIdx=='[' && *nextIdx==']' && prevIdx+1!=nextIdx)
		{
			char ctmp[512]="";
			strncat(ctmp, prevIdx, nextIdx-prevIdx+1);
			if (!substitute_single_references(ctmp, song, status))
				strcpy(ctmp,"");
			memcpy(prevIdx + strlen(ctmp)+1, \
				nextIdx , \
				strlen(nextIdx)+2); //make place to insert fstring	
			strncpy(prevIdx, ctmp, strlen(ctmp));
		} else prevIdx++;
	}
	while (prevIdx!=NULL&&nextIdx!=NULL);
	
	do
	{
		nextIdx=strpbrk(nextIdx+1, "|&");
		
	}
	while (0);
	
	

	return ret;
}


/*************************************************************************
 * EOF
 */
