/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_debug.c,v $ -- sample debugging module
 * $Id: mod_debug.c,v 1.4 2003/06/10 09:21:30 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

/*
 * This is an example module for ABase. It reads stdin and passes all
 * entered text as messages to the other modules. So you can enter
 * commands at the console. It also shows all other messages coming
 * from the other modules.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


#include "ABaselog.h"
#include "config.h"
#include "ABasetools.h"
#include "ABasemod.h"
#include "mod_debug.h"

/*************************************************************************
 * GLOBALS     
 */

extern int daemonize;	// set by ABased.c if we are a daemon
fd_set	mod_debug_fdset;

/*************************************************************************
 * MODULE INFO
 *	This structure is used to pass to mod_register to register
 *	our module with the main program.
 */
mod_t mod_debug = {
	"mod_debug",
	mod_debug_deinit,	// our deinit function
	mod_debug_reload,	// called when got SIGHUP
	&mod_debug_fdset,	// we're watching the stdin
	mod_debug_poll,		// and process the typed text
	NULL,			// we don't need to be called periodically
	mod_debug_message,	// our message handler
	NULL,			// SIGCHLD handler
	NULL,			// avoid warning
};


/*************************************************************************
 * POLL INPUT DATA
 *	This function will be called whenever data is available at our
 *	watched fd (the standard input). We need to read and process
 *	the new data here.
 */
void mod_debug_poll (int fd)
{
	char s[512];
	readline(fd, s, sizeof(s));
	log_printf(LOG_NOISYDEBUG, "mod_debug_poll(): got input: '%s'\n", s);
	if (s && *s)
		mod_sendmsg(MSGTYPE_INPUT, s);
}


/*************************************************************************
 * RECEIVE MESSAGE
 *	Whenever another module sends out a message via mod_sendmsg(),
 *	this function of all active modules will be called to notify
 *	them about the new message. msgtype shows the type of
 *	the message, msg contains the message.
 */
void mod_debug_message (int msgtype, char *msg)
{
	log_printf(LOG_NOISYDEBUG, "mod_debug_message(): got message type %d: '%s'\n", msgtype, msg);
}


/*************************************************************************
 * MODULE INIT FUNCTION
 *	This is called rigth after starting ABase and should set up global
 *	variables and register your module via mod_register().
 *	It should return NULL if everything went ok, or an error string
 *	with the error description.
 */
char *mod_debug_init (void)
{
	log_printf(LOG_DEBUG, "mod_debug_init(): initializing\n");
	FD_ZERO(&mod_debug_fdset);

	// register our module with the main program so that it knows
	// what we want to handle. We must provide a pointer to a
	// mod_t variable which is valid during the whole runtime, so
	// don't specify a local variable here.
	if (daemonize) {		// don't bother monitoring STDIN
		log_printf(LOG_DEBUG, "mod_debug_init(): in daemon mode... not monitoring stdin.\n");
		mod_debug.poll=NULL;
	} else
		FD_SET(STDIN_FILENO,&mod_debug_fdset);

	mod_register(&mod_debug);
	return NULL;
}


/*************************************************************************
 * MODULE DEINIT FUNCTION
 *	This is called right before ABase shuts down. You should clean
 *	up all your used data here to prepare for a clean exit.
 */
void mod_debug_deinit (void)
{
	// nothing to do here

	log_printf(LOG_DEBUG, "mod_debug_deinit(): deinitialized\n");
}


/*************************************************************************
 * MODULE RELOAD FUNCTION
 *	This is called whenever ABase needs to reload all configs (usually
 *	when receiving a SIGHUP). You should reinit your module or check
 *	for changed configuration here so that changes take effect.
 */
char *mod_debug_reload (void)
{
	log_printf(LOG_DEBUG, "mod_debug_reload(): reloading\n");

	// nothing to do here
	return NULL;
}


/*************************************************************************
 * EOF
 */
