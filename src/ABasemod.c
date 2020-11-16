/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/ABasemod.c,v $ -- Module handling functions
 * $Id: ABasemod.c,v 1.12 2003/11/29 21:31:31 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "config.h"
#include "ABaselog.h"
#include "modules.inc"
#include "ABasemod.h"

/*************************************************************************
 * GLOBALS
 */

fd_set	blank_fd;
mod_message_t* answer_head = NULL;
/*************************************************************************
 * LIST OF ALL MODULES
 */
mod_t *mod_list = NULL;
mod_message_t *mod_msgs = NULL;


/*************************************************************************
 * REGISTER A NEW MODULE
 */
void mod_register (mod_t *newmod)
{

	mod_t *last =mod_list;
	log_printf(LOG_NOISYDEBUG,"CORE: module %s registered blabla\n",newmod->mod_name);
	if(!mod_list) {
		mod_list = newmod;
		return;
	}
	while(last->next) last = last->next;
	last->next = newmod;
}


/*************************************************************************
 * SEND MESSAGE TO ALL MODULES
 */
void mod_sendmsg (int msgtype, char *msg)
{
	mod_message_t *newmsg, *m;
	log_printf(LOG_NOISYDEBUG,"CORE: message %s type %d sent",msg,msgtype);
	if(answer_head) { // we are in a query
		// the action has to be taken immediatly, so transform this to a query call
		// however, we don't care about the answer
		free_message(mod_query(msgtype,msg));
		// create new message entry
		newmsg = malloc(sizeof(mod_message_t));
		if (!newmsg) {
			return;
		}
		newmsg->msg = malloc(strlen(msg)+1);
		if (!newmsg->msg) {
			free(newmsg);
			return;
		}
		newmsg->msgtype = msgtype;
		strcpy(newmsg->msg, msg);
		newmsg->next = NULL;
		answer_head->next = newmsg;
		answer_head = newmsg;
		return;
	}

	// create new message entry
	newmsg = malloc(sizeof(mod_message_t));
	if (!newmsg)
		return;
	newmsg->msg = malloc(strlen(msg)+1);
	if (!newmsg->msg) {
		free(newmsg);
		return;
	}
	newmsg->msgtype = msgtype;
	strcpy(newmsg->msg, msg);
	newmsg->next = NULL;

	// append message to queue
	for (m=mod_msgs; m && m->next; m=m->next);
	m ? m->next : mod_msgs = newmsg;
}

void mod_sendmsgf (int msgtype, char *msg, ...)
{
	va_list ap;
	char buf[1024];
	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf)-1, msg, ap);
	buf[sizeof(buf)-1] = 0;
	va_end(ap);
	mod_sendmsg(msgtype, buf);
}


/*************************************************************************
 * INIT ALL MODULES
 */
void mod_init (void)
{
	mod_initfunc_t *mi;
	char *s;
	
	// call init function of all modules
	for (mi=mod_list_init; *mi; mi++) {
		s = (*mi)();
		if (s)
			log_printf(LOG_ERROR, "Error initializing module: %s\n", s);
	}
}


/*************************************************************************
 * DEINIT ALL MODULES
 */
void mod_deinit (void)
{
	mod_t *m;

	// call deinit function of all modules
	// and clean up module list
	for (m=mod_list; m; m=m->next)
		if (m->deinit) {
			log_printf(LOG_NOISYDEBUG,"CORE: deinitialising module %s\n",m->mod_name);
			m->deinit();
		}
	mod_list = NULL;
}


/*************************************************************************
 * RELOAD ALL MODULES
 */
void mod_reload (void)
{
	mod_t *m;
	char *s;

	// call reload function of all modules
	for (m=mod_list; m; m=m->next) {
		log_printf(LOG_NOISYDEBUG,"CORE: reinitialising module %s\n",m->mod_name);
		if (m->reload) {
			s = m->reload();
			if (s)
				log_printf(LOG_ERROR, "Error reloading module: %s\n", s);
		}
	}
}


/*************************************************************************
 * DISPERSE MESSAGES AND UPDATE ALL MODULES
 */
void mod_update (void)
{
	mod_t *m;
	mod_message_t *msg;
	mod_message_t msgdup;

	// process message queue
	while (mod_msgs) {
		msg = mod_msgs;

		// global messages we handle
		if (msg->msgtype == MSGTYPE_INPUT && !strcasecmp(msg->msg, "reload")) {
			raise(SIGHUP);

		// disperse message to modules, make a copy of the message
		// so that modules may alter it without affecting
		// other modules (think of strtok)
		} else {
			for (m=mod_list; m; m=m->next)
				if (m->message) {
					log_printf(LOG_NOISYDEBUG,"CORE: sending message %s type %d to module %s\n",msg->msg,msg->msgtype,m->mod_name);
					msgdup.msgtype = msg->msgtype;
					msgdup.msg = malloc(strlen(msg->msg)+1);
					strcpy(msgdup.msg, msg->msg);
					m->message(msgdup.msgtype, msgdup.msg);
					free(msgdup.msg);
				}
		}

		// remove old message from queue
		mod_msgs = mod_msgs->next;
		free(msg->msg);
		free(msg);
	}

	// call update function of all modules
	for (m=mod_list; m; m=m->next) {
		if (m->update){
			log_printf(LOG_NOISYDEBUG,"CORE: updating module %s\n",m->mod_name);
			m->update();
		}
	}
}

/*************************************************************************
 * HANDLE SIGCHLD CALLS FOR ALL MODULES
 */

void mod_sigchld(int __attribute__((unused)) signal) 
{
	pid_t pid;
	mod_t *m;
	int status;


	// this process was called because a child of ABase just died and
	// we received a SIGCHLD signal.  wait() won't block.
	pid = wait(&status);
	
	log_printf(LOG_NOISYDEBUG, "mod_sigchld(): notified that child %d died.\n", pid);

	// call child function of all modules
	for (m=mod_list;m;m=m->next)
		if (m->chld){
			log_printf(LOG_NOISYDEBUG,"CORE: sgnaling child death to module %s\n",m->mod_name);
			m->chld(pid,status);
		}
}


/*************************************************************************
 * POST A QUERY
 */

mod_message_t* mod_query(int msgtype, char* msg)
{
	mod_t *m;
	mod_message_t msgdup;
	mod_message_t* newmsg;
	mod_message_t* return_queue = malloc(sizeof(mod_message_t)); // create a new queue
	mod_message_t* tmp_answer = answer_head; // store previous head
	if(answer_head) { // we are in a query, add this to the list of answers
		// create new message entry
		newmsg = malloc(sizeof(mod_message_t));
		if (!newmsg) {
			free(return_queue);
			return NULL;
		}
		newmsg->msg = malloc(strlen(msg)+1);
		if (!newmsg->msg) {
			free(newmsg);
			free(return_queue);
			return NULL;
		}
		newmsg->msgtype = msgtype;
		strcpy(newmsg->msg, msg);
		newmsg->next = NULL;
		answer_head->next = newmsg;
		answer_head = newmsg;
	}
	return_queue->next= NULL;
	answer_head = return_queue; // the new queue is the active queue

	// process the message
	for (m=mod_list; m; m=m->next)
		if (m->message) {
			log_printf(LOG_NOISYDEBUG,"CORE: sending query %s type %d to module %s\n",msg,msgtype,m->mod_name);
			msgdup.msgtype = msgtype;
			msgdup.msg = malloc(strlen(msg)+1);
			strcpy(msgdup.msg, msg);
			m->message(msgdup.msgtype, msgdup.msg);
			free(msgdup.msg);
		}
	answer_head = tmp_answer; // restore previous queue
	tmp_answer = return_queue; // remove the first (fake) elt of the queue
	return_queue = return_queue->next;
	free(tmp_answer);
	return return_queue;
	
}

mod_message_t* mod_queryf(int msgtype, char* msg,...)
{
	va_list ap;
	char buf[1024];
	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf)-1, msg, ap);
	buf[sizeof(buf)-1] = 0;
	va_end(ap);
	return mod_query(msgtype, buf);
}
	
	

	

/*************************************************************************
 * FREE AN ANSWER
 */

void free_message(mod_message_t* to_free) 
{
	mod_message_t *freeing, *next;
	freeing = to_free;
	while(freeing) {
		next = freeing->next;
		free(freeing->msg);
		free(freeing);
		freeing = next;
	}
}
		
		
/*************************************************************************
 * EOF
 */
