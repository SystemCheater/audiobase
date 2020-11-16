/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/common/ABasetools.h,v $ -- some useful functions
 * $Id: ABasetools.h,v 1.3 2003/05/07 16:05:41 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#ifndef TOOLS_H
#define TOOLS_H


/*************************************************************************
 * GLOBAL VARIABLES
 */
// signals to reading function that they shouldn't wait for input and return.
extern int terminate;

/*************************************************************************
 * MACRO FUNCTIONS
 */
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))


/*************************************************************************
 * FUNCTIONS
 */
int freadtrimedline(FILE *fh, char *buf, int count);
int readline (int fd, char *buf, int count);
int sendtext (int fd, char *text, ...);
int sendtextWait (int fd, char *successMessage, char *text, ...);
char *trimleft (char *str);
char *trimright (char *str);
char *trim (char *str);

#endif /* TOOLS_H */

/*************************************************************************
 * EOF
 */
