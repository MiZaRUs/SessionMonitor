/***********************************************************************
 *  SyMon                                                              *
 *  Copyright (C) 2014-2015   by  Oleg Shirokov    olgshir@gmail.com   *
 *                                                                     *
 ***********************************************************************/
#include <windows.h>
//  --
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
//  --
#include "config.h"
#include "logger.h"
// =========================================================================
int get_string_proper( char *sstr, char *sfind, char *res ){		// получение строки значений параметра
    unsigned int sflen = strlen(sfind);
    unsigned int j = 0;
    for(; isalpha( sstr[j] ); j++ ){};					// к концу слова
    if(( j == sflen )&&( !strncmp( sstr, sfind, sflen ))){		// если сходится
        for(; j < strlen( sstr ); j++ ) if( isalnum( sstr[ j ] )) break;// отмечаем начало значения
        strcpy( res, sstr+j );						// копируем в выходной буфер
//prn_log( res );
        return 1;
    }
return 0;
}
//  -----------------------------------
int get_string_param( char *sstr, char *res ){		// получение строки параметров с отсеченными пропусками
    unsigned int i = 0;
    unsigned int slen = strlen(sstr);
    if( slen < 4 ) return 0;
    for(; i < slen; i++ ){
        if(( sstr[i] == '#' )||( sstr[i] == ';' )) return 0;
        if( isalpha( sstr[i] )) break;				// к началу слова
    }
    unsigned int j = i;
    for(; j < slen; j++ ) if( sstr[j] == '#' ) break;		// к концу строки или #
    j--;
    while( !isalnum( sstr[--j] )){};				// возврат к концу значения
    j = j - i + 1;
    if( j < 3 ) return 0;
//  --
    strncpy( res, sstr+i, j );					// копируем в выходной буфер
    res[ j ] = '\0';
    return 1;
}
//  -----------------------------------
//  -----------------------------------
int init_conf( char *path, char *name, CONFIG *par ){
    strcpy( par->log_debug, "On" );		// "debug"
    strcpy( par->zbx_host, "sm-conf" );		// "zbxHost"
    strcpy( par->zbx_server, "0" );		// "zbxServer"
    strcpy( par->zbx_port, "0" );		// "zbxPort"
    strcpy( par->cygwin, "0" );			// "cygwin"
    strcpy( par->maintenance, "0" );		// "Maintenance"
    strcpy( par->user_proc, "0" );		// "userProc"
    strcpy( par->git_exec, "0" );		// "gitExec"
//  --

//  -- Определить cygwin-путь
    char *cygwin_path = malloc(( strlen( path ) + strlen( name ) + 24 ));	//+"/cygdrive/" + conf/ + .conf
    unsigned int it = sprintf( cygwin_path, "/cygdrive/%c", path[0] );
    unsigned int i=2;		// сдвиг на "d:"
    for(; i < strlen( path ); i++, it++ ){
        if( path[ i ] == '\\' ){
            cygwin_path[ it ] = '/';
        }else{
            cygwin_path[ it ] = path[ i ];
        }
    }
    sprintf( cygwin_path + it, "conf/%s.conf", name );
//  -- prn_log( cygwin_path );

    FILE *hf = NULL;
    if(( hf = fopen( cygwin_path, "r")) == NULL){
        prn_err( "Open config-file failed. Variables are set by default.", GetLastError() );
    }else{
        const int bufsize = 128;
        char *buf = malloc( bufsize );
        char *sparam = malloc( bufsize );
//        static char sip[128];
//  --
        while( 1 ){
            memset( buf, '\0', bufsize );
            if( fgets( buf, bufsize, hf ) == NULL) break;
//  --
            if( get_string_param( buf, sparam )){
                get_string_proper( sparam, "debug",     par->log_debug );
                get_string_proper( sparam, "zbxHost",   par->zbx_host );
                get_string_proper( sparam, "zbxServer", par->zbx_server );
                get_string_proper( sparam, "zbxPort",   par->zbx_port );
                get_string_proper( sparam, "cygwin",    par->cygwin );
                get_string_proper( sparam, "gitExec",   par->git_exec );
                get_string_proper( sparam, "userProc",  par->user_proc );
                get_string_proper( sparam, "Maintenance",   par->maintenance );
            }
        }
        free( sparam );
        free( buf );
        fclose( hf );
    }
    if( cygwin_path ) free( cygwin_path );
//  --
return 1;
}
/*******************************************************************************/
