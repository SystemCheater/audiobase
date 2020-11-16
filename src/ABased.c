/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/ABased.c,v $ -- The main program
 * $Id: ABased.c,v 1.13 2003/09/21 16:02:37 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */
 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include "config.h"
#include "ABaseconfig.h"
#include "ABaselog.h"
#include "ABasemod.h"

// maximum number of allowed startup commands
#define COMMANDLINE_MESSAGE_MAX 64

#ifndef VERSION
#error "VERSION undefined - Use Makefile to compile!"
#endif
 
/*************************************************************************
 * globals
 **
 * progname: programname, taken from argv[0]
 * terminate: if true, we caught SIGTERM -> exit
 * reload: if true, we caught SIGHUP -> reload
 * daemonize: if true, run as daemon  --- removed in future
 */
static char *progname = NULL;	// progname (argv[0])
static int reload = 0;		// reload flag
extern int terminate;		// terminate flag
int daemonize = 1;		// daemon flag

/*************************************************************************
 * signal handler routins
 **
 * signal: Signal, that we should handle
 *    on SIGTERM, SIGINT terminate is set to true
 *    on SIGHUP reload is set to true
 *    else a message is written to log
 */
void sighandler(int signal)
{
   switch (signal) {
	case SIGTERM:
	case SIGINT:
	  terminate = signal;
	  break;
	case SIGHUP:
	  reload = signal;
	  break;
	default:
	  // it's not good to call other routines in a sighandler, but it's
	  // something wrong if we get an unwanted signal
	  log_printf(LOG_ERROR, "Something went wrong, caught signal %d", signal);
   }
}

/*************************************************************************
 * THE MAIN LOOP
 */
int loop (void)
{
	int retval, rc, highfd,n;
	fd_set fdset;
	struct timeval tv;
	mod_t *m;

	char buf[129];

	retval = 0;
	do {
		// set the fds to watch
		FD_ZERO(&fdset);
		highfd=0;
		for (m=mod_list; m; m=m->next)
			if (m->poll)
			{
				for (n=0;n<__FD_SETSIZE;n++) {
					if(FD_ISSET(n,m->watchfd_set)) {
						FD_SET(n, &fdset);
						 if (n > highfd)
							 highfd = n;
					}
				}
			}
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		highfd++;

		// wait for data or timeout

		// FORCES 500ms delay
		// each lcdproc action causes LCDd to send a success-message
		//  --> select returns always immediatelly, up to 30 times/sec!!
		//rc = select(highfd, NULL, NULL, NULL, &tv);

		rc = select(highfd, &fdset, NULL, NULL, &tv);
		if (terminate || reload)
			break;

		// select reported error
		if (rc < 0) {
			if (errno == EINTR) {
				log_printf(LOG_NOISYDEBUG, "loop(): Ignoring EINTR on select()...\n");
				rc = 0;
				continue;
			}
			perror("select()");
			retval = -1;
			break;

		// input detected, look who can handle it
		// cycle through each fd.  For each one that has input, check all mods
		// to have them poll it.  (need to handle this better)
		} else if (rc > 0) {
			for (n=0;n<=highfd;n++)
				if(FD_ISSET(n,&fdset))
					for (m=mod_list;m;m=m->next)
						if (m->poll && FD_ISSET(n,m->watchfd_set))
							m->poll(n);
		// select timed out
		} 

		// disperse messages and update modules
		mod_update();

	} while (rc >= 0);

	return retval;
}


/*************************************************************************
 * COMMAND LINE HELP
 */
void usage (void)
{
	printf("ABased %s (C) by Andreas Neuhaus, David Potter and Volker Wegert\n", VERSION);
	printf("Questions, comments, ideas to Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>\n");
	printf("Usage: %s [OPTION]...\n", progname);
	printf("	-h              this help\n");
	printf("	-q              silent operation (no logging except errors)\n");
	printf("	-qq             quiet operation (no logging)\n");
	printf("	-v              increase verbose level (more output)\n");
	printf("	-l file         logfile to use if running in background\n");
	printf(" 	                default: ABase.log\n");
	printf("	-f              run in foreground and log to stdout\n");
	printf("	-c file         specifies the config file to use\n");
	printf("	                default: %s\n",CONFFILE);
	printf("	-m message      input message to send at startup\n");
	printf("        -w welcomemsg   message to show at startup\n");
	printf("\n");
}


/*************************************************************************
 * MAIN
 */
int main (int argc, char *argv[])
{
	char c;
	int loglevel = LOG_NORMAL;
	char *logfile = "ABase.log";
	char *configfile = CONFFILE;

	char *commandLineMessage[COMMANDLINE_MESSAGE_MAX];	// multiple startup commands
	int   commandLineMessageCount = 0;			// multiple startup commands count

	char *welcomeMessage = NULL;				// welcome message

	char *execcmd=NULL,*execargs[48],*tmp=NULL;
	int rc;
	int i=0;						// multiple startup message counter
	char *configCommands;					// msg read from config file
	char *currentCommand;					// currentrly handled command from config file

	
	// parse command line parameters
  	(progname = strrchr(argv[0], '/')) ? progname++ : (progname = argv[0]);
	while ((c = (char)getopt(argc, argv, "hqvl:fc:m:w:")) != EOF) {
		if (c == ':' || c == '?') {
			usage();
			return EXIT_FAILURE;
		}
		switch (c) {
			case 'h':
				usage();
				return EXIT_FAILURE;
			case 'q':
				loglevel--;
				break;
			case 'v':
				loglevel++;
				break;
			case 'l':
				logfile = optarg;
				break;
			case 'f':
				daemonize = 0;
				break;
			case 'c':
				configfile = optarg;
				break;
			case 'm':       // multiple messages passed in from command line
				if (commandLineMessageCount < COMMANDLINE_MESSAGE_MAX)
				    commandLineMessage[commandLineMessageCount++] = optarg;
				break;
			case 'w':       // welcome message to show on display
				welcomeMessage = optarg;
				break;
			default:
				usage();
				return EXIT_FAILURE;
		}
	}
	if (optind < argc) {
		usage();
		return EXIT_FAILURE;
	}

	// read configuration
	if (config_read(configfile)) {
		fprintf(stderr, "ABased: Unable to read config file %s\n", configfile);
		return EXIT_FAILURE;
	}

	// setup logging
	log_open(daemonize ? logfile : NULL, loglevel);

	if (daemonize) 
	{
#ifdef __USE_BSD
//		daemon(0,0);
#else
		int child_pid, fd;
		switch (fork()) {
			case -1:
		 		log_printf(LOG_ERROR, "Cannot fork:%s\n", strerror(errno));
		 		return EXIT_FAILURE;
	   		case 0:  break;                      // Our child is born :-)
	   		default: return EXIT_SUCCESS;        // Parent ends here
	  	}

		if (setpgrp() == -1 ) {		// change process group
			log_printf(LOG_ERROR, "Cannot change process group ID:%s\n",
				   strerror(errno));
			return EXIT_FAILURE;
	  	}

		close(STDIN_FILENO);     // close unnecessary file descriptors.
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

# ifndef _PATH_DEVNULL
#  define _PATH_DEVNULL "/dev/null"
# endif
		if ((fd = open(_PATH_DEVNULL, O_RDWR)) == -1)
			log_printf(LOG_ERROR, "Cannot open "_PATH_DEVNULL" for output"
				   "redirection");
	  	else {
			dup2(fd, STDIN_FILENO);
	        	dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO) close(fd);
		}
#endif
	}
	chdir("/");			// don't prevent sysadmin from unmounting any filesystem	
	umask(0);			// discard my parent's umask.
   
	// print copyright
	log_printf(LOG_NORMAL, "");
	log_printf(LOG_NORMAL, "ABased %s (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>\n", VERSION);
	log_printf(LOG_NORMAL, "Questions, comments, ideas to current maintainer: Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>\n");

	// initialize all modules
	mod_init();


	// handle signals
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGCHLD, mod_sigchld);

	// display welcome message
	if (welcomeMessage) {
	    log_printf(LOG_DEBUG, "Sending welcome message '%s'.\n", welcomeMessage);
	    mod_sendmsgf(MSGTYPE_INFO, "welcome %s", welcomeMessage);
        }

	// parse commands from config file
	configCommands = config_getstr("ABase_init_command",NULL);
	if (configCommands) {
		currentCommand = strtok(configCommands,";");
		while (currentCommand) {
			log_printf(LOG_DEBUG, "Executing startup command (config file) '%s'.\n", currentCommand);
			mod_sendmsg(MSGTYPE_INPUT, currentCommand);
			currentCommand = strtok(NULL,";");
		}
		free(configCommands);
	}

	// issue multiple startup commands
	while (i < commandLineMessageCount) {
	    log_printf(LOG_DEBUG, "Executing startup command (command line) '%s'", commandLineMessage[i]);
	    mod_sendmsg(MSGTYPE_INPUT, commandLineMessage[i++]);
        }

	FD_ZERO(&blank_fd);

	// run the main loop
	do {

		// the main loop
		rc = loop();
		// reload data if neccessary
		if (reload) {
			config_reload();
			mod_reload();
			reload=0;
		}
	} while (!terminate && rc >= 0);

	// see if we should exec a program on exit
	tmp=config_getstr("exec_on_close",NULL);
	if(tmp) {
		execcmd=(char *)malloc(strlen(tmp)+2);
		strcpy(execcmd,tmp);
	}

	// deinitialize all modules
	mod_deinit();

	// free up configuration
	config_free();

	// shutdown logging
	log_printf(LOG_NORMAL, "ABase %s shutdown\n", VERSION);
	if(execcmd) log_printf(LOG_NORMAL,"executing on exit: %s\n",execcmd);
	log_close();

	// exit program
	if(execcmd) {
		rc=0;
		execargs[rc]=strtok(execcmd," ");
		while(execargs[rc] != NULL)
			execargs[++rc]=strtok(NULL," ");
		execvp(execargs[0],execargs);		// launch the specified program
	}
	return EXIT_SUCCESS;
}


/*************************************************************************
 * EOF
 */
