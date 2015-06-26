/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>
//  --
#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <string.h>

#include "defs.h"
#include "logger.h"
#include "config.h"
#include "sender.h"
#include "session.h"
//  --
// =========================================================================
extern CONFIG conf;
extern int DEBUG;
extern char *service_work;
//  --
static const int MAXGITBUF = 4096;		// длинна буффера для вывода git status -s
//  --
static volatile unsigned short gitStatus = 1;	// hash
//  --
static volatile int gitStatusStart = 0;
static volatile int sess_i;
static volatile SESSION *sess;
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
unsigned short ksumCRC( char *msg, int len ){ // CRC16
    unsigned short ks = 0xFFFF; //KSUM
    int j = 0, k = 0;
    while( k < len ){
        ks = ks ^ (short)msg[k];
        for( j = 0; j < 8; j++ ){
            if( ks & 0x0001 ) ks = ( ks >> 1 ) ^ 0xA001;
            else ks = ks >> 1;
        }
    k++;
    }
    return ks;
}// End ksumCRC
//  ------------------------------------------------------------------------
static void set_file_time( char *msg, int len ){ // size msg = len + 23
//prn_log( "%s\n", msg + 1 );
    if(( msg[1] == 'M' )||( msg[1] == '?' )||( msg[0] == 'A' )){
        msg[len] = 0;
        HANDLE hFile = CreateFile( msg + 3, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        FILETIME ftWrite;
// получить файловую дату и время
        if( !GetFileTime( hFile, NULL, NULL, &ftWrite )){	// возможно это директория !!!
            sprintf( msg + len, "* - - - ?.\n" );
            return;
        }
        CloseHandle( hFile );
//  --
        SYSTEMTIME stUTC, stLocal;
// преобразовать время модификации в локальное время.
        FileTimeToSystemTime(&ftWrite, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
//  --
        msg[len] = ' ';
        len++;
        for(; len < 59; len++) msg[len] = '_';
//  --
        sprintf( msg + len, " %d-%02d-%02d %02d:%02d:%02d\n", stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    } else {
        msg[len] = '\n';
        msg[len + 1] = 0;
    }
}// добавляет к имени файла в msg время записи.
//  ------------------------------------------------------------------------
static int status_file_list( char *flist, int len_flist, char *bgs, int len_bgs ){
    int c = 0;
    char msg[256];
    int i = 0, j = 0;
    while(( bgs[i] != 0 )&&( i < len_bgs )){
        if( bgs[ i ] != '\n' ){
            msg[ j ] = bgs[ i ];
            j++;
        } else {
            msg[ j + 1 ] = '\0';
            set_file_time( msg, j );
            j = 0;
            strcat( flist, msg );
            c++;
        }
        i++;
    }
return c;
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
static void git_report( char *st, int gs ){
// if( strcasecmp( conf.git_report, "0" ) == 0 ) return -1;

debug( 0, "git_report()." );

    char msg[256];
    sprintf( msg, "%slog\\git-status.log", service_work );
    DWORD dw =0;

    DWORD fl = CREATE_ALWAYS;
    if( gs > 0 ) fl = OPEN_ALWAYS;

    HANDLE hFile = CreateFile( msg, GENERIC_WRITE, FILE_SHARE_READ, NULL, fl, FILE_ATTRIBUTE_NORMAL, NULL );
    if( INVALID_HANDLE_VALUE != hFile ){

        if( gs > 0 ) SetFilePointer( hFile, 0, 0, FILE_END );

        SYSTEMTIME stm;
        GetLocalTime(&stm);
        sprintf( msg, "%d-%02d-%02d %02d:%02d:%02d   Git satatus   Count - %i:\n", stm.wYear, stm.wMonth, stm.wDay, stm.wHour, stm.wMinute, stm.wSecond, gs );
        WriteFile( hFile, msg, strlen( msg ), &dw, NULL );

        if( st ) WriteFile( hFile, st, strlen( st ), &dw, NULL );

        CloseHandle( hFile );
    }
}
//  ------------------------------------------------------------------------

//  ------------------------------------------------------------------------
static int git_status( char *bmsg, int len_bmsg, int *len_read ){
    if( bmsg == NULL ) prn_log( "git_status - Error!" );
//  --
    char imus_home[ 18 ];
    char git_exec[ 96 ];
    ZeroMemory(&git_exec, 96 );
    ZeroMemory(&imus_home, 18 );
//  --
    sprintf( git_exec, "%s status -s", conf.git_exec );
    strncpy( imus_home, service_work, ( sizeof( char ) * 8 ));
//  --
    if( !SetCurrentDirectory( imus_home )){		// d:\imus\
        sprintf( bmsg, "WARNING! SetCurrentDirectory: Git-path-imus-home<%s> failed!", imus_home );
        prn_log( bmsg );
        return -1;
    }
//  --
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES PipeSA = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

    PROCESS_INFORMATION spi;
    STARTUPINFO ssi;
    GetStartupInfo(&ssi);
    if( !CreatePipe( &hReadPipe, &hWritePipe, &PipeSA, 0 )){
        prn_err( "Pipe for 'git status' failed!", GetLastError());
        return -1;
    }
//  --
    ssi.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    ssi.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    ssi.hStdOutput = hWritePipe;
    ssi.dwFlags = STARTF_USESTDHANDLES; 

    imus_home[ 8 ] = 0;
    if( !CreateProcess( NULL, git_exec, NULL, NULL, TRUE, 0, NULL, imus_home, &ssi, &spi )){
        sprintf( bmsg, "WARNING! Run 'git status'<%s> failed! %i (IH <%s>)", git_exec, GetLastError(), imus_home );
        prn_log( bmsg );
        return -1;
    }
    CloseHandle(spi.hThread);
    WaitForSingleObject(spi.hProcess, INFINITE); 
    CloseHandle( hWritePipe );
//  --
    int isym = 0;
    int istr = 0;
//  --
    char ch;
    int sb;
    while( ReadFile( hReadPipe, &ch, 1, &sb, NULL ) ){
        if( ch == '\n' ) istr++;
        if( isym < len_bmsg ){
            bmsg[isym] = ch;
            isym++;
        }
    }
    bmsg[isym] = '\0';
    bmsg[isym + 1] = '\0';
//  --
    *len_read = istr;		// количество прочитанных строк
    CloseHandle(spi.hProcess); 
    CloseHandle( hReadPipe );
//  --
return isym;	//количество прочитанных символов
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
void git_test( void ){
    if( strcasecmp( conf.git_exec, "0" ) == 0 ) return;
    if( gitStatusStart != 1 ) return;
    gitStatusStart = 2;
//  --
    char *bxgit = malloc( MAXGITBUF + 2 );
    if( !bxgit ){
        prn_err( "git_test:1 malloc() failed!", GetLastError());
        return;
    }
//  --
    int str_gs = 0;
    int len_gs = git_status( bxgit, MAXGITBUF, &str_gs );
    if( len_gs == -1 ){		// послать аварию ???- ошибку выводит сама git_status()
        return;
    }
    if( ( len_gs + 2 ) >= MAXGITBUF ) prn_log( "WARNING! git_status: return is to long." );
//  --
    int str_fl = 0;
    unsigned short ks = 0;
//  --
    int cs = 1;
    for( int i = 0; i < MAX_SESSION; i++ ) if( sess[i].status > 0 ) cs++;	// ищем все сессии, минимум одна
    cs = ( cs * 82 ) + 16;	//<---->количество строк на длинну строки
    int len_bxreport = len_gs + ( str_gs * 80 ) + cs;	// с запасом
//  --
    char *bxreport = malloc( len_bxreport + 1 );
    if( !bxreport ){
        prn_err( "git_test:2 malloc() failed!", GetLastError());
        if( bxgit ) free( bxgit );
        return;
    }
    ZeroMemory( bxreport, len_bxreport );
//  --

int tmp = 0;

    if( str_gs > 0 ){
        str_fl = status_file_list( bxreport, len_bxreport, bxgit, len_gs );

tmp = len_bxreport;

        len_bxreport = strlen( bxreport );
        ks = ksumCRC( bxreport, len_bxreport );
    }
//  --
    if( str_gs != str_fl ){
        prn_log("WARNING! git_test - failed! ???.");
        gitStatus = 0;
    }
//  --

//***************
if(DEBUG){
    sprintf(bxgit, ">>git_test() GS:%i = KS:%i; str_gs:%i = str_fl:%i; len_gs=%i; strlen(bxreport)=%i < size( %i )", gitStatus, ks, str_gs, str_fl, len_gs, len_bxreport, tmp );
    prn_log( bxgit );
}
//***************

    if( ks != gitStatus ){
        if( !( gitStatus == 1 && ks == 0 )){	// не создавать пустой репорт при старте
            ZeroMemory( bxgit, MAXGITBUF );
            char ch = '>';				// ещё активен
            if( sess[sess_i].active == 0 ) ch = 'v';	// уже вышел
            sprintf( bxgit, " %c U:%-22s H:%-22s A:%s\n", ch, sess[sess_i].suser, sess[sess_i].shost, sess[sess_i].sip );
            strcat( bxreport, "Hosts:\n" );
            strcat( bxreport, bxgit );
            for( int i = 0; i < MAX_SESSION; i++ ){
                if(( sess[i].status == 1 )&&( i != sess_i )){	// ищем активные сессии
                    if( sess[i].active == 0 ) ch = '-';		// сессия не активна
                    else ch = '+';				// из сессии не выходили
                    ZeroMemory( bxgit, 128 );
                    sprintf( bxgit, " %c U:%-22s H:%-22s A:%s\n", ch, sess[i].suser, sess[i].shost, sess[i].sip );
                    strcat( bxreport, bxgit );
                }
            }
            ZeroMemory( bxgit, 128 );
            strcat( bxreport, "\n\n" );
            git_report( bxreport, str_gs );
        }
//  --
        send_integer( "gittrap", str_gs );
        gitStatus = ks;
    }
//  --
    if( bxreport ) free( bxreport );
    if( bxgit ) free( bxgit );
//  --
    gitStatusStart = 0;
//prn_log( ">>git_test - END!" );
}
//  ------------------------------------------------------------------------
void git_delayed_start( SESSION *ses, int di ){		// команда для отложенного запуска git_test()
    if( strcasecmp( conf.git_exec, "0" ) == 0 ) return;
    if( gitStatusStart != 0 ) return;
//  --
    sess_i = di;
    sess = ses;
//  --
//debug( 0, ">> git_delayed_start");
    gitStatusStart = 1;
// запуск git_test() из главного цикла service.c
}
//  ------------------------------------------------------------------------
// =========================================================================

