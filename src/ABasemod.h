/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/ABasemod.h,v $ -- Module handling functions
 * $Id: ABasemod.h,v 1.7 2003/07/25 19:36:13 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#ifndef MOD_H
#define MOD_H

#include <sys/types.h>
/*************************************************************************
 * MODULE FUNCTION TYPES
 */
typedef char *(*mod_initfunc_t)(void);
typedef void (*mod_deinitfunc_t)(void);
typedef char *(*mod_reloadfunc_t)(void);
typedef void (*mod_pollfunc_t)(int fd);
typedef void (*mod_updatefunc_t)(void);
typedef void (*mod_messagefunc_t)(int messagetype, char *message);
typedef void (*mod_sigchldfunc_t)(pid_t childpid, int status);

typedef struct mod_s {
	const char *mod_name;		// module name
	mod_deinitfunc_t deinit;	// deinit function
	mod_reloadfunc_t reload;	// reload function
	fd_set *watchfd_set;		// fd's to watch for input
	mod_pollfunc_t poll;		// called when watchfd has input
	mod_updatefunc_t update;	// called periodically
	mod_messagefunc_t message;	// called to distribute messages
	mod_sigchldfunc_t chld;		// called to handle dying children

	struct mod_s *next;
} mod_t;

extern mod_t *mod_list;


/*************************************************************************
 * MODULE MESSAGE DISTRIBUTION
 */
#define MSGTYPE_EVENT		0	// Events notification
#define MSGTYPE_INPUT		1	// message from input module
#define MSGTYPE_PLAYER		2	// message from player module
#define MSGTYPE_INFO		3	// Other info, answers to queries
#define MSGTYPE_QUERY		4	// Queries

typedef struct mod_message_s {
	int msgtype;			// type of message
	char *msg;			// the message itself

	struct mod_message_s *next;
} mod_message_t;

	
/*************************************************************************
 * PUBLIC MODULE FUNCTIONS (used by the modules)
 */
void mod_register (mod_t *newmod);

void mod_sendmsg (int msgtype, char *msg);
void mod_sendmsgf (int msgtype, char *msg, ...);

mod_message_t * mod_query(int msgtype, char *msg);
mod_message_t * mod_queryf(int msgtype, char *msg,...);


void free_message(mod_message_t* to_free);
/*************************************************************************
 * PRIVATE MODULE FUNCTIONS (only used by the main program)
 */
void mod_init (void);
void mod_deinit (void);
void mod_reload (void);
void mod_update (void);
void mod_sigchld (int signal);


#endif /* MOD_H */

/*************************************************************************
 * GLOBALS
 */

fd_set blank_fd;


/*************************************************************************
 * EOF
 */
