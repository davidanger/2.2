/*
 * Copyright (C) 2011 - 2012 David Bigagli
 * Copyright (C) 2007 Platform Computing Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "pim.h"

static int gothup;

struct config_param pimParams[] =
{
    {"LSF_LIM_DEBUG", NULL},
    {"LSF_LOGDIR", NULL},
    {"LSF_DEBUG_PIM",NULL},
    {"LSF_LOG_MASK",NULL},
    {"LSF_TIME_PIM",NULL},
    {"LSF_PIM_SLEEPTIME", NULL},
    {"LSF_PIM_INFODIR", NULL},
    {"LSF_PIM_NPROC", NULL},
    {"LSF_PIM_TRACE", NULL},
    {"LSF_PIM_UPDATE_INTERVAL", NULL},
    {NULL, NULL}
};

enum {
    LSF_LIM_DEBUG,
    LSF_LOGDIR,
    LSF_DEBUG_PIM,
    LSF_LOG_MASK,
    LSF_TIME_PIM,
    LSF_PIM_SLEEPTIME,
    LSF_PIM_INFODIR,
    LSF_PIM_NPROC,
    LSF_PIM_TRACE,
    LSF_PIM_UPDATE_INTERVAL
};

/* The pim info file with the process information
 * this file is used by the arch dipendent modules.
 */
char infofile[PATH_MAX];
int pimPort;
static int pim_debug;
static int sleepTime = PIM_SLEEP_TIME;
static int updInterval = PIM_UPDATE_INTERVAL;

static int doServ(void);
static void hup(int);
static void updateProcs(void);

static void
usage (const char *cmd)
{
    fprintf(stderr, "\
%s: [-V] [-h] [-debug_level] [-d env_dir]\n", cmd);
}

/* This is PIM process information manager.
 * This is the interface to the machine /proc and
 * based on its process table openlava implements
 * the getJInfo_() API that returns runtime
 * resource usage.
 */
int
main(int argc, char **argv)
{
    char *sp;
    char *traceVal;
    char *myHost;
    char *env_dir;
    int cc;

    myHost = "localhost";
    while ((cc = getopt(argc, argv, "12Vd:")) != EOF) {

        switch (cc) {
            case 'd':
                env_dir = optarg;
                break;
            case '1':
                pim_debug = 1;
                break;
            case '2':
                pim_debug = 2;
                break;
            case 'V':
                fputs(_LS_VERSION_, stderr);
                return 0;
            case '?':
            default:
                usage(argv[0]);
            return -1;
        }
    }

    if (env_dir == NULL) {
        if ((env_dir = getenv("LSF_ENVDIR")) == NULL) {
            env_dir = "/etc";
        }
    }

    if (initenv_(pimParams, env_dir) < 0) {

        sp = getenv("LSF_LOGDIR");
        if (sp != NULL)
            pimParams[LSF_LOGDIR].paramValue = sp;
        ls_openlog("pim",
                   pimParams[LSF_LOGDIR].paramValue, (pim_debug == 2),
                   pimParams[LSF_LOG_MASK].paramValue);
        ls_syslog(LOG_ERR, "%s: initenv_() failed %s.", __func__, env_dir);
        return -1;
    }

    if (!pim_debug && pimParams[LSF_LIM_DEBUG].paramValue) {
        pim_debug = atoi(pimParams[LSF_LIM_DEBUG].paramValue);
        if (pim_debug <= 0)
            pim_debug = 1;
    }

    traceVal = NULL;
    if (pimParams[LSF_PIM_TRACE].paramValue) {
        traceVal = pimParams[LSF_PIM_TRACE].paramValue;
    } else if (pimParams[LSF_DEBUG_PIM].paramValue) {
        traceVal = pimParams[LSF_DEBUG_PIM].paramValue;
    }
    getLogClass_(traceVal, pimParams[LSF_TIME_PIM].paramValue);

    if (pim_debug > 1)
        ls_openlog("pim", pimParams[LSF_LOGDIR].paramValue, TRUE, "LOG_DEBUG");
    else
        ls_openlog("pim", pimParams[LSF_LOGDIR].paramValue, FALSE,
                   pimParams[LSF_LOG_MASK].paramValue);

    if ((sp = pimParams[LSF_PIM_SLEEPTIME].paramValue)) {
        if ((sleepTime = atoi(sp) < 0)) {
            ls_syslog(LOG_ERR, "\
%s: LSF_PIM_SLEEPTIME value %s must be a positive integer, set to %d",
                      __func__, sp, PIM_SLEEP_TIME);
            sleepTime = PIM_SLEEP_TIME;
        }
    }

    if ((sp = pimParams[LSF_PIM_UPDATE_INTERVAL].paramValue)) {
        if ((updInterval = atoi(sp)) < 0) {
            ls_syslog(LOG_ERR, "\
%s: LSF_PIM_UPDATE_INTERVAL value %s must be a positive integer, set to %d",
                      __func__, sp, PIM_UPDATE_INTERVAL);
            updInterval = PIM_UPDATE_INTERVAL;
        }
    }

    myHost = ls_getmyhostname();
    /* Greet the world!
     */
    ls_syslog(LOG_INFO, "\
pim: Howdy this is Process Information Manager daemon on host %s.", myHost);

    sprintf(infofile, "/tmp/pim.info.%s", myHost);
    if (pimParams[LSF_PIM_INFODIR].paramValue) {
        sprintf(infofile, "\
%s/pim.info.%s", pimParams[LSF_PIM_INFODIR].paramValue, myHost);
    }

    /* Like a good old Unix deamon do something
     * upon receiving SIGHUP.
     */
    Signal_(SIGHUP, hup);
    Signal_(SIGTERM, hup);
    Signal_(SIGINT, hup);

    cc = doServ();
    if (cc < 0)
        return -1;

    return 0;
}

/* doServ()
 * Da main loop.
 */
static int
doServ(void)
{
    int ppid;
    socklen_t len;
    struct sockaddr_in sin;
    int asock;
    int cc;
    time_t nextTime;

    umask(022);
    if ((asock = TcpCreate_(TRUE, 0)) < 0) {
        ls_syslog(LOG_ERR, "%s: TcpCreate_() failed: %m.", __func__);
        return -1;
    }

    len = sizeof(sin);
    if (getsockname (asock, (struct sockaddr *)&sin, &len) < 0) {
        ls_syslog(LOG_ERR, "s: getsockname() failed: %m.", __func__);
        return -1;
    }

    pimPort = ntohs(sin.sin_port);

    TIMEIT(0, updateProcs(), "updateProcs");
    nextTime = time(NULL) + updInterval;

    for (;;) {
        struct timeval timeOut;
        fd_set rmask;
        int nready;
        int sock;
        struct LSFHeader hdrBuf;
        struct LSFHeader hdr;
        time_t t;

        ppid = getppid();
        if (ppid <= 1 || (kill(getppid(), 0) == -1)) {
            ls_syslog(LOG_INFO, "\
%s: Parent gone, PIM exiting good bye.", __func__);
            return 0;
        }

        /* heftago
         */
        if (gothup)
            break;

        t = time(NULL);
        if (t >= nextTime) {
            TIMEIT(0, updateProcs(), "updateProcs");
            nextTime = t + updInterval;
        }

        timeOut.tv_sec = sleepTime;
        timeOut.tv_usec = 0;

        FD_ZERO(&rmask);
        FD_SET(asock, &rmask);

        nready = select(asock + 1, &rmask, NULL, NULL, &timeOut);
        if (nready <= 0) {
            if (nready < 0)
                ls_syslog(LOG_ERR, "%s: select() failed: %m", __func__);
            continue;
        }

        sock = b_accept_(asock, (struct sockaddr *)&sin, &len);
        if (sock < 0) {
            ls_syslog(LOG_ERR, "%s: accept() failed %m.", __func__);
            continue;
        }

        if ((cc = lsRecvMsg_(sock,
                             (char *)&hdrBuf,
                             sizeof(hdrBuf),
                             &hdr,
                             NULL,
                             NULL,
                             nb_read_fix)) < 0) {
            close(sock);
            ls_syslog(LOG_ERR, "\
%s: lsRecvMsg_() failed, cc %d: %M", __func__, cc);
            continue;
        }

        ls_syslog(LOG_DEBUG, "\
%s: got opCode %d PGID %d updating now.", __func__,
                  hdr.opCode, hdr.reserved);

        /* Update processes and move the time ahead.
         */
        TIMEIT(0, updateProcs(), "updateProcs");
        nextTime = time(NULL) + updInterval;

        if ((cc = writeEncodeHdr_(sock, &hdr, nb_write_fix)) < 0) {
            ls_syslog(LOG_ERR, "%s: write() failed %m.", __func__);
            close(sock);
            continue;
        }

        close(sock);
    }

    return 0;
}

/* updateProcs()
 * Linux and Solaris implement the /proc reading
 * differently.
 */
static void
updateProcs(void)
{
    ls_syslog(LOG_DEBUG, "\
%s: updating PIM process table.", __func__);

    scan_procs();
}

static void
hup(int sig)
{
    gothup = 1;
}
