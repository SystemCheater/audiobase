/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/common/ABaselog.c,v $ -- Logging functions
 * $Id: ABaselog.c,v 1.2 2003/05/07 16:05:41 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <malloc.h>

#include "config.h"
#include "ABaseconfig.h"
#include "ABaselog.h"

/*************************************************************************
 * GLOBALS
 */
int log_level = 0;
FILE *log_filehandle = NULL;
char *tmfmt=NULL;


/*************************************************************************
 * OPEN/CLOSE LOG
 */
int log_open (char *filename, int level)
{
	log_close();
	if (filename) {
		log_filehandle = fopen(filename, "a");
		if (!log_filehandle)
			return -1;
		setlinebuf(log_filehandle);
	}
	log_level = level;
	tmfmt=strdup(config_getstr("log_timestr","%X"));
	return 0;
}
void log_close (void)
{
	if (log_filehandle)
		fclose(log_filehandle);
	if (tmfmt) {
		free(tmfmt);
		tmfmt=NULL;
	}
	log_filehandle = NULL;
}


/*************************************************************************
 * LOG A STRING OF SPECIFIED LEVEL
 */
void log_printf (int level, char *text, ...)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	char tmstr[40];
	va_list ap;
	int i;

	if (log_level < level)
		return;

	// Log to console
	if (!log_filehandle && tmfmt) {
		if (!text || !*text)
			fprintf(stderr, "\n");
		else {
			if (log_level >= LOG_DEBUG) {
				memset(tmstr,0,sizeof(tmstr));
				strftime(tmstr,sizeof(tmstr),tmfmt,tm);
				fprintf(stderr, "%s %d:",tmstr,level);
			}
			for (i=0; i<level; i++)
				fprintf(stderr, " ");
			va_start(ap, text);
			vfprintf(stderr, text, ap);
			va_end(ap);
		}
		fflush(stderr);
	}

	// Log to file
	if (log_filehandle && tmfmt) {
		if (!text || !*text)
			fprintf(log_filehandle, "\n");
		else {
			memset(tmstr,0,sizeof(tmstr));
			strftime(tmstr,sizeof(tmstr),tmfmt,tm);
			fprintf(log_filehandle,"%s ",tmstr);
			for (i=0; i<level; i++)
				fprintf(log_filehandle, " ");
			va_start(ap, text);
			vfprintf(log_filehandle, text, ap);
			va_end(ap);
		}
	}
}


/*************************************************************************
 * EOF
 */
