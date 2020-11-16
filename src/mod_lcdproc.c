/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_lcdproc.c,v $ -- LCDproc display interface
 * $Id: mod_lcdproc.c,v 1.16 2003/11/26 20:56:29 boucman Exp $
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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <locale.h>

#include "config.h"
#include "ABaseconfig.h"
#include "ABaselog.h"
#include "ABasetools.h"
#include "ABasemod.h"
#include "globalDef.h"
#include "mod_lcdproc.h"
#include "libmpdclient.h"
#include "util.h"
#include "charConv.h"

/*************************************************************************
 * GLOBALS
 */
unsigned int   mod_lcdproc_wid = 0;	// display width
int   mod_lcdproc_hgt = 0;		// display height
int   mod_lcdproc_cellwid = 0;		// cell width
int   mod_lcdproc_cellhgt = 0;		// cell height

int   mod_lcdproc_mode = 0;		// display mode
char *mod_lcdproc_playmode = "";	// playing mode
int   mod_lcdproc_time1 = 0;		// playing time
int   mod_lcdproc_time2 = 0;		// "       "
int   mod_lcdproc_time_len =0;		//length of current song (time1+time2)

//moved to player_song.artist / player_song.title
//char  mod_lcdproc_artist[128] = "";	// artist
//char  mod_lcdproc_title[128]  = "";	// title
char  mod_lcdproc_file_old[512] = "";  // title from last update

int   mod_lcdproc_repeat      = 0;	// repeat mode
//moved to player_song.fileext
//char  mod_lcdproc_filetype[10]= ""; //filetype of current song (ogg, mp3,...)
int   mod_lcdproc_updatesong_data=1;//set to 1 if new song is displayer
									//to set time_len...

char  mod_lcdproc_browserpwd[128] = "";        // browser parent working directory
char  mod_lcdproc_browsercursor[128] = "";     // browser cursor selection
int   mod_lcdproc_browsercursorpos = 0;        // current cursorpos in %
int   mod_lcdproc_browsercursorposenabled = 0; // scrollbar emulation enabled?

int   mod_lcdproc_shuffle     = 0;	// shuffle mode
int   mod_lcdproc_mute        = 0;	// mute mode
int   mod_lcdproc_sleep       = 0;	// sleep mode
time_t mod_lcdproc_tmptimeout = 0;	// timout for tmp status

time_t mod_lcdproc_browsertimeout = 0;         // timer for file browser
int    mod_lcdproc_browsertimeoutmax = 10;     // timeout for file browser

int   lcdproc_fd = 0;
fd_set mod_lcdproc_fdset;
int   mod_lcdproc_clockmode  = 0;		// flag for clock/song display
int   mod_lcdproc_clocktimer = 0;		// how many seconds of idle before clock starts
int   mod_lcdproc_songtime_hours = 0;		// display hours separately
int   mod_lcdproc_status_alignment = 0;		// align status right
int   mod_lcdproc_preserve_upper_right = 0;     // preserve upper right corner of display for heartbeat

int	mod_lcdproc_title_line=2;				//line in which the title should be displayed, added by Sebastian Bauer, 21.08.2004
int   	mod_lcdproc_artist_line=1;			//line with artist, added by Sebastian Bauer, 21.08.2004
int	mod_lcdproc_status_line=3;			//line with status (repeat, shuffle,...)
int	mod_lcdproc_time_line=4;				//line with time (song time)
int	mod_lcdproc_menu_line=2;				//first line for the browser/menu

int	mod_lcdproc_bitrate_counter=0;		//on var-bitrate songs the bitrate changes too fast
int	mod_lcdproc_bitrate_sum=0;			//sum up the bitrates and then divide it -> more exactly
int	mod_lcdproc_static_unselected=1;		//unselected lines in the browser don't scroll --> better readability

int	mod_lcdproc_curr_screen=0;		//what's displayed on the LCD now?
#define LCDSCREEN_SONGSTATUS 	0
#define LCDSCREEN_NOSONG 	1
#define LCDSCREEN_STOP		2
#define LCDSCREEN_BROWSER	3
#define LCDSCREEN_MENU		4
/*************************************************************************
 * MODULE INFO
 */
mod_t mod_lcdproc = {
	"mod_lcdproc",
	mod_lcdproc_deinit,	// deinit
	mod_lcdproc_reload,	// reload
	&mod_lcdproc_fdset,	// fd's to watch
	mod_lcdproc_poll,			// poll
	mod_lcdproc_update,	// update
	mod_lcdproc_message,	// message
	NULL,			// SIGCHLD handler
	NULL,			// avoid warning
};


/*************************************************************************
 * DISCONNECT FROM LCDPROC SERVER
 */
void mod_lcdproc_disconnect (void)
{
	if (lcdproc_fd)
		shutdown(lcdproc_fd, 2);
	log_printf(LOG_NOISYDEBUG,"mod_lcdproc_disconnect(): disconnecting from lcdproc.\n");
	lcdproc_fd = 0;
	FD_ZERO(&mod_lcdproc_fdset);
	mod_lcdproc.poll = NULL;
}


/*************************************************************************
 * CONNECT TO LCDPROC SERVER
 */
char *mod_lcdproc_connect (void)
{
	struct sockaddr_in server;
	struct hostent *hostinfo;
	int rc;

	// close if already open
	mod_lcdproc_disconnect();

	// resolve hostname
	server.sin_family = AF_INET;
	server.sin_port = htons(config_getnum("lcdproc_port", 13666));
	hostinfo = gethostbyname(config_getstr("lcdproc_host", "localhost"));
	if (!hostinfo)
		return "LCDproc: Unknown host";
	server.sin_addr = *(struct in_addr *) hostinfo->h_addr;

	// create socket
	lcdproc_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (lcdproc_fd < 0) {
		mod_lcdproc_disconnect();
		return "LCDproc: error creating socket";
	} else
		FD_SET(lcdproc_fd,&mod_lcdproc_fdset);
	log_printf(LOG_DEBUG, "mod_lcdproc_connect(): created socket on fd %d\n", lcdproc_fd);

	// connect
	rc = connect(lcdproc_fd, (struct sockaddr *) &server, sizeof(server));
	if (rc < 0) {
		mod_lcdproc_disconnect();
		return "LCDproc: unable to connect";
	}
	fcntl(lcdproc_fd, F_SETFL, O_NONBLOCK);
	mod_lcdproc.poll = mod_lcdproc_poll;

	return NULL;
}


/*************************************************************************
 * SETUP LCDPROC SCREENS
 */
void mod_lcdproc_setup (void)
{
	char *keylist, *tmpstr;
	char *key;

	if (!lcdproc_fd)
		return;
	log_printf(LOG_DEBUG, "mod_lcdproc_setup(): setting up screens\n");
	sendtext(lcdproc_fd, "hello\n");
	sendtextWait(lcdproc_fd, "success",  "screen_add ABase\n");
	sendtextWait(lcdproc_fd, "success",  "widget_add ABase time string\n");
	sendtextWait(lcdproc_fd, "success",  "widget_add ABase artist scroller\n");
	sendtextWait(lcdproc_fd, "success",  "widget_add ABase title scroller\n");
	sendtextWait(lcdproc_fd, "success",  "widget_add ABase status string\n");

	///TODO: nächste paar Zeilen löschen, browser-zeilen heißen jetzt anders!
    /*sendtextWait(lcdproc_fd, "success",  "widget_add ABase browserpwd scroller\n");
    sendtextWait(lcdproc_fd, "success",  "widget_add ABase browsercursor scroller\n");
    sendtextWait(lcdproc_fd, "success",  "widget_add ABase browsercursorpos string\n");
*/
   // sendtextWait(lcdproc_fd, "success",  "widget_add ABase browserline1 scroller\n");

	int i=1;
	///TODO: lcdproc_hgt ist hier noch nicht initialisiert!  -> in conf_file!
	display_hgt=4;
	display_wid=20;
    for (i=1; i<=display_hgt; i++)
	{
		sendtextWait(lcdproc_fd, "success",  "widget_add ABase browserline%d scroller\n",i);
		log_printf(LOG_DEBUG,"widget_add ABase browserline%d scroller\n",i);
		sendtextWait(lcdproc_fd, "success",  "widget_add ABase browsercursor%d string\n",i);
		log_printf(LOG_DEBUG,"widget_add ABase browsercursor%d string\n",i);
		
	}


	sendtextWait(lcdproc_fd, "success",  "widget_add ABase tmptext string\n");
	sendtextWait(lcdproc_fd, "success",  "widget_add ABase tmpbar hbar\n");
	sendtextWait(lcdproc_fd, "success",  "widget_set ABase tmpbar 0 0 0\n");
	sendtextWait(lcdproc_fd, "success",  "widget_del ABase heartbeat\n");

        /* lcdproc now requires the client to request which keypresses it wishes
           to receive. 
           This code takes an option from the config file which lists the keys
           you wish to receive from lcdproc and sends a command to lcdproc to receive them.
           2002-04-01 Jason Lewis jlewis@NOlordsSPAM.com       */

        /* get any keys to be received from LCDd */
	keylist = (char *) malloc(strlen(config_getstr("lcdproc_keylist", ""))+1);        
	if (keylist != NULL)
		strcpy(keylist, config_getstr("lcdproc_keylist", ""));
	/* get each key in the list, should be separated by "," no spaces */
	key = strtok(keylist,",");
	while (key != NULL) {
		tmpstr = (char *) malloc(strlen("client_add_key ") + strlen(key) + 1); //
		if (tmpstr != NULL)
			sprintf(tmpstr,"client_add_key %s\n",key);

		log_printf(LOG_NOISYDEBUG, "mod_lcdproc_setup(): adding client key %s\n", key);
		sendtextWait(lcdproc_fd, "success",  tmpstr); // request key from lcdproc

		free(tmpstr); // don't forget to free the malloc

		key = strtok(NULL,",");  // get the next key
	}
	       
	free(keylist);

}




/*************************************************************************
 * SET A DISPLAY WIDGET
 */
void mod_lcdproc_setstring (char *name, int y, char *text, int align)
{
	char buf[128];
	int wid;

	if (!lcdproc_fd || !name)
		return;

	wid = mod_lcdproc_wid;
	if (mod_lcdproc_preserve_upper_right) 	{
		if (y == 1 && align == 1)
			wid--;
	}

	if (!text || !*text)
		sendtextWait(lcdproc_fd, "success",  "widget_set ABase %s 0 0 \" \"\n", name);
	else {
		memset(buf, ' ', wid);
		if (align == 1)		// right align
			strcpy(buf+wid-strlen(text), text);
		else if (align == 2)	// center align
			strcpy(buf+((wid-strlen(text))/2), text);
		else			// left align
			strcpy(buf, text);
		sendtextWait(lcdproc_fd, "success",  "widget_set ABase %s 1 %d \"%s\"\n", name, y, buf);
	}
}

void mod_lcdproc_setscroller (char *name, int y, char *text)
{
	if (!lcdproc_fd || !name)
		return;

	if (!text || !*text)
		sendtextWait(lcdproc_fd, "success",  "widget_set ABase %s 1 1 1 1 m 2 \" \"\n", name);
	else
	if (strlen(text)>mod_lcdproc_wid)//added by Sebastian Bauer, 21.08.2004
		sendtextWait(lcdproc_fd, "success",  "widget_set ABase %s 1 %d %d %d m 2 \"%s *** \"\n", name, y, mod_lcdproc_wid, y, text);
	//	mod_lcdproc_setstring(name,y,text,1);//added by Sebastian Bauer, 21.08.2004
	else
		//mod_lcdproc_setstring(name,y,text,1);//added by Sebastian Bauer, 21.08.2004
		sendtextWait(lcdproc_fd, "success",  "widget_set ABase %s 1 %d %d %d m 2 \"%s \"\n", name, y, mod_lcdproc_wid, y, text);
}



/*************************************************************************
 * SHOW TEMPORARY STATUS TEXT/BAR
 */
void mod_lcdproc_settmp (char *text, int percent, int centerbar)
{
	int x, wid;
	char buf[128];

	if (!lcdproc_fd)
		return;

	if (!text && !percent && !centerbar) {
		mod_lcdproc_setstring("tmptext", 0, NULL, 0);
		sendtextWait(lcdproc_fd, "success",  "widget_set ABase tmpbar 0 0 0\n");
		return;
	}

	mod_lcdproc_tmptimeout = time(NULL) + 4;	//TODO: tmptimeout in config-file!
	if (text) {
		strcpy(buf, text);
		while (strlen(buf) < mod_lcdproc_wid)
			strcat(buf, " ");
		mod_lcdproc_setstring("tmptext", mod_lcdproc_hgt, buf, 0);
	} else
		mod_lcdproc_setstring("tmptext", 0, NULL, 0);
	if (percent == 0) {
		sendtextWait(lcdproc_fd, "success",  "widget_set ABase tmpbar 0 0 0\n");
		return;
	}
	x = text ? strlen(text)+2 : 1;
	if (centerbar) {
		x += (mod_lcdproc_wid-x) / 2;
		percent *= 2;
	}
	wid = (mod_lcdproc_wid - x + 1) * mod_lcdproc_cellwid;
	sendtextWait(lcdproc_fd, "success",  "widget_set ABase tmpbar %d %d %d\n", x, mod_lcdproc_hgt, wid*percent/100);
	
}


/*************************************************************************
 * REFRESH THE DISPLAY
 */
void mod_lcdproc_refreshtime (void)
{
	char buf[128];

/*	if (mod_lcdproc_songtime_hours == 0) {
		if (mod_lcdproc_mode == 1) {     // display remaining time
			if(mod_lcdproc_time2 == -1) {
				sprintf(buf,"..-..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "%-5s-%02d:%02d", mod_lcdproc_playmode, mod_lcdproc_time2%3600/60, mod_lcdproc_time2%3600%60);
			}
		}else {                            // display current time
			if(mod_lcdproc_time1 == -1) {
				sprintf(buf,".. ..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "%-5s %02d:%02d", mod_lcdproc_playmode, mod_lcdproc_time1%3600/60, mod_lcdproc_time1%3600%60);
			}
		}
	} else {
		if (mod_lcdproc_mode == 1) {	   // display remaining time
			if(mod_lcdproc_time2 == -1) {
				sprintf(buf,"..-..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "%-5s -%d:%02d:%02d", mod_lcdproc_playmode, mod_lcdproc_time2/3600, mod_lcdproc_time2%3600/60, mod_lcdproc_time2%3600%60);
			}
		} else {                          // display current time
			if(mod_lcdproc_time2 == -1) {
				sprintf(buf,".. ..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "%-5s  %d:%02d:%02d", mod_lcdproc_playmode, mod_lcdproc_time1/3600, mod_lcdproc_time1%3600/60, mod_lcdproc_time1%3600%60);
			}
		}
	}*/
	if (player_status.state==MPD_STATUS_STATE_STOP)
	{
		//do nothing, mod_lcdproc_refresh() will show the STOP-Display
		//strcpy(buf,"playback STOPPED");
	}
	else if (mod_lcdproc_songtime_hours == 0) 
	{
		if (mod_lcdproc_mode == 1) {     // display remaining time
			if(mod_lcdproc_time2 == -1) {
				sprintf(buf,"-..:../..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "-%02d:%02d/%02d:%02d", \
					(player_status.totalTime-player_status.elapsedTime)%3600/60,\
					(player_status.totalTime-player_status.elapsedTime)%3600%60,\
					(player_status.totalTime)%3600/60,(player_status.totalTime)%3600%60);
			}
		}else {                            // display current time
			if(mod_lcdproc_time1 == -1) {
				sprintf(buf," ..:../..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "%02d:%02d/%02d:%02d", \
					player_status.elapsedTime%3600/60, player_status.elapsedTime%3600%60,\
					(player_status.totalTime)%3600/60,(player_status.totalTime)%3600%60);
			}
		}
	} else {
		if (mod_lcdproc_mode == 1) {	   // display remaining time
			if(mod_lcdproc_time2 == -1) {
				sprintf(buf,"-..:..:../..:..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "-%d:%02d:%02d/%d:%02d:%02d", \
					(player_status.totalTime-player_status.elapsedTime)/3600,\
					(player_status.totalTime-player_status.elapsedTime)%3600/60,\
					(player_status.totalTime-player_status.elapsedTime)%3600%60,\
					(player_status.totalTime)/3600,\
					(player_status.totalTime)%3600/60,(player_status.totalTime)%3600%60);
			}
		} else {                          // display current time
			if(mod_lcdproc_time2 == -1) {
				sprintf(buf," ..:..:../..:..:..");
			} else {
				snprintf(buf, sizeof(buf)-1, "%d:%02d:%02d/%d:%02d:%02d", \
					player_status.elapsedTime/3600,\
					player_status.elapsedTime%3600/60, player_status.elapsedTime%3600%60,\
					(player_status.totalTime)/3600,\
					(player_status.totalTime)%3600/60,(player_status.totalTime)%3600%60);
			}
		}
	}
	mod_lcdproc_setstring("time", mod_lcdproc_time_line, buf, 2);	//line 4, 2->align center
}

void mod_lcdproc_browser_resettimer()
{
	mod_lcdproc_browsertimeout = time(NULL) + mod_lcdproc_browsertimeoutmax;
}

void mod_lcdproc_refresh_browser()
{
	int i=0;
	int corr=0;	//correction for headline
	char ctmp[128];
	char cname[62];
	char ccursor[32];
	
	log_printf(LOG_DEBUG,"mod_lcdproc_refresh_browser: activated.");
	browser.status=BROWSER_STATUS_ACTIVE;


	while (i<mod_lcdproc_hgt)
	{
		if (browser.DisplayPos+i+corr>=browser.ElementCount)
		{
			//Hier Zeile auffüllen!!! Sonst wird der Inhalt drunter nicht gelöscht!
			strcpy(ctmp," ");
			ccursor[0]='\0';
		}
		else
		{
			if (i==0&&strlen(browser.HeadLineFixed)!=0)
			{
				strcpy(ctmp,browser.HeadLineFixed);
				ccursor[0]='\0';
				corr=-1;
			}
			else
			{
				//construct browser line with cursor, text,...
				strcpy(ccursor," ");
				if (browser.DisplayPos+i+corr==browser.CursorPos)	//cursor marks this element
					//get cursor symbol
					ccursor[0]=browser.cursorEleStatus[browser.pElements[browser.DisplayPos+i+corr]->status];
				else
					ccursor[0]=browser.viewEleStatus[browser.pElements[browser.DisplayPos+i+corr]->status];
				strcpy(ctmp," "); // space for cursor
				strcat(ctmp,browser.pElements[browser.DisplayPos+i+corr]->name);
			}
		}
		while (strlen(ctmp)<mod_lcdproc_wid-1)	//-1 reserves space for cursor
			strcat(ctmp," ");

		// usr wants unselected lines not to scroll
		///TODO: write mod_lcdproc_static_unselected to conf-file
		if (mod_lcdproc_static_unselected&&((browser.DisplayPos+i+corr)!=browser.CursorPos))
			ctmp[mod_lcdproc_wid-1]='\0';
		sprintf(cname,"browserline%d",i+1);
		mod_lcdproc_setscroller(cname,i+1,ctmp);
		sprintf(cname,"browsercursor%d",i+1);
		mod_lcdproc_setstring(cname,i+1,ccursor,0);


		i++;
	}
}

void mod_lcdproc_browser_remove()
{
	int i=0;
	char cname[20];
	for (i=1; i<=mod_lcdproc_hgt; i++)
	{
		log_printf(LOG_DEBUG,"removing browser.\n");
		sprintf(cname,"browserline%d",i);
		mod_lcdproc_setscroller(cname,mod_lcdproc_hgt+2," ");	//move out of LCD
		sprintf(cname,"browsercursor%d",i);
		mod_lcdproc_setstring(cname,mod_lcdproc_hgt+2," ",0);	//move out of LCD
	}
	browser.status=BROWSER_STATUS_INACTIVE;
	//mod_sendmsg(MSGTYPE_INPUT, "browser deactivate");
	
	//force refresh of title and interpret line on LCD (updates only if something changed!)
	strcpy(mod_lcdproc_file_old,"");
	mod_lcdproc_refresh();
}

void mod_lcdproc_refresh (void)
{
	char buf[128];
	int status_alignment;

	log_printf(LOG_DEBUG, "mod_lcdproc_refresh(): refreshing display\n");

//	if (browser.status==BROWSER_STATUS_ACTIVE&&mod_lcdproc_browsertimeout+time(NULL)>=mod_lcdproc_browsertimeoutmax)
//		mod_lcdproc_browser_remove();
        if (browser.status!=BROWSER_STATUS_INACTIVE&&mod_lcdproc_browsertimeout && time(NULL) >= mod_lcdproc_browsertimeout)
		mod_lcdproc_browser_remove();
	if (browser.status==BROWSER_STATUS_ACTIVE)
	{
	//	if (mod_lcdproc_browsertimeout<=0)
	//		browser.status=BROWSER_STATUS_INACTIVE;
		//log_printf(LOG_DEBUG, "mod_lcdproc_refresh(): refreshing browser!\n");
		//mod_lcdproc_refresh_browser();
	}
	else
	{
		if (player_status.playlistLength<=0)
		{
			mod_lcdproc_curr_screen=LCDSCREEN_NOSONG;
			mpd_Stats *stats=NULL;
			mpd_finishCommand(conn);
			mod_mpd_print_error();
			mpd_sendStatsCommand(conn);
	
			stats = mpd_getStats(conn);
			mod_mpd_print_error();
			mpd_finishCommand(conn);
			mod_mpd_print_error();
				
			
			char buf1[50]="playlist empty";
			char buf2[50]="";
			int i;
			for (i=0; i<((mod_lcdproc_wid-strlen(buf1))/2); i++)
				strcat(buf2,">");
			strcat(buf2,buf1);
			while (strlen(buf2)<mod_lcdproc_wid)
				strcat(buf2,"<");
			mod_lcdproc_setscroller("browserline1",1,buf2);
			if (stats!=NULL)
			{
			if (mod_lcdproc_hgt>1)
			{
				sprintf(buf2," %d songs in DB", stats->numberOfSongs);
				mod_lcdproc_setscroller("browserline2",2,buf2);
			}
			if (mod_lcdproc_hgt>2)
			{
				sprintf(buf2," %02dd %02d:%02d:%02d songtime",
					(int)(stats->dbPlayTime/(24*3600)), (int)((stats->dbPlayTime%(24*3600))/3600), 
					(int)((stats->dbPlayTime%3600)/60),(int)((stats->dbPlayTime%60)));
				mod_lcdproc_setscroller("browserline3",3,buf2);
			}
			if (mod_lcdproc_hgt>3)
			{
				sprintf(buf2," %02dd %02d:%02d:%02d uptime",(int)(stats->uptime/(24*3600)), 
					(int)((stats->uptime%(24*3600))/3600), (int)((stats->uptime%3600)/60),
					(int)((stats->uptime%60)));
				mod_lcdproc_setscroller("browserline4",4,buf2);
			}
			mpd_freeStats(stats);
			mpd_finishCommand(conn);
			}
			else
			if (mod_lcdproc_hgt>1)
			{
				sprintf(buf2," no statistics availible");
				mod_lcdproc_setscroller("browserline2",2,buf2);
			}			
		} 
		else if (player_status.state==MPD_STATUS_STATE_STOP)
		{
			mod_lcdproc_curr_screen=LCDSCREEN_STOP;
			mod_lcdproc_setscroller("browserline1",1,"PLAYBACK STOPPED");
			//show here some information e. g. playlist-filename if saved
			//total playlist songtime
			//playlist length ....
		}
		else
		{
			if (mod_lcdproc_curr_screen!=LCDSCREEN_SONGSTATUS)	//and browser not active (see above)
			{
				int i=0;
				char cname[20];
				for (i=1; i<=mod_lcdproc_hgt; i++)
				{
					log_printf(LOG_DEBUG,"removing browser.\n");
					sprintf(cname,"browserline%d",i);
					mod_lcdproc_setscroller(cname,mod_lcdproc_hgt+2," ");	//move out of LCD
					sprintf(cname,"browsercursor%d",i);
					mod_lcdproc_setstring(cname,mod_lcdproc_hgt+2," ",0);	//move out of LCD
				}
				mod_lcdproc_curr_screen=LCDSCREEN_SONGSTATUS;
			}
			// refresh artist/title
			if (mod_lcdproc_hgt <=2) {
				// don't  display spacing " - " if at least on of artist/title is empty
				// nice on startup and for startup welcome message
				if (player_song.artist[0] && player_song.title[0])
					snprintf(buf, sizeof(buf)-1, "%s - %s", player_song.artist, player_song.title);
				else
					snprintf(buf, sizeof(buf)-1, "%s%s", player_song.artist, player_song.title);
				buf[sizeof(buf)-1] = 0;
				mod_lcdproc_setscroller("artist", mod_lcdproc_artist_line, NULL);
				mod_lcdproc_setscroller("title", mod_lcdproc_title_line, buf);
			} else {//more than 2 lines...
				if (strcasecmp(player_song.file,mod_lcdproc_file_old))  //got a new song!
				{	//so let's update
					mod_lcdproc_setscroller("artist", mod_lcdproc_artist_line, fromUtf8(player_song.artist));	//mod_lcdproc_artist_line (orig.: 2) by Sebastian Bauer, 21.08.2004
															//so let's update
					char title[256];
	
					/*if (!(*player_song.title))
					{
						if (strrchr(player_song.file,'/'))
							strcpy(title, fromUtf8(strrchr(player_song.file,'/'))+1);
						else 
							strcpy(title, fromUtf8(player_song.file));					
						
					}
					else 
						strcpy(title, fromUtf8(player_song.title));*/
					char ctmp[512]="%title%|%shortfile%";
					//char *tmp1=substituteString(&player_song,&player_status,ctmp);
					char *tmp1=infoToFormatedString(&player_song,&player_status,ctmp,NULL);
					if (!tmp1)
						strcpy(title,"n/a");
					else
						strcpy(title, tmp1);
					free(tmp1);
						
					mod_lcdproc_setscroller("title", mod_lcdproc_title_line, title); //mod_lcdproc_title_line (orig.: 3) by Sebastian Bauer, 21.08.2004
					strcpy(mod_lcdproc_file_old,player_song.file);
				}
			}
			
			// automatically align status left for smaller displays
			if (mod_lcdproc_wid < 20) {	//orig.: ...<=20 but i want the align=rigt!! by Sebastian Bauer, 22.08.2004
				status_alignment = 0;
			} else {
				status_alignment = mod_lcdproc_status_alignment;
			}
			
			/*	// refresh status
			if (mod_lcdproc_hgt > 2) {
			snprintf(buf, sizeof(buf)-1, "%s %s %s %s",
			mod_lcdproc_repeat==0 ? "    " : (mod_lcdproc_repeat==1 ? "Rpt1" : "Rpt+"),
			mod_lcdproc_mute ? "Mute" : "    ",
			mod_lcdproc_shuffle ? "Shfl" : "    ",
			mod_lcdproc_sleep ? "Slep" : "    ");
			buf[sizeof(buf)-1] = 0;
			mod_lcdproc_setstring("status", mod_lcdproc_hgt, buf, status_alignment); 
			} else {
			snprintf(buf, sizeof(buf)-1, "%c%c%c%c",
			mod_lcdproc_repeat==0 ? ' ' : (mod_lcdproc_repeat==1 ? 'r' : 'R'),
			mod_lcdproc_mute ?      'm' : ' ',
			mod_lcdproc_shuffle ?   '?' : ' ',
			mod_lcdproc_sleep ?     's' : ' ');
			buf[sizeof(buf)-1] = 0;
			mod_lcdproc_setstring("status", 1, buf, status_alignment);
		}*/
			// refresh status
			char plpos[128]="";
			sprintf(plpos,"%i/%i",player_status.song+1,player_status.playlistLength);
			if (mod_lcdproc_hgt > 2) 
			{
				// Handel es sich um eine Direkteingabe der Liednummer? z. B. 1__
				if (djump_status.value)
				{
					char hc[128];
					sprintf(hc,"%d",djump_status.value);
					int i;
					for (i=djump_status.cursor; i<(int)strlen(hc); i++)
						hc[i]='_';
					strcat(hc,strchr(plpos,'/'));
					strcpy(plpos,hc);
				}
				
				// set displayed bitrate to 0kbs if there's no available (--> do not middle!)
				if (player_status.bitRate==0)
				{
					mod_lcdproc_bitrate_sum=0;
					mod_lcdproc_bitrate_counter=0;
				}
				// now we got a bitRate-info again, so begin with a new average calculation
				if (player_status.bitRate!=0&&mod_lcdproc_bitrate_sum==0)
					mod_lcdproc_bitrate_sum=player_status.bitRate;
				mod_lcdproc_bitrate_counter++;
				// start a new "slice" of average calculation
				if (mod_lcdproc_bitrate_counter>=15)
				{
					mod_lcdproc_bitrate_counter=1;
					mod_lcdproc_bitrate_sum=0;
				}
				mod_lcdproc_bitrate_sum+=player_status.bitRate;
				
				snprintf(buf, sizeof(buf)-1, "%3d", mod_lcdproc_bitrate_sum/mod_lcdproc_bitrate_counter);
				while (strlen(plpos)+strlen(player_song.fileext)+strlen(buf)<mod_lcdproc_wid-12)
					strncat(plpos," ",1);
				snprintf(buf, sizeof(buf)-1, "#%s %s %3dkbs %s%s%s%s%s",	//added #%s by Sebastian Bauer, 22.08.2004
					plpos,
					player_song.fileext,
					mod_lcdproc_bitrate_sum/mod_lcdproc_bitrate_counter,
					player_status.updatingDb ? "U":" ",
					mod_lcdproc_mute ? "M" : " ",
					mod_lcdproc_sleep ? "S" : " ",
					player_status.random ? "?" : " ",
					player_status.repeat ? "R" : " ");
				buf[sizeof(buf)-1] = 0;
				mod_lcdproc_setstring("status", mod_lcdproc_status_line, buf, status_alignment); 
			} else {
				snprintf(buf, sizeof(buf)-1, "%c%c%c%c",
					player_status.repeat ? 'R' : ' ',
					mod_lcdproc_mute ?      'm' : ' ',
					player_status.random ?   '?' : ' ',
					mod_lcdproc_sleep ? 's' : ' ');
				buf[sizeof(buf)-1] = 0;
				mod_lcdproc_setstring("status",mod_lcdproc_status_line , buf, status_alignment);
			}
			mod_lcdproc_refreshtime();
		}
	}
}

/*****************************************************************************
* SHOW TEMPORARY FILE BROWSER INFORMATION
*/
void mod_lcdproc_browser(int display)
{
/*
       int y1, remain;
       char buf[1024];

       if (!lcdproc_fd)
           return;

       y1     = mod_lcdproc_menu_line;//mod_lcdproc_hgt < 4 ? 1 : 3;
       remain = mod_lcdproc_browsercursorposenabled ? 3 : 0;

       if (display) {          // show browser output
           mod_lcdproc_browsertimeout = time(NULL) + mod_lcdproc_browsertimeoutmax;

           if (mod_lcdproc_browserpwd) {
                   strcpy(buf, mod_lcdproc_browserpwd);
                   while (strlen(buf) < mod_lcdproc_wid-remain)
                       strcat(buf, " ");
		   if (strlen(buf) > mod_lcdproc_wid-remain) {
			   sendtextWait(lcdproc_fd, "success",  "widget_set ABase browserpwd 1 %d %d %d h 2 \"%s   \"\n", y1, mod_lcdproc_wid-remain, y1, buf);
		   } else {
			   sendtextWait(lcdproc_fd, "success",  "widget_set ABase browserpwd 1 %d %d %d h 2 \"%s\"\n", y1, mod_lcdproc_wid-remain, y1, buf);
		   }

           } else
                   mod_lcdproc_setstring("browserpwd", 0, NULL, 0);


           if (mod_lcdproc_browsercursorposenabled)
                   sendtextWait(lcdproc_fd, "success",  "widget_set ABase browsercursorpos %d %d \" %02d\"\n", mod_lcdproc_wid-2, y1, mod_lcdproc_browsercursorpos);

           if (mod_lcdproc_browsercursor) {
                   strcpy(buf, mod_lcdproc_browsercursor);
                   while (strlen(buf) < mod_lcdproc_wid)
                           strcat(buf, " ");
		   if (strlen(buf) > mod_lcdproc_wid) { //was here a bug? mod_lcdproc_wid-remain makes the childdir scrolling any time! by Sebastian Bauer, 22.08.2004
			   sendtextWait(lcdproc_fd, "success",  "widget_set ABase browsercursor 1 %d %d %d h 2 \"%s   \"\n", y1+1, mod_lcdproc_wid, y1+1, buf);
		   } else {
			   sendtextWait(lcdproc_fd, "success",  "widget_set ABase browsercursor 1 %d %d %d h 2 \"%s\"\n", y1+1, mod_lcdproc_wid, y1+1, buf);
		   }

           } else
                   mod_lcdproc_setstring("browsercursor", 0, NULL, 0);

       } else {        // clear display
               sendtextWait(lcdproc_fd, "success",  "widget_set ABase browserpwd 1 1 1 3 h 0 \" \"\n");
               sendtextWait(lcdproc_fd, "success",  "widget_set ABase browsercursor 1 1 3 1 h 0 \" \"\n");
               sendtextWait(lcdproc_fd, "success",  "widget_set ABase browsercursorpos 1 4 \" \"\n");
               mod_lcdproc_refresh();
	   mod_sendmsg(MSGTYPE_INPUT, "browser deactivate");
       }
*/
}


/*************************************************************************
 * POLL INPUT DATA
 */
void mod_lcdproc_poll (int fd)
{
	int rc;
	char s[512], buf[128];
	char *c0, *c1, *c2, *c3;

	// read LCDproc response
	rc = readline(fd, s, sizeof(s));
	if (rc < 0) {
		// close LCDproc connection
		mod_lcdproc_disconnect();
		log_printf(LOG_ERROR, "Lost connection to LCDproc server: %s\n", strerror(errno));
		return;
	}
	if (!*s)
		return;

	// process LCDproc response
	log_printf(LOG_NOISYDEBUG, "mod_lcdproc_poll(): LCDproc response: '%s'\n", s);
	c0 = s ? strdup(s) : NULL;
	c1 = strtok(c0, " \t");
	if(!c1) return;
	if ( !strcasecmp(c1, "connect")) {
		c2 = strtok(NULL, " \t");
		do {
			c3 =  strtok(NULL, " \t");
			if (c3 && !strcasecmp(c3, "wid")) {
				c3 = strtok(NULL, " \t");
				mod_lcdproc_wid = c3 ? atoi(c3) : 0;
				display_wid=mod_lcdproc_wid;
			} else if (c3 && !strcasecmp(c3, "hgt")) {
				c3 = strtok(NULL, " \t");
				mod_lcdproc_hgt = c3 ? atoi(c3) : 0;
				display_hgt=mod_lcdproc_hgt;
			} else if (c3 && !strcasecmp(c3, "cellwid")) {
				c3 = strtok(NULL, " \t");
				mod_lcdproc_cellwid = c3 ? atoi(c3) : 0;
			} else if (c3 && !strcasecmp(c3, "cellhgt")) {
				c3 = strtok(NULL, " \t");
				mod_lcdproc_cellhgt = c3 ? atoi(c3) : 0;
			}
		} while (c3);
		log_printf(LOG_DEBUG, "mod_lcdproc_poll(): display size %dx%d, cell size %dx%d\n", mod_lcdproc_wid, mod_lcdproc_hgt, mod_lcdproc_cellwid, mod_lcdproc_cellhgt);

	} else if ( !strcasecmp(c1, "listen")) {
		// ignore 'listen' response

	} else if ( !strcasecmp(c1, "ignore")) {
		// ignore 'ignore' response

	} else if ( !strcasecmp(c1, "key") ) {
		c2 = strtok(NULL, " \t");
		if(!c2) return;
		log_printf(LOG_DEBUG, "mod_lcdproc_poll(): got key '%s'\n", c2);
		snprintf(buf, sizeof(buf)-1, "lcdproc_key_%s", c2);
		c3 = config_getstr(buf, NULL);
		if (c3)
			mod_sendmsg(MSGTYPE_INPUT, c3);

	} else if ( !strcasecmp(c1, "bye")) {
		log_printf(LOG_ERROR, "LCDproc has terminated connection\n");
		mod_lcdproc_disconnect();

        } else if (!strcasecmp(c1, "success")) {
	        log_printf(LOG_NOISYDEBUG, "mod_lcdproc_poll(): LCDd returned success message\n");

	} else {
		log_printf(LOG_DEBUG, "mod_lcdproc_poll(): unknown response: '%s'\n", s);
	}
	free(c0);
}


/*************************************************************************
 * UPDATE
 */
void mod_lcdproc_update (void)
{
	time_t now;

	now = time(NULL);
	if (mod_lcdproc_tmptimeout && now >= mod_lcdproc_tmptimeout) {
		mod_lcdproc_tmptimeout = 0;
		mod_lcdproc_settmp(NULL, 0, 0);
	}

        if ((browser.status!=BROWSER_STATUS_INACTIVE) && mod_lcdproc_browsertimeout && now >= mod_lcdproc_browsertimeout)
		mod_lcdproc_browser_remove();
	mod_mpd_update();
	mod_lcdproc_refresh();

	//Read all success-messages from LCDd
	char buf[512];
	do
	{
		readline(lcdproc_fd,buf,sizeof(buf));
		log_printf(LOG_DEBUG,"read line: %s",buf);
	}
	while (!strcasecmp(buf,"success"));
}


/*************************************************************************
 * GO INTO CLOCK MODE
 */
void mod_lcdproc_clockstart()
{

    sendtextWait(lcdproc_fd, "success",  "screen_del ABase\n");
    sendtextWait(lcdproc_fd, "success",  "screen_add lcdtime\n");
    sendtextWait(lcdproc_fd, "success",  "screen_set lcdtime -heartbeat off\n");
    sendtextWait(lcdproc_fd, "success",  "widget_add lcdtime time string\n");
    sendtextWait(lcdproc_fd, "success",  "widget_add lcdtime date string\n");
//	mod_lcdproc_clockmode=1;
	mod_lcdproc_timedisplay();
}

/*************************************************************************
 * DISPLAY THE TIME
 */
void mod_lcdproc_timedisplay(void)
{
	time_t			t;
	struct tm *l_time;
	char			timestr[128];
	char			datestr[128];
	char			x=0,y=0,mx=0,my=0,dx=0;

	bzero(&timestr,sizeof(timestr));
	t=time(NULL);
								// this code makes the time march
								// around the display (to avoid burn-in)
	mx=mod_lcdproc_wid-10;		
	my=mod_lcdproc_hgt-1;
	if(mx<1) mx=1;				// maximum X starting point
	if(my<1) my=1;				// maximum Y starting point
	x=((t/60)%mx)+1;			// actual X starting point (based on time)
	y=((t/60/mx)%my)+1;			// actual Y starting point 	"	"	"
        
	l_time = localtime(&t);			
	setlocale(LC_TIME,"");		// Get the current locale from environment

	strftime(timestr,sizeof(timestr),config_getstr("lcdproc_timestr","%R"),l_time); 
	strftime(datestr,sizeof(datestr),config_getstr("lcdproc_datestr","%x"),l_time);

	dx = (strlen(datestr) - strlen(timestr)) / 2;

	sendtextWait(lcdproc_fd, "success",  "widget_set lcdtime date %d %d \"%s\"\n",x,y,datestr);
	sendtextWait(lcdproc_fd, "success",  "widget_set lcdtime time %d %d \"%s\"\n",x+dx,y+1,timestr);
}


	
	
/*************************************************************************
 * GET OUT OF CLOCK MODE
 */
void mod_lcdproc_clockstop()
{
    sendtextWait(lcdproc_fd, "success",  "screen_del lcdtime\n");
	mod_lcdproc_setup();
	mod_lcdproc_clockmode=0;
	mod_lcdproc_refresh();
}
	


/*************************************************************************
 * RECEIVE MESSAGE
 */
void mod_lcdproc_message (int msgtype, char *msg)
{
	char *c1 = NULL, *c2 = NULL, *c3 = NULL;
	char buf[128];
	int i, j;
	mod_message_t *answer = NULL;

	if (!lcdproc_fd)
		return;


	// pre-process input mesage for handling IDLE messages.
	if (msgtype == MSGTYPE_EVENT && !strncasecmp(msg,"idle",4)) {
		if(mod_lcdproc_clockmode) { 	// clock is already started
			mod_lcdproc_timedisplay();
		} else if (mod_lcdproc_clocktimer && mod_lcdproc_clocktimer <= strtol(msg + 5,NULL,0) ) {
			mod_lcdproc_clockstart();
		}
	} else if (mod_lcdproc_clockmode)  {
		mod_lcdproc_clockstop();
	}

	// process input messages
	if (msgtype == MSGTYPE_INPUT) {
		c1 = strtok(msg, " \t") ;
		if(!c1) return ;

		// displaymode
		if (!strcasecmp(c1, "displaymode")) {
			mod_lcdproc_mode++;
			if (mod_lcdproc_mode > 1)
				mod_lcdproc_mode = 0;
			mod_lcdproc_refreshtime();
		}

	// process player messages
	} else if (msgtype == MSGTYPE_PLAYER) {
		c1 = strtok(msg, " \t");
		if(!c1)return;
		c2 = strtok(NULL, " \t");
		c3 = strtok(NULL,"");
			// time info
		if (!strcasecmp(c1, "time")) {
			mod_lcdproc_time1 =  c2?atoi(c2) : 0;
			mod_lcdproc_time2 =  c3?atoi(c3) : 0;
			if (mod_lcdproc_updatesong_data)
			{
				mod_lcdproc_time_len=mod_lcdproc_time1+mod_lcdproc_time2;
				mod_lcdproc_updatesong_data=0;
			}
			mod_lcdproc_refreshtime();

		}

		// process mixer messages
	} else if (msgtype == MSGTYPE_EVENT ) {
		c1 = strtok(msg, " \t");
		if(!c1) return;
	
		if (!strcasecmp(c1, "repeat")) {
			c2 = strtok(NULL, "");
			mod_lcdproc_repeat = c2 ? atoi(c2) : 0;
			mod_lcdproc_refresh();

		// shuffle mode
		} else if ( !strcasecmp(c1, "shuffle")) {
			c2 = strtok(NULL, "") ;
			mod_lcdproc_shuffle = c2 ? atoi(c2) : 0;
			mod_lcdproc_refresh();

		// sleep timer
		} else if ( !strcasecmp(c1, "sleep")) {
			c2 = strtok(NULL, " \t") ;
			c3 = strtok(NULL, "") ;
			i = c2 ? atoi(c2) : 0;
			j = c3 ? atoi(c3) : 0;
			if(!strcasecmp(c2,"trigered")) {
				mod_lcdproc_settmp("Sleep timer expired", 0, 0);
				mod_lcdproc_sleep = 0;
			} else if(!strcasecmp(c2,"decreased")) {
				mod_lcdproc_settmp("Fading out", 0, 0);
			}else if (!i) {
				mod_lcdproc_settmp("Sleep timer off", 0, 0);
				mod_lcdproc_sleep = 0;
			} else {
				if (j)
					sprintf(buf, "Sleep %d/%d min", i, j);
				else
					sprintf(buf, "Sleep %d min", i);
				mod_lcdproc_settmp(buf, 0, 0);
				mod_lcdproc_sleep = 1;
			}
			mod_lcdproc_refresh();
		} else if ( !strcasecmp(c1,"alarm")) {
			c2 = strtok(NULL, "");
			i = c2 ? atoi(c2) : 0;
			if (i) 
				mod_lcdproc_settmp("Alarms enabled",0,0);
			else
				mod_lcdproc_settmp("Alarms disabled",0,0);
		} else if ( !strcasecmp(c1,"loadplaylist")) {

			// display a playlist name here...
			c2 = strtok(NULL, "");
			strcpy (buf,c2?c2:"");
			mod_lcdproc_settmp(buf,0,0);
		} else if(!strcasecmp(c1,"mixer")) { 
			c2 = strtok(NULL, " \t");
			if(!c2) return;
			if ( !strcasecmp(c2, "volume")) {
				c3 = strtok(NULL,"");
				mod_lcdproc_settmp("Vol",  c3 ? atoi(c3) : 0, 0);
			} else if (!strcasecmp(c2, "balance")) {
				c3 = strtok(NULL,"");
				mod_lcdproc_settmp("Balance", c3 ?atoi(c3) : 0, 1);
			} else if (!strcasecmp(c2, "bass")) {
				c3 = strtok(NULL,"");
				mod_lcdproc_settmp("Bass", c3 ? atoi(c3) : 0, 0);
			} else if (!strcasecmp(c2, "treble")) {
				c3 = strtok(NULL,"");
				mod_lcdproc_settmp("Treble", c3 ? atoi(c3) : 0, 0);
			} else if (!strcasecmp(c2, "mute")) {
				c3 = strtok(NULL,"");
				mod_lcdproc_mute = c3 ? atoi(c3) : 0;
				mod_lcdproc_refresh();
			}
		} else if (!strcasecmp(c1, "play")) {
			mod_lcdproc_playmode = " ";	//by Sebastian Bauer, 21.08.2004 originally ..="PLAY"
			mod_lcdproc_updatesong_data=1;  //update song length!
			mod_lcdproc_refresh();

		} else if (!strcasecmp(c1, "stop")) {
			mod_lcdproc_playmode = "S";
			mod_lcdproc_time1 = 0;
			mod_lcdproc_time2 = 0;
			mod_lcdproc_time_len=0;
			mod_lcdproc_refresh();

			// pause
		} else if ( !strcasecmp(c1, "unpause")) {
				mod_lcdproc_playmode = " ";
				mod_lcdproc_refreshtime();
		} else if ( !strcasecmp(c1, "pause")) {
				mod_lcdproc_playmode = "P";
				mod_lcdproc_refreshtime();
		} else if (!strcasecmp(c1, "halt")) {
			mod_lcdproc_playmode = "H";
			mod_lcdproc_time1 = 0;
			mod_lcdproc_time2 = 0;
			mod_lcdproc_time_len=0;
			mod_lcdproc_refresh();
		}

	} else if (msgtype == MSGTYPE_INFO) {
		c1 = strtok(msg, " \t");
		if(!c1) return;
         // file browser info
		if ( !strcasecmp(c1, "browser")) {
			c2 =  strtok(NULL, " \t");
			if(!c2) return;
                      /*  if (!strcasecmp(c2, "pwd")) {
				c3 =  strtok(NULL, "");
				if(!c3) return;
                                strcpy(mod_lcdproc_browserpwd, c3 ? c3 : "");
                                mod_lcdproc_browser(1);
                        } else if (!strcasecmp(c2, "cursor")) {
				c3 =  strtok(NULL, "");
				if(!c3) return;
                                strcpy(mod_lcdproc_browsercursor, c3 ? c3 : "");
                                mod_lcdproc_browser(1);
                        } else if (!strcasecmp(c2, "cursorpos")) {
				c3 =  strtok(NULL, "");
				if(!c3) return;
                                mod_lcdproc_browsercursorpos = c3 ? atoi(c3) : 0;
                                mod_lcdproc_browser(1);
                        } else if (!strcasecmp(c2, "clrscr")) {
                                mod_lcdproc_browser(0);
                        } else*/ if ( !strcasecmp(c2, "info")) {     // short browser info
				c3 =  strtok(NULL, "");
                                strcpy(buf, c3 ? c3 : "");
                                mod_lcdproc_settmp(buf, 0, 0);
                        }
		} else if ( !strcasecmp(c1, "lcdproc")) {
			c2 =  strtok(NULL, " \t");
			if(!c2) return;
            if (!strcasecmp(c2, "refresh"))
				mod_lcdproc_refresh();

		}

	// process generic messages
	} else if (msgtype == MSGTYPE_EVENT) {
		c1 = strtok(msg, " \t");
		if(!c1) return;

		// repeat mode
	}
}


/*************************************************************************
 * MODULE INIT FUNCTION
 */
char *mod_lcdproc_init (void)
{
	char *s;

	FD_ZERO(&mod_lcdproc_fdset);

	// register our module
	mod_register(&mod_lcdproc);

	// read some configuration values
	mod_lcdproc_songtime_hours       = (strcasecmp("yes",   config_getstr("lcdproc_songtime_hours", "no")) == 0);
	mod_lcdproc_status_alignment     = (strcasecmp("right", config_getstr("lcdproc_status_alignment", "right")) == 0);
	mod_lcdproc_preserve_upper_right = (strcasecmp("yes",   config_getstr("lcdproc_preserve_upper_right", "no")) == 0);
	mod_lcdproc_title_line=atoi(config_getstr("lcdproc_title_line", "2"));
	mod_lcdproc_artist_line=atoi(config_getstr("lcdproc_artist_line", "1"));
	mod_lcdproc_status_line=atoi(config_getstr("lcdproc_status_line", "3"));
    mod_lcdproc_time_line=atoi(config_getstr("lcdproc_time_line", "4"));	

	// connect to LCDproc server
	s = mod_lcdproc_connect();
	if (s)
		return s;
	log_printf(LOG_DEBUG, "mod_lcdproc_init(): socket connected\n");

        // show browser scrollbar emulation?
        mod_lcdproc_browsercursorposenabled = !strcasecmp(config_getstr("lcdproc_browser_scrollbar", "yes"), "yes") ? 1 : 0;
  
        // browser display timeout
        mod_lcdproc_browsertimeoutmax = config_getnum("lcdproc_browser_timeout", 10);
        if (mod_lcdproc_browsertimeoutmax < 2)          // minimum check
                mod_lcdproc_browsertimeoutmax = 2;


	// setup LCDproc
	mod_lcdproc_setup();

	// get clock timer
	mod_lcdproc_clocktimer=60*config_getnum("lcdproc_clock_timer",1);

	//clear display
	mod_sendmsg(MSGTYPE_PLAYER, "stop");

	return NULL;
}


/*************************************************************************
 * MODULE DEINIT FUNCTION
 */
void mod_lcdproc_deinit (void)
{
// not really necessary
/*	sendtextWait(lcdproc_fd, "success",  "widget_del ABase time string\n");
	sendtextWait(lcdproc_fd, "success",  "widget_del ABase artist scroller\n");
	sendtextWait(lcdproc_fd, "success",  "widget_del ABase title scroller\n");
	sendtextWait(lcdproc_fd, "success",  "widget_del ABase status string\n");

	int i;
    for (i=1; i<=display_hgt; i++)
	{
		sendtextWait(lcdproc_fd, "success",  "widget_del ABase browserline%d scroller\n",i);
		log_printf(LOG_DEBUG,"widget_del ABase browserline%d scroller\n",i);
		sendtextWait(lcdproc_fd, "success",  "widget_del ABase browsercursor%d string\n",i);
		log_printf(LOG_DEBUG,"widget_del ABase browsercursor%d string\n",i);
		
	}


	sendtextWait(lcdproc_fd, "success",  "widget_del ABase tmptext string\n");
	sendtextWait(lcdproc_fd, "success",  "widget_del ABase tmpbar hbar\n");
	sendtextWait(lcdproc_fd, "success",  "screen_del ABase\n");
		*/
	// close LCDproc connection
	mod_lcdproc_disconnect();

	log_printf(LOG_DEBUG, "mod_lcdproc_deinit(): socket closed\n");
}


/*************************************************************************
 * MODULE RELOAD FUNCTION
 */
char *mod_lcdproc_reload (void)
{
	char *s;

	log_printf(LOG_DEBUG, "mod_lcdproc_reload(): reconnecting\n");

	// close and reconnect to LCDproc server
	s = mod_lcdproc_connect();
	if (s)
		return s;

	// setup LCDproc
	mod_lcdproc_setup();

	// refresh display
	mod_lcdproc_refresh();

	return NULL;
}


/*************************************************************************
 * EOF
 */
