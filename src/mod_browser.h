/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_browser.h,v $ -- interactive file and directory browser
 * $Id: mod_browser.h,v 1.2 2003/05/07 16:05:41 boucman Exp $
 *
 * Copyright (C) by Alexander Fedtke
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */
#include "globalDef.h"

#ifndef MOD_BROWSER_H
#define MOD_BROWSER_H

void mod_browser_update (void);
void mod_browser_message (int msgtype, char *msg);
int mod_browser_get_mpddir(char *path);
void mod_browser_show();
int mod_browser_find_element(char *ElementName, int ElementType);
char *mod_browser_init (void);
char *mod_browser_reload (void);
void mod_browser_deinit (void);

/*cmp for browser_elements, alphabetically, dirs first, it sorts the pointers - not the content (!) */
int mod_browser_cmp_bwrelemnts(const void **element1, const void **element2);	


#endif /* MOD_BROWSER_H */

/*************************************************************************
 * EOF
 */
