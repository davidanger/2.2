
/*
 * Networking client example. Communicates over TCP or UDP, it can
 * use a HTTP or SOCKS5 proxy.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "proto.h"

/* These parameters describe program behaviour
 * they are set by the command line options.
 */
struct params {
    char       *server;
    uint16_t   port;
    char       *proxy;
    uint16_t   pport;
    uint8_t    udp;
    uint8_t    socks5;
};
static struct params   param;

static uint32_t   gethostaddr(const char *);
static int        serverPort(const char *, char **, uint16_t *);
static int        proxyHTTPhandshake(int, const char *, int);
static int        proxySOCKS5handshake(int,
                                       const char *,
                                       int,
                                       uint8_t,
                                       struct connReply *);
static int        do_tcp(void);
static int        do_udp(void);
static int        getrandbytes(struct header *, char **);
static int        encodeSOCKS5header(char *,
                                     struct connRequest *);
static int        decodeSOCKS5header(char *,
                                     struct connReply *);

static void
usage(void)
{
    fprintf(stderr, "\
client: [-s server@port] [-P proxy@port] [-S] [-u]\n");
}

int
main(int argc, char **argv)
{
    int   cc;

    while ((cc = getopt(argc, argv, "hp:s:P:Su")) != EOF) {
        switch (cc)  {
            case 'h':
                usage();
                return -1;
            case 's':
                cc = serverPort(optarg,
                                &param.server,
                                &param.port);
                if (cc < 0) {
                    usage();
                    return -1;
                }
                break;
            case 'P':
                cc = serverPort(optarg,
                                &param.proxy,
                                &param.pport);
                if (cc < 0) {
                    usage();
                    return -1;
                }
                break;
            case 'S':
                param.socks5 = 1;
                break;
            case 'u':
                param.udp = 1;
                break;
        }

    } /* while ((cc = getopt()) != EOF) */

    if (param.server == NULL)
        param.server = "localhost";
    if (param.port ==  0)
        param.port = 8765;

    /* run the one of the
     * protocols
     */
    if (param.udp)
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
    int                  cc;
    int                  s;
    struct sockaddr_in   addr;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }

    /* We either connect directly to the server
     * or we connect to the proxy.
     */
    addr.sin_family = AF_INET;
    if (param.proxy) {
        addr.sin_addr.s_addr = gethostaddr(param.proxy);
        addr.sin_port        = htons(param.pport);
    } else {
        addr.sin_addr.s_addr = gethostaddr(param.server);
        addr.sin_port        = htons(param.port);
    }

    cc = connect(s,
                 (struct sockaddr *)&addr,
                 sizeof(struct sockaddr));
    if (cc < 0) {
        perror("connect");
        return -1;
    }

    if (param.proxy
        && !param.socks5) {

        /* handshake with the proxy telling
         * him where we want him to tunnel
         * our stuff to.
         */
        cc = proxyHTTPhandshake(s,
                                param.server,
                                param.port);
        if (cc < 0) {
            fprintf(stderr, "\
%d:%lu HTTP proxy %s@%d handshake failed\n",
                    getpid(), time(NULL),
                    param.server, param.port);
            return -1;
        }

        printf("\
%d:%lu HTTP proxy %s@%d handshake all right\n",
               getpid(), time(NULL),
               param.proxy, param.pport);

    }

    /* We are asked to use a SOCKS5 proxy.
     */
    if (param.proxy
        && param.socks5) {
        struct connReply   rep;

        cc = proxySOCKS5handshake(s,
                                  param.server,
                                  param.port,
                                  param.udp,
                                  &rep);
        if (cc < 0) {
            fprintf(stderr, "\
%d:%lu tcp SOCKS5 proxy %s@%d handshake failed\n",
                    getpid(), time(NULL),
                    param.server, param.port);
            return -1;
        }

        printf("\
%d:%lu tcp SOCKS5 proxy %s@%d handshake all right\n",
               getpid(), time(NULL),
               param.proxy, param.pport);
    }

    while (1) {
        struct header        hd;
        char                 *buf;
        struct timeval       tv;
        struct sockaddr_in   from;
        socklen_t            L;

        getrandbytes(&hd, &buf);

        /* send the header
         */
        cc = write(s, &hd, sizeof(struct header));
        if (cc != sizeof(struct header)) {
            perror("write()");
            free(buf);
            return -1;
        }
        /* send the payload
         */
        cc = write(s, buf, hd.len);
        if (cc != hd.len) {
            perror("write2()");
            free(buf);
            return -1;
        }
        free(buf);
        printf("%d:%lu buffer sent...\n", getpid(), time(NULL));

        /* Read server's reply
         */
        cc = read(s, &hd, sizeof(struct header));
        if (cc <= 0) {
            perror("read() reply");
            return -1;
        }

        if (getsockname(s,
                        (struct sockaddr *)&from,
                        &L) < 0) {
            perror("getsockname()");
            return -1;
        }

        printf("\
%d:%lu got reply %dbytes status 0x%x %s@%d\n",
               getpid(), time(NULL), cc, hd.opCode,
               inet_ntoa(from.sin_addr),
               ntohs(from.sin_port));

        /* ZZ
         */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);

        printf("%d:%lu woken up...\n", getpid(), time(NULL));
    }

    return 0;
}

/* do_udp()
 */
static int
do_udp(void)
{
    struct header        hd;
    char                 *buf;
    int                  cc;
    int                  s;
    socklen_t            L;
    struct sockaddr_in   addr;
    struct connReply     crp;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = gethostaddr(param.server);
    addr.sin_port        = htons(param.port);

    if (param.socks5) {

        addr.sin_addr.s_addr = gethostaddr(param.proxy);
        addr.sin_port        = htons(param.pport);

        /* Handshake with the proxy server
         * asking to tunnel our UDP
         * communication.
         */
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            perror("socket");
            return -1;
        }

        cc = connect(s,
                     (struct sockaddr *)&addr,
                     sizeof(struct sockaddr));
        if (cc < 0) {
            perror("connect");
            return -1;
        }

        cc = proxySOCKS5handshake(s,
                                  param.server,
                                  param.port,
                                  param.udp,
                                  &crp);
        if (cc < 0) {
            fprintf(stderr, "\
%d:%lu SOCKS5 proxy %s@%d handshake failed\n",
                    getpid(), time(NULL),
                    param.server, param.port);
            return -1;
        }

        /* use the network coordinates indicated
         * by the proxy
         */
        addr.sin_addr.s_addr = crp.addr;
        addr.sin_port        = crp.port;

    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }

    while (1) {
        struct timeval       tv;
        struct sockaddr_in   from;

        getrandbytes(&hd, &buf);

        L = sizeof(struct sockaddr_in);

        if (param.socks5) {
            char                 zbuf[10];
            char                 *pbuf;
            struct connRequest   cr;

            /* In the UDP case set the
             * SOCKS5 header for every
             * packet we send out.
             */
            cr.version  = 0x05;
            cr.command  = 0x03;
            cr.reserved = 0x0;
            cr.addrtype = 0x01;
            cr.addr     = gethostaddr(param.server);
            cr.port     = htons(param.port);

            encodeSOCKS5header(zbuf, &cr);

            pbuf = calloc(10 + sizeof(struct header) + hd.len,
                          sizeof(char));
            memcpy(pbuf, zbuf, 10);
            memcpy(pbuf + 10, buf, hd.len + sizeof(struct header));

            cc = sendto(s,
                        pbuf,
                        10 + hd.len + sizeof(struct header),
                        0,
                        (struct sockaddr *)&addr,
                        L);
        } else {

            cc = sendto(s,
                        buf,
                        hd.len + sizeof(struct header),
                        0,
                        (struct sockaddr *)&addr,
                        L);
        }
        if (cc < 0) {
            perror("sendto()");
            free(buf);
            return -1;
        }
        free(buf);
        printf("%d:%lu buffer sent...\n", getpid(), time(NULL));

        /* Got the reply from the server.
         */
        cc = recvfrom(s,
                      &hd,
                      sizeof(struct header),
                      0,
                      (struct sockaddr *)&from,
                      &L);
        if (cc <= 0) {
            perror("recvfrom() reply");
            return -1;
        }

        printf("\
%d:%lu recvfrom() %dbytes status 0x%x from %s@%d\n",
               getpid(), time(NULL), cc, hd.opCode,
               inet_ntoa(from.sin_addr),
               ntohs(from.sin_port));


        /* ZZ
         */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);

        printf("%d:%lu woken up...\n", getpid(), time(NULL));

    }

    return 0;
}

/* gethostaddr()
 */
static uint32_t
gethostaddr(const char *host)
{
    struct hostent   *hp;
    uint32_t         ip;

    hp = gethostbyname(host);
    memcpy(&ip, hp->h_addr_list[0], sizeof(ip));

    return ip;

}

/* serverPort()
 */
static int
serverPort(const char *arg, char **server, uint16_t *port)
{
    char   *p;
    char   *cp;
    char   *srv;

    /* well yes... if you remember
     * free it before you go...
     */
    p = strdup(arg);

    srv = p;
    cp = strchr(srv, '@');
    if (cp == NULL)
        return -1;
    *cp = 0;
    ++cp;

    *port = atoi(cp);
    *server = srv;

    return 0;
}

/* proxyHandshake()
 * Tell the proxy to connect us at the specified
 * host and port.
 */
static int
proxyHTTPhandshake(int s, const char *server, int port)
{
    int   cc;
    int   L;
    int   code;
    char  buf[128];

    /* CONNECT method tells the proxy to start
     * relay messages to the server and port
     * indicated in the message.
     */
    sprintf(buf, "\
CONNECT %s:%d HTTP/1.1\n\n", server, port);

    /* send to proxy
     */
    L = strlen(buf);
    cc = write(s, buf, L);
    if (cc != L) {
        perror("write()");
        return -1;
    }
    /* get response.. we hope for:
     * HTTP/1.1 200 Tunnel established\r\n\r\n"
     */
    cc = read(s, buf, sizeof(buf));
    if (cc <= 0) {
        perror("read()");
        return -1;
    }

    /* HTTP is rudimentary so rudimentary
     * should be its parser...
     */
    sscanf(buf, "%*s%d", &code);
    if (code != 200) {
        fprintf(stderr, "\
%d:%lu proxy handshake failed %s", getpid(), time(NULL), buf);
        return -1;
    }

    return 0;
}

/* proxySOCKS5handshake()
 *
 * In the TCP case we do the initial handshake with the
 * authentication then send the request to route data
 * to the destination server.
 *
 * In the UDP case we do the initial handshake as well
 * then we ask for UDP associate indicating our address
 * and port. Then when we send the UDP packets we must
 * always set the SOCKS5 header in front of our UDP payload.
 */
static int
proxySOCKS5handshake(int s,
                     const char *server,
                     int port,
                     uint8_t udp,
                     struct connReply *rep)
{
    struct greetReq      gr;
    struct greetReply    grp;
    struct connRequest   cr;
    int                  cc;
    /* 10 bytes is the SOCK5 header
     */
    char                buf[10];

    /* first the initial auth handshake with
     * the proxy telling we need no stinky
     * authentication to begin with.
     */
    gr.version = 0x05;
    gr.authnum = 0x01;
    gr.auth    = 0x0;
    /* we should serialize this data structure
     * for now we rely on gcc not to align
     * the 3 bytes to 4.
     */
    cc = write(s, &gr, sizeof(struct greetReq));
    if (cc != sizeof(struct greetReq)) {
        perror("greet write()");
        return -1;
    }

    cc = read(s, &grp, sizeof(struct greetReply));
    if (cc < 0) {
        perror("greet read()");
        return -1;
    }
    if (cc == 0) {
        fprintf(stderr, "\
%d:%lu greeting disconnected by the proxy....\n",
                getpid(), time(NULL));
        return -1;
    }

    if (grp.version != 0x05
        || grp.auth != 0) {
        fprintf(stderr, "\
%d:%lu bad greeting reply received %d %d\n",
                getpid(), time(NULL),
                grp.version, grp.auth);
        return -1;
    }

    /* Great the initial handshake is done
     * now let's send the actually tunneling
     * request.
     */
    cr.version  = 0x05;
    if (param.udp)
        cr.command = 0x03; /* associate udp port */
    else
        cr.command  = 0x01; /* establish a tcp stream */
    cr.reserved = 0x0;
    cr.addrtype = 0x01;  /* we support only IPv4 for now */
    if (param.udp) {
        /* In the UDP case this is our addr and port
         * from which we will be sending UDP packets.
         * If we set both to 0, the proxy will use
         * sockaddr_in returned from recvfrom().
         */
        cr.addr = 0;
        cr.port = 0;
    } else {
        /* In the TCP case we tell the proxy the
         * address and port of the destination
         * server.
         */
        cr.addr     = gethostaddr(param.server);
        cr.port     = htons(param.port);
    }

    /* encode the structure into
     * sending buffer
     */
    encodeSOCKS5header(buf, &cr);

    cc = write(s, &buf, sizeof(buf));
    if (cc != sizeof(buf)) {
        perror("connection reqest write()");
        return -1;
    }

    /* read and decode the reply.
     */
    cc = read(s, &buf, sizeof(buf));
    if (cc < 0) {
        perror("connect read()");
        return -1;
    }
    if (cc == 0) {
        fprintf(stderr, "\
%d:%lu connection disconnected by the proxy....\n", getpid(), time(NULL));
        return -1;
    }

    /* decode the structure from the
     * sending buffer
     */
    decodeSOCKS5header(buf, rep);

    if (rep->version != 0x05
        || rep->status != 0) {
        fprintf(stderr, "\
%d:%lu bad connection reply received 0x0%d 0x0%d\n",
                getpid(), time(NULL),
                rep->version, rep->status);
        return -1;
    }

    /* wow we fooled dem again
     */

    return 0;
}

static int
getrandbytes(struct header *hd,
             char **payload)
{
    int    cc;
    char   *buf;

    /* This is just a joke protocol.
     * Define an operation code and
     * a payload with pseudo random bit
     * pattern.
     */
    hd->opCode = 0x01;

    /* generate a random payload upto 64 bytes.
     */
    hd->len = 11 + (int)(64.0 * (rand() / (RAND_MAX + 1.0)));

    /* generate a pseudo random bit pattern.
     */
    buf = calloc(hd->len + sizeof(struct header), sizeof(char));
    *payload = buf;
    memcpy(buf, hd, sizeof(struct header));
    buf = buf + sizeof(struct header);

    for (cc = 0; cc < hd->len; cc++)
        buf[cc] = rand() % 2;

    return 0;
}

/* encodeSOCKS5header()
 * Create the SOCK5 header with the destination
 * server address
 */
static int
encodeSOCKS5header(char *zbuf,
                   struct connRequest *cr)
{
    char   *p;

    /* Serialize the data structure before
     * sending to the proxy.
     */
    p = zbuf;

    memcpy(p, &cr->version, sizeof(uint8_t));
    p = p + sizeof(uint8_t);

    memcpy(p, &cr->command, sizeof(uint8_t));
    p = p + sizeof(uint8_t);

    memcpy(p, &cr->reserved, sizeof(uint8_t));
    p = p + sizeof(uint8_t);

    memcpy(p, &cr->addrtype, sizeof(uint8_t));
    p = p + sizeof(uint8_t);

    memcpy(p, &cr->addr, sizeof(uint32_t));
    p = p + sizeof(uint32_t);

    memcpy(p, &cr->port, sizeof(uint16_t));

    return 0;
}

/*
 * decodeSOCKS5header()
 */
static int
decodeSOCKS5header(char *zbuf,
                   struct connReply *rep)
{
    char   *p;

    p = zbuf;

    memcpy(&rep->version, p, sizeof(uint8_t));
    p = p + sizeof(uint8_t);

    memcpy(&rep->status, p, sizeof(uint8_t));
    p = p + sizeof(uint8_t);

    /* In the reply to a UDP ASSOCIATE request, the BND.PORT and BND.ADDR
     * fields indicate the port number/address where the client MUST send
     * UDP request messages to be relayed.
     */
    if (param.udp) {

        memcpy(&rep->reserved, p, sizeof(uint8_t));
        p = p + sizeof(uint8_t);

        memcpy(&rep->addrtype, p, sizeof(uint8_t));
        p = p + sizeof(uint8_t);

        memcpy(&rep->addr, p, sizeof(uint32_t));
        p = p + sizeof(uint32_t);

        memcpy(&rep->port, p, sizeof(uint16_t));
    }

    return 0;
}
