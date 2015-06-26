/***********************************************************************
 *  SMonitir                                                           *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>
//#include <synchapi.h>
//  --
#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <string.h>
//  --
#include "defs.h"
#include "ipc.h"
#include "logger.h"
//  --------------------------
extern int DEBUG;
//  --------------------------
static volatile int status = 0;
static char *pipename = NULL;
static volatile HANDLE hPipeThread = NULL;
static char bufi[ PIPE_SIZE ];
static char bufo[ PIPE_SIZE ];
static char strlog[80];
// ==========================================================================
void send_signal( char *spar ){		// желательна синхронизация
    if(( status == 0 )||( strlen( spar ) >= 80 )) return;
//  --
    strcpy( strlog, spar );
    status = 101;		// передать
    if( DEBUG ) prn_log( strlog );
}
//  -------------------------------------------------------------------------
void init_message( void ){
    if( status == 0 ){
        sprintf( bufi, "Begin" );
    }else if( status == 1 ){
        sprintf( bufi, "GetConf" );
    }else if( status == 101 ){
        sprintf( bufi, strlog );
    }else{
        sprintf( bufi, "Ok.." );
    }
}
//  -------------------------------------------------------------------------
int pars_message( int lenr ){
    int rez = 1;
    if( lenr < 4 ) return 0;
//  --
    if( strncmp( bufo, "Ok..", 4 )) prn_log( bufo );	// не равно "Ok.." - логируем
//  --		"Session ..."
    if( !strncmp( bufo+8, "CloseApp", 8 )){		// Комманда на закрытие приложения
        exit_gwindow();
        rez = 0;
    }else if( !strncmp(( bufo+8 ), "Entered", 7 )){	// Session Entered
        show_gwindow();
    }else if( !strncmp(( bufo ), "Command", 7 )){
        if( bufo[ 9  ] == '0' ) set_no_verbose();
        if( bufo[ 11 ] == '1' ) set_maintenance();
//        if( bufo[ 13 ] == '1' ) set_???();
    }
//  --
    if( status == 101 ) status = 10;
    if( status < 10 ) status++;
return rez;
}
//  -------------------------------------------------------------------------

//  -------------------------------------------------------------------------
DWORD WINAPI PipeThread( LPVOID lpParam ){
    status = 0;
//  --
//prn_log("Start PipeThread");
    DWORD buf_r;  // Количество байт, принятых через канал
    while( 1 ){
        ZeroMemory( &bufi, PIPE_SIZE );
        ZeroMemory( &bufo, PIPE_SIZE );
        init_message();
//  --
        if( !CallNamedPipe( pipename, bufi, ( strlen( bufi ) + 1 ), bufo, PIPE_SIZE, &buf_r, 4 )){
            prn_err( "CallNamedPipe - failed", GetLastError());
            break; // или еще 3 попытки через sleep(1) и убиться
        }
//  --
        if( pars_message( buf_r ) == 0 ) break;
        sleep(1);
    }// while
//  --
    status = 0;
    exit_gwindow();
    ExitThread( 0 );
}
//  -------------------------------------------------------------------------
int init_ipc( char *pname ){
    sprintf( bufi, "init_IPC(%s);", pname );
    prn_log( bufi );
//  --
    int ix = strlen( pname );
    pipename = malloc( ix );
    if( pipename ) strcpy( pipename, pname);
    else{
        prn_err( "init_IPC-mem - failed", GetLastError());
        return 0;
    }
//  --
    hPipeThread = CreateThread(NULL, 0, &PipeThread, NULL, 0, NULL );
    if( hPipeThread == NULL ){
        prn_err( "Create PipeThread - failed", GetLastError());
        return 0;
    }
//  --
return 1;
}
//  -------------------------------------------------------------------------
int wait_init_ipc( void ){
int i = 0;
int rez = 0;
    for(; i < 20; i++ ){
        if( status > 1 ){
            rez = 1;
            break;
        }
        usleep(200000);
    }
return rez;
}
//  -------------------------------------------------------------------------
void close_ipc( void ){
    if(DEBUG) prn_log("close_IPC");
    status = 0;
    if( hPipeThread ) CloseHandle( hPipeThread );
    if( pipename ) free( pipename );
}
// ==========================================================================
