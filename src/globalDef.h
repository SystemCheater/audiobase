///////////////////////////////////////////////////////////////
// Header-file for global definitions
//  e. g. Songinfos, playlist, playerstatus
// Sebastian Bauer, 20.03.2005
///////////////////////////////////////////////////////////////

#ifndef GLOBALDEF_H
#define GLOBALDEF_H

//TODO: this is task of the configure-script!
//used for charset-conversion
#define HAVE_LOCALE 
#define HAVE_LANGINFO_CODESET 
#define HAVE_ICONV 

#include "libmpdclient.h"

#define MOD_BROWSER_MAXENTRY 		1024	// maximum number of entries per directory

#define ELEMENT_STANDARD 		0
#define ELEMENT_RADIO_NOTCHECKED 	1	//not implemented
#define ELEMENT_RADIO_CHECKED 		2		//not implemented
#define ELEMENT_CHECK_NOTCHECKED 	3	//not implemented
#define ELEMENT_CHECK_CHECKED		4		//not implemented
#define ELEMENT_NUMBER_ORIGINAL		5
#define ELEMENT_NUMBER_EDITED 		6
#define ELEMENT_SUBMENU 		7
#define ELEMENT_SUB_HEADLINE 		8
#define ELEMENT_NOT_ACTIVE 		9
#define BROWSER_STATUS_INACTIVE 	0
#define BROWSER_STATUS_ACTIVE 		1

#define BROWSER_INFO_UNDEFINED 		0
#define BROWSER_INFO_DIRECTORY 		1
#define BROWSER_INFO_SONGFILE 		2
#define BROWSER_INFO_PLAYLIST 		3
#define BROWSER_INFO_SCRIPT 		4

#define BROWSER_FIND_INFOPATH 		0
#define BROWSER_FIND_NAME 		1

mpd_Connection *conn;

struct  MPD_PLAYER_STATUS 
{
	int volume;		/* 0-100, or MPD_STATUS_NO_VOLUME when there is no volume support */
	int repeat;		/* 1 if repeat is on, 0 otherwise */
	int random;		/* 1 if random is on, 0 otherwise */
	int playlistLength;	/* playlist length */
	long long playlist;	/* playlist, use this to determine when the playlist has changed */
	int state;		/* use with MPD_STATUS_STATE_* to determine state of player */
	int crossfade;	/* crossfade setting in seconds */
	 /* if a song is currently selected (always the case when state is
	  * PLAY or PAUSE), this is the position of the currently
	  * playing song in the playlist, beginning with 0*/
	int song;
	int songid;			/* Song ID of the currently selected song */
	int elapsedTime;	/* time in seconds that have elapsed in the currently playing/paused song */
	int totalTime;		/* length in seconds of the currently playing/paused song */
	int bitRate;		/* current bit rate in kbs */
	unsigned int sampleRate;/* audio sample rate */
	int bits;			/* audio bits */
	int channels;		/* audio channels */
	int updatingDb;		/* 1 if mpd is updating, 0 otherwise */
	char error[512];	/* error */
} player_status;


struct MPD_PLAYER_SONG
{
	char file[512];		/* filename of song */
	char artist[256];	/* artist, maybe NULL if there is no tag */
	char title[256];	/* title, maybe NULL if there is no tag */
	char album[256];	/* album, maybe NULL if there is no tag */
	char track[256];	/* track, maybe NULL if there is no tag */
	char name[256];		//name, maybe NULL if there is no tag; it's the name of the current song, f.e. the icyName of the stream */
	char fileext[10];
	int time;	/* length of song in seconds, check that it is not MPD_SONG_NO_TIME  */
	int pos;	/* if plchanges/playlistinfo/playlistid used, is the position of the song in the playlist */
	int id;		/* song id for a song in the playlist */
} player_song;


struct DJUMP_STATUS
{
	int cursor;		// the cursor's position
	int value;		// the current value
} djump_status;


struct BROWSER_ELEMENT
{
	char name[128];	  //name showed in Display
	char info_path[512];
	int info_type;
	char status;	  // --> ELEMENT_*
	int number_input; //if element can be used for number input
	int number_upper_limit;	//range for number input
	int number_lower_limit;	
	char suffix[32];  //eg for number input: Volume -10 dB
						//								^^
};

struct BROWSER_PROPERTIES
{
	char viewEleStatus[32];	//cursor doesn't mark the element
	char cursorEleStatus[32]; //cursor marks element
	char HeadLineFixed[128]; //headline that's fixed on the first line
	char currentDir[512];	//current parent dir/level of browser
	int status;		//--> BROWSER_STATUS_*
	struct BROWSER_ELEMENT *pElements[MOD_BROWSER_MAXENTRY];	//array to the elements 
	int DisplayPos;	//first line displayed
	int CursorPos;  //cursor pos in the array - not on the LCD
	int ElementCount; //no of elements
} browser;

int display_hgt;
int display_wid;

#endif
