/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_lirc.h,v $ -- LIRC input interface
 * $Id: mod_lirc.h,v 1.2 2003/05/07 16:05:42 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#ifndef MOD_LIRC_H
#define MOD_LIRC_H


void mod_lirc_poll (int fd);

char *mod_lirc_init (void);
void mod_lirc_deinit (void);
char *mod_lirc_reload (void);


#endif /* MOD_LIRC_H */

/*************************************************************************
 * EOF
 */
