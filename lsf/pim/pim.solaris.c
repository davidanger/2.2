/*
 * Copyright (C) 2011-2012 David Bigagli
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
#include <sys/procfs.h>

static char wfile[MAXFILENAMELEN];

/* scan_procs()
 * Read the /proc, call ioctl() to get the process
 * data and log them directly into the pim info file.
 */
int
scan_procs(void)
{
    DIR *dir;
    struct dirent *dr;
    struct prpsinfo info;
    char buf[128];
    FILE *fp;

    /* Create a temporary file to store
     * the PIM data.
     */
    sprintf(wfile, "%s.%lu", infofile, getpid());

    fp = fopen(wfile, "w");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "%s: fopen() %s failed %m.", wfile);
        return -1;
    }
    fprintf(fp, "%d\n", pimPort);

    dir = opendir("/proc");
    if (dir == NULL) {
        ls_syslog(LOG_ERR, "\
%s: opendir(/proc) failed: %m.", __func__);
        return -1;
    }

    while ((dr = readdir(dir))) {
        int cc;
        int d;

        if (strcmp(dr->d_name, ".") == 0
            || strcmp(dr->d_name, "..") == 0)
            continue;

        sprintf(buf, "/proc/%s", dr->d_name);

        ls_syslog(LOG_DEBUG, "%s: %s", __func__, buf);

        d = open(buf, O_RDONLY);
        if (d < 0) {
            ls_syslog(LOG_ERR, "\
%s: open() %s failed %m", __func__, buf);
            continue;
        }

        memset(&info, 0, sizeof(struct prpsinfo));
        cc = ioctl(d, PIOCPSINFO, &info);
        if (cc < 0) {
            ls_syslog(LOG_ERR, "\
%s: ioctl() PIOCPSINFO failed %m", __func__);
            continue;
        }

        fprintf(fp, "\
%lu %lu %lu %d %lu %d %lu %d %d %d %d %d\n",
                info.pr_pid,
                info.pr_ppid,
                info.pr_pgrp,
                0,
                /* For now on solaris lets cumulate
                 * user + sys CPU.
                 */
                info.pr_time.tv_sec, 0,
                info.pr_ctime.tv_sec, 0,
                info.pr_bysize,
                info.pr_byrssize,
                0,
                info.pr_state);

        close(d);

    } /* while() */

    closedir(dir);
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

    return 0;
}
