/* mpc
 * (c) 2003-2005 by normalperson and Warren Dukes (warren.dukes@gmail.com)
 *              and Daniel Brown (danb@cs.utexas.edu)
 * This project's homepage is: http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "util.h"
#include "libmpdclient.h"
#include "charConv.h"
#include "globalDef.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <assert.h>
//#include <sys/param.h>


char * appendToString(char * dest, const char * src, int len) {
	int destlen;

	if(dest == NULL) {
		dest = malloc(len+1);
		memset(dest, 0, len+1);
		destlen = 0;
	}
	else {
		destlen = strlen(dest);
		dest = realloc(dest, destlen+len+1);
	}

	memcpy(dest+destlen, src, len);
	dest[destlen+len] = '\0';

	return dest;
}


char * skipFormatting(char * p) {
	int stack = 0;
		
	while (*p != '\0') {
		if(*p == '[') stack++;
		if(*p == '#' && p[1] != '\0') {
			/* skip escaped stuff */
			++p;
		}
		else if(stack) {
			if(*p == ']') stack--;
		}
		else {
			if(*p == '&' || *p == '|' || *p == ']') {
				break;
			}
		}
		++p;
	}

	return p;
}

/* this is a little ugly... */
char * songToFormatedString (struct MPD_PLAYER_SONG *song, const char * format, char ** last)
{
	char * ret = NULL;
	char *p, *end;
	char * temp;
	int length;
	int found = 0;
	int labelFound = 0;

	/* we won't mess up format, we promise... */
	for (p = (char *)format; *p != '\0'; )
	{
		if (p[0] == '|') {
			++p;
			if(!found) {
				if(ret) {
					free(ret);
					ret = NULL;
				}
			}
			else {
				p = skipFormatting(p);
			}
			continue;
		}
		
		if (p[0] == '&') {
			++p;
			if(found == 0) {
				p = skipFormatting(p);
			}
			else {
				found = 0;
			}
			continue;
		}
		
		if (p[0] == '[')
		{
			temp = songToFormatedString(song, p+1, &p);
			if(temp) {
				ret = appendToString(ret, temp, strlen(temp));
				found = 1;
			}
			continue;
		}

		if (p[0] == ']')
		{
			if(last) *last = p+1;
			if(!found && ret) {
				free(ret);
				ret = NULL;
			}
			return ret;
		}

		/* pass-through non-escaped portions of the format string */
		if (p[0] != '#' && p[0] != '%')
		{
			ret = appendToString(ret, p, 1);
			++p;
			continue;
		}

		/* let the escape character escape itself */
		if (p[0] == '#' && p[1] != '\0')
		{
			ret = appendToString(ret, p+1, 1);
			p+=2;
			continue;
		}

		/* advance past the esc character */

		/* find the extent of this format specifier (stop at \0, ' ', or esc) */
		temp = NULL;

		end  = p+1;
		while(*end >= 'a' && *end <= 'z')
		{
			end++;
		}
		length = end - p + 1;

		labelFound = 0;
		char number[10];
		
		if(*end != '%')
			length--;
		else if (strncmp("%file%", p, length) == 0) {
			temp = fromUtf8(song->file);
		}else if (strncmp("%shortfile%", p, length) == 0) {
			if (strrchr(song->file,'/'))
				temp=strrchr(fromUtf8(song->file),'/')+1;
			else 
				temp=fromUtf8(song->file);
		}
		else if (strncmp("%fileext%", p, length) == 0) {
			labelFound = 1;
			temp = *song->fileext ? fromUtf8(song->fileext) : NULL;
		}
		else if (strncmp("%artist%", p, length) == 0) {
			labelFound = 1;
			temp = *song->artist ? fromUtf8(song->artist) : NULL;
		}
		else if (strncmp("%title%", p, length) == 0) {
			labelFound = 1;
			temp = *song->title ? fromUtf8(song->title) : NULL;
		}
		else if (strncmp("%album%", p, length) == 0) {
			labelFound = 1;
			temp = *song->album ? fromUtf8(song->album) : NULL;
		}
		else if (strncmp("%track%", p, length) == 0) {
			labelFound = 1;
			temp = *song->track ? fromUtf8(song->track) : NULL;
		}
		else if (strncmp("%name%", p, length) == 0) {
			labelFound = 1;
			temp = *song->name ? fromUtf8(song->name) : NULL;
		}
		else if (strncmp("%plpos%", p, length) == 0) {
			labelFound = 1;
			if (song->pos>0) {
				snprintf(number, 9, "%d", song->pos);
				temp = fromUtf8(number);
			}
		}
		else if (strncmp("%time%", p, length) == 0) {
			labelFound = 1;
			if (song->time != MPD_SONG_NO_TIME) {
				char s[10];
				snprintf(s, 9, "%d:%02d", song->time / 60, 
						song->time % 60);
				/* nasty hack to use static buffer */
				temp = fromUtf8(s);
			}
		}

		if( temp == NULL && !labelFound ) {
			ret = appendToString(ret, p, length);
		}
		else if( temp != NULL ) {
			found = 1;
			ret = appendToString(ret, temp, strlen(temp));
		}

		/* advance past the specifier */
		p += length;
	}

	if(last) *last = p;
	return ret;
}

/* this is a little ugly... */
char * infoToFormatedString (struct MPD_PLAYER_SONG *song, struct MPD_PLAYER_STATUS *player, \
			     const char * format, char ** last)
{
	char * ret = NULL;
	char *p, *end;
	char * temp;
	int length;
	int found = 0;
	int labelFound = 0;
	char number[10]="";
	
	/* we won't mess up format, we promise... */
	for (p = (char *)format; *p != '\0'; )
	{
		if (p[0] == '|') {
			++p;
			if(!found) {
				if(ret) {
					free(ret);
					ret = NULL;
				}
			}
			else {
				p = skipFormatting(p);
			}
			continue;
		}
		
		if (p[0] == '&') {
			++p;
			if(found == 0) {
				p = skipFormatting(p);
			}
			else {
				found = 0;
			}
			continue;
		}
		
		if (p[0] == '[')
		{
			temp = infoToFormatedString(song, player, p+1, &p);
			if(temp) {
				ret = appendToString(ret, temp, strlen(temp));
				found = 1;
			}
			continue;
		}

		if (p[0] == ']')
		{
			if(last) *last = p+1;
			if(!found && ret) {
				free(ret);
				ret = NULL;
			}
			return ret;
		}

		/* pass-through non-escaped portions of the format string */
		if (p[0] != '#' && p[0] != '%')
		{
			ret = appendToString(ret, p, 1);
			++p;
			continue;
		}

		/* let the escape character escape itself */
		if (p[0] == '#' && p[1] != '\0')
		{
			ret = appendToString(ret, p+1, 1);
			p+=2;
			continue;
		}

		/* advance past the esc character */

		/* find the extent of this format specifier (stop at \0, ' ', or esc) */
		temp = NULL;

		end  = p+1;
		while(*end >= 'a' && *end <= 'z')
		{
			end++;
		}
		length = end - p + 1;

		labelFound = 0;
		strcpy(number,"");
		if(*end != '%')
			length--;
				else if (strncmp("%file%", p, length) == 0) {
			temp = fromUtf8(song->file);
		}else if (strncmp("%shortfile%", p, length) == 0) {
			if (strrchr(song->file,'/'))
				temp=strrchr(fromUtf8(song->file),'/')+1;
			else 
				temp=fromUtf8(song->file);
		}
		else if (strncmp("%fileext%", p, length) == 0) {
			labelFound = 1;
			temp = *song->fileext ? fromUtf8(song->fileext) : NULL;
		}
		else if (strncmp("%artist%", p, length) == 0) {
			labelFound = 1;
			temp = *song->artist ? fromUtf8(song->artist) : NULL;
		}
		else if (strncmp("%title%", p, length) == 0) {
			labelFound = 1;
			temp = *song->title ? fromUtf8(song->title) : NULL;
		}
		else if (strncmp("%album%", p, length) == 0) {
			labelFound = 1;
			temp = *song->album ? fromUtf8(song->album) : NULL;
		}
		else if (strncmp("%track%", p, length) == 0) {
			labelFound = 1;
			temp = *song->track ? fromUtf8(song->track) : NULL;
		}
		else if (strncmp("%name%", p, length) == 0) {
			labelFound = 1;
			temp = *song->name ? fromUtf8(song->name) : NULL;
		}
		else if (strncmp("%plpos%", p, length) == 0) {
			labelFound = 1;
			if (song->pos>0) {
				snprintf(number, 9, "%d", song->pos);
				temp = fromUtf8(number);
			}
		}
		else if (strncmp("%time%", p, length) == 0) {
			labelFound = 1;
			if (song->time != MPD_SONG_NO_TIME) {
				char s[10];
				snprintf(s, 9, "%d:%02d", song->time / 60, 
						song->time % 60);
				/* nasty hack to use static buffer */
				temp = fromUtf8(s);
			}
		}
		else if (strncmp("%volume%", p, length) == 0) 
		{
			labelFound = 1;
			if (player->volume<0||player->volume>100)
				temp=NULL;
			else
				snprintf(number, 9, "%d", player->volume);
		}else if (strncmp("%repeat%", p, length) == 0) 
		{
			labelFound = 1;
			temp = player->repeat ? number : NULL;
		}else if (strncmp("%random%", p, length) == 0) 
		{
			labelFound = 1;
			temp = player->random ? number : NULL;
		}		
		if( temp == NULL && !labelFound ) {
			ret = appendToString(ret, p, length);
		}
		else if( temp != NULL ) {
			found = 1;
			ret = appendToString(ret, temp, strlen(temp));
		}

		/* advance past the specifier */
		p += length;
	}

	if(last) *last = p;
	return ret;
}


