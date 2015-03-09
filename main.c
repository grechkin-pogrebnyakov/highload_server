#include "main.h"

static void show_help( void ) {
    printf( "Usage: ./httpd [OPTIONS]\n" );
    printf( "\nOPTIONS:\n" );
    printf( "\t-h -- show this help\n\n" );
    printf( "\t-p PORT -- bind port\n" );
    printf( "\t\tdefault = %u\n\n", port );
    printf( "\t-r DOCUMENT_ROOT -- server root directory\n" );
    printf( "\t\tdefault = %s\n\n", document_root );
    printf( "\t-c NCPU -- number of user CPUs\n" );
    printf( "\t\tdefault is number of CPUs on device ( %u )\n", ncpu );
}

static int parse_params( int count, char **params) {
    int i;
    char *param;
    for( i = 0; i < count; ++i ) {
        param = params[i];
        if( *param != '-' ) return 1;

        param++;

        switch ( *param ) {
        case 'r':
            document_root = params[++i];
            break;
        case 'c':
            ncpu = atoi( params[++i] );
            break;
        case 'p':
            port = atoi( params[++i] );
            break;
        default:
            return 1;

        }
    }
    return 0;
}

static int try_params( char *doc_root, int port ) {
    if ( port < 0 || port > 65535 ) {
        fprintf( stderr, "Uncorrect port number.\n" );
        return -1;
    }
    if ( port < 1024 && ! is_root) {
        fprintf( stderr, "You must be root to use this port.\n" );
        return 1;
    }
}

int main( int argc, char **argv )
{
    is_root = !getuid();
    ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    document_root = "/home/serg/highload_server_root";
    port = 8347;

    struct sockaddr_in sin;
    if ( argc == 2 && !strncmp( argv[1], "-h", 2 ) ) {
       show_help();
       exit( EXIT_SUCCESS );
    }
    if ( argc % 2 == 0 ) {
        fprintf( stderr, "Uncorrect prameters, use -h for help.\n" );
        exit( EXIT_FAILURE );
    }

    if ( argc > 1 )
        if ( parse_params ( argc - 1, argv + 1 ) ) {
            fprintf( stderr, "Uncorrect prameters, use -h for help.\n" );
            exit( EXIT_FAILURE );
        }



    memset( &sin, 0, sizeof(sin) );
    sin.sin_family = AF_INET;    /* работа с доменом IP-адресов */
    sin.sin_addr.s_addr = htonl( INADDR_ANY );  /* принимать запросы с любых адресов */
    sin.sin_port = htons( port );

    main_loop( &sin );

    return 0;
}
