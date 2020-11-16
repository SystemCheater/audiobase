/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/common/ABaselog.h,v $ -- Logging functions
 * $Id: ABaselog.h,v 1.2 2003/05/07 16:05:41 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#ifndef LOG_H
#define LOG_H


#define LOG_QUIET	0	// nothing logged
#define LOG_ERROR	1	// errors
#define LOG_NORMAL	2	// normal messages
#define LOG_VERBOSE	3	// verbose output
#define LOG_DEBUG	4	// debug messages
#define LOG_NOISYDEBUG	5	// lots of debug info


void log_printf (int level, char *text, ...);

int log_open (char *filename, int level);
void log_close (void);


#endif /* LOG_H */

/*************************************************************************
 * EOF
 */
