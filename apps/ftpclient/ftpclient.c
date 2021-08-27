#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "ftpclient.h"
#include <string.h>
#include <errno.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


#define SERVER_PORT 21
#define BUF_SIZE 4096
#define CYGNUM_NET_FTPCLIENT_BUFSIZE 512
//char buf[BUF_SIZE] = "orange test.";

/* Build one command to send to the FTP server */
    static int 
build_cmd(char *buf,
        unsigned bufsize,
        char *cmd, 
        char *arg1)
{
    int cnt;

    if (arg1) {
        cnt = snprintf(buf,bufsize,"%s %s\r\n",cmd,arg1);
    } else {
        cnt = snprintf(buf,bufsize,"%s\r\n",cmd);
    }

    if (cnt < bufsize) 
        return 1;

    return 0;
}


/* Read one line from the server, being careful not to overrun the
   buffer. If we do reach the end of the buffer, discard the rest of
   the line. */
    static int 
get_line(int s, char *buf, unsigned buf_size) 
{
    int eol = 0;
    int cnt = 0;
    int ret;
    char c;

    while(!eol) {
        ret = read(s,&c,1);

        if (ret != 1) {
            printf("read %s\n",strerror(errno));
            return FTP_BAD;
        }

        if (c == '\n') {
            eol = 1;
            continue;
        }

        if (cnt < buf_size) {
            buf[cnt++] = c;
        }   
    }
    if (cnt < buf_size) {
        buf[cnt++] = '\0';
    } else {
        buf[buf_size -1] = '\0';
    }
    return 0;
}  

/* Read the reply from the server and return the MSB from the return
   code. This gives us a basic idea if the command failed/worked. The
   reply can be spread over multiple lines. When this happens the line
   will start with a - to indicate there is more*/
    static int 
get_reply(int s) 
{
    char buf[BUFSIZ];
    int more = 0;
    int ret;
    int first_line=1;
    int code=0;

    do {

        if ((ret=get_line(s,buf,sizeof(buf))) < 0) {
            return(ret);
        }

        printf("FTP: %s\n",buf);

        if (first_line) {
            code = strtoul(buf,NULL,0);
            first_line=0;
            more = (buf[3] == '-');
        } else {
            if (isdigit(buf[0]) && isdigit(buf[1]) && isdigit(buf[2]) &&
                    (code == strtoul(buf,NULL,0)) && 
                    buf[3]==' ') {
                more=0;
            } else {
                more =1;
            }
        }
    } while (more);

    return (buf[0] - '0');
}

/* Send a command to the server */
    static int 
send_cmd(int s, char * msgbuf) 
{  
    int len;
    int slen = strlen(msgbuf);

    if ((len = write(s,msgbuf,slen)) != slen) {
        if (len < 0) {
            printf("write %s\n",strerror(errno));
            return FTP_BAD;
        } else {
            printf("write truncated!\n");
            return FTP_BAD;
        }
    }
    return 1;
}

/* Send a complete command to the server and receive the reply. Return the 
   MSB of the reply code. */
    static int 
command(char * cmd, 
        char * arg, 
        int s, 
        char *msgbuf, 
        int msgbuflen) 
{
    int err;

    if (!build_cmd(msgbuf,msgbuflen,cmd,arg)) {
        printf("FTP: %s command to long\n",cmd);
        return FTP_BAD;
    }

    printf("FTP: Sending %s command\n",cmd);

    if ((err=send_cmd(s,msgbuf)) < 0) {
        return(err);
    }

    return (get_reply(s));
}

/* Open a socket and connect it to the server. Also print out the
   address of the server for debug purposes.*/

    static int
connect_to_server(char *hostname, 
        struct sockaddr* local) 
{ 
    int s;
    socklen_t len;
    int error;
    struct addrinfo *res, *nai;
    char name[80];
    char port[8];

    printf("%s:%d \n", __func__, __LINE__);
    error = getaddrinfo(hostname, "ftp", NULL, &res);
    if (error != EAI_NONE) {
        printf("%s:%d \n", __func__, __LINE__);
        return FTP_NOSUCHHOST;
    }

    nai = res;
    printf("%s:%d \n", __func__, __LINE__);

    while (nai) {
        printf("%s:%d \n", __func__, __LINE__);
        s = socket(nai->ai_family, nai->ai_socktype, nai->ai_protocol);
        if (s < 0) {
            printf("%s:%d \n", __func__, __LINE__);
            nai = nai->ai_next;
        }

        printf("%s:%d \n", __func__, __LINE__);
        if (connect(s, nai->ai_addr, nai->ai_addrlen) < 0) {
            getnameinfo(nai->ai_addr, nai->ai_addrlen, 
                    name, sizeof(name), NULL,0, NI_NUMERICHOST);
            close(s);
            nai = nai->ai_next;
            printf("%s:%d name: %s\n", __func__, __LINE__, name);
            continue;
        }
        printf("%s:%d \n", __func__, __LINE__);

        len = sizeof(struct sockaddr);
        if (getsockname(s, (struct sockaddr *)local, &len) < 0) {
            printf("getsockname failed %s\n",strerror(errno));
            close(s);
            nai = nai->ai_next;
            printf("%s:%d \n", __func__, __LINE__);
            continue;
        }
        printf("%s:%d \n", __func__, __LINE__);
        getnameinfo(nai->ai_addr, nai->ai_addrlen,
                name, sizeof(name), port, sizeof(port),
                NI_NUMERICHOST|NI_NUMERICSERV);

        printf("FTP: Connected to %s:%s\n", name, port);
        freeaddrinfo(res);
        return (s);
    }

    return FTP_NOSUCHHOST;
}

/* Perform a login to the server. Pass the username and password and
   put the connection into binary mode. This assumes a passwd is
   always needed. Is this true? */

static int 
login(char * username, 
        char *passwd, 
        int s, 
        char *msgbuf, 
        unsigned msgbuflen) {

    int ret;

    ret = command("USER",username,s,msgbuf,msgbuflen);
    if (ret != 3) {
        printf("FTP: User %s not accepted\n",username);
        return (FTP_BADUSER);
    }

    ret = command("PASS",passwd,s,msgbuf,msgbuflen);
    if (ret < 0) {
        return (ret);
    }
    if (ret != 2) {
        printf("FTP: Login failed for User %s\n",username);
        return (FTP_BADUSER);
    }

    printf("FTP: Login sucessfull\n");

    ret = command("TYPE","I",s,msgbuf,msgbuflen);
    if (ret < 0) {
        return (ret);
    }
    if (ret != 2) {
        printf("FTP: TYPE failed!\n");
        return (FTP_BAD);
    }
    return (ret);
}


/* Open a data socket. This is a client socket, i.e. its listening
   waiting for the FTP server to connect to it. Once the socket has been
   opened send the port command to the server so the server knows which
   port we are listening on.*/
    static int 
opendatasock(int ctrl_s,
        struct sockaddr *ctrl, 
        char *msgbuf, 
        unsigned msgbuflen) 
{
    struct sockaddr local;
    char name[64];
    char port[10];
    socklen_t len;
    int on = 1;
    char buf[80];
    int ret;
    int s;

    s = socket(ctrl->sa_family, SOCK_STREAM, 0);
    if (s < 0) {
        printf("socket: %s\n",strerror(errno));
        return FTP_BAD;
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0) {
        printf("setsockopt: %s\n",strerror(errno));
        close(s);
        return FTP_BAD;
    }

    memcpy(&local,ctrl,sizeof(struct sockaddr));
    switch (ctrl->sa_family) {
        case AF_INET: {
                          struct sockaddr_in * sa4 = (struct sockaddr_in *) &local;
                          sa4->sin_port = 0;
                          break;
                      }
#ifdef CYGPKG_NET_INET6
        case AF_INET6: {
                           struct sockaddr_in6 * sa6 = (struct sockaddr_in6 *) &local;
                           sa6->sin6_port = 0;
                           break;
                       }
#endif
        default:
                       close (s);
                       return FTP_BAD;
    }

    if (bind(s,&local,sizeof(local)) < 0) {
        printf("bind: %s\n",strerror(errno));
        close(s);
        return FTP_BAD;
    }

    len = sizeof(local);
    if (getsockname(s,&local,&len) < 0) {
        printf("getsockname: %s\n",strerror(errno));
        close(s);
        return FTP_BAD;
    }

    if (listen(s, 1) < 0) {
        printf("listen: %s\n",strerror(errno));
        close(s);
        return FTP_BAD;
    }

    getnameinfo(&local, sizeof(local), name, sizeof(name), port, sizeof(port),
            NI_NUMERICHOST|NI_NUMERICSERV);
    switch (local.sa_family) {
        case AF_INET: {
                          snprintf(buf, sizeof(buf), "|1|%s|%s|", name, port);
                          break;
                      }
#ifdef CYGPKG_NET_INET6
        case AF_INET6: {
                           snprintf(buf, sizeof(buf), "|2|%s|%s|", name, port);
                           break;
                       }
#endif
        default:
                       close (s);
                       return (FTP_BAD);
    }

    ret = command("EPRT",buf,ctrl_s,msgbuf,msgbuflen);
    if (ret < 0) {
        close(s);
        return (ret);
    }

    if (ret != 2) {
        int _port = atoi(port);
        char *str = name;
        while (*str) {
            if (*str == '.') *str = ',';
            str++;
        }
        snprintf(buf, sizeof(buf), "%s,%d,%d", name, _port/256, _port%256);
        ret = command("PORT",buf,ctrl_s,msgbuf,msgbuflen);
        if (ret < 0) {
            close(s);
            return (ret);
        }
        if (ret != 2) {
            printf("FTP: PORT failed!\n");
            close(s);
            return (FTP_BAD);
        }
    }
    return (s);
}

static int list_files(int data_s)
{
    char buf[512];
    int finished = 0;
    int len;
    int s;

    s = accept(data_s, NULL, 0);
    if(s < 0) {
        return FTP_BAD;   
    }

    do {
        len = read(s, buf, sizeof(buf));
        if (len < 0) {
        }
        printf("%s:%d len: %d, buf: %s\n", __func__, __LINE__, len, buf);

        if (len == 0) {
            finished = 1;
        }
    } while(!finished);


    return 0;
}

    static int 
receive_file(int data_s, ftp_write_t ftp_write, void *ftp_write_priv)
{
    char *buf;
    int finished = 0;
    int total_size=0;
    int len, wlen;
    int s;
    int fd;

    if ((buf = (char *)malloc(CYGNUM_NET_FTPCLIENT_BUFSIZE)) == (char *)0) {
        return FTP_NOMEMORY;
    }
    s = accept(data_s, NULL, 0);
    if (s < 0) {
        printf( "listen: %s\n",strerror(errno));
        free(buf);
        return FTP_BAD;   
    }

    fd = open("remote-confg", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return FTP_BAD;
    }

    do {
        len = read(s, buf, CYGNUM_NET_FTPCLIENT_BUFSIZE);
        if (len < 0) {
            printf( "read: %s\n",strerror(errno));
            close(s);
            free(buf);
            return FTP_BAD;   
        }

        printf("%s:%d len: %d, buf: %s\n", __func__, __LINE__, len, buf);

        if (len == 0) {
            finished = 1;
        } else {
            wlen = write(fd, buf, len);
            if (wlen != len) {
                printf( "FTP: File too big!\n");
                close(s);
                free(buf);
                return FTP_TOOBIG;
            }
            total_size += len;
        }
    } while (!finished);

    close(fd);
    close(s);
    free(buf);
    return total_size;
}

/* Receive the file into the buffer and close the socket afterwards*/
    static int 
send_file(int data_s, ftp_read_t ftp_read, void *ftp_read_priv)
{
    char *buf;
    int len, rlen;
    int s;

    if ((buf = (char *)malloc(CYGNUM_NET_FTPCLIENT_BUFSIZE)) == (char *)0) {
        return FTP_NOMEMORY;
    }
    s = accept(data_s,NULL,0);
    if (s<0) {
        printf("listen: %s\n",strerror(errno));
        free(buf);
        return FTP_BAD;   
    }

    do { 
        rlen = (*ftp_read)(buf, CYGNUM_NET_FTPCLIENT_BUFSIZE, ftp_read_priv);
        if (rlen > 0) {
            len = write(s, buf, rlen);
            if (len < 0) {
                printf("write: %s\n",strerror(errno));
                close(s);
                free(buf);
                return FTP_BAD;   
            }
        }
    } while (rlen > 0);

    close(s);
    free(buf);
    return 0;
}

/* All done, say bye, bye */
static int quit(int s, 
        char *msgbuf, 
        unsigned msgbuflen) {

    int ret;

    ret = command("QUIT",NULL,s,msgbuf,msgbuflen);
    if (ret < 0) {
        return (ret);
    }
    if (ret != 2) {
        printf("FTP: Quit failed!\n");
        return (FTP_BAD);
    }

    printf("FTP: Connection closed\n");
    return (0);
}

/* Get a file from an FTP server. Hostname is the name/IP address of
   the server. username is the username used to connect to the server
   with. Passwd is the password used to authentificate the
   username. filename is the name of the file to receive. It should be
   the full pathname of the file. buf is a pointer to a buffer the
   contents of the file should be placed in and buf_size is the size
   of the buffer. If the file is bigger than the buffer, buf_size
   bytes will be retrieved and an error code returned.  is a
   function to be called to perform printing. On success the number of
   bytes received is returned. On error a negative value is returned
   indicating the type of error. */

struct _ftp_data{
    char *buf;
    int   len;
    int   max_len;
};

static int _ftp_read(char *buf, int len, void *priv)
{
    struct _ftp_data *dp = (struct _ftp_data *)priv;
    int res = 0;

    // FTP data channel desires to write 'len' bytes.  Fetch up
    // to that amount into 'buf'
    if (dp->len > 0) {
        res = dp->len;
        if (res > len) res = len;
        memcpy(buf, dp->buf, res);
        dp->buf += len;
        dp->len -= res;
    }
    return res;
}

static int _ftp_write(char *buf, int len, void *priv)
{
    struct _ftp_data *dp = (struct _ftp_data *)priv;
    int res = 0;

    printf("%s:%d begin\n", __func__, __LINE__);
    // FTP data channel has 'len' bytes that have been read.
    // Move into 'buf', respecting the max size of 'buf'
    if (dp->len < dp->max_len) {
        res = dp->max_len - dp->len;
        if (res > len) {
            res = len;
        }
        memcpy(dp->buf, buf, res);
        dp->buf += len;
        dp->len += res;
    }
    printf("%s:%d end\n", __func__, __LINE__);
    return res;
}

static int ftp_rename(char* hostname, char* username, char* passwd, char* src, char* dst)
{
    struct sockaddr local;
    char msgbuf[256];
    int s;
    int ret;

    s = connect_to_server(hostname,&local);
    if (s < 0) {
        printf("%s:%d connect to server error.\n", __func__, __LINE__);
        return (s);
    }

    if (get_reply(s) != 2) {
        printf("FTP: Server refused connection\n");
        close(s);
        return FTP_BAD;
    }

    printf("%s:%d get reply suc.\n", __func__, __LINE__);

    ret = login(username,passwd,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        return (ret);
    }

    printf("%s:%d \n", __func__, __LINE__);
    ret = command("RNFR",src,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (ret);
    }

    printf("%s:%d \n", __func__, __LINE__);
    ret = command("RNTO",dst,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (ret);
    }

#if 0
    printf("%s:%d \n", __func__, __LINE__);
    ret = command("DELE",src,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (ret);
    }
#endif

    return ret;
}

static int ftp_list(char* hostname, char* username, char* passwd, char* dir)
{
    struct sockaddr local;
    char msgbuf[256];
    int s, data_s;
    int ret;

    s = connect_to_server(hostname,&local);
    if (s < 0) {
        printf("%s:%d connect to server error.\n", __func__, __LINE__);
        return (s);
    }

    if (get_reply(s) != 2) {
        printf("FTP: Server refused connection\n");
        close(s);
        return FTP_BAD;
    }

    printf("%s:%d get reply suc.\n", __func__, __LINE__);

    ret = login(username,passwd,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        return (ret);
    }

    data_s = opendatasock(s,&local,msgbuf,sizeof(msgbuf));
    if (data_s < 0) {
        close (s);
        printf("%s:%d \n", __func__, __LINE__);
        return (data_s);
    }

    printf("%s:%d \n", __func__, __LINE__);
    ret = command("LIST",dir,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        close(data_s);
        return (ret);
    }

    printf("%s:%d \n", __func__, __LINE__);
    if (ret != 1) {
        close (data_s);
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (FTP_BADFILENAME);
    }

    list_files(data_s);

    printf("%s:%d \n", __func__, __LINE__);
    if (get_reply(s) != 2) {
        close (data_s);
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (FTP_BAD);
    }
    printf("%s:%d \n", __func__, __LINE__);

    ret = quit(s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        close(data_s);
        printf("%s:%d \n", __func__, __LINE__);
        return (ret);
    }

    printf("%s:%d \n", __func__, __LINE__);
    close (data_s);
    close(s);
    return ret;
}

int ftp_put(char * hostname, 
        char * username, 
        char * passwd, 
        char * filename, 
        char * buf, 
        unsigned buf_size)
{
    struct _ftp_data ftp_data;

    ftp_data.buf = buf;
    ftp_data.len = buf_size;
    return ftp_put_var(hostname, username, passwd, filename, _ftp_read, &ftp_data);
}

int ftp_put_var(char *hostname,
        char *username,
        char *passwd,
        char *filename,
        ftp_read_t ftp_read,
        void *ftp_read_priv)
{

    struct sockaddr local;
    char msgbuf[256];
    int s,data_s;
    int ret;

    s = connect_to_server(hostname,&local);
    if (s < 0) {
        printf("%s:%d connect to server error.\n", __func__, __LINE__);
        return (s);
    }

    printf("%s:%d connect server suc.\n", __func__, __LINE__);

    /* Read the welcome message from the server */
    if (get_reply(s) != 2) {
        printf("FTP: Server refused connection\n");
        close(s);
        return FTP_BAD;
    }

    printf("%s:%d get reply suc.\n", __func__, __LINE__);

    ret = login(username,passwd,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        return (ret);
    }

    printf("%s:%d login suc.\n", __func__, __LINE__);

    /* We are now logged in and ready to transfer the file. Open the
       data socket ready to receive the file. It also build the PORT
       command ready to send */
    data_s = opendatasock(s,&local,msgbuf,sizeof(msgbuf));
    if (data_s < 0) {
        close (s);
        return (data_s);
    }

    printf("%s:%d open data socket suc.\n", __func__, __LINE__);

    /* Ask for the file */
    ret = command("STOR",filename,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        close(data_s);
        return (ret);
    }

    printf("%s:%d command STOR suc.\n", __func__, __LINE__);

    if (ret != 1) {
        printf("FTP: STOR failed!\n");
        close (data_s);
        close(s);
        return (FTP_BADFILENAME);
    }

    printf("%s:%d begin sending file.\n", __func__, __LINE__);

    if ((ret = send_file(data_s,ftp_read,ftp_read_priv)) < 0) {
        printf("FTP: Sending file failed\n");
        close (data_s);
        close(s);
        return (ret);
    }

    printf("%s:%d send file suc.\n", __func__, __LINE__);


    if (get_reply(s) != 2) {
        printf("FTP: Transfer failed!\n");
        close (data_s);
        close(s);
        return (FTP_BAD);
    }

    printf("%s:%d Transfer file suc.\n", __func__, __LINE__);

    ret = quit(s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        close(data_s);
        return (ret);
    }

    printf("%s:%d quit suc.\n", __func__, __LINE__);

    close (data_s);
    close(s);
    return 0;
}

static int ftp_get_var(char *hostname, char *username, char *passwd, char *filename, ftp_write_t ftp_write, void *ftp_write_priv)
{

    struct sockaddr local;
    char msgbuf[256];
    int s,data_s;
    int bytes;
    int ret;

    printf("%s:%d begin\n", __func__, __LINE__);

    s = connect_to_server(hostname,&local);
    if (s < 0) {
        printf("%s:%d \n", __func__, __LINE__);
        return (s);
    }

    printf("%s:%d \n", __func__, __LINE__);
    /* Read the welcome message from the server */
    if (get_reply(s) != 2) {
        printf("%s:%d \n", __func__, __LINE__);
        close(s);
        return FTP_BAD;
    }

    printf("%s:%d \n", __func__, __LINE__);
    ret = login(username,passwd,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (ret);
    }
    printf("%s:%d \n", __func__, __LINE__);

    /* We are now logged in and ready to transfer the file. Open the
       data socket ready to receive the file. It also build the PORT
       command ready to send */
    data_s = opendatasock(s,&local,msgbuf,sizeof(msgbuf));
    if (data_s < 0) {
        close (s);
        printf("%s:%d \n", __func__, __LINE__);
        return (data_s);
    }

    printf("%s:%d \n", __func__, __LINE__);
    /* Ask for the file */
    ret = command("RETR",filename,s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        close(data_s);
        return (ret);
    }

    printf("%s:%d \n", __func__, __LINE__);
    if (ret != 1) {
        close (data_s);
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (FTP_BADFILENAME);
    }

    printf("%s:%d \n", __func__, __LINE__);
    if ((bytes=receive_file(data_s,ftp_write,ftp_write_priv)) < 0) {
        close (data_s);
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (bytes);
    }

    printf("%s:%d \n", __func__, __LINE__);
    if (get_reply(s) != 2) {
        close (data_s);
        close(s);
        printf("%s:%d \n", __func__, __LINE__);
        return (FTP_BAD);
    }
    printf("%s:%d \n", __func__, __LINE__);

    ret = quit(s,msgbuf,sizeof(msgbuf));
    if (ret < 0) {
        close(s);
        close(data_s);
        printf("%s:%d \n", __func__, __LINE__);
        return (ret);
    }

    printf("%s:%d \n", __func__, __LINE__);
    close (data_s);
    close(s);
    return bytes;
}

static int ftp_get(char * hostname, char * username, char * passwd, char * filename, char * buf, unsigned buf_size)
{
    struct _ftp_data ftp_data;

    ftp_data.buf = buf;
    ftp_data.len = 0;
    ftp_data.max_len = buf_size;
    return ftp_get_var(hostname, username, passwd, filename, _ftp_write, &ftp_data);
}

int main(int argc, char** argv)
{
    char *buf = NULL;
    int buf_size = 0;
    int fd;
    int ret;
    struct stat sb;

    fd = open("modules.conf", O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    memset(&sb, 0, sizeof(struct stat));

    ret = fstat(fd, &sb);
    if (ret != 0) {
        return -1;
    }
    buf_size = sb.st_size;

    buf = malloc(buf_size);
    if (buf) {
        buf_size = read(fd, buf, buf_size);
    }

    close(fd);

    ftp_rename("192.168.110.138", "anonymous", "", "modules.conf", "modules.conf.1");

    ftp_list("192.168.110.138", "anonymous", "", "aspnet_client/system_web");

    ftp_put("192.168.110.138", "anonymous", "", "modules.conf", buf, buf_size);
    free(buf);

    buf = malloc(8192);
    buf_size = 8192;

    memset(buf, 0, 8192);

    ftp_get("192.168.110.138", "anonymous", "", "running-config", buf, buf_size);

    printf("%s:%d buf: %s\n", __func__, __LINE__, buf);
}
