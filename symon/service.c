/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>
#include <wtsapi32.h>
//  --
#include <stdio.h>
#include <unistd.h>
//#include <stdlib.h>
//#include <string.h>

#include "defs.h"
#include "git.h"
#include "config.h"
#include "logger.h"
#include "logcygwin.h"
#include "sender.h"
#include "session.h"
//  ----------------------
const char *version = "SyMon-2.7";
//  ----------------------
CONFIG	conf;
int	JOB = 0;
int	DEBUG = 1;
char *service_work;
static char *service_name;
static char *service_path;
//  ----------------------
SERVICE_STATUS service_status;
SERVICE_STATUS_HANDLE hServiceStatus;
//  ---------------------------------
void remove_symon(void);
void install_symon(void);
VOID WINAPI ServiceCtrlHandlerEx( DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);// callback-функция, вызывается ОС
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
LONG WINAPI UnhandledException(LPEXCEPTION_POINTERS exceptionInfo){// Exception идут сюда
    prn_log(msg_err);
    prn_log("\nFATAL_ERROR: UnhandledException(!!!)\n");
return EXCEPTION_EXECUTE_HANDLER;
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
void entering_info( DWORD SesId, DWORD SesTyp, HANDLE token, LPVOID param ){	//определение пользователя
    char *wuser = NULL;
    char *bufa = NULL;
    char *whost = NULL;
    DWORD usersize = 0;
    DWORD hostsize = 0;
    DWORD size = 0;
//  --
    if((!WTSQuerySessionInformation( WTS_CURRENT_SERVER_HANDLE, SesId, WTSUserName, &wuser, &usersize ))
    ||(!WTSQuerySessionInformation( WTS_CURRENT_SERVER_HANDLE, SesId, WTSClientAddress, &bufa, &size ))){
        prn_err( "WTSQuerySessionInformation failed.", GetLastError() );
        return;
    }else if( bufa ){
        WTS_CLIENT_ADDRESS *pAddr = (WTS_CLIENT_ADDRESS *) bufa;
        if(!WTSQuerySessionInformation( WTS_CURRENT_SERVER_HANDLE, SesId, WTSClientName, &whost, &hostsize )){
            prn_err( "WTSQuerySessionInformation failed.", GetLastError() );
        }else{
            char bs[10];
            ZeroMemory(&bs, 12 );
            char ba[25];
            ZeroMemory(&ba, 26 );
            sprintf( ba, "%u.%u.%u.%u", pAddr->Address[2], pAddr->Address[3], pAddr->Address[4], pAddr->Address[5] );
// ipv6     sprintf( ba, "%u.%u.%u.%u.%u.%u", pAddr->Address[0], pAddr->Address[1], pAddr->Address[2], pAddr->Address[3], pAddr->Address[4], pAddr->Address[5] );
//  --
            if(( SesTyp == 1 )||( SesTyp == 2 )){
                strcpy( bs, "Console" );
            }else if(( SesTyp == 3 )||( SesTyp == 4 )){
                strcpy( bs, "Remote" );
            }else if(( SesTyp == 5 )||( SesTyp == 6 )){
                strcpy( bs, "Logon" );
            }else if(( SesTyp == 7 )||( SesTyp == 8 )){
                strcpy( bs, "Lock" );
            }else{
                sprintf( bs, "Type-%u", SesTyp );
            }
//  --
            if( strlen( ba ) < 8 ) strcpy( ba, "localhost" );

            char pc[128];		// char *pc = malloc( sizeof( char ) * 64 );
            ZeroMemory(&pc, 128 );
            if( hostsize > 1 ) strcpy( pc, whost );
            else strcpy( pc, "localhost" );
//  --
            if( SesTyp % 2 ){	// если не кратна двум то начало
                sprintf( msg_err, "Begin Session Id-%i, Type-%i (%s), User:%s, Host:%s, IP:%s.", SesId, SesTyp, bs, wuser, pc, ba );
                prn_log( msg_err );
                if(( JOB > 100 )&&( SesTyp < 7 )) new_session( SesId, SesTyp, bs, wuser, token, pc, ba );
            }else{
                sprintf( msg_err, "End Session Id-%i, Type-%i (%s), User:%s.", SesId, SesTyp, bs, wuser );
                prn_log( msg_err );
                if(( JOB > 100 )&&( SesTyp < 7 )) end_session( SesId, SesTyp, bs, wuser );
            }
//  --
            WTSFreeMemory( whost );
        }
//  --
        WTSFreeMemory( bufa );
        WTSFreeMemory( wuser );
    }
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv){
    sprintf( msg_err, "ServiceMain - init." );
    DWORD xParam = 1;
    JOB = 1;
//  --
    hServiceStatus = RegisterServiceCtrlHandlerEx( service_name, (LPHANDLER_FUNCTION_EX)ServiceCtrlHandlerEx, (PVOID)&xParam );
    if( !hServiceStatus ){
        prn_err( "RegisterServiceCtrlHandlerEx failed.", GetLastError() );
        return;
    }
// init
    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_START_PENDING;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE;
    service_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
    service_status.dwServiceSpecificExitCode = 0;
    service_status.dwCheckPoint = 0;
    service_status.dwWaitHint = 5000;

    if( !SetServiceStatus( hServiceStatus, &service_status )){
        prn_err( "SetServiceStatus 'NULL' failed.", GetLastError() );
        return;
    }

// Rabota
    service_status.dwCurrentState = SERVICE_RUNNING;
    service_status.dwWin32ExitCode = NO_ERROR;
// Set New
    if( !SetServiceStatus( hServiceStatus, &service_status )){
        prn_err( "SetServiceStatus 'SERVICE_START_PENDING' failed.", GetLastError() );
        return;
    }
//  --
    usleep(10000);
//  ------------------------------------------------------------------------
prn_log( "." );
    SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
prn_log( "START SERVICE." );
//  ---------------------------------
    if( !init_conf( service_work, service_name, &conf )){	// возврат всегда "1"
        prn_err( "Init config-data failed. This message can not be", 0 );
    }else{
        if( strcasecmp( conf.log_debug, "Off" ) == 0 ) DEBUG = 0;
        else prn_log( "Logger - DEBUG" );
//  --
        if( strcasecmp( conf.user_proc,   "0" ) == 0 ) prn_log( "WARNING! User Process - disabled." );
        if( strcasecmp( conf.maintenance, "0" ) == 0 ) prn_log( "WARNING! Maintenances - disabled." );
        if( strcasecmp( conf.git_exec,    "0" ) == 0 ) prn_log( "WARNING! Git status   - disabled." );
    }
//  --
    JOB = 2;
// Создаем поток логирования cygwin (сокет /dev/log) для ssh-сессий
    HANDLE hCygWinLogThread = CreateThread( NULL, 0, &CygWinLogThread, (LPVOID)&conf, 0, NULL );
    if( !hCygWinLogThread ){
        prn_err( "LogSocket: Create CygWinLogThread - failed.", GetLastError() );
    }
//  ---------------------------------
    JOB = 3;
    if( !open_send( conf.zbx_server, conf.zbx_port, conf.zbx_host )){
        prn_log( "WARNING! Sender not available: Maintenances - will not be available." );
        strcpy( conf.maintenance, "0" );
    }
//  ---------------------------------
    JOB = 22;
// потоки обслуживания
//    HANDLE hGTimeThread = CreateThread( NULL, 0, &GTimeThread, NULL, 0, NULL );// поток
//  ---------------------------------

    usleep(10000);
    JOB = 101;
    send_sm_string( "SM;SyMon;START;!" );
//  --
//  проверка активных сессий
    DWORD pCount;
    HANDLE hToken;
    PWTS_SESSION_INFO ppSessionInfo = NULL;
    HANDLE hTS = WTSOpenServer( "" );
//  --
    if( WTSEnumerateSessions(hTS, 0, 1, &ppSessionInfo, &pCount )){
        while( pCount ){
            if( ppSessionInfo->State == 0 ){
                if( WTSQueryUserToken( ppSessionInfo->SessionId, &hToken )) entering_info( ppSessionInfo->SessionId, 5, hToken, NULL );
                else prn_err( "WTS Token failed.", GetLastError() );
            }

            ppSessionInfo++;
            pCount--;
        }
    }

//  -----------------------------------
    while( JOB > 100 ){ // главный цикл
        git_test();	// выполнение отложенных (тормозных) функций

        sleep(1);
    }
//  --
//    CloseHandle( hGTimeThread );
    sleep( 2 );
    close_send();
    sprintf( msg_err, "ServiceMain - return." );
//  ------------------------------------------------------------------------
}
//  ------------------------------------------------------------------------
//  ------------------------------------------------------------------------
/**************************************************************************/
//  ------------------------------------------------------------------------
VOID WINAPI ServiceCtrlHandlerEx( DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext){// callback-функция, вызывается ОС
    WTSSESSION_NOTIFICATION *wn;
    HANDLE hToken;
//  -------------------------
    switch( dwControl ){
        case SERVICE_CONTROL_SESSIONCHANGE:
            wn = (WTSSESSION_NOTIFICATION*)lpEventData;
            if( WTSQueryUserToken( wn->dwSessionId, &hToken )) entering_info( wn->dwSessionId, dwEventType, hToken, lpContext );
            break;
//  --
        case SERVICE_CONTROL_STOP:
            send_sm_string( "SM;SyMon;STOP;!" );
            prn_log( "WARNING! STOP SERVICE." );
            kill_all_user_proc();
            JOB = 3;
            prn_log( "End." );
            sleep(1);
            service_status.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(hServiceStatus, &service_status);
            break;
//  --
        case SERVICE_CONTROL_SHUTDOWN:
            send_sm_string( "SM;SYSTEM;SHUTDOWN;!" );
            prn_log( "WARNING! SYSTEM SHUTDOWN." );
            kill_all_user_proc();
            prn_log( "End." );
            sleep(1);
            service_status.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(hServiceStatus, &service_status);
            break;
//  --
        default:
            ++service_status.dwCheckPoint;
            SetServiceStatus(hServiceStatus, &service_status);
            break;
    }
  return;
}
//  ------------------------------------------------------------------------
// =========================================================================
int main( int argc, char *argv[] ){					// Предстартовая инициализация
 JOB = 0;
    int  buf_in_len = 0;
    char *buf_in = malloc( sizeof( char ) * ( BUF_SIZE + 2 ));
//  --
    sprintf( msg_err, "Main init" );
    buf_in_len = GetModuleFileName( NULL, buf_in, BUF_SIZE );	//путь и имя запускаемого сервиса
    if( buf_in_len < 4 ) return GetLastError();
//  --
    buf_in_len -= 5;				// отрезаем ".exe"
    buf_in[buf_in_len+1] = 0;
    buf_in[buf_in_len+2] = 0;
//  --
    int it = buf_in_len;
    for(; it > 1 ; it--) if( buf_in[it] == '\\' )break;
    it++;
    service_path = malloc( sizeof( char ) * ( it + 1 ));
    strncpy( service_path, buf_in, it );
//  --
    service_name = malloc( sizeof( char ) * (( buf_in_len - it) + 2 ));
    strcpy( service_name, ( buf_in + it ));
//  --
    it -= 2;
    for(; it > 1; it--) if( buf_in[it] == '\\' )break;
    it++;
    service_work = malloc( sizeof( char ) * ( it + 1 ));
    strncpy( service_work, buf_in, it );

//  --
    if(( strlen( service_path ) > 3 )||( strlen( service_work ) > 3 )){
        memset( buf_in, '\0', BUF_SIZE );
        sprintf( buf_in, "%slog\\%s.log", service_work, service_name );
        if( open_log( buf_in )){
            sprintf( msg_err, "Main - Ok." );
            free( buf_in );
            SetUnhandledExceptionFilter( UnhandledException );			// Ловим некоторые ексепшены
//  --
            SERVICE_TABLE_ENTRY  service_table[] = {{service_name, ServiceMain}, { NULL, NULL }};
            if( !StartServiceCtrlDispatcher( service_table )){		// Регистрация сервиса
                DWORD er = GetLastError();
                if( er == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT ){
                    if( argc != 2 ){					// Получение консольных параметров
                        fprintf( stderr, "\n*******    %s    *******\n", version );
                        fprintf( stderr, "No Exec! Run is Service ( install, remove ).\n" );
                    }else{
                        if( strcasecmp( argv[1], "install" ) == 0 ) install_symon();
                        if( strcasecmp( argv[1], "remove" ) == 0  ) remove_symon();
                    }
                    fprintf( stderr, "Press <Enter> to exit.\n");
                    getc(stdin);
                }else if( er ){
                    fprintf( stderr, "StartServiceCtrlDispatcher failed.\n");
                }
            }
        }
    }
//  --
//    prn_log("Ok.");
    close_log();
//    if( buf_in ) free( buf_in );
    if( service_name ) free( service_name );
    if( service_work ) free( service_work );
    if( service_path ) free( service_path );
//  --
return GetLastError();
}
// =========================================================================
//  ------------------------------------------------------------------------
void install_symon(void){					// Инсталируем сервис
    SC_HANDLE  hServiceControlManager, hService;
    if( NULL == ( hServiceControlManager = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE ))){
        fprintf( stderr, "ERROR-%u. Open SCM failed.\n", GetLastError() );
        return;
    }
//  --
    char *service_exec = malloc( MAX_PATH );
    GetModuleFileName( NULL, service_exec, BUF_SIZE );	//путь и имя запускаемого сервиса

    if( NULL == ( hService = CreateService( hServiceControlManager, service_name, "SyMon",
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, service_exec, NULL, NULL, NULL, NULL, NULL ))){
        fprintf( stderr, "ERROR-%u. Create service failed.\n", GetLastError() );
        CloseServiceHandle(hServiceControlManager);
        return;
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hServiceControlManager);

    fprintf( stderr, "Installed: %s\n", service_exec );	// Готово
    fprintf( stderr, "Full path: %s\n", service_path );
    fprintf( stderr, "Work path: %s\n", service_work );
    if( service_exec ) free( service_exec );
}
//  ------------------------------------------------------------------------
void remove_symon(void){					// Удаляем сервис из системы
    SC_HANDLE  hServiceControlManager, hService;
    if( NULL == ( hServiceControlManager = OpenSCManager( NULL, NULL, SC_MANAGER_CONNECT ))){
        fprintf( stderr, "ERROR-%u. Open SCM failed. Admin\n", GetLastError() );
        return;
    }
    if( NULL == ( hService = OpenService( hServiceControlManager, service_name, SERVICE_ALL_ACCESS | DELETE ))){
        fprintf( stderr, "ERROR-%u. Open service failed.\n", GetLastError() );
        CloseServiceHandle(hServiceControlManager);
        return;
    }
    if( !QueryServiceStatus( hService, &service_status )){
        fprintf( stderr, "ERROR-%u. Query service status failed.\n", GetLastError() );
        CloseServiceHandle(hServiceControlManager);
        CloseServiceHandle(hService);
        return;
    }
    if( service_status.dwCurrentState != SERVICE_STOPPED ){	// Если запущен - останавливаем
        fprintf( stderr, "WARNING! %u. Service is working. It will be stoped.\n", GetLastError() );
        if( !ControlService(hService, SERVICE_CONTROL_STOP, &service_status )){
            fprintf( stderr, "ERROR-%u. Control service failed.\n", GetLastError() );
            CloseServiceHandle(hServiceControlManager);
            CloseServiceHandle(hService);
            return;
        }
    Sleep(500);
    }
    if( !DeleteService( hService )){
        fprintf( stderr, "ERROR-%u. Delete service failed.\n", GetLastError() );
        CloseServiceHandle(hServiceControlManager);
        CloseServiceHandle(hService);
        return;
    }
    CloseServiceHandle(hServiceControlManager);
    CloseServiceHandle(hService);
    fprintf( stderr, "The service is deleted.\n" );	// Готово
}
// =========================================================================
