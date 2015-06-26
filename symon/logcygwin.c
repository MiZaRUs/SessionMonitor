/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>

//#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
//#include <errno.h>
//  --
#include "defs.h"
#include "logger.h"
#include "config.h"
//  --
// =========================================================================
//extern int DEBUG;
static int iCygWinLogThread = 0;
//  -------------------------------------------------------------------------
static void pars_str_log( char *slog ){
    unsigned int sl = strlen( slog );
    unsigned int i = 0, ln = 0;
    for(; i <  sl; i++ ){
        if( slog[i] == ':' ) ln++;
        if( ln == 2 ) break;
    }
    i+=2;
    sprintf( slog, "CYGWIN: %s", slog + i );
//  --
    if( !strncmp( slog + 8, "Did not receive identification string from", 42 )) return;	// пинги
    if( !strncmp( slog + 8, "Accepted publickey for", 22 )){		// если сходится
        sprintf( slog, "CYGWIN: ssh connected %s", slog + 31 );
        ln = 0;
        for(i = 22; i < strlen( slog ); i++, ln++ ){
            if( slog[ i ] == ' ') break;
        }
        sprintf( slog + i, " %s", slog + i + ln+ 3 );

        for(i = i + ln + 4; i < strlen( slog ); i++ ){
            if( slog[ i ] == ' ') break;
            if( slog[ i ] == ':') break;
        }
        slog[ i ] = 0;
        prn_log( slog );
        return;
    }
    if( !strncmp( slog + 8, "Received disconnect from", 24 )){		// если сходится
        sprintf( slog, "CYGWIN: ssh disconnect %s", slog + 33 );
        for(i = 34; i < strlen( slog ); i++ ){
            if( slog[ i ] == ':') break;
            if( slog[ i ] == ' ') break;
        }
        slog[ i ] = 0;
        prn_log( slog );
        return;
    }
    prn_log( slog );
}
//  -------------------------------------------------------------------------
DWORD WINAPI CygWinLogThread( LPVOID lpPar ){
    CONFIG *cnf = lpPar;
    if( strcasecmp( cnf->cygwin, "0" ) == 0 ){
        prn_log( "CygWinLog   Disabled." );
        ExitThread(0);
    }
//  --
    unlink( cnf->cygwin );
    static const int bufsize=2048;
    char buf[bufsize];
//  --
//    static struct sockaddr srvr_name;
    static struct sockaddr_un srvr_name;
    int sock_log = socket(AF_UNIX, SOCK_DGRAM, 0);
    if( sock_log < 0 ){
        prn_err( "open_cygwin_log_socket: socket failed.", GetLastError());
        ExitThread(0);
    }
//  --
    srvr_name.sun_family = AF_UNIX;
    strcpy( srvr_name.sun_path, cnf->cygwin );
//debug( 0, srvr_name.sun_path );
//  --
    if( bind( sock_log, (struct sockaddr*)&srvr_name, strlen( srvr_name.sun_path ) + sizeof( srvr_name.sun_family )) < 0 ){
        prn_err( "open_cygwin_log_socket: bind failed.", GetLastError());
        sock_log = -1;
    }else{
        sprintf( buf, "CygWin socket-log \"%s\" is created.", srvr_name.sun_path );
        prn_log( buf );

        iCygWinLogThread = 1;
        while( iCygWinLogThread ){
            memset( buf, 0, bufsize );
            int bytes = read( sock_log, buf, bufsize - 1 );
            if( bytes > 1 ) pars_str_log( buf );		//распарсить !!!
        }
    }
//  --
    debug( 0, "CygWinLogThread Exit." );
    close( sock_log );
    unlink( cnf->cygwin );
    iCygWinLogThread = 0;
ExitThread(0);
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
