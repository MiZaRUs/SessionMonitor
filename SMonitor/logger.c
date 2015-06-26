/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>
#include <synchapi.h>
//  --
#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <string.h>

#include "defs.h"
#include "logger.h"

static const DWORD MAX_LEN_LOG = 1000000;	//1M

static int JOB = 0;
//  ---------------------------------
CRITICAL_SECTION log_guard;
//  ---------------------------------
static char *service_logf;
//  ---------------------------------
// =========================================================================
void prn_log( char *m ){
    if( JOB < 1 ) return;
    EnterCriticalSection( &log_guard );
//  --
    char *msg_log = malloc( strlen(m) + 23 ); // + "2014-02-04 23:59:59;";
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf( msg_log, "%d-%02d-%02d %02d:%02d:%02d\t%s\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, m );
//  --
    DWORD dw =0;
    HANDLE hFile = CreateFile( service_logf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( INVALID_HANDLE_VALUE != hFile ){
        dw = SetFilePointer( hFile, 0, 0, FILE_END );
        if( dw >= MAX_LEN_LOG ){
            CloseHandle( hFile );
            dw = strlen( service_logf );
            char *tmpstr = malloc(  dw + 6 );
            sprintf( tmpstr, "%s.old", service_logf );			// переименовать, и создать новый !!!
            MoveFileEx( service_logf, tmpstr, MOVEFILE_REPLACE_EXISTING );
            if( tmpstr ) free( tmpstr );
            hFile = CreateFile( service_logf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
        }
    }
    if( hFile ){
        WriteFile( hFile, msg_log, strlen( msg_log ), &dw, NULL );
        CloseHandle( hFile );
    }
//  --
    if( msg_log ) free( msg_log );
    LeaveCriticalSection( &log_guard );
}
//  ------------------------------------------------------------------------
void prn_err( char *m, int i ){
    char msg[MSG_SIZE];
    sprintf( msg, "ERROR - %u : %s", i, m );
    prn_log( msg );
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
int open_log( char *spath ){
    if( !( service_logf = malloc( strlen( spath ) + 2 ))) return 0;
    strcpy( service_logf, spath );
    JOB = 1;
//  --
    InitializeCriticalSection( &log_guard );
    if(!InitializeCriticalSectionAndSpinCount( &log_guard, 0x00000400 )){
        prn_err( "InitializeCriticalSection Log failed.", GetLastError() );
        return 0;
    }
//  --
    JOB = 2;
    prn_log( "." );
return 1;
}
//  ------------------------------------------------------------------------
void close_log( void ){
    if( JOB == 0 ) return;
    prn_log("End!");
    if( service_logf ) free( service_logf );
    DeleteCriticalSection( &log_guard );
    JOB = 0;
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
