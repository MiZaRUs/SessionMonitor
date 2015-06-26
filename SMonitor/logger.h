/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#ifndef _SM_logger_
#define _SM_logger_

// =========================================================================
//  ------------------------------------------------------------------------
void prn_log( char *m );
void prn_err( char *m, int i );
int open_log( char *path );
void close_log( void );
//  ------------------------------------------------------------------------
// =========================================================================
#endif