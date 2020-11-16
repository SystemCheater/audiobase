/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/common/ABaseconfig.h,v $ -- Runtime configuration management
 * $Id: ABaseconfig.h,v 1.3 2003/06/05 11:15:18 boucman Exp $
 *
 * Copyright (C) by Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */

#ifndef CONFIG_H
#define CONFIG_H


/*************************************************************************
 * TYPES
 */
typedef struct config_entry_s {
	char *key;
	char *value;

	struct config_entry_s *next;
} config_entry_t;

extern config_entry_t *config_list;


/*************************************************************************
 * FUNCTIONS
 */
char *config_getstr (char *key, char *defaultvalue);
int config_getnum (char *key, int defaultvalue);

int config_read (char *configfile);
void config_free (void);
void config_reload (void);


int config_remove(char* key);


#endif /* CONFIG_H */

/*************************************************************************
 * EOF
 */
