/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#ifndef _SM_config_
#define _SM_config_

typedef struct CONFIGSTRUCT{
    char log_debug[8];
    char zbx_host[32];
    char zbx_server[26];
    char zbx_port[8];
    char cygwin[128];
    char git_exec[128];
    char user_proc[32];
    char maintenance[8];
}CONFIG;

// =========================================================================
int init_conf( char *fpath, char *fname, CONFIG *res );				//
// =========================================================================
#endif