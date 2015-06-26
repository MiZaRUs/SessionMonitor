/***********************************************************************
 *  SMonitir                                                           *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>

//#include <stdlib.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <string.h>
//  --
#include "defs.h"
//  -------------------------------------------------------------------------
static NOTIFYICONDATA nid; 
static volatile HINSTANCE hInst;
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
int create_tray_icon( HINSTANCE hi, HWND hw, char *icon, char *msg ){	// создать иконку
    hInst = hi;
//  --
    nid.cbSize = sizeof( NOTIFYICONDATA );
    nid.hWnd = hw;
    nid.uID = 0;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = MYWM_NOTIFYICON;	// Сообщение, при клике по иконке defs.h
//  --
    nid.hIcon = LoadIcon( hInst, icon );
    lstrcpyn(nid.szTip, msg, sizeof(nid.szTip));
return Shell_NotifyIcon( NIM_ADD, &nid );
}
//  -------------------------------------------------------------------------
int update_tray_icon( char *icon, char *msg ){				// изменить иконку
    if( !hInst ) return -1;
    nid.hIcon = LoadIcon( hInst, icon );
    lstrcpyn(nid.szTip, msg, sizeof(nid.szTip));
return Shell_NotifyIcon( NIM_MODIFY, &nid );
}
//  -------------------------------------------------------------------------
void destroy_tray_icon( void ){						// удалить иконку
if( !hInst ) return;
    Shell_NotifyIcon( NIM_DELETE, &nid );
}
//  -------------------------------------------------------------------------
void regen_tray_icon( void ){						// обновить иконку
    if( !hInst ) return;
//    Shell_NotifyIcon( NIM_DELETE, &nid );
//    Shell_NotifyIcon( NIM_ADD, &nid );
    Shell_NotifyIcon( NIM_MODIFY, &nid );
}
//  -------------------------------------------------------------------------
void clear_tray_icon( HWND hw ){						// удалить иконку
    if( !hw ) return;
    NOTIFYICONDATA nix;
    nix.cbSize = sizeof( NOTIFYICONDATA );
    nix.hWnd = hw;
    nix.uID = 0;
    nix.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
//    nix.uCallbackMessage = NULL;
    nix.hIcon = NULL;
//    nix.szTip = NULL;
    Shell_NotifyIcon( NIM_DELETE, &nix );
}
//  -------------------------------------------------------------------------
//  -------------------------------------------------------------------------
