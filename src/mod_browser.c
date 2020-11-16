/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_browser.c,v $ -- interactive file and directory browser
 * $Id: mod_browser.c,v 1.9 2003/09/13 16:37:44 boucman Exp $
 *
 * Copyright (C) by Alexander Fedtke
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "config.h"
#include "ABaseconfig.h"
#include "ABaselog.h"
#include "ABasetools.h"
#include "ABasemod.h"
#include "mod_browser.h"
#include "globalDef.h"
#include "util.h"



/*************************************************************************
 * GLOBALS
 */


//move to globalDef.h
//#define MOD_BROWSER_MAXENTRY 1024	// maximum number of entries per directory

#define MOD_BROWSER_DIRSTYPE_OTH 0
#define MOD_BROWSER_DIRSTYPE_DIR 1
#define MOD_BROWSER_DIRSTYPE_MP3 2
#define MOD_BROWSER_DIRSTYPE_M3U 3
#define MOD_BROWSER_DIRSTYPE_M3L 4
#define MOD_BROWSER_DIRSTYPE_EXE 5
#define MOD_BROWSER_DIRSTYPE_HID 6	// will not be displayed

char mod_browser_dirstype_icon[7] = {'?', '>', '+', '*', '=', '!', 'x'};

char mod_browser_path[512];
char mod_browser_root[512];
char mod_browser_exec[512];
char mod_browser_list[512]; //path to playlists  (by Sebi Bauer 04.10.2004)
char mod_browser_execmsg[512];
// char browser.pElements[MOD_BROWSER_MAXENTRY][512]; //moved to BROWSER_ELEMENT::info_path[512]
// int  mod_browser_dirstype[MOD_BROWSER_MAXENTRY];   //moved to BROWSER_ELEMENT::info_type
int  mod_browser_activate_first;

char mod_browser_mask_audio[512];
char mod_browser_mask_list[512];
char mod_browser_mask_listlist[512];
char mod_browser_mask_exec[512];
char mod_browser_mask_hide[512];

int  mod_browser_mask_sensitiveflag;

int  mod_browser_cdup               = 1;	// insert ".." directory?
int  mod_browser_cdup_initial_updir = 0;	// stay on ".." after entering directory?
int  mod_browser_pwddepth           = 1;
//int  mod_browser_dirspos            = 0;
//int  mod_browser_dirscount          = 0;

int  mod_browser_chld_pid;
int  mod_browser_chld_exited;
int  mod_browser_chld_status;
//int  mod_browser_isactivated=0;	//is browser activated (then the into,back,... are accepted)
					//moved to struct BROWSER::status







/*************************************************************************
 * MODULE INFO
 */
mod_t mod_browser = {
	"mod_browser",
	mod_browser_deinit,	// deinit
	mod_browser_reload,	// reload
	0,			// watchfd
	NULL,			// poll
	NULL,			// update
	mod_browser_message,	// message
	NULL,			// SIGCHLD handler
	NULL,			// avoid warning
};






/*************************************************************************
 * CHECK FILE TYPE
 * function retrieves the dirtype (configured in abase.conf) of PHYSICAL dirs
 */
int mod_browser_checkdirstype(char *dest)	//dest = path+filename
{
    struct stat st;

 
    if (!dest)
	return MOD_BROWSER_DIRSTYPE_OTH;

    if (stat(dest, &st)) {
	log_printf(LOG_DEBUG, "mod_browser_checkdirstype(): Can't stat '%s'. Skipping.\n", dest);
	return MOD_BROWSER_DIRSTYPE_OTH;
    }

    if (S_ISDIR(st.st_mode)) return MOD_BROWSER_DIRSTYPE_DIR;

    if (!fnmatch(mod_browser_mask_audio, dest, FNM_NOESCAPE | mod_browser_mask_sensitiveflag))
	return MOD_BROWSER_DIRSTYPE_MP3;

    if (!fnmatch(mod_browser_mask_list, dest, FNM_NOESCAPE | mod_browser_mask_sensitiveflag))
	return MOD_BROWSER_DIRSTYPE_M3U;
    
    if (!fnmatch(mod_browser_mask_listlist, dest, FNM_NOESCAPE | mod_browser_mask_sensitiveflag))
	return MOD_BROWSER_DIRSTYPE_M3L;

    if (mod_browser_exec && !strncasecmp(mod_browser_exec, dest, strlen(mod_browser_exec)))
	if (!fnmatch(mod_browser_mask_exec, dest, FNM_NOESCAPE | mod_browser_mask_sensitiveflag))
	    return MOD_BROWSER_DIRSTYPE_EXE;

    if (!fnmatch(mod_browser_mask_hide, dest, FNM_NOESCAPE | mod_browser_mask_sensitiveflag))
	return MOD_BROWSER_DIRSTYPE_HID;

    return MOD_BROWSER_DIRSTYPE_OTH;
}


/*************************************************************************
 * SCAN DIRECTORY
 * function scans PHYSICAL directorys
 */
 // dest: full absolute path
 // return: browser.pElements[i]->info_path: path/filename relative to parent dir
 //// NOT TESTED, DOES NOT WORK!
int mod_browser_intodirscan(char *dest)
{
    DIR *dir;
    struct dirent *dirent;
    struct stat st;
    
    char buf[512];
    char filelist[MOD_BROWSER_MAXENTRY][512];
    int  dircount=0;
    int  i;

    if (!dest)
	return 0;
    
    log_printf(LOG_DEBUG, "mod_browser_intodirscan(): Scanning dir '%s'.\n", dest);

    dir = opendir(dest);
    if (!dir) {
	log_printf(LOG_DEBUG, "mod_browser_intodirscan(): Unable to read dir '%s'.\n", mod_browser_path);
	return 0;
    }

    while (1) {
	dirent = readdir(dir);
	if (!dirent)
	    break;
	if (!strcmp(dirent->d_name, "."))
	    continue;
	if (!strcmp(dirent->d_name, ".."))
	    if (!mod_browser_cdup)
		continue;
	if (!strcasecmp(dest, "/"))
	    snprintf(buf, sizeof(buf)-1, "/%s", dirent->d_name);
	else
	    snprintf(buf, sizeof(buf)-1, "%s/%s", dest, dirent->d_name);

	//check if element can be stated
	if (stat(buf, &st)) 
	{
		log_printf(LOG_DEBUG, "mod_browser_intodirscan(): Can't stat '%s'. Skipping.\n", buf);
		continue;
		dircount++;
		//elements haven't any memory yet!
		if (!browser.pElements[i])
		{
			struct BROWSER_ELEMENT *pBE=NULL;
			pBE  = (struct BROWSER_ELEMENT*) malloc(sizeof(struct BROWSER_ELEMENT));
			browser.pElements[i]=pBE;//malloc(sizeof(struct BROWSER_ELEMENT));
		}
		//there's still no memory for pElements!!
		if (browser.pElements[i]->info_path==NULL)
			log_printf(LOG_ERROR,"mod_browser_getdir(): not enough memory to allocate memory.\n");    
	}
	else if (S_ISDIR(st.st_mode))
		browser.pElements[dircount]->info_type=BROWSER_INFO_DIRECTORY;
	     else browser.pElements[dircount]->info_type=BROWSER_INFO_SCRIPT; //TODO: add stdtype to parameterlist
	strcpy(browser.pElements[dircount]->info_path, dirent->d_name);
	strcpy(browser.pElements[dircount]->name, dirent->d_name);
	if (dircount >= MOD_BROWSER_MAXENTRY) 
	    break;
    }
    closedir(dir);

    strcpy(browser.currentDir, dest);		//Set current path
    browser.CursorPos = 0;			//first entry
    if (!(dircount))
     {		//Dir empty
	strcpy(browser.pElements[0]->info_path, "EMPTY");
	browser.pElements[0]->info_type=BROWSER_INFO_UNDEFINED;
	browser.ElementCount=1;
	log_printf(LOG_DEBUG, "mod_browser_intodirscan(): directory is empty.\n");
    } else {
	//sort dirs and files (but dont mix them)
	//qsort(mod_browser_dirs, dircount, 512, (void*)strcasecmp);
	//qsort(filelist, filecount, 512, (void*)strcasecmp);

	//concatenate dirs+files
//	for (i=0; i<filecount; i++)
//	    strcpy(browser.pElements[dircount+i]->info_path, filelist[i]->info_path);

	browser.ElementCount = dircount;	//# of entries

	//check file type again
	//do we realy need this?
/*	for (i=0; i<browser.ElementCount; i++) {
	    if (!strcasecmp(dest, "/"))
		snprintf(buf, sizeof(buf)-1, "/%s", browser.pElements[i]->info_path);
	    else
		snprintf(buf, sizeof(buf)-1, "%s/%s", dest, browser.pElements[i]->info_path);
	    browser.pElements[i]->info_type=mod_browser_checkdirstype(buf);
	}*/
	//log_printf(LOG_DEBUG, "mod_browser_intodirscan(): %d directories, %d files found.\n", dircount, filecount);

        if (!mod_browser_cdup_initial_updir) {
	    // display 1st entry instead of ".."
	    if (mod_browser_cdup && (browser.ElementCount > 1))
		browser.CursorPos++;
	}

    }
    return 1;						//success
}



/*************************************************************************
 * SCROLL WITHIN CURRENT pElements ARRAY
 * 
 */
void mod_browser_cursor(char *p)
{
    int	 step;
    char *current;
	int newpos=browser.CursorPos;

    if (!p) 
	return;

    if (p[1]==0 && (p[0]=='+' || p[0]=='-')) 
	{	//search next letter
		step = (p[0]=='+') ? 1 : -1;		//search direction
		
		current=browser.pElements[browser.CursorPos]->name;
		while (newpos+step >= 0 &&		//get element with next letter
			newpos+step <= browser.ElementCount-1 &&
			!strncasecmp(browser.pElements[newpos]->name, current, 1))
		{
			newpos+=step;
		}
		
		if (!strncasecmp(browser.pElements[newpos]->name, current, 1))
			newpos=step>0?0:browser.ElementCount-1;
			
		
    }
	else 
	{		//scroll x positions up/down
		
		if (*p!='+' && *p!='-') 		//set absolute position
            newpos = 0;
		
		step=atoi(p);
        if (newpos+step < 0) 
			newpos = browser.ElementCount-1;
        else if (newpos+step > browser.ElementCount-1)
			newpos = 0;
        else 
			newpos += step;
    }

	// now calculate the DisplayPos
	int newdisp=browser.DisplayPos;
	int middleline=((float)(display_hgt+1))/2.0+0.5;	//middle line on even display_hgt, else line after middle
	log_printf(LOG_DEBUG,"mod_browser_cursor(): calculatetd middleline %i",middleline);
	//position of the first LCD-Line
	if (newpos>browser.CursorPos)	//cursor moved down
	{
		if (newpos+1<=middleline)	//cursor's at the beginning of the list
			newdisp=0;
		else	//cursor's in the middle of the list
		{
			if (newpos+1-middleline+display_hgt<browser.ElementCount)	//not at the end
				newdisp=newpos+1-middleline;	
			else							//at the end of the list
				newdisp=browser.ElementCount-display_hgt;	
		}
		if (strlen(browser.HeadLineFixed)!=0)
		{
			if (newpos-newdisp>=middleline-1)
				newdisp++;
		}

	}
	else	// cursor moved up!
	{
		if (newpos+1<=middleline-1)	//cursor at beginning
			newdisp=0;
		else
		{
			if (newpos+1-middleline+1+display_hgt<browser.ElementCount)	//not at the end
				newdisp=newpos+1-middleline+1;	
			else							//at the end of the list
				newdisp=browser.ElementCount-display_hgt;
		}
		if (strlen(browser.HeadLineFixed)!=0&&newpos-newdisp>=middleline-1)
			newdisp++;
	}
	browser.CursorPos=newpos;
	browser.DisplayPos=newdisp;
//	if (strlen(browser.HeadLineFixed)!=0)
	mod_lcdproc_refresh_browser();
	return;
}


/*************************************************************************
 * CHANGE DIRECTORY BACK (cd ..)
 */
int mod_browser_back()
{
    char buf[512];

    //got "cd .." --> call browser_back()
//    if (!strcmp(browser.pElements[browser.CursorPos]->info_path, ".."))
	
	char ctmp[512];
	char newHeadLine[512];
	char oldDir[512];
	strcpy(ctmp,browser.currentDir);
	
	if (strlen(ctmp)==1&&ctmp[0]=='/')	//we're already in the root dear --> no back possible
		return 1;

	char *hch;	
	if ((hch=strrchr(ctmp,'/'))&&strlen(ctmp)>1) 
		strcpy(oldDir, hch+1);
	else
		strcpy(oldDir, ctmp);
		
	//currentDir-format: SUB_SUB_DIR/SUB_DIR/DIRECTORY
	//cut last directory
	hch=strrchr(ctmp,'/');
	if (hch)
		hch[0]='\0';
	else
		strcpy(ctmp,"/");
	
	if (mod_browser_get_mpddir(ctmp))
	{
		//create new headline -- remove leading dirs
		
		if ((hch=strrchr(ctmp,'/'))&&strlen(ctmp)>1) 
			memmove(browser.HeadLineFixed, hch+1,strlen(hch));
		else
			strcpy(browser.HeadLineFixed,ctmp);
		
		//position the cursor on the item with the dirname we've backed from
		int newCursorPos=mod_browser_find_element(oldDir, BROWSER_FIND_INFOPATH);
		if (newCursorPos<0)
			browser.CursorPos=0;
		else
			browser.CursorPos=newCursorPos;
			
		//it's nice to show the item not in the first line but in the second - if there's enough space	
		if (browser.CursorPos>0 && 
			(  (display_hgt>3 && strcasecmp(browser.HeadLineFixed,"")==0)	//Headline shown --> 3 lines for menu
			 ||(display_hgt>2 && strcasecmp(browser.HeadLineFixed,"")!=0) )) //Headline not shown --> 3 lines for menu
			browser.DisplayPos=newCursorPos-1;
		else
			browser.DisplayPos=0;
			
		mod_browser_show();
		mod_lcdproc_refresh_browser();
		return 1;
	}
	else
		return 0; //can't cd..
	mod_lcdproc_refresh_browser();
	//mod_lcdproc_refresh();
 
    return 0;	//cant cd ..
}



/*************************************************************************
 * CHANGE INTO DIRECTORY
 */
int mod_browser_into()
{
    char buf[512];

    //got "cd .." --> call browser_back()
//    if (!strcmp(browser.pElements[browser.CursorPos]->info_path, ".."))
//	return mod_browser_back();

	
	char ctmp[512];
	char newHeadLine[512];
	if (browser.pElements[browser.CursorPos]->status!=ELEMENT_SUBMENU)
		return 0;
	strcpy(newHeadLine,browser.pElements[browser.CursorPos]->name);
	
	strcpy(ctmp,browser.currentDir);
	if (ctmp[0]!='/'&&strlen(ctmp)!=1)	//add '/' if it's not the root ('/')
		strcat(ctmp,"/");
	//strcat(ctmp, browser.pElements[browser.CursorPos]->info_path);
	strcpy(ctmp, browser.pElements[browser.CursorPos]->info_path);
	
	//open script-directory
	if (strcasecmp(ctmp, mod_browser_exec)==0)
	{
		mod_browser_intodirscan(ctmp);
		if (newHeadLine[strlen(newHeadLine)-1]=='/'&&strlen(newHeadLine)>1)
			newHeadLine[strlen(newHeadLine)-1]='\0';
		strcpy(browser.HeadLineFixed,newHeadLine);
		browser.DisplayPos=0;
		browser.CursorPos=0;		
		mod_browser_show();
		mod_lcdproc_refresh_browser();
		
	}
	else if (mod_browser_get_mpddir(ctmp))
	{
		if (newHeadLine[strlen(newHeadLine)-1]=='/'&&strlen(newHeadLine)>1)
			newHeadLine[strlen(newHeadLine)-1]='\0';
		strcpy(browser.HeadLineFixed,newHeadLine);
		browser.DisplayPos=0;
		browser.CursorPos=0;		
		mod_browser_show();
		mod_lcdproc_refresh_browser();
	}
	mod_lcdproc_refresh_browser();
	//mod_lcdproc_refresh();
    

    return 0;	
}



void mod_browser_sigchld(pid_t childpid, int status)
{
    if (mod_browser_chld_pid == childpid) {
	mod_browser_chld_status = status;
	mod_browser_chld_exited = 1;
    }
}


int mod_browser_script(char *command)
{
    int  pid;
    char *argv[6];
    if (!command)
	return 1;
   pid = fork();
    if (pid == -1)
	return -1;
    if (pid == 0) {

	char type[16];
	//argv[0] = "sh";
	char opt[]="-v";
	argv[0] = opt;	//options for sh
	argv[1] = command;
	argv[2] = mod_browser_execmsg;
	argv[3] = browser.currentDir;	//first argument passed to script ($1)
	argv[4] = browser.ElementCount!=0  ?  browser.pElements[browser.CursorPos]->info_path  :  0;
//	argv[4] = type;
	argv[5] = 0;
	log_printf(LOG_DEBUG, "mod_browser_script(): executing command %s in child  process.\n",command);
	execv("/bin/sh",argv);
        exit(127);
    }
    mod_browser_chld_pid = pid;
    mod_browser_chld_exited = 0;
    mod_browser.chld = mod_browser_sigchld;
    do {
	usleep(50000);
    } while (!mod_browser_chld_exited);
    mod_browser.chld = NULL;
    return mod_browser_chld_status;
    return 0;
}


/*************************************************************************
 * EXECUTE SHELL SCRIPT
 */
void mod_browser_execute(char *command) {

    int  fd;
    char buf[512];

    if (!command)
	return;

    //clear message file
    if (mod_browser_execmsg)
	if (!truncate(mod_browser_execmsg, 0))
	    log_printf(LOG_DEBUG, "mod_browser_exec(): Message file '%s' cleared.\n", mod_browser_execmsg);
	else	
	    log_printf(LOG_DEBUG, "mod_browser_exec(): Error clearing message file '%s'.\n", mod_browser_execmsg);
    else
	log_printf(LOG_DEBUG, "mod_browser_exec(): Message file disabled.\n");


    log_printf(LOG_DEBUG, "mod_browser_exec(): Executing '%s'...\n", command);

    if (!mod_browser_script(command)) {

        log_printf(LOG_DEBUG, "mod_browser_exec(): Executing done.\n");

	if (mod_browser_execmsg) {	//try to open msg file and display it
	
	    log_printf(LOG_DEBUG, "mod_browser_exec(): Reading message file '%s'.\n", mod_browser_execmsg);

	    fd=open(mod_browser_execmsg, O_RDONLY);
	    if (fd >= 0) {
		if (readline(fd, buf, sizeof(buf)) >= 0) {
		    log_printf(LOG_DEBUG, "mod_browser_exec(): Script reports '%s'.\n", buf);
		    mod_sendmsgf(MSGTYPE_INFO, "browser info %s", buf);
		} else {
		    log_printf(LOG_DEBUG, "mod_browser_exec(): Script reports nothing.\n");
		}
		//read multiple ABase commands from message file
		while (readline(fd, buf, sizeof(buf)) >= 0) {
		    log_printf(LOG_DEBUG, "mod_browser_exec(): Script issues command '%s'.\n", buf);
		    mod_sendmsgf(MSGTYPE_INPUT, "%s", buf);
		}
		close(fd);
	    } else {
		log_printf(LOG_DEBUG, "mod_browser_exec(): Message file '%s' not found.\n", mod_browser_execmsg);
		mod_sendmsgf(MSGTYPE_INFO, "browser info Done");
	    }
	} else {
	    log_printf(LOG_DEBUG, "mod_browser_exec(): Message file disabled.\n");
	    mod_sendmsgf(MSGTYPE_INFO, "browser info Done");
	}
    } else {
	log_printf(LOG_DEBUG, "mod_browser_exec(): Script execution failed.\n");
	mod_sendmsgf(MSGTYPE_INFO, "browser info Error");
    }
    return;
}



/*************************************************************************
 * ADD CURRENT PLAYLIST/DIR/... POINTED TO BY CURSOR TO PLAYLIST
 */
void mod_browser_add()
{
    char buf[512];
    log_printf(LOG_DEBUG, "mod_browser_add(): entered.\n");
    // disable browser_add on ".."
    if (!strcmp(browser.pElements[browser.CursorPos]->name, "..")) {
	log_printf(LOG_DEBUG, "mod_browser_add(): Sorry, cannot add '..' to playlist.\n");
	return;
    }
//    strcpy(buf, browser.currentDir);
//    strcat(buf, "/");
//    strcat(buf, browser.pElements[browser.CursorPos]->info_path);
	mpd_finishCommand(conn);
	mod_mpd_print_error();
	strcpy(buf, browser.pElements[browser.CursorPos]->info_path);
    switch (browser.pElements[browser.CursorPos]->info_type) {
	case BROWSER_INFO_DIRECTORY:
	case BROWSER_INFO_SONGFILE:	log_printf(LOG_NORMAL, "browser add songfile/directory: %s\n",buf);
					mpd_sendAddCommand(conn, buf);
					mod_lcdproc_browser_remove();
					mod_sendmsg(MSGTYPE_INFO, "browser info added files/dir.");
					mod_mpd_print_error();	
					mpd_finishCommand(conn);
					break;
	case BROWSER_INFO_PLAYLIST: 	log_printf(LOG_NORMAL, "browser load playlist: %s\n",buf);
					mpd_sendLoadCommand(conn, buf);
					mod_lcdproc_browser_remove();
					mod_sendmsg(MSGTYPE_INFO, "browser info added playlist.");					
					mod_mpd_print_error();	
					mpd_finishCommand(conn);					
					break;
	default: 			mod_sendmsg(MSGTYPE_INFO, "browser info Error");
					log_printf(LOG_DEBUG, "mod_browser_add(): Ignoring entry '%s'.\n", buf);
					break;
    }
   

    
    /*switch (mod_browser_dirstype[browser.CursorPos]) {
	case MOD_BROWSER_DIRSTYPE_DIR: 	strcat(buf, "/");
					strcat(buf, mod_browser_mask_audio);
					mod_sendmsgf(MSGTYPE_INPUT, "playlist loaddirplus %s", buf);
					log_printf(LOG_DEBUG, "mod_browser_add(): Adding directory '%s'.\n", buf);
					break;
	case MOD_BROWSER_DIRSTYPE_MP3: 	mod_sendmsgf(MSGTYPE_INPUT, "playlist addplus %s", buf);
					log_printf(LOG_DEBUG, "mod_browser_add(): Adding mp3 file '%s'.\n", buf);
					break;
	case MOD_BROWSER_DIRSTYPE_M3U: 	mod_sendmsgf(MSGTYPE_INPUT, "playlist loadplus %s", buf);
					log_printf(LOG_DEBUG, "mod_browser_add(): Adding playlist '%s'.\n", buf);
					break;
	case MOD_BROWSER_DIRSTYPE_M3L: 	//Maybe future playlistlist support
					//mod_sendmsgf(MSGTYPE_INPUT, "playlistlist loadplus %s", buf);
					//log_printf(LOG_DEBUG, "mod_browser_add(): Adding playlist-list '%s'\n", buf);
					//break;
					break;
	case MOD_BROWSER_DIRSTYPE_EXE:	mod_browser_execute(buf);
					break;
	default: 			mod_sendmsg(MSGTYPE_INFO, "browser info Error");
					log_printf(LOG_DEBUG, "mod_browser_add(): Ignoring entry '%s'.\n", buf);
					break;
    }*/
}


/*************************************************************************
 * PLAY CURRENT CURSOR
 */
void mod_browser_play()
{
 /*   if (mod_browser_dirstype[browser.CursorPos] != MOD_BROWSER_DIRSTYPE_EXE) {
	log_printf(LOG_DEBUG, "mod_browser_play(): stopping and clearing playlist.\n");
        mod_sendmsg(MSGTYPE_INPUT, "stop");			//stop playing
        mod_sendmsg(MSGTYPE_PLAYER, "stop");			//notify mod_playlist that we are stopped
        mod_sendmsg(MSGTYPE_INPUT, "playlist clearnostop");	//clear playlist w/o issuing STOP command
        mod_sendmsg(MSGTYPE_PLAYER, "browser clrscr");		//remove directory listing from screen
    }
    log_printf(LOG_DEBUG, "mod_browser_play(): Adding current cursor.\n");
    mod_browser_add();					//add and start playing automatically*/
}


/*************************************************************************
 * SET CURRENT DIRECTORY
 */
int mod_browser_setpwd(char *dest)
{
    char buf[512];
    
    if (!dest) {
        log_printf(LOG_ERROR, "mod_browser_setpwd(): unable to set NULL directory.\n");
	return 0;
    }

    // relative path?
    if (dest[0]!='/') {
	if (strlen(mod_browser_root)==1)
	    snprintf(buf, sizeof(buf)-1, "/%s", dest);
	else
	    snprintf(buf, sizeof(buf)-1, "%s/%s", mod_browser_root, dest);
        log_printf(LOG_DEBUG, "mod_browser_setpwd(): relative -> absolute path '%s'.\n", buf);
    } else
	strncpy(buf, dest, 512);	//already absolute path

    //subdir of browser_root?
    if (strncasecmp(mod_browser_root, buf, strlen(mod_browser_root))) {
        log_printf(LOG_ERROR, "mod_browser_setpwd(): '%s' is not a subdir of browser_root '%s'.\n", buf, mod_browser_root);
	return 0;
    }	

    //dir?
    if (mod_browser_checkdirstype(buf) != MOD_BROWSER_DIRSTYPE_DIR) {
	log_printf(LOG_ERROR, "mod_browser_setpwd(): not a directory '%s'.\n", buf);
	return 0;
    }

    //directory scan ok?
    if (!mod_browser_intodirscan(buf)) {
	log_printf(LOG_ERROR, "mod_browser_setpwd(): unable to scan directory '%s'.\n", buf);
	return 0;
    }
    
    log_printf(LOG_DEBUG, "mod_browser_setpwd(): set to '%s'.\n", buf);
    return 1;
}











/*************************************************************************
* CREATE A NEW BROWSER ON THE DISPLAY
**************************************************************************/
void mod_browser_show()
{

}

void mod_browser_cursor_up()
{
}

void mod_browser_cursor_down()
{
}

void mod_browser_select()
{
}


int mod_browser_find_element(char *ElementName, int ElementType)
{
	int i;
	if (strcasecmp(ElementName,"")==0)
		return -1;
	for (i=0; i<browser.ElementCount; i++)
	{
		if (ElementType==BROWSER_FIND_NAME&&strcasecmp(browser.pElements[i]->name, ElementName)==0)	//identical
		{	return i;}
		else if (ElementType==BROWSER_FIND_INFOPATH&&strcasecmp(browser.pElements[i]->info_path, ElementName)==0)	//identical
		{	return i;}
		
	}
	return -1;
}

void mod_browser_open()
{
	// rebuilt last directory pos
	char oldDir[512];
	char ctmp[512];
	strcpy(ctmp,browser.currentDir);
	char *hch;
	
	//adapt oldDir to rigth format for find_element() (only name of dir instead of global path)
	if ((hch=strrchr(ctmp,'/'))&&strlen(ctmp)>1) 
		strcpy(oldDir, hch+1);
	else
		strcpy(oldDir, ctmp);
	mod_browser_get_mpddir(browser.currentDir);
	//int newCursorPos=mod_browser_find_element(oldDir, BROWSER_FIND_INFOPATH);
	//if (newCursorPos<0)
	//	browser.CursorPos=0;
	//else
	//	browser.CursorPos=newCursorPos;
	mod_browser_show();
	browser.status=BROWSER_STATUS_ACTIVE;
	
	//naja, die Zeichen sind noch wirklich nicht die schönsten: DELUX: Symbole!!
	mod_lcdproc_browser_resettimer();
	mod_lcdproc_refresh_browser();
	mod_lcdproc_refresh();
}








/*************************************************************************
 * REFRESH DISPLAY
 */
void mod_browser_refresh()
{
    //mod_browser_printpwd();
    //mod_browser_printcursor();
    return;
}


//path: relative to mpd-root!
//saves new browser-elements to struct browser
//FORMAT: path=[/]SUB_SUB_DIR/SUB_DIR/DIR[/]  !!case-sensitive!!
int mod_browser_get_mpddir(char *path)
{
	mpd_InfoEntity * entity;
	char * ls = path;
	int i = 0;
	char * sp;
	char * dp;
	
//	mpd_sendListallCommand(conn,"");
//	mod_mpd_print_error();
	
	sp = ls;
//	while((sp = strchr(sp,' '))) *sp = '_';
	
	//do we really need this? is it for case sensitivity?
	//search first correct Name for directory!
	//the WHOLE database is scanned --> that's too much!!
	/*while((entity = mpd_getNextInfoEntity(conn))) {
		if(entity->type==MPD_INFO_ENTITY_TYPE_DIRECTORY) {
			mpd_Directory * dir = entity->info.directory;
			sp = dp = strdup(dir->path);
//			while((sp = strchr(sp,' '))) *sp = '_';
			if(strcmp(dp,ls)==0) {
				free(dp);
				ls = dir->path;
				break;
			}
			free(dp);
		}
		mpd_freeInfoEntity(entity);
	}
	mod_mpd_print_error();
	mpd_finishCommand(conn);*/
	log_printf(LOG_DEBUG,"mod_browser_getdir(): entered.\n");
	//correct name now found --> saved in ls
	//  if correct name not found (isn't a dir) ls == path --> we can leave the array unchanged!
	//log_printf(LOG_DEBUG,"mod_browser_getdir(): Correct path is %s\n",ls);
	browser.ElementCount=0;
	// desired dir-format: SUB_SUB_DIR/SUB_DIR/DIR
	if (ls[0]=='/'&&strlen(ls)>1)	//remove leading '/'
		ls+=sizeof(ls[0]);
	
	strcpy(browser.currentDir,ls);
	
	///CHECK THIS LINE!!
	char *hch=strrchr(browser.currentDir, '/');
	if (strlen(browser.currentDir)>1&&hch&&hch[1]=='\0' ) //remove ending '/'
		hch[0]='\0';
	ls=browser.currentDir;
	
	// desired dir-format: SUB_SUB_DIR/SUB_DIR/DIR
	mpd_finishCommand(conn);
	mpd_sendLsInfoCommand(conn,ls);
	mod_mpd_print_error();
	mod_mpd_print_error();
	i=-1;
	while((entity = mpd_getNextInfoEntity(conn)))
	{
		//log_printf(LOG_DEBUG,"mod_browser_getdir(): round %d\n",i);
		
		//Info is a DIRECTORY
		if(entity->type==MPD_INFO_ENTITY_TYPE_DIRECTORY) 
		{
			i++;
			mpd_Directory * dir = entity->info.directory;
			//printf("%s\n",fromUtf8(dir->path));
			//construct line for browser
			
			// e. g. dir->path="Bob Dylan/Live 1966"
			// remove leading dirnames and '/' --> ctmp="Live 1966"
			char *ctmp;
			if (!(ctmp=strrchr(dir->path,'/')))
				//no '/' found
				ctmp=dir->path;
			else if (strlen(ctmp)>1)
				ctmp=ctmp+sizeof(ctmp[0]);
		
			//elements haven't any memory yet!
			if (!browser.pElements[i])
			{
				struct BROWSER_ELEMENT *pBE=NULL;
				pBE  = (struct BROWSER_ELEMENT*) malloc(sizeof(struct BROWSER_ELEMENT));
				browser.pElements[i]=pBE;//malloc(sizeof(struct BROWSER_ELEMENT));
			}
			 //there's still no memory for pElements!!
			if (browser.pElements[i]->info_path==NULL)
				log_printf(LOG_ERROR,"mod_browser_getdir(): not enough memory to allocate memory.\n");
			
			//save path in array
			//copy dirname to browser-dir-array (relative to parent dir)
			strcpy(browser.pElements[i]->info_path,fromUtf8(dir->path));
			//copy browser-styled dirname to menu-elements
			strcpy(browser.pElements[i]->name,fromUtf8(ctmp)); 
			strcat(browser.pElements[i]->name,"/");
			browser.pElements[i]->status=ELEMENT_SUBMENU;
			browser.pElements[i]->info_type=BROWSER_INFO_DIRECTORY;
			log_printf(LOG_DEBUG,"mod_browser_getdir(): added path: %s.\n", browser.pElements[i]->name);
		}
		//Info is a SONG
		else if(entity->type==MPD_INFO_ENTITY_TYPE_SONG) 
		{
			i++;
			mpd_Song * songinfo = entity->info.song;
			struct MPD_PLAYER_SONG song;
			
			strcpy(song.file, songinfo->file?songinfo->file:"");
			strcpy(song.album, songinfo->album?songinfo->album:"");
			strcpy(song.artist, songinfo->artist?songinfo->artist:"");
			strcpy(song.name, songinfo->name?songinfo->name:"");
			strcpy(song.title, songinfo->title?songinfo->title:"");
			strcpy(song.track, songinfo->track?songinfo->track:"");
			song.time = songinfo->time;
			song.pos = songinfo->pos;
			song.id = songinfo->id;
			//printf("%s\n",fromUtf8(song->file));
			//construct line for browser
			
			
			// TODO: read ctmp out of config-file, 
			//if possible there are several strings thus more views of files are possible
			char ctmp[512]="[%track%. ][%artist% - ][%title%]|%shortfile%";
			//char ctmp[512]="%shortfilename%";
			
			if (!browser.pElements[i]) //get some mem for pElements
			{
				struct BROWSER_ELEMENT *pBE=NULL;
				pBE  = (struct BROWSER_ELEMENT*) malloc(sizeof(struct BROWSER_ELEMENT));
				browser.pElements[i]=pBE;//malloc(sizeof(struct BROWSER_ELEMENT));
				log_printf(LOG_DEBUG,"mod_browser_getdir(): allocated some memory.\n");
			}
			if (browser.pElements[i]==NULL) //there's still no mem for pElements!!
				log_printf(LOG_ERROR,"mod_browser_getdir(): not enough memory to allocate memory.\n");
			//save path in array
			strcpy(browser.pElements[i]->info_path,fromUtf8(song.file));
			
			//substitute_references_song(ctmp,&song);
			char *tmp1=songToFormatedString(&song,ctmp,NULL);
			if (!tmp1)
				strcpy(ctmp,"n/a");
			else
				strcpy(ctmp, tmp1);
			free(tmp1);

			//Oh my God there's NO TAG
			/*if (strlen(song.album)==0 && strlen(song.artist)==0 && strlen(song.title)==0 && strlen(song.track)==0)
			{
				strcpy(ctmp,song.file);
				char *hch;
				if ((hch=strrchr(ctmp, '/')))
					memmove(ctmp,hch+1,strlen(hch));
			}*/
			strcpy(browser.pElements[i]->name,ctmp);
			browser.pElements[i]->status=ELEMENT_STANDARD;
			browser.pElements[i]->info_type=BROWSER_INFO_SONGFILE;
			log_printf(LOG_DEBUG,"mod_browser_getdir(): Got file %s\n",ctmp);	
		}
		mpd_freeInfoEntity(entity);
		if (i>=MOD_BROWSER_MAXENTRY)
		{
			log_printf(LOG_ERROR,"browser: too many files in dir '%s'! Maximum is %d\n",path, MOD_BROWSER_MAXENTRY);
			break;
		}
		
	}
	mod_mpd_print_error();	
	mpd_finishCommand(conn);
	
	if (i<0)	//found no dirs in path
	{
		///ONLY FOR DEBUG!!
		strcpy(browser.pElements[0]->name,"<NONE>");
		browser.pElements[0]->info_type=BROWSER_INFO_UNDEFINED;
		log_printf(LOG_DEBUG,"mod_browser_get_mpddir(): no dir/files found --- current dir is now %s\n", browser.currentDir);
		return 0;
	}
	else
	{
		browser.ElementCount=i+1;
		browser.pElements[i+1]=NULL;
		strcpy(browser.currentDir,ls);
		log_printf(LOG_DEBUG,"mod_browser_get_mpddir(): current dir is now %s.\n", browser.currentDir);
		
		//sort pointer (!) to elements (not case-sensitive, first dirs,
		// then songs, songs: first lower numbers then higher (1...100...)
		qsort(&browser.pElements, browser.ElementCount, sizeof(struct BROWSER_ELEMENT*), (void*)mod_browser_cmp_bwrelemnts);
		
		return 1;
	}
}

//
//compare pointers (!) to BROWSER_ELEMENT
//(not case-sensitive, first dirs, then songs, songs: first lower numbers then higher (1...100...)
int mod_browser_cmp_bwrelemnts(const void **element1, const void **element2)
{
	struct BROWSER_ELEMENT *el1 = (struct BROWSER_ELEMENT *) *element1;
	struct BROWSER_ELEMENT *el2 = (struct BROWSER_ELEMENT *) *element2;
	
	int i, j;
	if (el1->info_type==el2->info_type)	//elements are of same type
	{
		char buf1[128];
		char buf2[128];
		strcpy(buf1,el1->name);
		strcpy(buf2,el2->name);
		i=(int)strtol(el1->name,NULL,10);
		j=(int)strtol(el2->name,NULL,10);
		if (i!=0&&j!=0)
			return i-j;
		toupper(el1->name);
		toupper(el2->name);						
		return strcasecmp(buf1,buf2);
	}
	else					//elements are of diffrent types 
		return (el1->info_type-el2->info_type);	// >0 if el1 is dir and el2 is file
}


/*************************************************************************
 * RECEIVE MESSAGE
 */
void mod_browser_message(int msgtype, char *msg)
{
    char *c1, *c2, *c3;

    if (msgtype == MSGTYPE_INPUT)
	{
		c1 = strtok(msg, " \t");
		if(!c1) return;

		// BROWSER MESSAGES
		
		// message should not be processed if the condition c_* doesn't comply with the browser_state!
		if (!strcasecmp(c1, "c_nbrowser"))
			if (browser.status==BROWSER_STATUS_INACTIVE)
				c1 = strtok(NULL, " \t");
			else return;
		else if (!strcasecmp(c1, "c_browser"))
			if (browser.status==BROWSER_STATUS_ACTIVE)
				c1 = strtok(NULL, " \t");
			else return;

		c2 = strtok(NULL, " \t");
		if(!c2) return;			
					
		if ( !strcasecmp(c1, "browser")) {
			//ACTIVATE BROWSER
			if (!strcasecmp(c2, "activate"))
			{
				//browser.status==BROWSER_STATUS_ACTIVE;
				//mod_browser_refresh();
				if (browser.status==BROWSER_STATUS_ACTIVE)
					mod_lcdproc_browser_remove();
				else
					mod_browser_open();
				mod_lcdproc_browser_resettimer();
			}
			else if (!strcasecmp(c2, "deactivate"))
				mod_lcdproc_browser_remove();
			else if (!strcasecmp(c2, "playlistopen"))
			{
				//TODO: toggle
				//mod_browser_openplaylist();
				//mod_lcdproc_browser_resettimer();
			}
			else if ( !strcasecmp(c2, "cursor") && browser.status==BROWSER_STATUS_ACTIVE)//CMOVEMENT (+,-,+1,-1)
			 {
				c3 = strtok(NULL, "");
				if(!c3)return;
				mod_browser_cursor(c3);
				mod_lcdproc_browser_resettimer();
			//	mod_browser_refresh();
				mod_lcdproc_refresh_browser();
			} else if ( !strcasecmp(c2, "back") && browser.status==BROWSER_STATUS_ACTIVE){// PROCESS BACK
				mod_browser_back();
				mod_lcdproc_browser_resettimer();
				mod_lcdproc_refresh_browser();
			//	mod_browser_refresh();
			} else if ( !strcasecmp(c2, "into") && browser.status==BROWSER_STATUS_ACTIVE) {// PROCESS INTO
				mod_browser_into();
				mod_lcdproc_browser_resettimer();
				mod_lcdproc_refresh_browser();
			//	mod_browser_refresh();
			} else if ( !strcasecmp(c2, "add") && browser.status==BROWSER_STATUS_ACTIVE) {
				mod_browser_add();
			//	mod_browser_refresh();
			//mod_lcdproc_browser_resettimer();
			//mod_lcdproc_refresh_browser();
			} else if ( !strcasecmp(c2, "setpwd")) {
				c3 = strtok(NULL, "");
				if(!c3)return;
			//	mod_browser_setpwd(c3);
			//	mod_browser_refresh();
			mod_lcdproc_browser_resettimer();
			mod_lcdproc_refresh_browser();
			} else if ( !strcasecmp(c2, "clear")) {
				
			//	mod_browser_refresh();
			} else if ( !strcasecmp(c2, "show")) {
				mod_browser_refresh();
				mod_lcdproc_browser_resettimer();
				mod_lcdproc_refresh_browser();
			} else if ( !strcasecmp(c2, "play") && browser.status==BROWSER_STATUS_ACTIVE) {
				mod_mpd_pl_clear();
				mod_browser_add();
				mod_mpd_play();
			} else if ( !strcasecmp(c2, "exec") ) {
				c3 = strtok(NULL, "");
				if(!c3)return;
				mod_browser_execute(c3);
				//mod_lcdproc_browser_resettimer();
				//mod_lcdproc_refresh_browser();
			} else if (!strcasecmp(c2, "djump")){
				c3 = strtok(NULL, "");
				if (!c3) return;
				else if (!browser.status==BROWSER_STATUS_ACTIVE)
				{
					char hc[128];
					sprintf(hc,"djump %s",c3);
					mod_sendmsg(MSGTYPE_INPUT, hc);
				}
			} else {
				c3 = strtok(NULL, "");
				log_printf(LOG_DEBUG, "mod_browser_message(): ignoring '%s %s %s'.\n", c1, c2, c3);
			}
			
		}
    }

	else if (msgtype==MSGTYPE_QUERY)	//by Sebastian Bauer 04.10.2004
	{
		c1 =  strtok(msg, " \t");
		if(!c1)
			return;
		c2 = strtok(NULL, " \t");
		if(!c2)
			return;
		if ( !strcasecmp(c1,"browser"))
		{
			if ( !strcasecmp(c2, "list"))
				mod_sendmsgf(MSGTYPE_INFO,"%s", mod_browser_list);
		}
	}


	return;
}


/*************************************************************************
 * MODULE RELOAD FUNCTION
 */
char *mod_browser_reload(void)
{

    log_printf(LOG_DEBUG, "mod_browser_reload(): reloading\n");
	
	//need to activate the browser before activating it?
    if (!strcasecmp(config_getstr("browser_activate_first", "yes"), "yes"))
		mod_browser_activate_first = 1;
    else
		mod_browser_activate_first = 0; 
    log_printf(LOG_DEBUG, "mod_browser_reload(): activate browser first: %s\n", mod_browser_activate_first? "no" : "yes");

    //set audio file mask
    strcpy(mod_browser_mask_audio, config_getstr("browser_mask_audio", "/*/*.[Mm][Pp]3"));
    log_printf(LOG_DEBUG, "mod_browser_reload(): mask_audio is '%s'\n", mod_browser_mask_audio);

    //set playlist file mask
    strcpy(mod_browser_mask_list, config_getstr("browser_mask_list", "*.[Mm]3[Uu]"));
    log_printf(LOG_DEBUG, "mod_browser_reload(): mask_list is '%s'\n", mod_browser_mask_list);

    //set playlist-list file mask
    strcpy(mod_browser_mask_listlist, config_getstr("browser_mask_listlist", "*.[Mm]3[Ll]"));
    log_printf(LOG_DEBUG, "mod_browser_reload(): mask_listlist is '%s'\n", mod_browser_mask_listlist);

    //set executable file mask
    strcpy(mod_browser_mask_exec, config_getstr("browser_mask_exec", "*.[Ss][Hh]"));
    log_printf(LOG_DEBUG, "mod_browser_reload(): mask_exec is '%s'\n", mod_browser_mask_exec);

    //set execlude file mask
    strcpy(mod_browser_mask_hide, config_getstr("browser_mask_hide", "*"));
    log_printf(LOG_DEBUG, "mod_browser_reload(): mask_hide is '%s'\n", mod_browser_mask_hide);

    //case sensitive matching
    //according to fnmatch(2) mod_browser_mask_sensitiveflag=0 means sensitive
    //and mod_browser_mask_sensitiveflag=FNM_CASEFOLD means NOT case sensitive
    if (!strcasecmp(config_getstr("browser_mask_casesensitive", "yes"), "yes"))
	mod_browser_mask_sensitiveflag = 0;
    else
	mod_browser_mask_sensitiveflag = 0; //should be FNM_CASEFOLD (defined in fnmatch.h) but only when GNU source....????;
    log_printf(LOG_DEBUG, "mod_browser_reload(): mask case sensitive: %s\n", mod_browser_mask_sensitiveflag ? "no" : "yes");

    // set pwd path depth to show on LCD
    mod_browser_pwddepth=config_getnum("browser_pwddepth", 2);
    if (mod_browser_pwddepth<1) mod_browser_pwddepth=1;
    log_printf(LOG_DEBUG, "mod_browser_reload(): Setting pwd path depth to '%d'\n", mod_browser_pwddepth);

    // show ".." directory?
    mod_browser_cdup=(strcasecmp("yes", config_getstr("browser_cdup", "no")) == 0);
    log_printf(LOG_DEBUG, "mod_browser_reload(): show '..' directory='%s'\n", mod_browser_cdup ? "yes" : "no");

    // stay on ".." entry after entering directory?
    mod_browser_cdup_initial_updir = (strcasecmp("yes", config_getstr("browser_cdup_initial_updir", "no")) == 0);
    log_printf(LOG_DEBUG, "mod_browser_reload(): stay on '..' after cd='%s'\n", mod_browser_cdup_initial_updir ? "yes" : "no");
    //set chroot directory
    strcpy(mod_browser_root, config_getstr("browser_root", "/"));
    if (!mod_browser_root || mod_browser_root[0]!='/')	//allow ablosute path only
	strcpy(mod_browser_root, "/");	
/*    if (!mod_browser_intodirscan(mod_browser_root))	//only exsting directories
	strcpy(mod_browser_root, "/");
    log_printf(LOG_DEBUG, "mod_browser_reload(): root dir is '%s'\n", mod_browser_root);
*/
    //set exec directory
    if (strcasecmp(config_getstr("browser_exec", "-"), "-")) {	//enabled
	strcpy(mod_browser_exec, mod_browser_root);
	if (mod_browser_exec[strlen(mod_browser_exec)-1] != '/')
	    strcat(mod_browser_exec, "/");
	strcat(mod_browser_exec, config_getstr("browser_exec", ""));
	if ( (mod_browser_exec[strlen(mod_browser_exec)-1] == '/') && (strlen(mod_browser_exec)>1) )
	    mod_browser_exec[strlen(mod_browser_exec)-1]=0;
	log_printf(LOG_DEBUG, "mod_browser_reload(): exec dir is '%s'\n", mod_browser_exec);
    } else {
	mod_browser_exec[0]=0;
	log_printf(LOG_DEBUG, "mod_browser_reload(): exec dir disabled\n");
    }

    //set exec output file
    strcpy(mod_browser_execmsg, config_getstr("browser_execmsg", "/tmp/ABase.tmp.execmsg"));
    if (!mod_browser_execmsg)
	strcpy(mod_browser_execmsg, "/ABase.tmp.execmsg");	//DONT allow empty execmsg because of command line paramter count
    log_printf(LOG_DEBUG, "mod_browser_reload(): exec messages file is '%s'\n", mod_browser_execmsg);

	//set m3u directory by Sebi Bauer 04.10.2004
	//format: /browser_root/browser_list
    strcpy(mod_browser_list, mod_browser_root);
    if (mod_browser_list[strlen(mod_browser_list)-1] != '/')
		strcat(mod_browser_list, "/");
    strcat(mod_browser_list, config_getstr("browser_playlist", "/"));
    if ((mod_browser_path[strlen(mod_browser_path)-1] == '/') && (strlen(mod_browser_path)>1) )
		mod_browser_list[strlen(mod_browser_list)-1]=0;
/*    if (!mod_browser_intodirscan(mod_browser_path))
	{
		log_printf(LOG_ERROR, "mod_browser_reload(): unable to read playlist dir '%s'. Using '%s'.\n", mod_browser_list, mod_browser_root);
		if (!mod_browser_intodirscan(mod_browser_root))
		    return strcpy("error scanning fallback home directory ", mod_browser_root);
    }*/

    //set home directory
    strcpy(mod_browser_path, mod_browser_root);
    if (mod_browser_path[strlen(mod_browser_path)-1] != '/')
	strcat(mod_browser_path, "/");
    strcat(mod_browser_path, config_getstr("browser_home", "/"));
    if ( (mod_browser_path[strlen(mod_browser_path)-1] == '/') && (strlen(mod_browser_path)>1) )
	mod_browser_path[strlen(mod_browser_path)-1]=0;
//    if (!mod_browser_intodirscan(mod_browser_path)) {
//	log_printf(LOG_ERROR, "mod_browser_reload(): unable to read home dir '%s'. Using '%s'.\n", mod_browser_path, mod_browser_root);
//	if (!mod_browser_intodirscan(mod_browser_root))
//	    return strcpy("error scanning fallback home directory ", mod_browser_root);
//    }
//    log_printf(LOG_DEBUG, "mod_browser_reload(): home dir is '%s'\n", mod_browser_path);

	//browser always activated if mod_browser_active_first no
	//browser deactivated if mod_browser_active_first yes
	browser.status=mod_browser_activate_first?BROWSER_STATUS_INACTIVE:BROWSER_STATUS_ACTIVE;	
	
    return NULL;
}



/*************************************************************************
 * MODULE INIT FUNCTION
 */
char *mod_browser_init(void)
{
	char *p;
	
	log_printf(LOG_DEBUG, "mod_browser_init(): initializing\n");

	if ((p=mod_browser_reload())) return p;
	
	// register our module
	mod_register(&mod_browser);
	djump_status.cursor=0;
	djump_status.value=0;
	int i=0;
	for (i=0; i<MOD_BROWSER_MAXENTRY; i++)
		browser.pElements[i]=NULL;
	browser.status=BROWSER_STATUS_INACTIVE;
	browser.ElementCount=0;
	
	strcpy(browser.HeadLineFixed,mod_browser_root);
	browser.DisplayPos=0;
	browser.CursorPos=0;
	strcpy(browser.cursorEleStatus,">O+-X>>>>>>");
	strcpy(browser.viewEleStatus,  " o*=x      ");	
	strcpy(browser.currentDir,"/");
	///TODO: initialize display_* from conf
	display_hgt=4;
	display_wid=20;
	setLocaleCharset(); //for charset conversion	
	return NULL;
}


/*************************************************************************
 * MODULE DEINIT FUNCTION
 */
void mod_browser_deinit(void)
{
	log_printf(LOG_DEBUG, "mod_browser_deinit(): deinitialized\n");
}


/*************************************************************************
 * EOF
 */
