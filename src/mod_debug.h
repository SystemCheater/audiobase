/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_debug.h,v $ -- sample debugging module
 * $Id: mod_debug.h,v 1.2 2003/05/07 16:05:41 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#ifndef MOD_DEBUG_H
#define MOD_DEBUG_H


void mod_debug_poll (int fd);
void mod_debug_message (int msgtype, char *message);

char *mod_debug_init (void);
void mod_debug_deinit (void);
char *mod_debug_reload (void);

int	watchfd;
fd_set	mod_debug_fdset;


#endif /* MOD_DEBUG_H */

/*************************************************************************
 * EOF
 */
