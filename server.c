#include "server.h"

int file_exist (char *filename)
{
  struct stat   buffer;
  return (stat (filename, &buffer) == 0);
}

int urlndecode(char* dst, int dst_size, const char* src, int src_size) {
    char a, b;
    int i = 0, j = 0;
    while (*src && (i < src_size) && (j < dst_size)) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = (char) (16 * a + b);
            src += 3;
            i += 3;
            j++;
        }
        else {
            *dst++ = *src++;
            i++;
            j++;
        }
    }
    if ( j < dst_size )
        *dst++ = '\0';
    return j;
}

void print_log( char* format, ... )
{
    va_list args;
    va_start( args, format );
    if( format && ( log_level > 0 ) )
    {
        vprintf( format, args );
        printf( "\n" );
    }
    va_end( args );
}

char* current_datatime( void ) {
    time_t _t = time( NULL );
    char *time_str = asctime( gmtime( &_t ) );
    time_str[strlen(time_str) - 1] = '\0';
    return time_str;
}

static inline void free_cn( conn_t **cn_ptr ) {
    if ( (!cn_ptr) || !(*cn_ptr) )
        return;
    conn_t *cn = *cn_ptr;
    if ( cn->addr ) free( cn->addr );
    if ( cn->method ) free( cn->method );
    if ( cn->request ) free( cn->request );
    if ( cn->data != -1 ) {
        close( cn->data );
    }
//    if ( cn->vary ) free( cn->vary );
    cn->addr = NULL;
    cn->method = NULL;
    cn->request = NULL;
    cn->data = -1;
    cn->content_type = NULL;
    free( cn );
    cn = NULL;
}

static inline char* http_status_descr( uint32_t status ) {
    switch ( status ) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 408:
        return "Request timeout";
    case 503:
        return "Service Unavailable";
    case 505:
        return "HTTP Version Not Supported";
    default:
        return " ";
    }
}

#define SET_HTTP_DATA(_buf, _data, _data_len)\
    do {\
    evbuffer_add_printf(_buf, "\r\n");\
    evbuffer_add(_buf, _data, _data_len);\
    } while(0)

#define SET_HTTP_DATA_FILE(_buf, _data, _data_len)\
    do {\
    evbuffer_add_printf(_buf, "\r\n");\
    evbuffer_add_file(_buf, _data, 0, _data_len);\
    } while(0)

#define SET_HTTP_STR_HEADER(_buf, _key, _value)\
    evbuffer_add_printf( _buf, "%s: %s\r\n", _key, _value ? : "" )

#define SET_HTTP_SIZE_T_HEADER(_buf, _key, _value)\
    evbuffer_add_printf( _buf, "%s: %zu\r\n", _key, _value )

#define SET_HTTP_STATUS(_buf, _status)\
    evbuffer_add_printf( _buf, "HTTP/1.1 %u %s\r\n", _status, http_status_descr( _status ) )

/* Функция обратного вызова для события: данные готовы для записи в buf_ev */
static void connection_write_cb( struct bufferevent *buf_ev, void *arg )
{
    conn_t *cn = (conn_t*)arg;
    if( !cn->read_finished || !cn->data_ready )
    {
        printf("read not finished\n");
        return;
    }
    if( cn->write_finished )
    {
        print_log("[cn_id=%lu_%lu]closing\n", pid, cn->id);
        free_cn( &cn );
        bufferevent_free( buf_ev );
        return;
    }
    struct evbuffer *buf_output = bufferevent_get_output( buf_ev );
    SET_HTTP_STATUS( buf_output, cn->status );

    SET_HTTP_STR_HEADER( buf_output, "Content-Type", cn->content_type );
 //   SET_HTTP_STR_HEADER( buf_output, "Vary" , cn->vary );
    SET_HTTP_STR_HEADER( buf_output, "Date", current_datatime() );
    SET_HTTP_STR_HEADER( buf_output, "Connection", "close" );
    SET_HTTP_STR_HEADER( buf_output, "Server", "Grechkin-Pogrebnyakov Server" );
    SET_HTTP_STR_HEADER( buf_output, "Cache-Control", "private" );
    if( cn->data == -1 ) {
        char *data = http_status_descr( cn->status );
        SET_HTTP_SIZE_T_HEADER( buf_output, "Content-Length", strlen( data ) );
        SET_HTTP_DATA( buf_output, data, strlen(data) );
    }
    else
    {
        SET_HTTP_SIZE_T_HEADER( buf_output, "Content-Length", cn->data_len );
        if ( !(( cn->status == 200 ) && ( !strcasecmp( cn->method, "HEAD" ) ) ) )
            SET_HTTP_DATA_FILE( buf_output, cn->data, cn->data_len );
        else
            SET_HTTP_DATA( buf_output, NULL, 0 );

    }
    cn->write_finished = 1;
}

static char* get_type( char *file_name ) {
    char *extension = strrchr( file_name, '.' );
    if ( !extension )
        return "text/html";
    extension++;
    if ( strstr( extension, "html" ) ) {
        return "text/html";
    } else if ( !strcmp( extension, "css" ) ) {
        return "text/css";
    } else if ( !strcmp( extension, "js" ) ) {
        return "application/x-javascript";
    } else if ( !strcmp( extension, "jpg" ) ) {
        return "image/jpeg";
    } else if ( !strcmp( extension, "jpeg" ) ) {
        return "image/jpeg";
    } else if ( !strcmp( extension, "png" ) ) {
        return "image/png";
    } else if ( !strcmp( extension, "gif" ) ) {
        return "image/gif";
    } else if ( !strcmp( extension, "swf" ) ) {
        return "application/x-shockwave-flash";
    } else {
        return "text/html";
    }
}

static void prepare_data( struct bufferevent *buf_ev, conn_t *cn ) {
    if ( !cn->method || !cn->request ) {
        cn->data = open( "400.html", O_RDONLY );
        cn->content_type = "text/html";
        cn->status = 400;
    } else if ( strcmp( cn->method, "GET" ) && strcmp( cn->method, "HEAD" ) ) {
        cn->data = open( "405.html", O_RDONLY );
        cn->content_type = "text/html";
        cn->status = 405;
    } else if ( strstr( cn->request, "/." )) {
        cn->data = open( "404.html", O_RDONLY );
        cn->content_type = "text/html";
        cn->status = 404;
    } else {
        cn->status = 200;
        cn->data = open( cn->request + 1, O_RDWR );
        if ( cn->data == -1 ) {
            if ( cn->is_index_request ) {
            cn->data = open( "403.html", O_RDONLY );
            cn->content_type = "text/html";
            cn->status = 403;
            } else {
                cn->data = open( "404.html", O_RDONLY );
                cn->content_type = "text/html";
                cn->status = 404;
            }
        } else {
            char *file = strrchr( cn->request, '/' );
            if ( file )
                cn->content_type = get_type( file );
            else
                cn->content_type = "text/html";
        }
    }
    struct stat buf;
    fstat(cn->data, &buf);
    cn->data_len = buf.st_size;
    cn->data_ready = 1;
}



/* Функция обратного вызова для события: данные готовы для чтения в buf_ev */
static void connection_read_cb( struct bufferevent *buf_ev, void *arg )
{
    conn_t *cn = (conn_t*)arg;
    struct evbuffer *buf_input = bufferevent_get_input( buf_ev );
    while ( evbuffer_get_length( buf_input ) ) {
        size_t line_len = 0;
        char *request = evbuffer_readln( buf_input, &line_len, EVBUFFER_EOL_CRLF);
        if ( request ) {
            if ( line_len == 0 ) {
                cn->read_finished = 1;
                evbuffer_drain( buf_input, 1024 );
                bufferevent_disable( buf_ev, EV_READ );
                prepare_data( buf_ev, cn );
                bufferevent_enable( buf_ev, EV_WRITE );
            } else if ( !cn->read_url ) {
                cn->read_url = 1;
                char* method_end = strchr( request, ' ' );
                char *uri = strchr( request, '/' );
                char *uri_end = NULL;
                int uri_len = 0;
                if ( uri )
                {
                    uri_end = strchr( uri, ' ' );
                    char *question_pos = strchr( uri, '?' );
                    if ( uri_end && question_pos && ( question_pos < uri_end ) )
                        uri_end = question_pos;
                }
                if ( uri && uri_end )
                {
                    uri_len = (int)(uri_end - uri);
                    char decode_buff[1024];
                    int decode_len = urlndecode( decode_buff, sizeof(decode_buff) - 1, uri, uri_len );
                    decode_buff[decode_len] = '\0';
                    if ( decode_buff[decode_len - 1] == '/' )
                    {
                        if (file_exist(decode_buff+1))
                            cn->is_index_request = 1;
                        cn->request = (char*)malloc(decode_len + 11);
                        snprintf( cn->request, decode_len + 11, "%s%s", decode_buff, "index.html" );
                    } else
                        cn->request = strndup( decode_buff, decode_len );
                }
                if ( method_end )
                    cn->method = strndup( request, method_end - request );
                print_log("[cn_id=%lu_%lu] %s %s %s", pid, cn->id, cn->method, cn->addr, cn->request );
            } /*else {
                char *key_end = strchr( request, ':' );
                if (
            }*/
            free( request );
        } else
            break;
    }
}

static void connection_event_cb( struct bufferevent *buf_ev, short events, void *arg )
{
    if( events & (BEV_EVENT_TIMEOUT|BEV_EVENT_READING) )
    {
        conn_t *cn = (conn_t*)arg;
        print_log("[cn_id=%lu_%lu] W read timeout.", pid, cn->id);
        return;
    }
    if( events & BEV_EVENT_ERROR )
    {
        perror( "Ошибка объекта bufferevent" );
        free_cn( (conn_t**)&arg );
        bufferevent_free( buf_ev );
        return;
    }
    if( events & (BEV_EVENT_EOF | BEV_EVENT_ERROR) )
    {
        free_cn( (conn_t**) &arg );
        bufferevent_free( buf_ev );
        return;
    }
}

static void accept_connection_cb( struct evconnlistener *listener,
                   evutil_socket_t fd, struct sockaddr *addr, int sock_len,
                   void *arg )
{
  /* При обработке запроса нового соединения необходимо создать для него
     объект bufferevent */
    struct event_base *base = evconnlistener_get_base( listener );
    struct bufferevent *buf_ev = bufferevent_socket_new( base, fd, BEV_OPT_CLOSE_ON_FREE );
    conn_t *cn = (conn_t*)malloc(sizeof(conn_t));
    memset( cn, 0, sizeof(conn_t) );
    cn->id = conn_id++;
    cn->addr = (char*)malloc( sock_len );
    inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr),
                                cn->addr, sock_len);
    bufferevent_setcb( buf_ev, connection_read_cb, connection_write_cb, connection_event_cb, cn );
    bufferevent_enable( buf_ev, EV_READ );
    bufferevent_disable( buf_ev,  EV_WRITE );
    struct timeval timeout = {3, 0};
    bufferevent_set_timeouts( buf_ev, &timeout, &timeout );
}

static void accept_error_cb( struct evconnlistener *listener, void *arg )
{
    struct event_base *base = evconnlistener_get_base( listener );
    int error = EVUTIL_SOCKET_ERROR();
    fprintf( stderr, "Ошибка %d (%s) в мониторе соединений. Завершение работы.\n",
            error, evutil_socket_error_to_string( error ) );
    free_cn( (conn_t**)&arg );
    event_base_loopexit( base, NULL );
}



int main_loop( int socketfd ) {
    struct event_base *base;
    struct evconnlistener *listener;
    conn_id = 0;

    base = event_base_new();
    if( !base )
    {
        fprintf( stderr, "Ошибка при создании объекта event_base.\n" );
        return -1;
    }

    listener = evconnlistener_new( base, accept_connection_cb, NULL,
                                        (LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE),
                                        -1, socketfd );
    if( !listener )
    {
        perror( "Ошибка при создании объекта evconnlistener" );
        return -1;
    }
    evconnlistener_set_error_cb( listener, accept_error_cb );

    event_base_dispatch( base );

    return 0;
}
