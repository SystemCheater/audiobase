/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/common/ABaseconfig.c,v $ -- Runtime configuration management
 * $Id: ABaseconfig.c,v 1.4 2003/11/12 17:51:41 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer,Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 *
 * for information and support regarding ABase.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "ABaseconfig.h"
#include "ABaselog.h"
#include "ABasetools.h"



/*************************************************************************
 * GLOBALS
 */
char config_file[512] = "";
config_entry_t *config_list = NULL;


/*************************************************************************
 * GET A CONFIG KEY'S VALUE
 */
char *config_getstr (char *key, char *defaultvalue)
{
	config_entry_t *ent;
	for (ent=config_list; ent; ent=ent->next)
		if (!strcasecmp(ent->key, key)) {
			log_printf(LOG_NOISYDEBUG, "config_getstr(): '%s': '%s'\n", ent->key, ent->value);
			return ent->value;
		}
	log_printf(LOG_NOISYDEBUG, "config_getstr(): queried unknown key '%s'\n", key);
	return defaultvalue;
}

int config_getnum (char *key, int defaultvalue)
{
	char *s = config_getstr(key, NULL);
	return s ? (int)strtol(s,NULL,0) : defaultvalue;
}


/*************************************************************************
 * ADD A CONFIG KEY/VALUE PAIR
 * Note: we cannot use log_printf here, because the logfile may
 *       not have been opened yet.
 */
void config_add (char *key, char *value)
{
	config_entry_t *ent;

	ent = malloc(sizeof(config_entry_t));
	if (!ent)
		return;
	ent->key = malloc(strlen(key)+1);
	if (!ent->key) {
		free(ent);
		return;
	}
	strcpy(ent->key, key);
	ent->value = malloc(strlen(value)+1);
	if (!ent->value) {
		free(ent->key);
		free(ent);
		return;
	}
	strcpy(ent->value, value);
	ent->next = config_list;
	config_list = ent;
}


/*************************************************************************
 * READ THE CONFIG FILE
 * Note: we cannot use log_printf here, because the logfile may
 *       not have been opened yet.
 */
int config_read (char *configfile)
{
	int fd;
	char buf[255], *s;

	// open config file
	strncpy(config_file, configfile, sizeof(config_file)-1);
	fd = open(config_file, O_RDONLY);
	if (fd < 0)
		return -1;

	// read the config file
	while (readline(fd, buf, sizeof(buf)) >= 0) {
		trim(buf);
		if (!*buf || *buf == '#')
			continue;
		s = strchr(buf, ':');
		if (!s)
			continue;
		*s++ = 0;
		config_add(trim(buf), trim(s));
	}

	// close config file
	close(fd);
	return 0;
}


/*************************************************************************
 * FREE ALL CONFIG ENTRIES
 */
void config_free (void)
{
	config_entry_t *n, *ent = config_list;
	while (ent) {
		if (ent->value)
			free(ent->value);
		if (ent->key)
			free(ent->key);
		n = ent->next;
		free(ent);
		ent = n;
	}
	config_list = NULL;
	log_printf(LOG_DEBUG, "config_free(): freed all config entries\n");
}


/*************************************************************************
 * RELOAD THE CONFIG FILE
 */
void config_reload (void)
{
	config_free();
	config_read(config_file);
	log_printf(LOG_DEBUG, "config_reload(): reloaded config file\n");
}


/*************************************************************************
 * REMOVE AN OPTION FROM THE LIST
 */
int config_remove (char *key)
{
	config_entry_t *ent;
	config_entry_t **prev_ptr;
	log_printf(LOG_DEBUG, "config_remove(): removing key %s\n",key);
	prev_ptr = &config_list;
	
	for (ent=config_list; ent; ent=ent->next) {
		if (!strcasecmp(ent->key, key)) {
			log_printf(LOG_NOISYDEBUG, "config_remove(): found key '%s' with value '%s'\n", ent->key, ent->value);
			if (ent->value)
				free(ent->value);
			if (ent->key)
				free(ent->key);
			*prev_ptr = ent->next;
			free(ent);
			return 1;
		} else {
			prev_ptr = &ent->next;
		}

	}
	log_printf(LOG_DEBUG, "config_remove(): didn't find key '%s'\n",key);
	return 0;
}
/*************************************************************************
 * EOF
 */
