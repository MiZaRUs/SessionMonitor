/***********************************************************************
 *  SMonitir                                                           *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>
#include <commctrl.h>

#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <string.h>
//  --
#include "defs.h"
#include "logger.h"
#include "ipc.h"
//  -------------------------------------------------------------------------
volatile int DEBUG = 1;
//  --
static volatile int wx_maintenance = 0;
//  -------------
static volatile int CTM_VIS = 10;	// время отображения панели в секундах - если 0 то закончить работу
static volatile int CTM_METER = 4;	// время ожидания активности пользователя в минутах
//  -------------
static volatile HINSTANCE hInsts;
static volatile HWND hComBo;
static volatile HWND hGW;
static volatile int mainten = 0;
static volatile int endwait = 0;
static volatile int tm_mainten = 0;
static volatile int tm_vis = 0;
//  --
static volatile int tm_meter = 0;
static volatile DWORD dwt = 0;
LASTINPUTINFO idle;
static char bufs[80];
static char *poptxt1 = "Session Monitor\n    hide\\show";
static char *poptxt2 = "SM Maintenances\n      hide\\show";
static char *poptxt3 = "SM Maintenances < 60c!\n      hide\\show";
static char popicon[ 10 ];
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
void show_gwindow( void ){	// вызывается при входе пользователя
if( DEBUG ) prn_log("show_gwindow");
    tm_meter = 0;		// начать отсчет до Hush с "0"
    regen_tray_icon();	// обновить иконку

    HDC hDCScreen =  GetDC(NULL);
    int Horres = GetDeviceCaps(hDCScreen,  HORZRES);
    int Vertres = GetDeviceCaps(hDCScreen,  VERTRES);
    ReleaseDC(NULL, hDCScreen);

    MoveWindow( hGW, ( Horres - 400 ), (( Vertres/2 ) + 100), 340, 180, TRUE );
    SendMessage( hGW, MYWM_ON_NOTIFY, (WPARAM)NULL, (LPARAM)NULL );
}
//  -------------------------------------------------------------------------
void exit_gwindow( void ){
if( DEBUG ) prn_log("exit_gwindow");
    SendMessage( hGW, WM_DESTROY, (WPARAM)NULL, (LPARAM)NULL );
}
//  -------------------------------------------------------------------------
void set_maintenance( void ){
    prn_log( "Set Maintenance" );
    wx_maintenance = 1;
}
//  -------------------------------------------------------------------------
void set_no_verbose( void ){
    prn_log( "Set No Verbose" );
    DEBUG = 0;
}
//  -------------------------------------------------------------------------
void start_maintenances( HWND hwnd ){
    char str_tm[5];
    int i = SendMessage(hComBo, CB_GETCURSEL, 0, (LPARAM)str_tm );
    switch( i ){
        case 0:		// 15
            mainten = 15 * 60;
            break;
        case 1:		// 30
            mainten = 30 * 60;
            break;
        case 2:		// 1
            mainten = 1 * 3600;
            break;
        case 3:		// 3
            mainten = 3 * 3600;
            break;
        case 4:		// 8
            mainten = 8 * 3600;
            break;
        default:
            mainten = 15 * 60;
    }
//    SendMessage(hComBo, CB_GETLBTEXT, i, (LPARAM)str_tm );
//    mainten = atoi( str_tm );
//    if( mainten < 2 ) mainten = 10;
//    mainten = mainten * 60;
//  --
    sprintf( bufs, "Mainten:%i", mainten );
    send_signal( bufs );					//Start
    endwait = 0;
    mainten = mainten - 90;	// Предупредить заранее
    tm_mainten = 0;

    update_tray_icon( "icon_orng", poptxt2 );	// изменить иконку
//    tm_vis = 0;		// отсчёт с нуля
    Sleep( 100 );
    ShowWindow( hwnd, SW_HIDE );// скрыть окно
//  --
}
//  -------------------------------------------------------------------------
void stop_maintenances( HWND hwnd ){
    update_tray_icon( "icon_ok", poptxt1 );	// изменить
//  --
    if( mainten > 0){
        sprintf( bufs, "Mainten:%i", STOP );
        send_signal( bufs );					//Stop
    }
//  --
    Sleep( 100 );
    ShowWindow( hwnd, SW_HIDE );// скрыть окно
    endwait = 0;
    mainten = 0;
    tm_mainten = 0;
}
//  -------------------------------------------------------------------------
void init_meter( void ){
    tm_meter = 0;
    idle.cbSize = sizeof( LASTINPUTINFO );
    GetLastInputInfo( &idle );
    dwt = idle.dwTime;
}
//  -------------------------------------------------------------------------
void test_meter( void ){	// вызов каждую минуту
    GetLastInputInfo( &idle );
    if(( idle.dwTime - dwt ) == 0 ) {		// Passive
        if(( tm_meter != 0 )&&( ++tm_meter > CTM_METER )){
            sprintf( bufs, "Hush: >%i min", CTM_METER );
            send_signal( bufs );
            tm_meter = 0;
        }
    }else{					// Active
        tm_meter = 1;
    }
    dwt = idle.dwTime;
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
void label_show( HWND hwnd ){
    char *txt;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint( hwnd, &ps );
    SetBkMode( hdc, TRANSPARENT );
//  --
    if(( mainten > 0 )&&( endwait == 0 )){
        SetTextColor( hdc, RGB( 255, 140, 40 ));
        TextOut( hdc, 128, 7, "��������!", 9 );
        txt = "������ ��������� � ������ ������������.";	// mainten
        TextOut( hdc, 15, 25, txt, strlen( txt ));
    }else if(( mainten > 0 )&&( endwait == 1 )){
        SetTextColor( hdc, RGB( 255, 00, 40 ));
        TextOut( hdc, 128, 7, "��������!", 9 );
        txt = "������������� ����� ������ ������������.";	// wait
        TextOut( hdc, 10, 25, txt, strlen( txt ));

    }else{
        SetTextColor( hdc, RGB( 77, 77, 255 ));	// start
        txt = "����� ������������ �������.";
        TextOut( hdc, 60, 20, txt, strlen( txt ));
    }

    if( !wx_maintenance ) TextOut( hdc, 124, 80, "����������!", 11 );
//  --
    EndPaint( hwnd, &ps );
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
LRESULT CALLBACK WindProc( HWND hwnd, UINT wmsg, WPARAM wparam, LPARAM lparam ){
    static volatile int iTray;
    if( wmsg == MYWM_NOTIFYICON && lparam == WM_LBUTTONDOWN){
        if ( iTray == 0 ){
            ShowWindow( hwnd, SW_HIDE );// скрыть окно при нажатии на иконку
            iTray = 1;
        }else{
            ShowWindow( hwnd, SW_SHOW );// показать окно при нажатии на иконку
            iTray = 0;
            tm_vis = 0;
        }
    }
//  --
    switch( wmsg ){
        case WM_PAINT:
            label_show( hwnd );
        break;
//  ------------------
        case WM_KEYDOWN:
            if( wparam == 27 ){
                ShowWindow( hwnd, SW_HIDE );// скрыть окно при нажатии esc
                iTray = 1;
            }
        break;
//  ------------------
        case MYWM_ON_NOTIFY:
            ShowWindow( hwnd, SW_SHOW );// показать окно
            iTray = 0;
            tm_vis = 0;
        break;
//  ------------------
        case WM_TIMER:
            if( wparam == CL_TIMER ){	// вызов каждую минуту
                test_meter();		// проверка активности пользователя
            }
            if( wparam == MY_TIMER ){
                if( tm_vis <= CTM_VIS ) tm_vis++;
//  --
                if(( tm_vis == CTM_VIS )&&( iTray == 0 )){
                    tm_vis++;
                    ShowWindow( hwnd, SW_HIDE );// скрыть окно
                    iTray = 1;
                }
//  --
                if( mainten > 0 ){
                    tm_mainten++;
                    if( tm_mainten == mainten - 60 ){
                        endwait = 1;
                        update_tray_icon( "icon_warn", poptxt3 );	// изменить иконку
                        ShowWindow( hwnd, SW_SHOW );// показать окно при нажатии на иконку
                        iTray = 0;
                        tm_vis = 0;
                        sprintf( bufs, "Mainten:%i", WAIT );
                        send_signal( bufs );				//Wait
                    }else if( tm_mainten > mainten ){
                        stop_maintenances( hwnd );
                    }
                }
            }
        break;
//  --------------------
        case WM_COMMAND:
            if( wparam == BTN_YES ){		// start
                iTray = 1;
                start_maintenances( hwnd );

            }else if( wparam == BTN_NO ){	// stop
                iTray = 1;
                stop_maintenances( hwnd );

            }else{
                tm_vis = 0;		// отсчёт с нуля
            }
        break;
//  --
        case WM_CREATE:
            hGW = hwnd;
            endwait = 0;
            mainten = 0;
            tm_vis = 0;
            iTray = (wx_maintenance) ? 0 : 1;
            init_meter();
            SetTimer( hwnd, MY_TIMER, 1000, NULL );		// таймер показа окна и время maintenance
            SetTimer( hwnd, CL_TIMER, ( 1000 * 60 ), NULL );	//msec = 1 min
//  --
            if( wx_maintenance ) sprintf( popicon, "icon_ok");
            else sprintf( popicon, "icon_off");

            if( !create_tray_icon(hInsts, hwnd, popicon, poptxt1 )){	// создать иконку в трее
                prn_log("create_tray_icon - unsuccessful attempt. Exit for reinitial.");
                SendMessage( hGW, WM_DESTROY, (WPARAM)NULL, (LPARAM)NULL );	//завершить процесс
            }
        break;

        case WM_CLOSE:
            ShowWindow( hwnd, SW_HIDE );// скрыть окно
            iTray = 1;
        break;
//  --
        case WM_DESTROY:
            if(DEBUG) prn_log("WM_DESTROY");
            PostQuitMessage( 0 );
            destroy_tray_icon();	// удалить иконку
        break;
//  --
        default:
            return DefWindowProc( hwnd, wmsg, wparam, lparam );
    }
    return 0;
}
//  -------------------------------------------------------------------------
void smMainten( HWND hwnd ){
    hComBo = CreateWindow( WC_COMBOBOX, TEXT("������"), CBS_DROPDOWNLIST | WS_VSCROLL | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
        125, 55, 90, 150, hwnd, NULL, hInsts, NULL);

    SendMessage(hComBo, CB_ADDSTRING, 0, (LPARAM)"15 �����");
    SendMessage(hComBo, CB_ADDSTRING, 0, (LPARAM)"30 �����");
    SendMessage(hComBo, CB_ADDSTRING, 0, (LPARAM)"1 ���");
    SendMessage(hComBo, CB_ADDSTRING, 0, (LPARAM)"3 ����");
    SendMessage(hComBo, CB_ADDSTRING, 0, (LPARAM)"8 �����");
    SendMessage(hComBo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
//  --

    HWND hButY = CreateWindow( "BUTTON", TEXT("����������"), WS_CHILD | WS_VISIBLE,
        30, 100, 90, 28, hwnd, (HMENU)BTN_YES, hInsts, NULL);

    HWND hButN = CreateWindow( "BUTTON", TEXT("��������"), WS_CHILD | WS_VISIBLE,
        220, 100, 90, 28, hwnd, (HMENU)BTN_NO, hInsts, NULL);

    ShowWindow( hwnd, SW_SHOW );
}
//  -------------------------------------------------------------------------
void NoMainten( HWND hwnd ){
//  --
    ShowWindow( hwnd, SW_HIDE );
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
DWORD main( int argc, char *argv[] ){
    if( argc != 3 ){
        printf("\nA user process for SyMon !\n   *** Version 2.7 ***\n");
        return -1;
    }
//  --
    char buf[128];
    convert_utf8_to_windows1251( argv[2], buf, strlen( argv[2] ));
    open_log( buf );
//  --
    ZeroMemory( &buf, sizeof( buf ));
    convert_utf8_to_windows1251( argv[1], buf, strlen( argv[1] ));
    if( !init_ipc( buf )) return -1;
//  --
    hInsts = GetModuleHandle(NULL);
    HWND hwnd;
    MSG msg;
    WNDCLASSEX w;

    if( wait_init_ipc()){
//  --
        HDC hDCScreen =  GetDC(NULL);
        int Horres = GetDeviceCaps(hDCScreen,  HORZRES);
        int Vertres = GetDeviceCaps(hDCScreen,  VERTRES);
        ReleaseDC(NULL, hDCScreen);
//  --
        w.hInstance = hInsts;
        w.lpszClassName = "WindowsApp";
        w.lpfnWndProc = (WNDPROC)WindProc;
        w.style = CS_DBLCLKS;//    w.style = CS_HREDRAW | CS_VREDRAW;

        w.cbSize = sizeof( WNDCLASSEX );
        w.hIcon = LoadIcon( hInsts,"icon_ok" );
        w.hIconSm = LoadIcon( hInsts,"icon_ok" );
        w.hCursor = LoadCursor ( NULL, IDC_ARROW );
        w.lpszMenuName = NULL;
        w.cbClsExtra = 0;
        w.cbWndExtra = 0;
        w.hbrBackground = ( HBRUSH ) COLOR_BACKGROUND; //= (HBRUSH)GetStockObject(WHITE_BRUSH);
//        w.hbrBackground = ( HBRUSH )GetStockObject(WHITE_BRUSH);
        if( !RegisterClassEx ( &w )) return 0;
        hwnd = CreateWindowEx( WS_EX_TOOLWINDOW | WS_EX_TOPMOST, w.lpszClassName, "Session Monitor",WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, ( Horres - 400 ), (( Vertres/2 ) + 100), 340, 180, HWND_DESKTOP, NULL, hInsts, NULL );

//  --
        if( wx_maintenance ){
            smMainten( hwnd );
        }else{
            NoMainten( hwnd );
        }
        UpdateWindow(hwnd);
//  --
        while( GetMessage( &msg, NULL, 0, 0 )){
            TranslateMessage( &msg );
            DispatchMessage( &msg );
            if( CTM_VIS == 0 ) break;
        }
//  --
    }// if( wait_init_send
    close_ipc();
    close_log();
    return msg.wParam;
}
//  -------------------------------------------------------------------------
