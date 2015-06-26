/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>

#ifndef _SM_session_
#define _SM_session_
//  ---------------------------------
#define MAX_SESSION	17
#define PIPE_SIZE	128
#define PIPE_TOUT	5000
//  ---------------------------------
typedef struct SESSIONSTRUCT{
    DWORD	id;		//	из WTSSESSION_NOTIFICATION.dwSessionId
    DWORD	type;		//	из WTSSESSION_NOTIFICATION.dwEventType
    HANDLE	htoken;		//	из WTSSESSION_NOTIFICATION.hToken
    HANDLE	hpipe;		//	канал для связи с сессионным процессом
    HANDLE	hproc;		//	сессионный Thread-процесс
    HANDLE	hutil;		//	процесс утилит сессии
    int		i;		//	индекс
    int		error;		//	ошибки сессии
    int		status;		//	состояние сессии / активность сессионного процесса
    int		timeout;	//	таймаут сессии
    int		restart;	//	счетчик перезапуска сессии
    int		active;		//	активность пользователя
    char	suser[32];	//	имя пользователя сессии cp1251
    char	shost[32];	//	имя ПК пользователя
    char	sip[32];	//	IP ПК пользователя
//    char	smac[32];	//	MAC ПК пользователя
//    SISTEMTIME	tm_in;		//	время входа пользователя
//    SISTEMTIME	tm_out;		//	время выхода пользователя
//    SISTEMTIME	tm_act_n;	//	время начала активности пользователя
//    SISTEMTIME	tm_act_k;	//	время окончания активности пользователя
    char	pipename[127];	//	имя канала
    char	procuser[512];	//	строка запуска прграммы пользовательской сессии
    char	ibuf[PIPE_SIZE];//	буффер канала связи c сессионным процессом
    char	obuf[PIPE_SIZE];//	выходной буффер канала связи c сессионным процессом
    char	msg[MSG_SIZE];	//	сообщения для лога и сокета
//  --
    DWORD	cbwr, cbrd, wd;
    OVERLAPPED	ovlpd;		//	служебная для канала
    PROCESS_INFORMATION ppi;	//	служебная для процесса
    STARTUPINFO ssi;		//	служебная для процесса
    SECURITY_ATTRIBUTES  seca;	//	служебная для доступа к каналу
    SYSTEMTIME	ttime;		//	служебная для имени канала
    DWORD	dwtmp;		//	временная dword
    TCHAR	*strSD;		//	строка для ACL
}SESSION;
//  ------------------------------------------------------------------------
void msg_for_user( SESSION *si );
//  --
int get_list_session(char*, int, DWORD);
//  --
void new_session( DWORD id, DWORD type, char *stype, char *user, HANDLE token, char *host, char *ip ); // вход пользователя
void end_session( DWORD id, DWORD type, char *stype, char *user );	// выход пользователя
void kill_all_user_proc( void );			// завершение всех сессионых процессов
//  --
DWORD WINAPI PipeThread( LPVOID lpParam );		// процесс создания канала и пользовательского сессионного процесса
//  ------------------------------------------------------------------------
// =========================================================================
#endif
