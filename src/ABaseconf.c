/*************************************************************************
 * 
 * ABase - Multimedia Audio Jukebox for Linux
 * http://ABase.sourceforge.net
 *
 * $Source: /cvsroot/ABase/ABase/src/ABaseconf/ABaseconf.c,v $ -- config file generator
 * $Id: ABaseconf.c,v 1.16 2003/11/12 17:51:41 boucman Exp $
 *
 * Copyright (C) by Volker Wegert
 *
 * Please contact the current maintainer, Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
 * for information and support regarding ABase.
 *
 *
 */
 
#include <unistd.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <argp.h>
#include <string.h>
#include <stdlib.h>
#include <fnmatch.h>

#include "config.h"
#include "ABaseconfig.h"
#include "ABaselog.h"
 
/****** Default parameters *******/
char* used_input = CONFFILE;
char* used_output = NULL;

int  verbosity = LOG_NORMAL;
/**********************************/

const char *argp_program_version     = "ABaseconf 0.5.5";
const char *argp_program_bug_address = "Jeremy rosen <jeremy.rosen@enst-bretagne.fr>";
static char doc[] = 
"ABaseconf -- a command line client to generate and update ABased configuration files \
\v \
This program will generate a correct ABased.conf file \
using correct defaults as much as possible, \
and reading from an existing config file when available";

static char args_doc[]= "";

static struct argp_option options[] = {
        {"config",'c',"CONFIG_FILE",0,"The ABase config file from which default values will be read\n defaults to the default config file.",0},
	{"file",'f',"FILENAME",0,"file to write to.\nIf this option is not provided, will write to the default config file",0},
	{"verbose",'v',0,0,"More verbose output. Can be specified multiple times",0},
	{0,0,0,0,NULL,0}};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static struct argp our_argp =  { options,parse_opt,args_doc,doc,NULL,NULL,0};


static error_t parse_opt (int key, char *arg,  __attribute__((unused)) struct argp_state *state) 
{
	switch(key) {
		case 'c':
			used_input = strdup(arg);
			return 0;
		case 'v':
			verbosity++;
			return 0;
		case 'f':
			used_output =strdup(arg);
			return 0;
		default:
			return ARGP_ERR_UNKNOWN;
	}
}	
/*************************************************************************
 * GLOBALS
 */
#ifndef VERSION
#error "VERSION undefined - Use Makefile to compile!"
#endif
FILE *conffile = NULL;

/*************************************************************************
 *
 * check file existence
 *
 */
int file_exists(char *filename) {
  struct stat st;
  return (stat(filename, &st) >= 0);
}


/*************************************************************************
 *
 * various output functions
 *
 */
void confwrite(char *s) {
  fprintf(conffile, "%s\n", s);
}
void line_hash() {
  confwrite("##########################################################################");
}
void line_dash() {
  confwrite("##----------------------------------------------------------------------##");
}
void simple_comment(char *s) {
  confwrite("");
  confwrite("#");
  fprintf(conffile, "# %s\n", s);
  confwrite("#");
}
void write_pair(char* name, char* value) {
  fprintf(conffile, "%s: %s\n", name, value);
}
void smart_remove(char* key) {
	config_remove(key);
	if(config_remove(key))
		log_printf(LOG_ERROR,"Key %s defined multiple time in source config, keeping only the last one\n");
}
void config_value(char *name, char *value) {
	write_pair(name,config_getstr(name,value));
	smart_remove(name);
	
}

void player_default(char* type, char*player)
{
	char tmp[256],*tmp2;
	sprintf(tmp, "Should %s be the default player for %s files",player,type);
	simple_comment(tmp);
	sprintf(tmp,"playtype_%s",type);
	tmp2 = config_getstr("tmp",NULL);
	if (tmp2 && !strcmp(tmp2,player)) {
		config_value(tmp, player);
	} else {
		sprintf(tmp,"playtype_%s: %s",type,player);
		simple_comment(tmp);

	}
}
/*************************************************************************
 *
 * config file header
 *
 */
void write_header() {
  line_hash();
  confwrite("##");
  confwrite("## ABased.conf -- Sample ABased configuration file");
  confwrite("##");
  confwrite("## This file has been generated by ABaseconf. Please see the ABased");
  confwrite("## documentation for more information or visit the ABase web site at");
  confwrite("## http://ABase.sourceforge.net");
  confwrite("##");
  confwrite("## config file generator (C) by Volker Wegert <vwegert@web.de>");
  confwrite("##");
  line_hash();
  confwrite("");
}

/*************************************************************************
 *
 * generic module header
 *
 */
void module_header(char *name, char *description) {
  log_printf(LOG_NORMAL,"Generating configuration for %s...\n", name);
  confwrite("");
  line_dash();
  confwrite("##");
  fprintf(conffile, "## configuration options for %s (%s)\n", name, description);
  confwrite("##");
}

/*************************************************************************
 *
 * general default configuration
 *
 */
void write_general() {
	char * tmp;
	module_header("ABased", "general options");

	simple_comment("version of config file generator");
	write_pair("ABaseconf_version", VERSION);
	smart_remove("ABaseconf_version");

	simple_comment("format for date/time in log file output");
	write_pair("log_timestr",config_getstr("log_timestr","%X"));

	simple_comment("commands to execute on startup");
	config_value("ABase_init_command", "");

	simple_comment("define this if you want ABased to execute a command upon exiting");
	tmp = config_getstr("exec_on_close",NULL);
	if (tmp) {
		config_value("exec_on_close", tmp);
	} else {
		simple_comment("exec_on_close: cat /dev/null");

	}
}



/*************************************************************************
 *
 * default configuration for mod_ogg123
 *
 */
void write_mod_ogg123() {
#ifdef MOD_OGG123
	module_header("mod_ogg123", "ogg123 interaction");

	simple_comment("location of the ogg123 binary");
	config_value("ogg123_binary", OGG123_PATH);

	simple_comment("additional ogg123 parameters (see ogg123 --help for a list of supported parameters)");
	config_value("ogg123_param", "");

	simple_comment("Should ABase halt playback when ogg123 reports unknown status?");
	config_value("abort_on_unknown_ogg123_status", "no");
	player_default("ogg","mod_ogg123");


#endif
}



/*************************************************************************
 *
 * default configuration for mod_mpg123
 *
 */
void write_mod_mpg123() {
#ifdef MOD_MPG123
	module_header("mod_mpg123", "mpg123 interaction");

	simple_comment("location of the mpg123 binary");
	config_value("mpg123_binary", MPG123_PATH);

	simple_comment("additional mpg123 parameters (see mpg123 --help for a list of supported parameters)");
	config_value("mpg123_param", "");

	simple_comment("Should ABase halt playback when mpg123 reports unknown status?");
	config_value("abort_on_unknown_mpg123_status", "no");
	player_default("mpg","mod_mpg123");
	player_default("mp2","mod_mpg123");
	player_default("mp3","mod_mpg123");


#endif
}

/*************************************************************************
 *
 * default configuration for mod_timidity
 *
 */
void write_mod_timidity() {
#ifdef MOD_TIMIDITY
	module_header("mod_timidity", "timidity interaction");

	simple_comment("location of the timidity binary");
	config_value("timidity_binary", TIMIDITY_PATH);

	simple_comment("additional timidity parameters (see timidity -h for a list of supported parameters)");
	config_value("timidity_param", "");

	simple_comment("Should ABase halt playback when timidity reports unknown status?");
	config_value("abort_on_unknown_timidity_status", "no");
	player_default("midi","mod_timidity");
	player_default("recompose","mod_timidity");
	player_default("mod","mod_timidity");
	player_default("kar","mod_timidity");

#endif
}
/*************************************************************************
 *
 * default configuration for mod_lcdproc
 *
 */
void write_mod_lcdproc() {
#ifdef MOD_LCDPROC
	char *keylist;
	char*key;
	char tmp[256];
	int max_default = 11;
	int index =0;
	char *defaults[] = { 
		"mixer volume +2",
		"mixer volume -2",
		"playlist load /home/MP3/cd4.m3u",
		"playlist load /home/MP3/cd5.m3u",
		"playlist load /home/MP3/cd6.m3u",
		"playlist jump +1",
		"playlist jump -1",
		"pause",
		"playlist repeat",
		"stop",
		"playlist shuffle",
		};
	module_header("mod_lcdproc", "LCDproc output");

	simple_comment("hostname and port for LCDproc server connection");
	config_value("lcdproc_host", "localhost");
	config_value("lcdproc_port", "13666");

	simple_comment("minutes of idle time until the screen shows a clock");
	config_value("lcdproc_clock_timer", "1");

	simple_comment("key bindings");
	confwrite("# You must list the keys you wish ABase to receive from lcdproc here.");
	confwrite("# Use a comma-delimited format, no spaces!");
	confwrite("# If you do not list a key here, ABase will not receive that key at all!");
	confwrite("#");
	keylist = strdup(config_getstr("lcdproc_keylist","e,f,g,h,i,j,k,p,r,s,t"));
	config_value("lcdproc_keylist", "e,f,g,h,i,j,k,p,r,s,t");
	for(key = strtok(keylist,",") ;key;key = strtok(NULL,",")) {
		sprintf(tmp,"lcdproc_key_%s",key);
		config_value(tmp,index < max_default?defaults[index]:"");
		index++;
	}
	free(keylist);
	

	simple_comment("file browser scrollbar emulation [yes|no]");
	config_value("lcdproc_browser_scrollbar", "yes");

	simple_comment("file browser display timeout in seconds (default 10)");
	config_value("lcdproc_browser_timeout", "10");

	simple_comment("preserve upper right corner of display");
	confwrite("# If this is set to yes, ABased will try not to display a character");
	confwrite("# at the upper right corner of the display as not to disturbe the");
	confwrite("# LCDproc heartbeat");
	confwrite("#");
	config_value("lcdproc_preserve_upper_right", "no");

	simple_comment("time and date formats - see 'man strftime'");
	config_value("lcdproc_timestr", "%R");
	config_value("lcdproc_datestr", "%x");

	simple_comment("configure status,title,artist & time lines");
	config_value("lcdproc_title_line", "2");
	config_value("lcdproc_artist_line", "1");
	config_value("lcdproc_status_line", "3");
    config_value("lcdproc_time_line", "4");	
	
	simple_comment("control whether hours are displayed separately");
	config_value("lcdproc_songtime_hours", "no");

	simple_comment("alignment of status indicators (right or left)");
	config_value("lcdproc_status_alignment", "right");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_lirc
 *
 */
void write_mod_lirc() {
#ifdef MOD_LIRC
  module_header("mod_lirc", "LIRC input");

  simple_comment("name and path of LIRC configuration file");
  config_value("lirc_config", "");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_playlist
 *
 */
void write_mod_playlist() {
#ifdef MOD_PLAYLIST
  module_header("mod_playlist", "playlist handling");

  simple_comment("skipback timeout (seconds until a 'last song' command translates to 'restart this song'");
  config_value("playlist_skip_back_timeout", "2");

  simple_comment("default playlist repeat mode [off|current|all]");
  config_value("playlist_repeat", "all");
  
  simple_comment("default playlist shuffle mode [yes|no]");
  config_value("playlist_shuffle", "no");

  simple_comment("playlist sorting mode [auto|manual]");
  config_value("playlist_sort_mode", "manual");

  simple_comment("on shutdown the playlist will be saved to the playlistpath as:");
  config_value("playlist_saveas_on_shutdown","lastpl.m3u");

  simple_comment("playlist that should be load at startup (relative to PL-Path)");
  config_value("playlist_load_at_startup","lastpl.m3u");

  confwrite("#\n");
  confwrite("# yes: use yes if the songtype can allways be recognized by the filename's extension\n");
  confwrite("# no: each module tries to find out what songtype a file is\n");
  confwrite("#\n");
  config_value("only_songtypeguess","no");
#endif
}

/*************************************************************************
 *
 * default configuration for mod_playlistlist
 *
 */
void write_mod_playlistlist() {
#ifdef MOD_PLAYLISTLIST
  module_header("mod_playlistlist", "support for lists of playlists");
#endif
}

/*************************************************************************
 *
 * default configuration for mod_alarm
 *
 */
void write_mod_alarm() {
#ifdef MOD_ALARM
  module_header("mod_alarm", "signal handling facilities");

  simple_comment("alarm environments");
  confwrite("# If you use mod_alarm, you'll need an environment_alarm1 and/or environment_alarm2");
  confwrite("# environment_alarm1 will be executed when ABase receives a SIGUSR1 signal.");
  confwrite("# environment_alarm2 will be executed when ABase receives a SIGUSR2 signal.");
  confwrite("#");
  config_value("environment_alarm1", "mixer volume 55;playlist clear;play /home/mp3/alarm/wind_chimes.mp3");
  config_value("environment_alarm2", "mixer volume 55;beep disable;playlist shuffle 1;sleep 10 0;beep enable;playlist load /home/mp3/alarm/Vivaldi.pl");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_autoshutdown
 *
 */
void write_mod_autoshutdown() {
#ifdef MOD_AUTOSHUTDOWN
  module_header("mod_autoshutdown", "");

  simple_comment("minutes of inactivity before ABased automatically shuts down (0 to disable)");
  config_value("autoshutdown_time", "0");


#endif
}

/*************************************************************************
 *
 * default configuration for mod_beep
 *
 */
void write_mod_beep() {
#ifdef MOD_BEEP
  module_header("mod_beep", "");

  simple_comment("location of soundfiles (see contrib/beep)");
  config_value("beep_on", "/usr/local/share/ABase/beep/on.au");
  config_value("beep_off", "/usr/local/share/ABase/beep/off.au");

  simple_comment("dsp device to use for beep playback"); 
  config_value("beep_device", "/dev/dsp1");

  simple_comment("command to play beeps (%f -> filename, %d -> device)");
  config_value("beep_play", "/usr/bin/sox %f -t ossdsp -w -s %d");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_browser
 *
 */
void write_mod_browser() {
#ifdef MOD_BROWSER
  module_header("mod_browser", "file and directory browser");


  simple_comment("[yes/no] if set to yes the browser accepts browser-message only after");
  confwrite("# being activated by sending browser activate (so the browser-keys can be"); 
  confwrite("# used for anything else");
  config_value("browser_activate_first","no");
  
  simple_comment("your music collection's absolute root directory");
  config_value("browser_root", "/");

  simple_comment("startup browser path (relative to 'browser_root')");
  config_value("browser_home", "/");

  simple_comment("path of your playlists (relative to 'browser_root')");
  config_value("browser_playlist","M3Us");

  simple_comment("path where you can store shell scripts (relative to 'browser_root')");
  confwrite("# Disable shell script execution with: 'browser_exec: -'");
  confwrite("#");
  config_value("browser_exec", "-");

  simple_comment("file extension to be recognized as audio/mp3 file (default=*.[Mm][Pp]3)");
  config_value("browser_mask_audio", "*.[Mm][Pp]3");
  
  simple_comment("file extension to be recognized as playlist (default=*.[Mm]3[Uu])");
  config_value("browser_mask_list", "*.[Mm]3[Uu]");
  
  simple_comment("file extension to be recognized as playlist-list (default=*.[Mm]3[Ll])");
  config_value("browser_mask_listlist", "*.[Mm]3[Ll]");

  simple_comment("file extension to be recognized as executable (default=*.[Ss][Hh])");
  config_value("browser_mask_exec", "*.[Ss][Hh]");

  simple_comment("files that should be NOT displayed");
  confwrite("# '*' means all remaining files will be not shown");
  confwrite("# '*.count' means only '*.count' files will be excluded, all other remaining");
  confwrite("# files will be displayed");
  confwrite("#");
  config_value("browser_mask_hide", ".*");

  simple_comment("perform file matching case sensitive?");
  config_value("browser_mask_casesensitive", "yes");

  simple_comment("filename where shell scripts can store temporary display output");
  confwrite("# The 1st line of this file will be displayed by ABase on lcd after the script returns.");
  confwrite("# Shell scripts will get this filename as 1st argument.");
  confwrite("#");
  config_value("browser_execmsg", "/tmp/ABase.execmsg.tmp");

  simple_comment("path depth you want to be shown as parent working directory");
  confwrite("# If your pwd is '/home/mp3/music/albums/gathering', setting 'browser_pwddepth'");
  confwrite("# to 2 results in 'albums/gathering' to be displayed as pwd");
  confwrite("#");
  config_value("browser_pwddepth", "2");

  simple_comment("show '..' directory?");
  confwrite("# Using this feature you dont need the 'browser back' command. Simply do");
  confwrite("# a 'browser into' on the '..' directory in order to do a 'cd ..'");
  confwrite("#");
  config_value("browser_cdup", "no");

  simple_comment("stay on '..' after entering directory?");
  confwrite("# Set this to 'yes' if you'd like mod_browser to stay on the '..' entry");
  confwrite("# after cd-ing into a directory. Defaults to 'no' which makes mod_browser");
  confwrite("# advance to the first file inside the directory (if any).");
  confwrite("#");
  config_value("browser_cdup_initial_updir", "no");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_cd
 *
 */
void write_mod_cd() {
#ifdef MOD_CD
	int num_cd_dev = config_getnum("num_cd_device",1);
	int i;
	char tmp[100],tmp3[100], tmp4[100];
	module_header("mod_cd", "CD-DA playing functions");

	simple_comment("number of cdrom devices");
	config_value("num_cd_device", "1");

	// old option conversion
	strcpy(tmp,config_getstr("cd_device", "/dev/cdrom"));
	smart_remove("cd_device");
	simple_comment("CD-ROM 0 device ");
	config_value("cd_device_0", tmp);
	
	strcpy(tmp,config_getstr("cd_mountpoint", "/mnt/cdrom"));
	smart_remove("cd_mountpoint");
	simple_comment("CD-ROM 0 mount point");
	config_value("cd_mountpoint_0", tmp);


	for(i = 1 ; i < num_cd_dev ; i++) {
		sprintf(tmp,"CD-ROM %d device",i);
		sprintf(tmp3,"/dev/cdroms/cdrom%d",i);
		sprintf(tmp4,"cd_device_%d",i);
		simple_comment(tmp);
		config_value(tmp4,tmp3);
		
		sprintf(tmp,"CD-ROM %d mountpoint",i);
		sprintf(tmp3,"/mnt/cdrom%d",i);
		sprintf(tmp4,"cd_mountpoint_%d",i);
		simple_comment(tmp);
		config_value(tmp4,tmp3);
	}
		

	simple_comment("pattern for playlist generation for data CDs");
	strcpy(tmp,config_getstr("cd_scandir_pattern", " "));
	if(tmp[0] == '/') {
		log_printf(LOG_NORMAL,"Warning, the syntax of cd_scandir_pattern has changed\n");
		log_printf(LOG_NORMAL,"the mountpoint should not be included, only the search pattern\n");
	}
	config_value("cd_scandir_pattern", "*.[Mm][Pp]3");

	simple_comment("use external mount command? (NOTE: ABased needs root privileges if set to 'no'!)");
	config_value("cd_externmount", "yes");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_env
 *
 */
void write_mod_env() {
#ifdef MOD_ENV
	config_entry_t *ent;
	int found = 0;
	char *tmp;
	module_header("mod_env", "environments");
	simple_comment("Put your environment entries in this format:");
	confwrite("# environment_<env name>: <ABase command>;<ABase command>;<ABase command>");
	confwrite("#");
	ent=config_list;
	while(ent) {
		if(!fnmatch("environment_*",ent->key,0)) {
			tmp= ent->key;
			ent = ent->next;
			config_value(tmp,"");
			found = 1;
		} else {
			ent =ent->next;
		}
	}
	if(!found) {
		config_value("environment_bedtime", "beep disable;mixer volume 20;sleep 45 3;playlist shuffle 1;playlist load /home/mp3/bed1.pl;beep enable");
		config_value("environment_party", "mixer volume 85;playlist shuffle 1;playlist loaddir /home/mp3/funky/*/*.mp3");
	}

#endif
}

/*************************************************************************
 *
 * default configuration for mod_idle
 *
 */
void write_mod_idle() {
#ifdef MOD_IDLE
  module_header("mod_idle", "");

  simple_comment("interval (seconds) between idle checks/announcements ");
  config_value("idle_interval", "60");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_mixer
 *
 */
void write_mod_mixer() {
#ifdef MOD_MIXER
  module_header("mod_mixer", "");

  simple_comment("the mixer device ABase uses");
  config_value("mixer_device", "/dev/mixer");

  simple_comment("the mixer's startup values (set when ABased starts or when the 'mixer default' command is received");
  config_value("mixer_volume", "-1");
  config_value("mixer_balance", "0");
  config_value("mixer_bass", "-1");
  config_value("mixer_treble", "-1");

  simple_comment("mixer channel used to manipulate volume");
  config_value("mixer_vol_channel", "vol");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_netctl
 *
 */
void write_mod_netctl() {
#ifdef MOD_NETCTL
  module_header("mod_netctl", "network control facility");
  
  simple_comment("enable network client support?");
  config_value("netctl_enabled", "yes");

  simple_comment("interface (hostname or ip address) for ABased to listen on. Defaults to 'all' interfaces.");
  config_value("netctl_interface", "all");

  simple_comment("port for ABased status monitoring & control");
  config_value("netctl_port", "9232");

  simple_comment("maximum number of clients to allow to connect at a time.");
  config_value("netctl_max_clients", "5");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_timer
 *
 */
void write_mod_timer() {
#ifdef MOD_TIMER
  module_header("mod_timer", "");
#endif
}

/*************************************************************************
 *
 * default configuration for mod_debug
 *
 */
void write_mod_debug() {
#ifdef MOD_DEBUG
  module_header("mod_debug", "optional debugging facility");
#endif
}

/*************************************************************************
 *
 * default configuration for mod_fifo
 *
 */
void write_mod_fifo() {
#ifdef MOD_FIFO
  module_header("mod_fifo", "unix domain control socket");
  
  simple_comment("enable mod_fifo?");
  config_value("fifo_enabled", "no");

  simple_comment("directory where control socket should reside");
  config_value("fifo_directory", "/tmp");

  simple_comment("name of socket file (%%p = PID, %%U = UID)");
  config_value("fifo_name", "ABase-%u-%p.tmp");

  simple_comment("create a new socket (yes) or use existing socket?");
  config_value("fifo_create", "yes");

  simple_comment("remove socket at shutdown?");
  config_value("fifo_remove", "yes");

  simple_comment("permissions of socket (may be specified as 0... or 0x...)");
  config_value("fifo_perms", "0622");

#endif
}

/*************************************************************************
 *
 * default configuration for mod_mplayer
 *
 */
void write_mod_mplayer() {
#ifdef MOD_MPLAYER
	module_header("mod_mplayer", "mplayer interaction");

	simple_comment("location of the mplayer binary");
	config_value("mplayer_binary", MPLAYER_PATH);

	simple_comment("additional mplayer parameters (see mplayer --help for a list of supported parameters)");
	config_value("mplayer_param", "");

	simple_comment("Should ABase halt playback when mplayer reports unknown status?");
	config_value("abort_on_unknown_mplayer_status", "no");
	player_default("mpg","mod_mplayer");
	player_default("mp2","mod_mplayer");
	player_default("mp3","mod_mplayer");
	player_default("asf","mod_mplayer");
	player_default("rm","mod_mplayer");
	player_default("avi","mod_mplayer");
	player_default("http","mod_mplayer");
#endif
}
/*************************************************************************
 *
 * EXTRA THINGS AT END OF CONFIG FILE
 *
 */
void write_footer() {
	log_printf(LOG_NORMAL,"Generating configuration footer...\n");
	confwrite("");
	line_dash();
	confwrite("##");
	fprintf(conffile, "## Configuration file footer\n");
	confwrite("##");
	smart_remove("log_timestr");
	if (config_list) {
		log_printf(LOG_ERROR,"ABaseconf has found unknown options, please check them manually\n");
		simple_comment("Extra options unrecognised by ABaseconf");
	}
	while(config_list) {
		log_printf(LOG_ERROR,"\t%s\n",config_list->key);
		config_value(config_list->key,"");
	}

}
  

/*************************************************************************
 *
 * MAIN
 *
 */
int main (int argc, char *argv[])
{
	argp_parse(&our_argp,argc,argv,0,0,NULL);
	log_open(NULL,verbosity); //open log, with command line verbosity
	if(used_input && file_exists(used_input)) {
			config_read(used_input); 
	} else {
		log_printf(LOG_ERROR,"Source file doesn't exist, using all default parameters\n");
	}
		


	// print banner
	log_printf(LOG_NORMAL,"\n");
	log_printf(LOG_NORMAL,"  config file generator for ABase %s (C) by Volker Wegert <vwegert@web.de>\n", VERSION);
	log_printf(LOG_NORMAL,"  Questions, comments, ideas to current maintainer: Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>\n");
	log_printf(LOG_NORMAL,"\n");

	// open output file
	if(!used_output) {
		used_output = CONFFILE;
	}
	if (file_exists(used_output)) {
		log_printf(LOG_NORMAL,"Target file %s exists, will overwrite it...\n",used_output);
	}
	conffile = fopen(used_output, "w");
	if (!conffile) {
		perror("fopen()");
		log_printf(LOG_ERROR,"Unable to create output file %s!\n\n", used_output);
		return EXIT_FAILURE; 
	}
	setlinebuf(conffile);

	// write config file header
	write_header();

	// write general configuration options
	write_general();

	// write default options for each module
	write_mod_mpg123();
	write_mod_ogg123();
	write_mod_mplayer();
	write_mod_timidity();
	write_mod_lcdproc();
	write_mod_lirc();
	write_mod_playlist();
	write_mod_playlistlist();
	write_mod_alarm();
	write_mod_autoshutdown();
	write_mod_beep();
	write_mod_browser();
	write_mod_cd();
	write_mod_env();
	write_mod_idle();
	write_mod_mixer();
	write_mod_netctl();
	write_mod_timer();
	write_mod_debug();
	write_mod_fifo();

	// write footer
	write_footer();

	// close output file
	fclose(conffile);

	log_printf(LOG_NORMAL,"\n");
	log_printf(LOG_NORMAL,"Done. You may now edit the generated configuration file to suit your needs.\n");
	log_printf(LOG_NORMAL,"\n");
	return 0;
}
