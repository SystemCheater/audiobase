/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_lirc.c,v $ -- LIRC input interface
 * $Id: mod_lirc.c,v 1.5 2003/08/06 18:58:51 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "ABaseconfig.h"
#include "ABaselog.h"
#include "ABasemod.h"
#include "mod_lirc.h"

#if LIRC_VERSION_INT >= 060
#include "lirc_client.h"
#else
#include "lirc_client.h"
#endif

/*************************************************************************
 * GLOBALS
 */
fd_set			 mod_lirc_fdset;
struct lirc_config 	*mod_lirc_irconfig;
char mod_lirc_prog[128];

/*************************************************************************
 * MODULE INFO
 */
mod_t mod_lirc = {
	"mod_lirc",
	mod_lirc_deinit,	// deinit
	mod_lirc_reload,	// reload
	&mod_lirc_fdset,	// watch_fdset
	mod_lirc_poll,		// poll
	NULL,			// update
	NULL,			// message
	NULL,			// SIGCHLD handler
	NULL,			// avoid warning
};


/*************************************************************************
 * POLL INPUT DATA
 */
void mod_lirc_poll (int __attribute__((unused)) fd)
{
#if LIRC_VERSION_INT >= 060
	char *code, *command;
	int rc;
	log_printf(LOG_DEBUG,"mod_lirc_poll(): entered.\n");
	if (!lirc_nextcode(&code)) {
		while ((rc = lirc_code2char(mod_lirc_irconfig, code, &command)) == 0 && command) {
			log_printf(LOG_NOISYDEBUG, "mod_lirc_poll(): got command '%s'\n", command);
			mod_sendmsg(MSGTYPE_INPUT, command);
		}
		free(code);
#else
	char *ir, *command;

	if ((ir = lirc_nextir()) != NULL) {
		while ((command = lirc_ir2char(mod_lirc_irconfig, ir)) != NULL) {
			log_printf(LOG_NOISYDEBUG, "mod_lirc_poll(): got command '%s'\n", command);
			mod_sendmsg(MSGTYPE_INPUT, command);
		}
		free(ir);
		
#endif
	} else {	// lirc has died
			log_printf(LOG_NORMAL, "mod_lirc_poll(): lirc read returned error .\n");
			FD_ZERO(&mod_lirc_fdset);
	}
}


/*************************************************************************
 * MODULE INIT FUNCTION
 */
char *mod_lirc_init (void)
{
	log_printf(LOG_DEBUG, "mod_lirc_init(): initializing\n");

	int		mod_lirc_fd=0;

	FD_ZERO(&mod_lirc_fdset);

	char* c=config_getstr("lirc_prog_name","abase");
	strncpy(mod_lirc_prog,c,strlen(c));
	log_printf(LOG_DEBUG, "mod_lirc_init(): lirc will hear on messages for %s\n", mod_lirc_prog);

	// connect to LIRC
#if LIRC_VERSION_INT >= 060
	if ((mod_lirc_fd = lirc_init(mod_lirc_prog, 0)) < 0)
#else
	if ((mod_lirc_fd = lirc_init(mod_lirc_prog)) < 0)
#endif
		return "Unable to connect to LIRC";

	log_printf(LOG_DEBUG, "mod_lirc_init(): lirc connection on fd %d\n", mod_lirc_fd);


	// read LIRC config
	if (lirc_readconfig(config_getstr("lirc_config", NULL), &mod_lirc_irconfig, NULL)) {
		lirc_deinit();
		return "Unable to read LIRC config";
	}

	// register our module
	FD_SET(mod_lirc_fd,&mod_lirc_fdset);
	mod_register(&mod_lirc);

	return NULL;
}


/*************************************************************************
 * MODULE DEINIT FUNCTION
 */
void mod_lirc_deinit (void)
{
	// free LIRC config
	lirc_freeconfig(mod_lirc_irconfig);

	// disconnect from LIRC
	lirc_deinit();
	
	log_printf(LOG_DEBUG, "mod_lirc_deinit(): closed lirc connection\n");
	FD_ZERO(&mod_lirc_fdset);
}


/*************************************************************************
 * MODULE RELOAD FUNCTION
 */
char *mod_lirc_reload (void)
{
	log_printf(LOG_DEBUG, "mod_lirc_reload(): reloading lirc config\n");

	// free LIRC config
	lirc_freeconfig(mod_lirc_irconfig);

	// read LIRC config
	lirc_readconfig(config_getstr("lirc_config", NULL), &mod_lirc_irconfig, NULL);

	return NULL;
}


/*************************************************************************
 * EOF
 */
