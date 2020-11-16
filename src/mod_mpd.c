/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABased/mod_mpd.c,v $ -- mpd input interface
 * $Id: mod_mpd.c,v 1.5 2003/08/06 18:58:51 boucman Exp $
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
#include "mod_mpd.h"
#include "string.h"
#include "globalDef.h"

#include "libmpdclient.h"


/*************************************************************************
 * GLOBALS
 */
fd_set			 mod_mpd_fdset;
struct mpd_config 	*mod_mpd_config;
static char artist[31],title[31],album[31],year[5],genre[31],comment[31];
static char songfile[512];

mpd_Connection *conn;
//mpd_Status player_status;
//mpd_Song player_song;

static int seekcounter=0;
static int lastsecpos = -1;
static int lastsecremain = -1;

/*************************************************************************
 * MODULE INFO
 */
mod_t mod_mpd = {
	"mod_mpd",
	mod_mpd_deinit,	// deinit
	mod_mpd_reload,	// reload
	&mod_mpd_fdset,	// watch_fdset
	mod_mpd_poll,		// poll
	NULL,			// update
	mod_mpd_message,	// message
	NULL,			// SIGCHLD handler
	NULL,			// avoid warning
};


void mod_mpd_update(void)
{
	mod_mpd_update_status();
	mod_mpd_update_songinfo();
}

void mod_mpd_print_error()
{
	if (conn->error)
	{
		char c[]="BUFFER OVERRUN";
		switch (conn->error)
		{
		case 0:
			sprintf(c,"NOERROR");
			break;
		case MPD_ERROR_TIMEOUT:
			sprintf(c,"TIMEOUT");
			break;
		case MPD_ERROR_SYSTEM:
			sprintf(c,"SYSTEM");
			break;
		case MPD_ERROR_UNKHOST:
			sprintf(c,"UNKHOST");
			break;
		case MPD_ERROR_CONNPORT:
			sprintf(c,"CONNPORT");
			break;
		case MPD_ERROR_NOTMPD:
			sprintf(c,"NOTMPD");
			break;
		case MPD_ERROR_NORESPONSE:
			sprintf(c,"NORESPONSE");
			break;
		case MPD_ERROR_SENDING:
			sprintf(c,"SENDING");
			break;
		case MPD_ERROR_CONNCLOSED:
			sprintf(c,"CONNCLOSED");
			break;
		case MPD_ERROR_ACK:
			sprintf(c,"ACK");
			break;
		case MPD_ERROR_BUFFEROVERRUN:
			sprintf(c,"BUFFEROVERRUN");
			break;
		default:
			sprintf(c,"UNKNOWN: %i",conn->error);
			break;
		}
		log_printf(LOG_DEBUG,"  MPD-error (%s) reported: %s\n",c,conn->errorStr);
	}
}




void mod_mpd_update_status()
{
//	mpd_InfoEntity * entity=NULL;

//	if (player_status)
//		mpd_freeStatus(player_status);
	mpd_Status *status;
	mpd_finishCommand(conn);
	mpd_sendStatusCommand(conn);
	status = mpd_getStatus(conn);
	mpd_finishCommand(conn);
	if (conn->error)
	{
		mod_mpd_print_error();
		log_printf(LOG_DEBUG,"mod_mpd_update_status: status command failed! MPD: %s\n",conn->error);
		return;
	}
	

	log_printf(LOG_DEBUG,"mod_mpd_update_status: mpd_getStatus done.\n");
	if (conn->error||status==NULL)
	{
		mod_mpd_print_error();
		log_printf(LOG_DEBUG,"mod_mpd_update_status: getting status failed! MPD: %s\n",conn->error);
		return;
	}


	char c[]="UNKNOWN";


	player_status.bitRate=status->bitRate;
	player_status.bits=status->bits;
	player_status.channels=status->channels;
	player_status.crossfade=status->crossfade;
	player_status.elapsedTime=status->elapsedTime;
	strcpy(player_status.error,status->error?status->error:"");
	player_status.playlist=status->playlist;
	player_status.playlistLength=status->playlistLength;
	player_status.random=status->random;
	player_status.repeat=status->repeat;
	player_status.sampleRate=status->sampleRate;
	player_status.song=status->song;
	player_status.songid=status->songid;
	player_status.state=status->state;
	player_status.totalTime=status->totalTime;
	player_status.updatingDb=status->updatingDb;
	player_status.volume=status->volume;


	if (player_status.state==MPD_STATUS_STATE_STOP)
		strcpy(c,"STOP");
	else if (player_status.state==MPD_STATUS_STATE_PLAY)
		strcpy(c,"PLAYING");
	else if (player_status.state==MPD_STATUS_STATE_PAUSE)
		strcpy(c,"PAUSED");

	log_printf(LOG_DEBUG,"mod_mpd_update_status: Got status: %s, %s random: %s, repeat: %s, crossfade: %s\n",c,player_status.updatingDb?"UPDATING DB":"NO UPDATING",player_status.random?"YES":"NO",player_status.repeat?"YES":"NO",player_status.crossfade?"YES":"NO");
	log_printf(LOG_DEBUG,"				songtime: %i, totaltime: %i, bitrate: %ikbs; channels: %i; bits: %ibit; samplerate: %iHz\n",player_status.elapsedTime,player_status.totalTime,player_status.bitRate,player_status.channels,player_status.bits,player_status.sampleRate);
	mpd_freeStatus(status);
}

void mod_mpd_update_songinfo()
{
	mpd_Status * status;
	mpd_InfoEntity * entity;
	mpd_Song * song;

	mpd_sendCommandListOkBegin(conn);
	mpd_sendStatusCommand(conn);
	mpd_sendCurrentSongCommand(conn);
	mpd_sendCommandListEnd(conn);
	
	status = mpd_getStatus(conn);

	if(status->state == MPD_STATUS_STATE_PLAY || 
		status->state == MPD_STATUS_STATE_PAUSE) 
	{
		mpd_nextListOkCommand(conn);
		
		while((entity = mpd_getNextInfoEntity(conn))) {
			song = entity->info.song;
			
			if(entity->type!=MPD_INFO_ENTITY_TYPE_SONG) {
				mpd_freeInfoEntity(entity);
				continue;
			}

			//Save information to player_song
			if (song==NULL)
			{
				log_printf(LOG_DEBUG,"mod_mpd_update_songinfo: no songinfo available.\n");
				return;
			}
			log_printf(LOG_DEBUG,"mod_mpd_update_songinfo: songinfo available.\n");
			// copy data to player_song
			strcpy(player_song.file, song->file?song->file:"");
			strcpy(player_song.album, song->album?song->album:"");
			strcpy(player_song.artist, song->artist?song->artist:"");
			strcpy(player_song.name, song->name?song->name:"");
			strcpy(player_song.title, song->title?song->title:"");
			strcpy(player_song.track, song->track?song->track:"");
			player_song.time = song->time;
			player_song.pos = song->pos;
			player_song.id = song->id;
 
			mpd_freeInfoEntity(entity);
			
			break;
		}
		
		mpd_finishCommand(conn);
		mpd_freeStatus(status);	
		
		if (strrchr(player_song.file,'.'))
		{
			strcpy(player_song.fileext, strrchr(player_song.file,'.')+1);
		}
		
		log_printf(LOG_DEBUG,"mod_mpd_update_songinfo: got song information from file: %s\n",player_song.file);
		log_printf(LOG_DEBUG,"                             Artist: %s, Title: %s, Name: %s\n",player_song.artist,player_song.title,player_song.name);
		log_printf(LOG_DEBUG,"                             Album: %s, Track: %s, time: %i, pos: %i, id: %i\n",player_song.album,player_song.track,player_song.time,player_song.pos,player_song.id);
	}
	else 
		log_printf(LOG_DEBUG,"mod_mpd_update_songinfo(): Oh, MPD is not playing!\n");
		
}

void mod_mpd_play()
{
	mod_mpd_update_status();
	if (player_status.state==MPD_STATUS_STATE_PAUSE)
		mpd_sendPauseCommand(conn,0);
	else if (player_status.state==MPD_STATUS_STATE_PLAY)
		mpd_sendSeekCommand(conn,player_status.song,0);	//faster than mpd_sendPlayCommand(conn,player_status.song);
	else
		mpd_sendPlayCommand(conn,0);
	mod_mpd_print_error();
	mpd_finishCommand(conn);
}

/*************************************************************************
* RECEIVE MESSAGE
*/
void mod_mpd_message (int msgtype, char *rawmsg)
{
	char *c1, *c2, *c3, msg[512];
	log_printf(LOG_DEBUG,"mod_mpd_message: entered.\n");

	strncpy(msg,rawmsg,sizeof(msg));	// pad msg with nulls

	// handle input messages
	if (msgtype == MSGTYPE_INPUT) {
		c1 = strtok(msg, " \t");
		if(!c1) return;
		
		// message should not be processed if the condition c_* doesn't comply with the browser_state!
		if (!strcasecmp(c1, "c_nbrowser"))
			if (browser.status==BROWSER_STATUS_INACTIVE)
				c1 = strtok(NULL, " \t");
			else return;
		else if (!strcasecmp(c1, "c_browser"))
			if (browser.status==BROWSER_STATUS_ACTIVE)
				c1 = strtok(NULL, " \t");
			else return;
		     
		if (!strcasecmp(c1, "play"))
		{
			mod_mpd_play();
		} else if (!strcasecmp(c1, "stop"))
		{
			mod_sendmsgf(MSGTYPE_PLAYER,rawmsg);
			mod_sendmsgf(MSGTYPE_EVENT,rawmsg);
			mpd_sendStopCommand(conn);
			mpd_finishCommand(conn);
			mod_mpd_update_status();
		} 
		else if (!strcasecmp(c1, "pause")) 
		{
			mod_sendmsgf(MSGTYPE_PLAYER,rawmsg);
			if (player_status.state==MPD_STATUS_STATE_PAUSE)
			{
				log_printf(LOG_DEBUG,"mod_mpd_message: player paused.\n");
				mpd_sendPauseCommand(conn,0);
			}
			else if (player_status.state==MPD_STATUS_STATE_PLAY)
				mpd_sendPauseCommand(conn,1);
			mod_mpd_print_error();
			mpd_finishCommand(conn);
			mod_mpd_print_error();
			mod_mpd_update_status();
		}
		else if ( !strcasecmp(c1, "seek")) 
		{
			mod_sendmsgf(MSGTYPE_PLAYER,rawmsg);
			log_printf(LOG_DEBUG,"seekcounter=%i",seekcounter);
			if (seekcounter>0)
				seekcounter--;
			else
			{
				seekcounter=1;
				c2 = strtok(NULL, " \t") ;
				if(!c2) return;
				mpd_sendSeekCommand(conn, player_status.song,player_status.elapsedTime+atoi(c2));
				mpd_finishCommand(conn);
				mod_mpd_update_status();
				mod_mpd_print_error();
			}
		}
		else if (!strcasecmp(c1,"jump"))
		{
			int oldstate=player_status.state;
			if (player_status.state!=MPD_STATUS_STATE_PAUSE&&
				player_status.state!=MPD_STATUS_STATE_PLAY)
				return;
			c2 =  strtok(NULL, " \t") ;
			if(!c2) return;
			if (!strcasecmp(c2,"+1"))
			{
				mpd_sendNextCommand(conn);
			}
			else if (!strcasecmp(c2,"-1"))
			{
				mpd_sendPrevCommand(conn);
			}
			else
			{
				int newpos=player_status.song+atoi(c2);
				log_printf(LOG_DEBUG,"jump to newpos: %i",newpos);
				if (newpos>=player_status.playlistLength)
				{
					if (player_status.repeat)
						newpos=newpos%player_status.playlistLength;
					else
						newpos=player_status.playlistLength;
				}
				else if (newpos<0)
					if (player_status.repeat)
					{
						newpos=player_status.playlistLength-(abs(newpos)%player_status.playlistLength);
						newpos=newpos>=player_status.playlistLength ? 0 : newpos;
					} 
					else newpos=0;
				mpd_sendPlayCommand(conn,newpos);
			}
			mod_mpd_print_error();
			mpd_finishCommand(conn);
			if (oldstate==MPD_STATUS_STATE_PAUSE)
				mpd_sendPauseCommand(conn,1);
			mpd_finishCommand(conn);
		}
		else if (!strcasecmp(c1,"djump")){
			// browser active check not nesasarry
			// browser will send djump to mod_playlist if it's not active
			// -> user can send message to mod_browser either to mod_playlist by editing lircirmp3.conf
			// -> playlist djump accomplishes in every case
			// -> browser only if the browser is not active
			
			int oldstate=player_status.state;
			//reset cursor pos
			if (djump_status.value==0)
				djump_status.cursor=0;
			c2 = strtok(NULL, "");
			char hc[8]="";
			if(!c2) return ;
			log_printf(LOG_DEBUG, "mod_mpd_message(): c2='%s'\n", c2);
			if (*c2 == '+')
			{
				djump_status.value+=10;	//for +10
				sprintf(hc,"%d",djump_status.value);
				djump_status.cursor=strlen(hc)-1;
				if (djump_status.value>player_status.playlistLength)
					djump_status.value=0; //exit input mode
				log_printf(LOG_DEBUG, "mod_mpd_message(): +10: value=%d, cursor=%d\n", djump_status.value,djump_status.cursor);
				//show number on lcd
				mod_sendmsg(MSGTYPE_INFO, "lcdproc refresh");
				return;
			} 
			else if (*c2 == '>')
			{
				if (djump_status.value==0)
					djump_status.value=1;
				djump_status.value*=10;	//for >10
				if (djump_status.value>player_status.playlistLength)
					djump_status.value=0;	//exit input mode
				//show number on lcd
				mod_sendmsg(MSGTYPE_INFO, "lcdproc refresh");
				log_printf(LOG_DEBUG, "mod_mpd_message(): >10: value=%d, cursor=%d\n", djump_status.value,djump_status.cursor);
				return;
			}
			
			int hint=atoi(c2);
			char hc2[8];
			sprintf(hc2,"%d",hint);
			sprintf(hc,"%d",djump_status.value);
			
			if ((int)strlen(hc)>djump_status.cursor)
				hc[djump_status.cursor]=hc2[0];	
			if (hint==0&&djump_status.cursor==0)	//0 at first
			{
				if (atoi(hc)==0&&strlen(hc)>1)
					hc[1]='1';
				djump_status.cursor--;				
			}
			djump_status.value=atoi(hc);
			djump_status.cursor++;
			
			if (djump_status.value>player_status.playlistLength||djump_status.value<=0)
			{
				djump_status.value=0;	//exit input mode
				djump_status.cursor=0;
			}
			sprintf(hc,"%d",djump_status.value);
			log_printf(LOG_DEBUG, "mod_mpd_message(): number# value=%d, cursor=%d\n", djump_status.value,djump_status.cursor);
			
			//input finished, now play
			if (djump_status.cursor>=(int)strlen(hc))
			{
				mpd_sendPlayCommand(conn,djump_status.value-1);
				djump_status.value=0;	//exit input mode
				djump_status.cursor=0;
				mod_sendmsg(MSGTYPE_INFO, "lcdproc refresh");
			}
			else
				mod_sendmsg(MSGTYPE_INFO, "lcdproc refresh");
		}
		else if (!strcasecmp(c1,"volume"))
		{
			c2 =  strtok(NULL, " \t") ;
			if(!c2) return;
			mpd_sendVolumeCommand(conn,atoi(c2));
			mod_mpd_print_error();
			mpd_finishCommand(conn);
		}
		else if (!strcasecmp(c1,"repeat"))
		{
			mpd_sendRepeatCommand(conn,!player_status.repeat);
			mod_mpd_print_error();
			mpd_finishCommand(conn);
		}
		else if (!strcasecmp(c1,"random"))
		{
			mpd_sendRandomCommand(conn,!player_status.random);
			mod_mpd_print_error();
			mpd_finishCommand(conn);
		}
		else if (!strcasecmp(c1,"delcurrsong"))
		{
			mod_mpd_clearcurrsong();
		}
		else if (!strcasecmp(c1,"pl_clear"))	//former browser clear
		{
			mod_mpd_pl_clear();
		}


	} 

	
	// handle player messages
	else if (msgtype == MSGTYPE_PLAYER) {
		c1 =  strtok(msg, " \t") ;
		if(!c1) return;
		if ( !strcasecmp(c1, "endofsong")) {
			mod_sendmsgf(MSGTYPE_EVENT,"endofsong");
			//	mod_player_status = PLAYER_STATUS_STOP;
		} else if (!strcasecmp(c1, "error")) {
			//launch_song();
			;
		} else if(!strcasecmp(c1, "playing")) {
		/*		mod_sendmsgf(MSGTYPE_EVENT,"play %s %s",mod_player_player->msg, mod_player_song);
		//	mod_player_status = PLAYER_STATUS_PLAY;
		// we've start playing, don't start the next one even in case of error
		// this is not NULL, or the message wouldn't be generated
		free_message(mod_player_player->next);
		mod_player_player->next = NULL;
		free_message(mod_player_player_extra);
		mod_player_player_extra = NULL;
		if(mod_player_type) {
		free_message(mod_player_type->next);
			mod_player_type->next = NULL;*/
		}
		
	} else if (msgtype == MSGTYPE_QUERY) 
	{
		c1 =  strtok(msg, " \t") ;
		if(!c1) return;
		if ( !strcasecmp(c1, "status"))
		{
			switch (player_status.state) {
			case MPD_STATUS_STATE_STOP:
				mod_sendmsgf(MSGTYPE_INFO,"stop");
				break;
			case MPD_STATUS_STATE_PLAY:
				//Käse:
				mod_sendmsgf(MSGTYPE_INFO,"play %s %i", "mpc",player_status.song);
				break;
			case MPD_STATUS_STATE_PAUSE:
				mod_sendmsgf(MSGTYPE_INFO,"pause");
				break;
			}
			
		}
	}
}


void mod_mpd_clearcurrsong()
{
	mpd_sendDeleteIdCommand(conn, player_song.id);
	mod_mpd_print_error();
	mpd_finishCommand(conn);
}

void mod_mpd_pl_clear()
{
	mpd_sendClearCommand(conn);
	mod_mpd_print_error();
	mpd_finishCommand(conn);
}

/*************************************************************************
 * POLL INPUT DATA
 */
void mod_mpd_poll (int __attribute__((unused)) fd)
{
	char *code, *command;
	int rc;

/*	if (!lirc_nextcode(&code)) {
		while ((rc = lirc_code2char(mod_lirc_irconfig, code, &command)) == 0 && command) {
			log_printf(LOG_NOISYDEBUG, "mod_lirc_poll(): got command '%s'\n", command);
			mod_sendmsg(MSGTYPE_INPUT, command);
		}
		free(code);*/
/*
	char *ir, *command;

	if ((ir = lirc_nextir()) != NULL) {
		while ((command = lirc_ir2char(mod_lirc_irconfig, ir)) != NULL) {
			log_printf(LOG_NOISYDEBUG, "mod_lirc_poll(): got command '%s'\n", command);
			mod_sendmsg(MSGTYPE_INPUT, command);
		}
		free(ir);
		
#endif	} else {	// lirc has died
			log_printf(LOG_NORMAL, "mod_mpd_poll(): MPD read returned error .\n");
			FD_ZERO(&mod_mpd_fdset);
	}*/
}


/*************************************************************************
 * MODULE INIT FUNCTION
 */
char *mod_mpd_init (void)
{
	log_printf(LOG_DEBUG, "mod_mpd_init(): initializing\n");

	int	mod_mpd_fd=0;
	FD_ZERO(&mod_mpd_fdset);

	char mpd_host[100];
	strcpy(mpd_host,config_getstr("mpd_host","localhost"));
	int mpd_port=config_getnum("mpd_port", 6600);


//	mpd_Connection * conn;	//now global

	conn = mpd_newConnection(mpd_host,mpd_port,10);
	if(conn->error) {
		log_printf(LOG_ERROR, "mod_mpd_init(): MPD connection failed on host %s port %i!\n", mpd_host,mpd_port);
		log_printf(LOG_ERROR, "\tMPD client error message: %s\n",conn->errorStr);
		mpd_closeConnection(conn);
		return conn->errorStr;
	}

	//yeah now we're connected
	mod_mpd_fd=conn->sock;
	log_printf(LOG_DEBUG, "mod_mpd_init(): connected to MPD: host %s port %i; socket %i!\n",mpd_host,mpd_port,mod_mpd_fd);
	log_printf(LOG_DEBUG, "	MPD Version %i.%i.%i\n",conn->version[0],conn->version[1],conn->version[2]);

	// register our module
	FD_SET(mod_mpd_fd,&mod_mpd_fdset);
	mod_register(&mod_mpd);


	mod_mpd_update_status();
	mod_mpd_update_songinfo();

	return NULL;
}


/*************************************************************************
 * MODULE DEINIT FUNCTION
 */
void mod_mpd_deinit (void)
{
	// free mpd config
//	mpd_freeconfig(mod_mpd_config);

	// disconnect from mpd
	mpd_closeConnection(conn);

//	mpd_freeSong(player_song);
	log_printf(LOG_DEBUG, "mod_mpd_deinit(): closed MPD connection\n");
	FD_ZERO(&mod_mpd_fdset);
}


/*************************************************************************
 * MODULE RELOAD FUNCTION
 */
char *mod_mpd_reload (void)
{
	log_printf(LOG_DEBUG, "mod_mpd_reload(): reloading MPD config\n");

	// free mpd config
//	mpd_freeconfig(mod_mpd_config);

	// read mpd config
//	mpd_readconfig(config_getstr("mpd_config", NULL), &mod_mpd_config, NULL);

	return NULL;
}


/*************************************************************************
 * EOF
 */
