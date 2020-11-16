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