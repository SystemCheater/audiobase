/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_lcdproc.h,v $ -- LCDproc display interface
 * $Id: mod_lcdproc.h,v 1.2 2003/05/07 16:05:41 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#ifndef MOD_LCDPROC_H
#define MOD_LCDPROC_H

void mod_lcdproc_poll (int fd);
void mod_lcdproc_update (void);
void mod_lcdproc_message (int msgtype, char *message);
void mod_lcdproc_clockstart (void);
void mod_lcdproc_clockstop (void);
void mod_lcdproc_timedisplay (void);
void mod_lcdproc_refresh();

char *mod_lcdproc_init (void);
void mod_lcdproc_deinit (void);
char *mod_lcdproc_reload (void);


#endif /* MOD_LCDPROC_H */

/*************************************************************************
 * EOF
 */
