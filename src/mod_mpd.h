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

#ifndef MOD_MPD_H
#define MOD_MPD_H


void mod_mpd_poll (int fd);
void mod_mpd_update (void);
char *mod_mpd_init (void);
void mod_mpd_deinit (void);
char *mod_mpd_reload (void);
void mod_mpd_message (int msgtype, char *rawmsg);
void mod_mpd_print_error();

void mod_mpd_update_status();
void mod_mpd_update_songinfo();

#endif /* MOD_MPD_H */

/*************************************************************************
 * EOF
 */
