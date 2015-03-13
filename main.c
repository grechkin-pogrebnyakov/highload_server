#include "main.h"

static void show_help( uint32_t port, char *document_root, uint16_t ncpu ) {
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

pid_t multi_fork(const int count) {
    pid_t pid = 1;
    for (int i = 0; i < count; ++i) {
        pid = fork();
        if (!pid) {
            break;
        }
    }
    return pid;
}

static int parse_params( char **params, int count, char **document_root, int32_t *port, int16_t *ncpu ) {
    int i;
    char *param;
    for( i = 0; i < count; ++i ) {
        param = params[i];
        if( *param != '-' ) return 1;

        param++;

        switch ( *param ) {
        case 'r':
            *document_root = params[++i];
            break;
        case 'c':
            *ncpu = atoi( params[++i] );
            break;
        case 'p':
            *port = atoi( params[++i] );
            break;
        case 'l':
            log_level = atoi( params[++i] );
            break;
        default:
            return 1;

        }
    }
    return 0;
}

static int try_params( char *doc_root, int32_t des_port, int16_t des_ncpu, uint16_t *ncpu, uint32_t *port ) {
    if ( des_port < 0 || des_port > 65535 ) {
        fprintf( stderr, "Uncorrect port number.\n" );
        return -1;
    }
    if ( des_port < 1024 && !is_root) {
        fprintf( stderr, "You must be root to use this port.\n" );
        return 1;
    }
    *port = des_port;
    if ( des_ncpu <= 0 || des_ncpu > sysconf(_SC_NPROCESSORS_ONLN) )
        printf( "NCPU that you asked is not available. %u CPUs will be used.\n", *ncpu );
    else
        *ncpu = des_ncpu;
    int res = chdir( doc_root );
    if ( res ) {
        switch ( errno ){
        case EACCES:
            fprintf( stderr, "ACCES TO %s DENIED.\n", doc_root );
            break;
        case ENOENT:
        case ENOTDIR:
            fprintf( stderr, "%s does not exist or not a directory.\n", doc_root );
            break;
        default:
            fprintf( stderr, "Document root is wrong.\n" );
        }
        return 1;
    }
    if ( log_level > 3 || log_level < 0) log_level = 3;
    return 0;
}

int main( int argc, char **argv )
{
    log_level = 3;
    is_root = !getuid();
    uint16_t ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    char *document_root = "/home/serg/highload_server_root";
    uint32_t port = 8347;

    if ( argc == 2 && !strncmp( argv[1], "-h", 2 ) ) {
       show_help( port, document_root, ncpu );
       exit( EXIT_SUCCESS );
    }
    if ( argc % 2 == 0 ) {
        fprintf( stderr, "Uncorrect prameters, use -h for help.\n" );
        exit( EXIT_FAILURE );
    }
    int32_t des_port = port;
    int16_t des_ncpu = ncpu;

    if ( argc > 1 )
        if ( parse_params ( argv + 1, argc - 1, &document_root, &des_port, &des_ncpu ) ) {
            fprintf( stderr, "Uncorrect prameters, use -h for help.\n" );
            exit( EXIT_FAILURE );
        }

    if ( try_params( document_root, des_port, des_ncpu, &ncpu, &port ) )
        exit( EXIT_FAILURE );

    struct sockaddr_in sin;
    memset( &sin, 0, sizeof(sin) );
    sin.sin_family = AF_INET;    /* работа с доменом IP-адресов */
    sin.sin_addr.s_addr = htonl( INADDR_ANY );  /* принимать запросы с любых адресов */
    sin.sin_port = htons( port );
    int socketfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (bind( socketfd, (struct sockaddr *)&sin, sizeof( sin ) ) == -1 )
    {
        switch ( errno ){
        case EADDRINUSE:
            fprintf( stderr, "PORT %u ALLREADY IN USE.\n", port );
            break;
        default:
            fprintf( stderr, "Document root is wrong.\n" );
        }
        return 1;
    }

    pid = multi_fork(ncpu);

    main_loop( socketfd );

    return 0;
}
