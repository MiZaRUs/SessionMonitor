/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/

#include <windows.h>
#include <sddl.h>
//  --
#include <stdio.h>
#include <unistd.h>
//#include <stdlib.h>
//#include <string.h>
//  --
#include "defs.h"
#include "git.h"
#include "config.h"
#include "logger.h"
#include "sender.h"
#include "session.h"
//  ---------------------------------
const char *csPipeName = "\\\\.\\pipe\\SyMon";  // первая часть имени канала Pipe
//  ---------------------------------
extern int DEBUG;
extern CONFIG conf;
extern char *service_work;
//  ------------------------------------------------------------------------
static SESSION sessions[MAX_SESSION];
// =========================================================================
//  ------------------------------------------------------------------------
void msg_for_user( SESSION *si ){	// обработка сообщений из трубы и ответы сессионному процессу
    int fl = 0;
    ZeroMemory( &si->obuf, PIPE_SIZE );
    if( si->status == 1 ){
        if( !strncmp( si->ibuf, "GetConf", 7 )){
            sprintf( si->obuf, "Command: 0 0 0 0 0 0 0" );
            if( DEBUG )	si->obuf[ 9 ] = '1';
            if( strcasecmp( conf.maintenance, "0" ) != 0 ) si->obuf[ 11 ] = '1';
        }
//  --
        if( !strncmp( si->ibuf, "Hush", 4 )){
            sprintf( si->obuf, "SyMon: Hush - %s", si->shost );
            git_delayed_start( sessions, si->i );
        }
//  --
        if( !strncmp( si->ibuf, "Mainten", 7 )){
            int par = atoi( si->ibuf + 8 );
            if( par == 0 ) sprintf( si->obuf, "Stop maintenance;%s", si->suser );
            else if( par > 9 ) sprintf( si->obuf, "Start maintenance;%s;%s;%i", si->suser, si->shost, par );// maintenance;user;name_pc;1800
            if(( par == 0 )||( par > 9 ))send_sm_string( si->obuf );
        }
//  --
    }else if( si->status == 2 ) sprintf( si->obuf, "Session Entered : %s", si->shost );		// прежняя
     else if( si->status == 4 ) sprintf( si->obuf, "Session Exit : %s", si->shost );		// брошена
     else if( si->status == 3 ) sprintf( si->obuf, "Session New : %s", si->shost );		// новая
     else if( si->status == 9 ) sprintf( si->obuf, "Session CloseApp : %s", si->shost );	// завершить	!!!
//  --
    if( strlen( si->obuf ) < 2 ) sprintf( si->obuf, "Ok..;%s", si->shost );		// подтверждение получения
//    prn_log( si->obuf );
    si->status = 1;
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
DWORD WINAPI PipeThread( LPVOID lpParam ){	// процесс создания канала и пуска пользовательского сессионного процесса
    SESSION *si = lpParam;
    if(( si->status == 0 )||( si->error > 0 )||( si->id == 0 )){
        si->status = 0;
        ExitThread(0);
    }
    si->restart = 8;

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS );

//  указываем права доступа к трубе
    si->seca.nLength = sizeof(SECURITY_ATTRIBUTES);
    si->seca.bInheritHandle = FALSE;
    si->strSD = TEXT("D:")		// Discretionary ACL
        TEXT("(A;OICI;GRGW;;;BG)")	// built-in guests
        TEXT("(A;OICI;GRGW;;;AN)")	// anonymous logon
        TEXT("(A;OICI;GRGW;;;AU)")	// to authenticated users
        TEXT("(A;OICI;GRGW;;;BA)");	// to administrators
    ConvertStringSecurityDescriptorToSecurityDescriptor( si->strSD, SDDL_REVISION_1, &si->seca.lpSecurityDescriptor, NULL );
//  --
REPEAT_CREATE:
//  --
//  создание уникального имени трубы
    GetSystemTime( &si->ttime );
    si->dwtmp = ( si->ttime.wHour + 1 ) * ( si->ttime.wMinute + 1 ) * ( si->ttime.wSecond + 1 );
    ZeroMemory(&si->pipename, sizeof( si->pipename ));
    sprintf( si->pipename, "%s-%s-%x%x", csPipeName, si->suser, si->dwtmp, si->ttime.wMilliseconds );

//  указываем пользовательский сессионный процесс
    sprintf( si->procuser, "%sbin\\%s %s %slog\\SM-%s.log", service_work, conf.user_proc, si->pipename, service_work, si->suser );
//  --
    if( DEBUG ){
        sprintf( si->msg, "Create [%i] New PipeThread & UserProcess: Id-%i, Exec=%s", si->i, si->id, si->procuser );
        prn_log( si->msg );
    }
//  --
    si->hpipe = CreateNamedPipe( si->pipename, PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, PIPE_SIZE, PIPE_SIZE, PIPE_TOUT, &si->seca);
    if( si->hpipe == INVALID_HANDLE_VALUE ){
        si->error = GetLastError();
        prn_err("CreateNamedPipe filed", si->error );
        si->status = 0;
        ExitThread(0);
    }
//  --
    usleep( PIPE_TOUT * 500 );		// подождём 0,5 таймаута - окончания создания трубы
//  запуск пользовательского процесса
//  --
    ZeroMemory(&si->ppi, sizeof( si->ppi ));
    ZeroMemory(&si->ssi, sizeof( si->ssi ));
    si->ssi.lpDesktop = "WinSta0\\Default";	// "WinSta0\\Winlogon"
    si->ssi.cb = sizeof( si->ssi );
    si->ssi.wShowWindow = SW_HIDE;
    si->ovlpd.hEvent = NULL;
//  --
    if( !CreateProcessAsUser( si->htoken, NULL, si->procuser, NULL, NULL, FALSE, CREATE_NO_WINDOW | HIGH_PRIORITY_CLASS , NULL, service_work, &si->ssi, &si->ppi)){
        si->error = GetLastError();
        prn_err( "CreateProcessAsUser failed.", si->error );
        CloseHandle( si->hpipe );
    }else{
        sprintf( si->msg, "Create [%i] ProcessAsUser:%s ID-%i - Ok.", si->i, si->suser, si->ppi.dwProcessId );
        prn_log( si->msg );
//  --
        while( si->status ){			// обмен данными с пользовательским процессом
            ConnectNamedPipe( si->hpipe, &si->ovlpd );
            si->wd = WaitForSingleObject( si->hpipe, PIPE_TOUT );	// ждем подключения
            if( si->wd == WAIT_OBJECT_0 ){
                ZeroMemory( &si->ibuf, PIPE_SIZE );
                ReadFile( si->hpipe, si->ibuf, PIPE_SIZE, &si->cbrd, &si->ovlpd );
                WaitForSingleObject( si->hpipe, PIPE_TOUT );		// получаем сообщение
//  --
                msg_for_user( si );					// подготовка ответа
//  --
                WriteFile( si->hpipe, si->obuf, strlen( si->obuf ) + 1, &si->cbwr, &si->ovlpd ); // отправляем
                WaitForSingleObject( si->hpipe, PIPE_TOUT );		// передаем сообщение
                usleep(500000);
                DisconnectNamedPipe( si->hpipe );			// разрываем соединение
                ZeroMemory( &si->obuf, PIPE_SIZE );			// очистить выходной буффер
            }else{
                si->error = GetLastError();
                break;
            }
        }// while
//  --
    sprintf( si->msg, "WARNING! Interrupted [%i] PipeThread: Id-%i, User:%s, Pipe:%s, Error-%i, WFSO-%x.", si->i, si->id, si->suser, si->pipename, si->error, si->wd );
    if( si->status > 0 ) prn_log( si->msg );

//    закрыть трубу и подождать
        CloseHandle( si->hpipe );
        usleep( PIPE_TOUT * 1200 );
//    завершить пользовательский процесс
        if( si->ppi.hProcess ){
            si->dwtmp = 0;
            if( !TerminateProcess( si->ppi.hProcess, (DWORD)-1 )) si->dwtmp = GetLastError();
            sprintf( si->msg, "Terminate [%i] UserSession ProcessID-%i, Id-%i, User:%s, Return-%i", si->i, si->ppi.dwProcessId, si->id, si->suser, si->dwtmp );
            debug( 0, si->msg );
        }
        usleep( PIPE_TOUT * 1000 );				// подождем
    }//  CreateProcessAsUser
//  --
//    error 2:				// сессия не создана
//    error 5:				// сессия убита
//    error 535:			// ошибка трубы
//    ( wd == WAIT_TIMEOUT )||( wd == WAIT_FAILED );

    if( si->error == 997 ){					// процесс убит или повис
        goto REPEAT_CREATE;					// повторим
    }else if(( si->error == 6 )&&(si->wd == WAIT_TIMEOUT )){	// ошибка соединения с трубой - ERROR_INVALID_HANDLE + 258 - TIMEOUT
        sprintf( si->msg, "WARNING! [%i] Pipe:%s - TIMEOUT (%i).", si->i, si->pipename, si->restart );
        prn_log( si->msg );
        usleep( PIPE_TOUT * 4000 );				// подождем 4 таймаута (mcsec)
        si->restart--;
        if( si->restart > 0 ) goto REPEAT_CREATE;
    }else if(( si->error == 5 )&&( si->status != 4 )){	// сессия закрыта послать сигнал Session ???
        sprintf( si->msg, "WARNING! Closed [%i] Session: Id-%i, User:%s, Host:%s.", si->i, si->id, si->suser, si->shost );
        prn_log( si->msg );
        sprintf( si->msg, "SM;Session;Closed;-;%s;%s;-", si->suser, si->shost );
        send_sm_string( si->msg );
    }

//  --
    si->hpipe = NULL;
    si->ppi.hProcess = NULL;
    si->id = 0;
    si->error = 0;
    si->status = 0;		// completed
    si->active = 0;

    ExitThread(0);
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------

//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
void kill_all_user_proc( void ){
    char msg[MSG_SIZE];
    sprintf( msg, "Finality All Users Sessions Process." );
    prn_log( msg );
//  --
    int i;
    for( i = 0; i < MAX_SESSION; i++ ){
        if( sessions[i].ppi.hProcess ) sessions[i].status = 9;	// послать сигнал завершения
    }
    sleep( 4 );
//  --
    for( i = 0; i < MAX_SESSION; i++ ){				// добить всех кто не успел
        if( sessions[i].hpipe ) CloseHandle( sessions[i].hpipe );
        if( sessions[i].ppi.hProcess ){
            int er = 0;
            int pid = sessions[i].ppi.dwProcessId;
            if( !TerminateProcess( sessions[i].ppi.hProcess, (DWORD)-1 )) er = GetLastError();
            sprintf( msg, "Terminate User Session [%i] ProcessID-%i, Index-%i, User:%s, Return-%i.", i, pid, sessions[i].id, sessions[i].suser, er );
            prn_log( msg );
        }
    sessions[i].status = 0;
    sessions[i].id = 0;
    sessions[i].active = 0;
    }
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
void new_session( DWORD id, DWORD type, char *stype, char *suser, HANDLE token, char *host, char *ip ){ // вход пользователя
    char msg[MSG_SIZE];
    sprintf( msg, "SM;Session;Begin;%s;%s;%s;%s", stype, suser, host, ip );
    send_sm_string( msg );
//  ----
    if( strcasecmp( conf.user_proc, "0" ) == 0 ) return;
//  ----

//  вход пользователя - подготовка сессионной структуры
    int i = 0;
    for(; i < MAX_SESSION; i++ ) if( sessions[i].id == id ) break;	// ищем эту сессию
    if( i >= MAX_SESSION ){	// нет такой сессии - ищем первую свободную
        i = 0;
        for(; i < MAX_SESSION; i++ ) if(( sessions[i].id == 0 )&&( sessions[i].status == 0 )) break;
    }
    if( i >= MAX_SESSION ){	// нет места под сессию
        prn_err( "i : new_session - MAX_SESSION! Not enough space for the new session!", i );
        return;
    } //  allocate & create ???
//  --
    sessions[i].i = i;
    sessions[i].type = type;
    sessions[i].htoken = token;
    sessions[i].active = 1;
    strcpy( sessions[i].suser, suser );
    strcpy( sessions[i].shost, host );
    strcpy( sessions[i].sip, ip );
//  --
    if( sessions[i].id == id ){	// вход в ту-же самую сессию
        sessions[i].status = 2;	// послать сообщение в трубу
    }else{
        sessions[i].status = 3;	// новая сессия послать сообщение в трубу
        sessions[i].id = id;
        sessions[i].error = 0;
//  создание сессионного потока
        sessions[i].hproc = CreateThread(NULL, 0, &PipeThread, (LPVOID)&sessions[i], 0, NULL );
        if( !sessions[i].hproc ) prn_err( "Create Thread User Process failed.", GetLastError() );
    }
}
//  ------------------------------------------------------------------------
void end_session( DWORD id, DWORD type, char *stype, char *suser ){	// выход пользователя
    char msg[MSG_SIZE];
    sprintf( msg, "SM;Session;End;%s;%s;-;-", stype, suser );
    send_sm_string( msg );
//  ----
    if( strcasecmp( conf.user_proc, "0" ) == 0 ) return;
//  ----
    int i = 0;
    for(; i < MAX_SESSION; i++ ) if( sessions[i].id == id ) break;	// ищем сессию
    if( i >= MAX_SESSION ){	// нет такой сессии
        prn_log( "WARNING! End_session - Session not found!" );
    }else{
//  можно проверить по type & suser
        sessions[i].status = 4;		// сессия не закрыта (покинута) послать сообщение в трубу
        sessions[i].active = 0;
    }
}
//  ------------------------------------------------------------------------
// =========================================================================
