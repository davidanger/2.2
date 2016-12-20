
/* Networking server example. It does TCP and UDP communication.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "proto.h"

#define MAXCONN 16
static int    conn[MAXCONN];
static int    do_tcp(void);
static int    do_udp(void);
static int    Sock;

static void
usage(void)
{
    fprintf(stderr, "server: [-p port] [-h] [-u]\n");
}

int
main(int argc, char **argv)
{
    int                  cc;
    uint32_t             port;
    uint32_t             udp;
    struct sockaddr_in   addr;
    socklen_t            one;

    /* bah default port
     */
    port = 8765;
    udp = 0;
    while ((cc = getopt(argc, argv, "hup:")) != EOF) {
        switch (cc) {
            case 'h':
                usage();
                return -1;
            case 'p':
                port = atoi(optarg);
                break;
            case 'u':
                udp = 1;
                break;
            case '?':
                usage();
                return -1;
        }
    }

    /* Get the mother socket
     */
    if (udp)
        Sock = socket(AF_INET, SOCK_DGRAM, 0);
    else
        Sock = socket(AF_INET, SOCK_STREAM, 0);
    if (Sock < 0) {
        perror("socket()");
        return -1;
    }

    one = 1;
    setsockopt(Sock,
               SOL_SOCKET,
               SO_REUSEADDR,
               &one,
               sizeof(socklen_t));

    for (cc = 0; cc < MAXCONN; cc++)
        conn[cc] = -1;

    /* bind() it to a specified port
     */
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    cc = bind(Sock,
              (struct sockaddr *)&addr,
              sizeof(struct sockaddr_in));
    if (cc < 0) {
        perror("bind()");
        return -1;
    }

    fprintf(stderr, "\
%d:%lu: socket %d to port %d bind() all right\n",
            getpid(), time(NULL), Sock, port);

    if (!udp)
        listen(Sock, SOMAXCONN);

    if (udp)
        do_udp();
    else
        do_tcp();

    return 0;
}

/* do_tcp()
 */
static int
do_tcp(void)
{
    int              cc;
    int              s;
    struct timeval   tv;
    int              n;
    fd_set           rs;

    while (1) {

        n = Sock;
        FD_ZERO(&rs);
        FD_SET(Sock, &rs);

        for (cc = 0; cc < MAXCONN; cc++) {

            if (conn[cc] != -1) {
                FD_SET(conn[cc], &rs);
                n = conn[cc];
            }
        }

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        ++n;
        cc = select(n, &rs, NULL, NULL, &tv);
        if (cc == 0) {
            printf("\
%d:%lu timed out\n", getpid(), time(NULL));
            continue;
        }

        /* First accept() a new connection
         * on the listening socket, reduce the
         * enqueued clients.
         */
        if (FD_ISSET(Sock, &rs)) {
            socklen_t            L;
            struct sockaddr_in   from;

            L = sizeof(struct sockaddr_in);
            s = accept(Sock,
                       (struct sockaddr *)&from,
                       &L);
            if (s < 0) {
                perror("accept()");
                goto hosed;
            }

            for (cc = 0; cc < MAXCONN; cc++) {
                if (conn[cc] == -1) {
                    conn[cc] = s;
                    break;
                }
            }

            printf("\
%d:%lu accept() from %s:%d on chan %d\n",
                   getpid(), time(NULL),
                   inet_ntoa(from.sin_addr),
                   ntohs(from.sin_port), cc);
        }

    hosed:

        /* Then process the array of existing
         * clients...
         */
        for (cc = 0; cc < MAXCONN; cc++) {
            int             n;
            struct header   hd;
            char            *buf;

            /* do not reuse cc in this
             * loop...
             */

            if (conn[cc] == -1)
                continue;

            if (! FD_ISSET(conn[cc], &rs))
                continue;


            n = read(conn[cc], &hd, sizeof(struct header));
            if (n <= 0) {
                printf("\
%d:%lu closing while reading header from chan %d\n",
                       getpid(), time(NULL), conn[cc]);
                close(conn[cc]);
                FD_CLR(conn[cc], &rs);
                conn[cc] = -1;
                continue;
            }

            printf("\
%d:%lu header read() from chan %d opcode %u len %u \n",
                   getpid(), time(NULL),
                   cc, hd.opCode, hd.len);

            buf = calloc(hd.len, sizeof(char));
            n = read(conn[cc], buf, hd.len);
            if (n <= 0) {
                printf("\
%d:%lu closing while reading body from %d %s\n",
                       getpid(), time(NULL), conn[cc], strerror(errno));
                close(conn[cc]);
                FD_CLR(conn[cc], &rs);
                conn[cc] = -1;
                free(buf);
                continue;
            }

            for (n = 0; n < hd.len; n++)
                printf("%d", buf[n]);
            printf("\n");

            free(buf);

            /* Send the reply back.
             */
            hd.opCode = 0x200;
            hd.len = 0;

            n = write(conn[cc], &hd, sizeof(struct header));
            if (n != sizeof(struct header)) {
                printf("\
%d:%lu closing while reading body from chan %d %s\n",
                       getpid(), time(NULL), conn[cc], strerror(errno));
                close(conn[cc]);
                FD_CLR(conn[cc], &rs);
                conn[cc] = -1;
                continue;
            }

            printf("\
%d:%lu reply on chan %d ERR_NOERR sent \n",
                   getpid(), time(NULL), cc);

        } /* for (cc = 0; cc < MAXCONN; cc++) */

    }

    return 0;
}

/* do_udp()
 */
static int
do_udp(void)
{
    int                  cc;
    char                 buf[sizeof(struct header) + 64];
    struct sockaddr_in   from;
    socklen_t            L;
    struct header        hd;

    while (1) {

        printf("\
%d:%lu waiting for UDP message...\n", getpid(), time(NULL));

        L = sizeof(struct sockaddr_in);
        cc = recvfrom(Sock,
                      buf,
                      sizeof(buf),
                      0,
                      (struct sockaddr *)&from,
                      &L);
        if (cc < 0) {
            perror("recvfrom()");
            return -1;
        }

        memcpy(&hd, buf, sizeof(struct header));

        printf("\
%d:%lu recvfrom() %s@%d opCode %u len %u\n",
               getpid(), time(NULL),
               inet_ntoa(from.sin_addr),
               from.sin_port,
               hd.opCode, hd.len);

        for (cc = 0; cc < hd.len; cc++)
            printf("%d", buf[sizeof(struct header) + cc]);

        putchar('\n');

        /* Reply to the client ERR_NOERR
         */
        hd.opCode = 0x200;
        hd.len = 0;

        cc = sendto(Sock,
                    &hd,
                    sizeof(struct header),
                    0,
                    (struct sockaddr *)&from,
                    L);
        if (cc < 0) {
            perror("sendto()");
            return -1;
        }

        printf("\
%d:%lu reply ERR_NOERR sent\n", getpid(), time(NULL));

    } /* while (1) */
}
