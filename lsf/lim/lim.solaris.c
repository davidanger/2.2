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

#include "lim.h"

static char cbuf[BUFSIZ];
static unsigned long long int swap;
static unsigned long long int mem;
static unsigned long long int pageing;
static unsigned long long int ut;
static int numdisk;

static void runLoadCollector(void);

/* numCPUs()
 */
int
numCPUs(void)
{
    int ncpus;
    FILE *fp;

/* Returns one line information per CPU.
 */
#define NCPUS "/usr/sbin/psrinfo"

    fp = popen(NCPUS, "r");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "\
%s: popen() %s failed %m", __func__, NCPUS);
        return 0;
    }

    ncpus = 0;
    while (fgets(cbuf, BUFSIZ, fp)) {
        ++ncpus;
    }
    pclose(fp);

    return ncpus;
}

/* queueLength()
 */
int
queuelength(float *r15s, float *r1m, float *r15m)
{
    FILE *fp;

/* This is how we get the machine load average
 * over the last 1, 5 and 15 minutes
 */
#define LOADAV "/usr/bin/uptime|/usr/bin/awk '{print $(NF-2), $(NF-1), $NF}'|/usr/bin/sed -e's/,//g'"

    fp = popen(LOADAV, "r");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "\
%s: popen() %s failed %m", __func__, LOADAV);
        return 1;
    }
    fscanf(fp,"%f%f%f", r15s, r1m, r15m);
    pclose(fp);

    ls_syslog(LOG_DEBUG, "\
%s: Got run queue length r15s %f r1m %f r15m %f",
              __func__, *r15s, *r1m, *r15m);

    return 0;
}
/* Get the dynamic values for swap, memory paging and CPU utilization.
 */

/* cputime()
 */
float
cputime(void)
{
    FILE *fp;

    fp = fopen("/var/tmp/lim.runload", "r");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "\
%s: fopen() /var/tmp/lim failed %m", __func__);
        return 0.0;
    }
    fscanf(fp, "\
%llu %llu %llu %llu", &swap, &mem, &pageing, &ut);
    fclose(fp);

    ls_syslog(LOG_DEBUG, "\
%s: swap %llu mem %llu pageing %llu ut %llu", __func__,
              swap, mem, pageing, ut);

    /* The caller expects a value < 1.
     */
    return (float)ut/100.0;
}

/* realMem()
 * Return free memory.
 */
unsigned long long int
freemem(void)
{
    /* Free memory is reported in kilobytes.
     * Start the collector for the next cycle.
     */
    runLoadCollector();
    return mem;
}

/* tmpspace()
 */
unsigned long long int
freetmp(void)
{
    struct statvfs fs;
    double tmps;

    tmps = 0.0;
    if (statvfs("/tmp", &fs) < 0) {
        ls_syslog(LOG_ERR, "%s: statfs() /tmp failed: %m", __func__);
        return 0.0;
    }

    tmps = (fs.f_bavail * fs.f_bsize)/(1024 * 1024);

    return tmps;
}

/* freewap()
 */
unsigned long long int
freeswap(void)
{
    /* Kilobytes
     */
    return swap;
}

/* getpaging()
 */
float
paging(void)
{
    return pageing;
}

float
iorate(void)
{
    FILE *fp;
    float iorate;

    fp = fopen("/var/tmp/lim.iorate", "r");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "\
%s: popen() /var/tmp/lim.iorate failed %m", __func__);
        return 0.0;
    }
    fscanf(fp, "%f", &iorate);
    fclose(fp);

    return iorate;
}

/* The memory is in megabytes already.
 */
#define MEMMAX "/usr/sbin/prtconf|/usr/bin/awk '{if ($1 == \"Memory\"){print $3; exit}}'"
/* swap -s pritns kilobytes so we convert it to megabytes, awk
 * has dynamic type conversion.
 */
#define SWAPMAX "/usr/sbin/swap -s|/usr/bin/awk '{a = $(NF-1); x = substr(a, 0, length(a) - 1); print x;}"
/* initReadLoad()
 */
void
initReadLoad(int checkMode)
{
    float  maxmem;
    float maxswap;
    struct statvfs fs;
    FILE *fp;

    myHostPtr->loadIndex[R15S] =  0.0;
    myHostPtr->loadIndex[R1M]  =  0.0;
    myHostPtr->loadIndex[R15M] =  0.0;

    if (checkMode)
        return;

    if (statvfs("/tmp", &fs) < 0) {
        ls_syslog(LOG_ERR, "%s: statvfs() failed /tmp: %m", __func__);
        myHostPtr->statInfo.maxTmp = 0;
    } else {
        myHostPtr->statInfo.maxTmp =
            (float)fs.f_blocks/((float)(1024 * 1024)/fs.f_bsize);
    }

    fp = popen(MEMMAX, "r");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "\
%s: popen() %s failed %m", __func__, MEMMAX);
        maxmem = 0.0;
    }

    fgets(cbuf, BUFSIZ, fp);
    pclose(fp);
    maxmem = atof(cbuf);
    ls_syslog(LOG_DEBUG, "%s: Got maxmem %4.2f", __func__, maxmem);

    fp = popen(SWAPMAX, "r");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "\
%s: popen() %s failed %m", __func__, SWAPMAX);
        maxswap = 0;
    }
    fgets(cbuf, BUFSIZ, fp);
    pclose(fp);
    maxswap = atof(cbuf);
    maxswap = maxswap / 1024;
    ls_syslog(LOG_DEBUG, "%s: Got maxswap %4.2f", __func__, maxswap);

    myHostPtr->statInfo.maxMem = maxmem;
    myHostPtr->statInfo.maxSwap = maxswap;

    /* Get the number of disks from which we are
     * going to collect the Kb/s.
     */
#define NUMDISK "/usr/bin/iostat -dsx|/usr/bin/grep -v device|/usr/bin/wc -l"
    fp = popen(NUMDISK, "r");
    if (fp == NULL) {
        ls_syslog(LOG_ERR, "\
%s: popen() %s failed %m", __func__, NUMDISK);
        return;
    }
    fscanf(fp, "%d", &numdisk);
    pclose(fp);

    /* Start the collector which ahead of time is
     * going to collect load metrics which we are
     * going to read from /var/tmp
     */
    runLoadCollector();
}

/* getHostModel()
 */
char *
getHostModel(void)
{
    static char model[MAXLSFNAMELEN];

    strcpy(model, "solaris");

    return model;
}

/* runLoadCollector()
 * Get the load information ahead of the next load reading cycle.
 */
void
runLoadCollector(void)
{
    int cc;
    /* Get the disk IO statistics.
     */
    sprintf(cbuf, "\
/usr/bin/iostat -dsx 1 2|/usr/bin/grep -v device|/usr/bin/tail -%d |/usr/bin/awk '{x=x + $4+$5} END {print x}' 1> /var/tmp/lim.iorate 2> /var/tmp/lim.iorate.err", numdisk);
    cc = lim_system(cbuf);
    if (cc < 0) {
        ls_syslog(LOG_ERR, "\
%s: lim_system() failed for %s %m", __func__, cbuf);
    }

#define RUNLOAD "/usr/bin/vmstat -Sq 1 2|/usr/bin/tail -1|/usr/bin/awk '{print $4, $5, ($8+$9), (100-$NF)}' 1> /var/tmp/lim.runload 2> /var/tmp/lim.runload.err"
    cc = lim_system(RUNLOAD);
    if (cc < 0) {
        ls_syslog(LOG_ERR, "\
%s: lim_system() failed for %s %m", __func__, RUNLOAD);
    }
}
