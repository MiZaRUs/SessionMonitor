/***********************************************************************
 *  SMonitor                                                           *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#ifndef _SM_ipc_
#define _SM_ipc_
// ==========================================================================
void signal_maintwnance( int s );
void signal_x( int cmd, int t );
int wait_init_ipc( void );
int init_ipc( char *pname );
void close_ipc( void );
// ==========================================================================
#endif
