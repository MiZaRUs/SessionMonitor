/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>
#include <synchapi.h>
//  --
//#include <excpt.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
//  --
#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
//#include <string.h>
//#include <regex.h>

#include "defs.h"
#include "sender.h"
#include "logger.h"
//  ----------------------------------
extern char *service_work;
//extern int DEBUG;
//  ----------------------------------
#define QULABEL 0xFDFCFBFA
#define QUGDATA 16
#define MAX_LEN_MSG 256
//  ----------------------------------
typedef struct SMSGSTRUCT{
    CRITICAL_SECTION queue_guard;
    CRITICAL_SECTION send_guard;
    volatile HANDLE hFile;
    volatile HANDLE hMapFile;
    volatile LPVOID lpMapAddress;
    volatile int  port;
    char server[ 26 ];
    char host[ 26 ];
    char queue_name[ 128 ];
    char buffer[ MAX_LEN_MSG ];
    char inbuff[ MAX_LEN_MSG ];
}SMSG;
//  -------------------------------------------------------------------------
static const unsigned int len_queue = ( MAX_LEN_MSG * 16 ) + QUGDATA;
static SMSG msgs;
unsigned int qu_pop( char *buf );
//  -------------------------------------------------------------------------

// ==========================================================================
int send_socket( SMSG *m ){
    int sock, rc = 0;
    struct sockaddr_in * addrInfo;
    struct addrinfo hints, *addr;

    memset( &hints, 0, sizeof( hints ));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    if(( rc = getaddrinfo( m->server, NULL, &hints, &addr ))){
        prn_err( strerror( errno ), GetLastError() );
        return 0;
    }
    addrInfo = ( struct sockaddr_in * ) addr->ai_addr;
    if(( sock = socket( addrInfo->sin_family, addr->ai_socktype, addr->ai_protocol )) < 0 ){
        prn_err( strerror( errno ), GetLastError() );
        return 0;
    }
//  --
    addrInfo->sin_port = htons( m->port );
    if( connect( sock, ( struct sockaddr * ) addrInfo, addr->ai_addrlen ) < 0 ){
        prn_err( strerror( errno ), GetLastError() );
    }else{
//  --
        if(( write(sock, m->buffer, strlen( m->buffer ))) < 1 )return 0;
//  --
        memset( m->inbuff, 0, sizeof( m->inbuff ));
        rc = read( sock, m->inbuff, sizeof( m->inbuff ) - 1 );
    }
//  --
    freeaddrinfo( addr );
    if( sock ) close( sock );
//  --
return rc;
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
DWORD WINAPI SenderThread( LPVOID lpPar ){
    SMSG *sg = lpPar;
    debug( 0, "Sender: SenderThread starting." );
    int flag, ptm = 10;
//  --
    ZeroMemory( &sg->buffer, sizeof( sg->buffer ));
    while( sg->hFile && sg->hMapFile && sg->lpMapAddress ){
        int a = qu_pop( sg->buffer );
        if( a > 0 ){			// есть данные для передачи
//  --
            flag = ptm - 1;
            while( flag ){
                int r = send_socket( sg );
                if ( r >= 5 ){			//	OK
                    debug( 0, sg->buffer );
                    debug( 0, sg->inbuff );
                    if( r > 40 ) debug( 0, sg->inbuff + 39 );// +39 +13
                    sleep( 1 );
                    break;
                }else if( r == 0 ){			//	Error socket
                    int tm = ( ptm - flag ) * 10;
                    if( flag <= 1 ) tm = 60 * 10;
                    sleep( tm );
                    if( flag > 1 ) flag--;
                }else{					//	Error format reset message
                    prn_log( "Sender: return( 0 < data < 5 )." );
                    sleep( 10 );
                    flag--;
                    if( flag < 1 ){
                        prn_log( sg->buffer );
                        prn_log( sg->inbuff );
                    }
                }
            }//while( flag )

            ZeroMemory( &sg->buffer, sizeof( sg->buffer ));
//  --
        } else {			// очередь пуста
            sleep( 1 );
        }
    }// while main
ExitThread(0);
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
unsigned int qu_pop( char *buf ){
    if( msgs.lpMapAddress == NULL ) return 0;
    unsigned int *pi_queue = ( unsigned int* )msgs.lpMapAddress;
    if( pi_queue[0] == 0 ) return 0;	// очередь пуста
//  --
    EnterCriticalSection( &msgs.queue_guard );
//  --
    unsigned int tail, head, flag;
    head = pi_queue[0];
    tail = pi_queue[1];
    flag = pi_queue[2];
    char *f_queue = (char*)msgs.lpMapAddress + QUGDATA;
//  --
    unsigned int rez = 0;
//  --
    for( ; f_queue[ rez ] != 0; rez++ ){
        buf[ rez ] = f_queue[ rez ];
    }
    buf[ rez ] = 0;
    rez++;
//  --
    memmove( &f_queue[0], &f_queue[ rez ], (head - rez));
    head = head - rez;
    ZeroMemory( &f_queue[ head ], (len_queue - head));
//  --
    pi_queue[0] = head;
    pi_queue[1] = tail;
    pi_queue[2] = flag;
    LeaveCriticalSection( &msgs.queue_guard );
return rez;
}
//  -------------------------------------------------------------------------
unsigned int qu_push( char *mstr ){
    if( msgs.lpMapAddress == NULL ) return 0;
    unsigned int xm = strlen( mstr );
    if(( xm < 1 )||( xm > MAX_LEN_MSG)) return 0;
//  --
    EnterCriticalSection( &msgs.queue_guard );
//  --
    unsigned int tail, head, flag;
    unsigned int *pi_queue = ( unsigned int* )msgs.lpMapAddress;
    head = pi_queue[0];
    tail = pi_queue[1];
    flag = pi_queue[2];
    char *f_queue = (char*)msgs.lpMapAddress + QUGDATA;
//  --
//  пишем сообщения, при переполнении переносим старые сообщения в лог
    unsigned int rez = 0;
    if(( head + xm + 2 ) >= len_queue ){
        char blog[ MAX_LEN_MSG * 2 ];
        for( ; rez <= xm; rez++ ){
            blog[ rez ] = f_queue[ rez ];
            if( f_queue[ rez ] == 0 ) blog[ rez ] = '\n';
            else blog[ rez ] = f_queue[ rez ];
        }
        for( ; f_queue[ rez ] != 0; rez++ ){
            blog[ rez ] = f_queue[ rez ];
        }
        rez++;
        blog[ rez ] = 0;
        prn_log( "WARNING! Queue is full! Reset old messages:" );
        prn_log( blog );
//  --
        memmove( &f_queue[0], &f_queue[ rez ], (head - rez));
        head = head - rez;
        ZeroMemory( &f_queue[ head ], (len_queue - head));
        rez = 0;
    }
    rez = head;
    strncpy( f_queue + head, mstr, xm );
    head = head + xm + 1;
    f_queue[ head ] = 0;
    rez = head - rez;
//  --
    pi_queue[0] = head;
    pi_queue[1] = tail;
    pi_queue[2] = flag;

    LeaveCriticalSection( &msgs.queue_guard );
return rez;
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
void send_integer( char *key, int value ){	// key = "gittrap"
    if( msgs.port == 0 ) return;
    EnterCriticalSection( &msgs.send_guard );
    char bstr[ MAX_LEN_MSG ];
    sprintf( bstr, "{\"request\":\"sender data\",\"data\":[{\"host\":\"%s\",\"key\":\"%s\",\"value\":%i}]}", msgs.host, key, value );
    qu_push( bstr );
    LeaveCriticalSection( &msgs.send_guard );
}
//  -------------------------------------------------------------------------
void send_string( char *key, char *value ){	// key = "smtrap"
    if( msgs.port == 0 ) return;
    EnterCriticalSection( &msgs.send_guard );
    int len_val = strlen( value );
    if( len_val > ( MAX_LEN_MSG - 60 )) return;
    char bstr[ MAX_LEN_MSG ];
    sprintf( bstr, "{\"request\":\"sender data\",\"data\":[{\"host\":\"%s\",\"key\":\"%s\",\"value\":\"%s\"}]}", msgs.host, key, value );
    qu_push( bstr );
    LeaveCriticalSection( &msgs.send_guard );
}
//  -------------------------------------------------------------------------
void send_sm_string( char *value ){
    SYSTEMTIME st;
    GetLocalTime(&st);
//  --
    int len_val = strlen( value );
    char utmp[ MAX_LEN_MSG ];
    memset( utmp, 0, MAX_LEN_MSG );
    int i = sprintf( utmp, "%s;%d-%02d-%02d %02d:%02d:%02d;", msgs.host, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
    win2utf8( utmp + i, value, len_val );
    send_string( "smtrap", utmp );
}
//  -------------------------------------------------------------------------
// ==========================================================================
int open_send( char *server, char *port, char *host ){
    sprintf( msgs.host, "%s", host );
    sprintf( msgs.server, "%s", server );
    char sport[8];
    sprintf( sport, "%s", port );
    msgs.port = atoi( sport );
    msgs.hFile = NULL;
    msgs.hMapFile = NULL;
    msgs.lpMapAddress = NULL;
//  --
    if(( msgs.port < 100 )||( strlen( msgs.server ) < 6 )||( strlen( msgs.host ) < 2 )){
        msgs.port = 0;
        prn_log( "WARNING! MsgSender    - disabled." );
        return 0;
    }
//  --
    InitializeCriticalSection( &msgs.send_guard );
    if( !InitializeCriticalSectionAndSpinCount( &msgs.send_guard, 0x00000400 )){
        prn_err( "Sender: InitializeCriticalSection - failed.", GetLastError());
        msgs.port = 0;
        return 0;
    }
//  --
    InitializeCriticalSection( &msgs.queue_guard );
    if( !InitializeCriticalSectionAndSpinCount( &msgs.queue_guard, 0x00000400 )){
        prn_err( "Sender: InitializeCriticalSection - failed.", GetLastError());
        msgs.port = 0;
        return 0;
    }
//  --
    sprintf( msgs.queue_name, "%slog\\queue.sender", service_work );
//  открытие прежнего или создание нового файла очереди и отображение его на lpMapAddress.
    char tmpstr[64];
    DWORD dw_count = 0, dw_hsize = 0;

    msgs.hFile = CreateFile( msgs.queue_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL );
    if( INVALID_HANDLE_VALUE == msgs.hFile ){
        prn_err( "Sender: CreateFile - failed.", GetLastError());
        msgs.port = 0;
        return 0;
    }	// если файл уже кем-то открыт - будет ошибка 32
//  --
// проверить есть ли данные в очереди,
    dw_count = GetFileSize( msgs.hFile, &dw_hsize );
    LONGLONG len_file = ( dw_hsize * ( MAXDWORD + 1 )) + dw_count;

// если 0 значит только что создан.
// если размер не стандартный, переименовать и создать новый.
// если размер в норме - ОК.

    if( len_file != len_queue && len_file != 0 ){			// проверка размера файла очереди
        prn_log( "WARNING! Sender: Incorrect queue size. Move to BAK & New create file!" );
        CloseHandle( msgs.hFile );
        sprintf( tmpstr, "%s.BAK", msgs.queue_name );			// переименовать, и создать новый !!!
        MoveFileEx( msgs.queue_name, tmpstr, MOVEFILE_REPLACE_EXISTING );
        msgs.hFile = CreateFile( msgs.queue_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL );
        if( INVALID_HANDLE_VALUE == msgs.hFile ){
            prn_err( "Sender: CreateFile - failed.", GetLastError());
            msgs.port = 0;
            return 0;
        }
        len_file = 0;
    }
//  --
// отображение файла
    msgs.hMapFile = CreateFileMapping( msgs.hFile, NULL, PAGE_READWRITE, 0, len_queue, NULL );
    if( msgs.hMapFile == NULL ){
        prn_err( "Sender: CreateFileMapping - failed.", GetLastError());

        msgs.port = 0;
        return 0;
    }
//  --
    msgs.lpMapAddress = MapViewOfFile( msgs.hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if( msgs.lpMapAddress == NULL ){ 
        prn_err( "Sender: MapViewOfFile - failed.", GetLastError());
        return 0;
    }
//  --
    if( len_file == 0 ){		//    else dw_count = len_queue;	// New queue
        int *piq = ( int* )msgs.lpMapAddress;
        piq[0] = 0;
        piq[1] = 0;
        piq[2] = 0x78563412;
        piq[3] = QULABEL;
    }
//  --
//  желательна проверка хеша. пока только метка.
    unsigned int *pi_queue = ( unsigned int* )msgs.lpMapAddress;
    if( pi_queue[3] != QULABEL ){
        pi_queue[0] = 0;
        pi_queue[1] = 0;
        pi_queue[2] = 0;
        pi_queue[3] = QULABEL;
        prn_log( "WARNING! Sender: incorrect structure queue! Create new." );
    }
//  --
    HANDLE hSenderThread = CreateThread( NULL, 0, &SenderThread, (LPVOID)&msgs, 0, NULL );// поток sender
    if( !hSenderThread ){
        prn_err( "Sender: Create SenderThread - failed.", GetLastError() );
        return 0;
    }
//  --
    sprintf( msgs.buffer, "Sender: \"%s\" ( %s:%i ) - Initialization.", msgs.host, msgs.server, msgs.port );
    prn_log( msgs.buffer );
return 1;
}
//  -------------------------------------------------------------------------
void close_send( void ){
    if( msgs.lpMapAddress ) UnmapViewOfFile( msgs.lpMapAddress );
    if( msgs.hMapFile ) CloseHandle( msgs.hMapFile );
    if( msgs.hFile ) CloseHandle( msgs.hFile );
    msgs.hFile = NULL;
    msgs.hMapFile = NULL;
    msgs.lpMapAddress = NULL;
    if( InitializeCriticalSectionAndSpinCount( &msgs.send_guard, 0x00000400 )) DeleteCriticalSection( &msgs.send_guard );
    if( InitializeCriticalSectionAndSpinCount( &msgs.queue_guard, 0x00000400 )) DeleteCriticalSection( &msgs.queue_guard );
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
