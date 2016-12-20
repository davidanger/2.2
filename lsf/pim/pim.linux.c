/*
 * Copyright (C) 2011-2012 David Bigagli
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

static struct lsPidInfo procs[MAX_PROC_ENT];
static int numprocs;
static int ls_pidinfo(int, struct lsPidInfo *);
static int parse_stat(char *, struct lsPidInfo *);
static void logProcessInfo(void);

int
scan_procs(void)
{
    DIR *dir;
    struct dirent *process;
    struct lsPidInfo rec;

    dir = opendir("/proc");
    if (dir == NULL) {
        ls_syslog(LOG_ERR, "\
%s: opendir(/proc) failed: %m.", __func__);
        return -1;
    }

    numprocs = 0;
    while ((process = readdir(dir))) {

        if (! isdigit(process->d_name[0]))
            continue;

        if (ls_pidinfo(atoi(process->d_name), &rec) != 0)
            continue;

        if (rec.pgid == 1)
            continue;

        procs[numprocs].pid = rec.pid;
        procs[numprocs].ppid = rec.ppid;
        procs[numprocs].pgid = rec.pgid;

        procs[numprocs].utime = rec.utime/100;
        procs[numprocs].stime = rec.stime/100;
        procs[numprocs].cutime = rec.cutime/100;
        procs[numprocs].cstime = rec.cstime/100;

        procs[numprocs].proc_size = rec.proc_size;
        procs[numprocs].resident_size
            = rec.resident_size * (sysconf(_SC_PAGESIZE)/1024);
        procs[numprocs].stack_size = rec.stack_size;
        if ( procs[numprocs].stack_size < 0 )
            procs[numprocs].stack_size = 0;
        procs[numprocs].status = rec.status;

        ++numprocs;
        if (numprocs == MAX_PROC_ENT - 1) {
            ls_syslog(LOG_INFO, "\
%s: maximum number of processes %d reached.", __func__, numprocs);
            break;
        }
    }

    closedir(dir);

    logProcessInfo();

    return 0;
}

/* logProcessInfo()
 */
static void
logProcessInfo(void)
{
    int i;
    FILE *fp;
    static char wfile[MAXFILENAMELEN];

    sprintf(wfile, "%s.%d", infofile, getpid());

    fp = fopen(wfile, "w");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "%s: fopen() %s failed %m.", wfile);
        return;
    }

    fprintf(fp, "%d\n", pimPort);
    for (i = 0; i < numprocs; i++) {

        fprintf(fp, "\
%d %d %d %d %d %d %d %d %d %d %d %d\n",
                procs[i].pid, procs[i].ppid, procs[i].pgid, procs[i].jobid,
                procs[i].utime, procs[i].stime, procs[i].cutime,
                procs[i].cstime, procs[i].proc_size,
                procs[i].resident_size, procs[i].stack_size,
                (int) procs[i].status);
    }
    fclose(fp);

    if (unlink(infofile) < 0 && errno != ENOENT) {
        ls_syslog(LOG_DEBUG, "\
%s: unlink(%s) failed: %m.", __func__, infofile);
    }

    if (rename(wfile, infofile) < 0) {
        ls_syslog(LOG_ERR, "\
%s: rename() %s to %s failed: %m.", __func__, wfile, infofile);
    }
    unlink(wfile);

    ls_syslog(LOG_DEBUG, "\
%s: process table updated.", __func__);

}

/* ls_pidinfo()
 */
static int
ls_pidinfo(int pid, struct lsPidInfo *rec)
{
    int fd;
    static char filename[PATH_MAX];
    static char buffer[BUFSIZ];

    sprintf(filename, "/proc/%d/stat", pid);

    fd = open(filename, O_RDONLY, 0);
    if (fd == -1) {
        ls_syslog(LOG_ERR, "\
%s: open() failed %s %m.", __func__, filename );
        return -1;
    }

    if (read(fd, buffer, sizeof(buffer) - 1) <= 0) {
        ls_syslog(LOG_ERR, "\
%s: read() failed %s %m.", __func__, filename);
        close(fd);
        return -1;
    }
    close(fd);

    if (parse_stat(buffer, rec) < 0) {
        ls_syslog(LOG_ERR, "\
%s: parse_stat() failed process %d.", __func__, pid);
        return -1;
    }

    return 0;
}

/* parse_stat()
 */
static int
parse_stat(char *buf, struct lsPidInfo *pinfo)
{
    unsigned int rss_rlim;
    unsigned int start_code;
    unsigned int end_code;
    unsigned int start_stack;
    unsigned int end_stack;
    unsigned char status;
    unsigned long vsize;

    sscanf(buf, "\
%d %s %c %d %d %*d %*d %*d %*u %*u %*u %*u %*u %d %d %d "
           "%d %*d %*d %*u %*u %*d %lu %u %u %u %u %u %u",
           &pinfo->pid, pinfo->command, &status, &pinfo->ppid, &pinfo->pgid,
           &pinfo->utime, &pinfo->stime, &pinfo->cutime, &pinfo->cstime,
           &vsize, &pinfo->resident_size, &rss_rlim, &start_code,
           &end_code, &start_stack, &end_stack);

    if (pinfo->pid == 0) {
        ls_syslog(LOG_ERR, "\
%s: invalid process 0 found: %s", __func__, buf);
        return -1;
    }

    pinfo->stack_size = start_stack - end_stack;
    pinfo->proc_size = vsize/1024;

    switch (status) {
        case 'R' :
            pinfo->status = LS_PSTAT_RUNNING;
            break;
        case 'S' :
            pinfo->status = LS_PSTAT_SLEEP;
            break;
        case 'D' :
            pinfo->status = LS_PSTAT_SLEEP;
            break;
        case 'T' :
            pinfo->status = LS_PSTAT_STOPPED;
            break;
        case 'Z' :
            pinfo->status = LS_PSTAT_ZOMBI;
            break;
        case 'W' :
            pinfo->status = LS_PSTAT_SWAPPED;
            break;
        default :
            pinfo->status = LS_PSTAT_RUNNING;
            break;
    }

    return 0;
}
