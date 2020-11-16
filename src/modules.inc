/*************************************************************************
 * 
 * irmp3 - Multimedia Audio Jukebox for Linux
 * http://irmp3.sourceforge.net
 *
 * $Source: /cvsroot/irmp3/irmp3/src/irmp3d/modules.inc,v $ -- List of available modules
 * $Id: modules.inc,v 1.6 2003/09/07 19:39:59 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding irmp3.
 *
 *
 */

#ifndef MODULES_INC
#define MODULES_INC

#include "ABasemod.h"


/*************************************************************************
 * MODULE INIT FUNCTION PROTOTYPES
 */
char *mod_browser_init (void);
//char *mod_playlist_init (void);
//char *mod_env_init (void);
//char *mod_timer_init (void);
//char *mod_alarm_init (void);
char *mod_mpd_init(void);
char *mod_lirc_init (void);
char *mod_lcdproc_init (void);
//char *mod_netctl_init (void);
//char *mod_idle_init (void);
//char *mod_autoshutdown_init (void);
char *mod_debug_init (void);
//char *mod_fifo_init (void);
//char *mod_player_init (void);


/*************************************************************************
 * LIST OF MODULE INIT FUNCTIONS
 */
mod_initfunc_t mod_list_init[] = {

#ifdef MOD_BROWSER
	mod_browser_init,
#endif

#ifdef MOD_PLAYLIST
//	mod_playlist_init,
#endif

#ifdef MOD_ENV
//	mod_env_init,
#endif

#ifdef MOD_TIMER
//	mod_timer_init,
#endif

#ifdef MOD_ALARM
//	mod_alarm_init,
#endif

#ifdef MOD_LIRC
	mod_lirc_init,
#endif

#ifdef MOD_MPD
	mod_mpd_init,
#endif

#ifdef MOD_NETCTL
//	mod_netctl_init,
#endif

#ifdef MOD_IDLE
//	mod_idle_init,
#endif

#ifdef MOD_LCDPROC
	mod_lcdproc_init,
#endif

#ifdef MOD_AUTOSHUTDOWN
//	mod_autoshutdown_init,
#endif

#ifdef MOD_FIFO
//	mod_fifo_init,
#endif

#ifdef MOD_TIMIDITY
//	mod_timidity_init,
#endif

#ifdef MOD_PLAYER
//	mod_player_init,
#endif

#ifdef MOD_DEBUG
	mod_debug_init,
#endif

	NULL
};


#endif /* MODULES_INC */

/*************************************************************************
 * EOF
 */
