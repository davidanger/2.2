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

#include <sys/types.h>
#include <sys/wait.h>
#include "lim.h"
#include <math.h>
#include <utmp.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/utsname.h>
#include "../lib/mls.h"
#include <unistd.h>
#include "../lib/lproto.h"

static FILE *lim_popen(char **, char *);
static int lim_pclose(FILE *);

pid_t elim_pid = -1;
int defaultRunElim = FALSE;

static void getusr(void);
static char * getElimRes (void);
static int saveSBValue (char *, char *);
static int callElim(void);
static int startElim(void);
static void termElim(void);
static int isResourceSharedInAllHosts(char *resName);

int ELIMrestarts = -1;
int ELIMdebug = 0;
int ELIMblocktime = -1;

extern int maxnLbHost;
float initLicFactor(void);
static void setUnkwnValues (void);
static int loginses;
extern char *getExtResourcesVal(char *);
extern int  blockSigs_(int, sigset_t*, sigset_t*);
static void unblockSigs_(sigset_t* );
void
satIndex(void)
{
    int   i;

    for (i = 0; i < allInfo.numIndx; i++)
        li[i].satvalue = myHostPtr->busyThreshold[i];
}

void
loadIndex(void)
{
    li[R15S].exchthreshold += 0.05*(myHostPtr->statInfo.maxCpus - 1 );
    li[R1M].exchthreshold += 0.04*(myHostPtr->statInfo.maxCpus - 1 );
    li[R15M].exchthreshold += 0.03*(myHostPtr->statInfo.maxCpus - 1 );
}

/* idletime()
 * Get the machine tty idle time. Traverse on utmp and
 * check if when it is the last time somebody send a character
 * down the tty. The min of this value is the machine tty idle time.
 */
static float
idletime(void)
{
    struct utmp *u;
    struct stat statbuf;
    static char buf[128];
    int idle;
    int l;
    time_t t;

    setutent();
    t = time(NULL);
    idle = 3600*24*30*60;
    loginses = 0;
    while ((u = getutent())) {

        if (u->ut_user[0] == 0
            || u->ut_type != USER_PROCESS)
            continue;

        sprintf(buf, "/dev/%s", u->ut_line);

        if (stat(buf, &statbuf) < 0) {
            ls_syslog(LOG_ERR, "\
%s: stats() failed %s %s %m", __func__, u->ut_user, u->ut_line);
            continue;
        }

        /* Count the login session on the system.
         */
        ++loginses;

        ls_syslog(LOG_DEBUG, "\
%s: %s %s %s %d", __func__, u->ut_user, u->ut_line, buf, t - statbuf.st_atime);
        l = t - statbuf.st_atime;
        if (l <= 0) {
            /* Somebody just send a char down the line
             * the machine is certanly no longer idle.
             */
            idle = 0;
            break;
        }
        if (l < idle)
            idle = l;
    } /* while () */

    endutent();

    return idle;
}

/* readLoad()
 */
void
readLoad(int kernelPerm)
{
    int i;
    int busyBits;
    float extrafactor;
    float cpu_usage;
    float avrun15;
    float avrun1m;
    float avrun15m;
    static time_t next;

    if (next == 0)
        next = time(NULL) - 1;

    TIMEIT(0, getusr(), "getusr()");

    queuelength(&avrun15, &avrun1m, &avrun15m);
    cpu_usage = cputime();

    TIMEIT(0, myHostPtr->loadIndex[IT] = idletime(), "idletime");
    TIMEIT(0, myHostPtr->loadIndex[PG] = paging(), "getpaging");
    TIMEIT(0, myHostPtr->loadIndex[IO] = iorate(), "getIoRate");
    TIMEIT(0, myHostPtr->loadIndex[SWP] = freeswap(), "getswap");
    TIMEIT(0, myHostPtr->loadIndex[TMP] = freetmp(), "tmpspace");
    TIMEIT(0, myHostPtr->loadIndex[MEM] = freemem(), "realMem");

    /* Convert kB in MB
     */
    myHostPtr->loadIndex[MEM] = myHostPtr->loadIndex[MEM]/1024.0;
    myHostPtr->loadIndex[SWP] = myHostPtr->loadIndex[SWP]/1024.0;

    /* call sendload() only evry exchange interval.
     */
    if (time(NULL) < next)
        return;

    extrafactor = 0;
    if (jobxfer) {
        extrafactor = (float)jobxfer/(float)keepTime;
        jobxfer--;
    }
    myHostPtr->loadIndex[R15S] = avrun15  + extraload[R15S] * extrafactor;
    myHostPtr->loadIndex[R1M]  = avrun1m  + extraload[R1M]  * extrafactor;
    myHostPtr->loadIndex[R15M] = avrun15m + extraload[R15M] * extrafactor;

    myHostPtr->loadIndex[UT] = cpu_usage + extraload[UT] * extrafactor;
    if (myHostPtr->loadIndex[UT] > 1.0)
        myHostPtr->loadIndex[UT] = 1.0;

    myHostPtr->loadIndex[PG] += extraload[PG] * extrafactor;

    myHostPtr->loadIndex[LS] = loginses;

    myHostPtr->loadIndex[IT] += extraload[IT] * extrafactor;
    if (myHostPtr->loadIndex[IT] < 0)
        myHostPtr->loadIndex[IT] = 0;

    myHostPtr->loadIndex[SWP] += extraload[SWP] * extrafactor;
    if (myHostPtr->loadIndex[SWP] < 0)
        myHostPtr->loadIndex[SWP] = 0;

    myHostPtr->loadIndex[TMP] += extraload[TMP] * extrafactor;
    if (myHostPtr->loadIndex[TMP] < 0)
        myHostPtr->loadIndex[TMP] = 0;

    myHostPtr->loadIndex[MEM] += extraload[MEM] * extrafactor;
    if (myHostPtr->loadIndex[MEM] < 0)
        myHostPtr->loadIndex[MEM] = 0;

    for (i = 0; i < allInfo.numIndx; i++) {

        if (i == R15S || i == R1M || i == R15M) {
            li[i].value = normalizeRq(myHostPtr->loadIndex[i],
                                      1,
                                      myHostPtr->statInfo.maxCpus) - 1;
        } else {
            li[i].value = myHostPtr->loadIndex[i];
        }
    }

    for (i = 0; i < allInfo.numIndx; i++) {

        if ((li[i].increasing && fabs(li[i].value - INFINIT_LOAD) < 1.0)
            || (! li[i].increasing
                && fabs(li[i].value + INFINIT_LOAD) < 1.0)) {
            continue;
        }

        if (!THRLDOK(li[i].increasing, li[i].value, li[i].satvalue))  {
            SET_BIT (i + INTEGER_BITS, myHostPtr->status);
            myHostPtr->status[0] |= LIM_BUSY;
        } else
            CLEAR_BIT(i + INTEGER_BITS, myHostPtr->status);
    }

    busyBits = 0;
    for (i = 0; i < GET_INTNUM (allInfo.numIndx); i++)
        busyBits += myHostPtr->status[i+1];
    if (!busyBits)
        myHostPtr->status[0] &= ~LIM_BUSY;

    if (LOCK_BY_USER(limLock.on)) {
        if (time(NULL) > limLock.time ) {
            limLock.on &= ~LIM_LOCK_STAT_USER;
            limLock.time = 0;
            mustSendLoad = TRUE;
            myHostPtr->status[0] = myHostPtr->status[0] & ~LIM_LOCKEDU;
        } else {
            myHostPtr->status[0] = myHostPtr->status[0] | LIM_LOCKEDU;
        }
    }

    myHostPtr->loadMask = 0;

    TIMEIT(0, sendLoad(), "sendLoad()");

    for(i = 0; i < allInfo.numIndx; i++) {

        if (myHostPtr->loadIndex[i] < MIN_FLOAT16
            && i < NBUILTINDEX){
            myHostPtr->loadIndex[i] = 0.0;
        }

        if (i == R15S || i == R1M || i == R15M ) {
            float rawql;

            rawql = myHostPtr->loadIndex[i];
            myHostPtr->loadIndex[i]
                = normalizeRq(rawql,
                              (myHostPtr->hModelNo >= 0) ?
                              shortInfo.cpuFactors[myHostPtr->hModelNo] : 1.0,
                              myHostPtr->statInfo.maxCpus);
            myHostPtr->uloadIndex[i] = rawql;
        } else {
            myHostPtr->uloadIndex[i] = myHostPtr->loadIndex[i];
        }
    }
    /* Reload the timer.
     */
    next = time(NULL) + exchIntvl;

    return;
}

static FILE *
lim_popen(char **argv, char *mode)
{
    static char fname[] = "lim_popen()";
    int p[2], pid, i;

    if (mode[0] != 'r')
        return NULL;

    if (pipe(p) < 0)
        return NULL;

    if ((pid = fork()) == 0) {
        char  *resEnv;
        resEnv = getElimRes();
        if (resEnv != NULL) {
            if (logclass & LC_TRACE)
                ls_syslog (LOG_DEBUG, "lim_popen: LS_ELIM_RESOURCES <%s>", resEnv);
            putEnv ("LS_ELIM_RESOURCES", resEnv);
        }
        close(p[0]);
        dup2(p[1], 1);

        alarm(0);

        for(i = 2; i < sysconf(_SC_OPEN_MAX); i++)
            close(i);
        for (i = 1; i < NSIG; i++)
            Signal_(i, SIG_DFL);

        lsfExecvp(argv[0], argv);
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "execvp", argv[0]);
        exit(127);
    }
    if (pid == -1) {
        close(p[0]);
        close(p[1]);
        return NULL;
    }

    elim_pid = pid;
    close(p[1]);

    return fdopen(p[0], mode);
}

static int
lim_pclose(FILE *ptr)
{
    sigset_t   omask;
    sigset_t   newmask;
    pid_t      child;

    child = elim_pid;
    elim_pid = -1;

    if (ptr)
        fclose(ptr);

    if (child == -1)
        return -1;

    kill(child, SIGTERM);

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGINT);
    sigaddset(&newmask, SIGQUIT);
    sigaddset(&newmask, SIGHUP);
    sigprocmask(SIG_BLOCK, &newmask, &omask);

    sigprocmask(SIG_SETMASK, &omask, NULL);

    return 0;
}

static int
saveIndx(char *name, float value)
{
    static char **names;
    int indx;
    int i;

    if (!names) {
        if (!(names = calloc(allInfo.numIndx + 1, sizeof(char *)))) {
            ls_syslog(LOG_ERR, "%s: calloc() failed %m", __func__);
            lim_Exit(__func__);
        }
    }
    indx = getResEntry(name);

    if (indx < 0) {

        for(i = NBUILTINDEX; names[i] && i < allInfo.numIndx; i++) {
            if (strcmp(name, names[i]) == 0)
                return 0;
        }

        ls_syslog(LOG_ERR, "\
%s: Unknown index name %s from ELIM", __func__, name);
        if (names[i]) {
            FREEUP(names[i]);
        }
        names[i] = putstr_(name);
        return 0;
    }

    if (allInfo.resTable[indx].valueType != LS_NUMERIC
        || indx >= allInfo.numIndx) {
        return 0;
    }

    myHostPtr->loadIndex[indx] = value;

    return 0;
}

static int
getSharedResBitPos(char *resName)
{
    struct sharedResourceInstance *tmpSharedRes;
    int bitPos;

    if (resName == NULL)
        return -1;

    for (tmpSharedRes=sharedResourceHead, bitPos=0;
         tmpSharedRes;
         tmpSharedRes=tmpSharedRes->nextPtr, bitPos++ ){
        if (!strcmp(resName,tmpSharedRes->resName)){
            return bitPos;
        }
    }
    return -1;

}

static void
getExtResourcesLoad(void)
{
    int i, bitPos;
    char *resName, *resValue;
    float fValue;

    for (i=0; i < allInfo.nRes; i++) {
        if (allInfo.resTable[i].flags & RESF_DYNAMIC
            && allInfo.resTable[i].flags & RESF_EXTERNAL) {
            resName = allInfo.resTable[i].name;

            if (!defaultRunElim){
                if ((bitPos=getSharedResBitPos(resName)) == -1)
                    continue;
            }
            if ((resValue=getExtResourcesVal(resName)) == NULL)
                continue;

            if (saveSBValue (resName, resValue) == 0)
                continue;
            fValue = atof(resValue);

            saveIndx(resName, fValue);
        }
    }

}

int
isResourceSharedByHost(struct hostNode *host, char * resName)
{
    int i;
    for (i = 0; i < host->numInstances; i++) {
        if (strcmp(host->instances[i]->resName, resName) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

#define timersub(a,b,result)                                    \
    do {                                                        \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;           \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;        \
        if ((result)->tv_usec < 0) {                            \
            --(result)->tv_sec;                                 \
            (result)->tv_usec += 1000000;                       \
        }                                                       \
    } while (0)

/* getusr()
 * Read the pipe with elim and gather the external
 * load indexes or shared resources.
 */
static void
getusr(void)
{
    static FILE *fp;
    static time_t lastStart;
    int i;
    int nfds;
    struct timeval timeout;
    static char resbuf[BUFSIZ];

    if (!callElim()) {
        return;
    }

    getExtResourcesLoad();

    if (!startElim()) {
        return;
    }

    if ((elim_pid < 0) && (time(0) - lastStart > 90)) {

        if (ELIMrestarts < 0 || ELIMrestarts > 0) {

            if (ELIMrestarts > 0) {
                ELIMrestarts--;
            }

            if (!myClusterPtr->eLimArgv) {
                static char path[PATH_MAX];

                strcpy(path, limParams[LSF_SERVERDIR].paramValue);
                strcat(path, "/");
                strcat(path, "elim");

                myClusterPtr->eLimArgv
                    = parseCommandArgs(path,
                                       myClusterPtr->eLimArgs);
            }

            if (fp) {
                fclose(fp);
                fp = NULL;
            }

            lastStart = time(NULL);

            if (masterMe)
                putEnv("LSF_MASTER", "Y");
            else
                putEnv("LSF_MASTER", "N");

            for (i = NBUILTINDEX; i < allInfo.nRes; i++) {

                if (allInfo.resTable[i].flags & RESF_EXTERNAL)
                    continue;

                if ((allInfo.resTable[i].flags & RESF_DYNAMIC)
                    && !(allInfo.resTable[i].flags & RESF_BUILTIN)){

                    if ((allInfo.resTable[i].flags & RESF_SHARED)
                        && (!masterMe)
                        && (isResourceSharedInAllHosts(allInfo.resTable[i].name))){
                        continue;
                    }

                    if ((allInfo.resTable[i].flags & RESF_SHARED)
                        && (!isResourceSharedByHost(myHostPtr, allInfo.resTable[i].name)) )
                        continue;

                    if (resbuf[0] == '\0')
                        sprintf(resbuf, "%s ", allInfo.resTable[i].name);
                    else {
                        sprintf(resbuf + strlen(resbuf), "\
%s ", allInfo.resTable[i].name);
                    }
                }
            }
            putEnv ("LSF_RESOURCES", resbuf);

            if ((fp = lim_popen(myClusterPtr->eLimArgv, "r")) == NULL) {
                ls_syslog(LOG_ERR, "\
%s: lim_popen() failed %s", __func__, myClusterPtr->eLimArgv[0]);
                setUnkwnValues();

                return;
            }
            ls_syslog(LOG_INFO, "\
%s: Started ELIM %s pid %d", __func__,
                      myClusterPtr->eLimArgv[0], (int)elim_pid);
            ls_syslog(LOG_DEBUG, "\
%s: elim LSF_RESOURCES %s", __func__, resbuf);

            mustSendLoad = TRUE ;
        }
    }

    if (elim_pid < 0) {
        setUnkwnValues();
        if (fp) {
            fclose(fp);
            fp = NULL;
        }
        return;
    }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 5;

    if ((nfds = rd_select_(fileno(fp), &timeout)) < 0) {
        ls_syslog(LOG_ERR, "%s: rd_select() failed %m", __func__);
        lim_pclose(fp);
        fp = NULL;
        return;
    }

    if (nfds == 1) {
        int numIndx;
        int cc;
        static char name[MAXLSFNAMELEN];
        static char svalue[MAXLSFNAMELEN];
        float value;
        sigset_t  oldMask;
        sigset_t  newMask;

        blockSigs_(0, &newMask, &oldMask);

        if (logclass & LC_TRACE) {
            ls_syslog(LOG_DEBUG,"\
%s: Signal mask has been changed, all are signals blocked now", __func__);
        }

        cc = fscanf(fp, "%d", &numIndx);
        if (cc != 1) {
            ls_syslog(LOG_ERR, "\
%s: Protocol error numIndx not read (cc=%d): %m", __func__, cc);
            lim_pclose(fp);
            fp = NULL;
            unblockSigs_(&oldMask);
            return;
        }
        if (numIndx < 0) {
            ls_syslog(LOG_ERR, "%\
s: Protocol error numIndx %d", __func__, numIndx);
            setUnkwnValues();
            lim_pclose(fp);
            fp = NULL;
            unblockSigs_(&oldMask);
            return;
        }

        for (i = 0; i < numIndx; i++) {
            cc = fscanf(fp,"%s %s", name, svalue);
            if (cc != 2) {
                ls_syslog(LOG_ERR, "\
%s: Protocol error on indx %d (cc = %d): %m", __func__, i, cc);
                lim_pclose(fp);
                return;
            }

            ls_syslog(LOG_DEBUG, "\
%s: numIndx %d name %s value %s", __func__, numIndx, name, svalue);

            /* Shared resource.
             */
            if (saveSBValue(name, svalue) == 0)
                continue;
            value = atof(svalue);
            /* Load index.
             */
            saveIndx(name, value);
        }

        unblockSigs_(&oldMask);
    } /* if (nfds == 1) */
}

void
unblockSigs_(sigset_t*  mask)
{
    static char fname[] = "unblockSigs_()";

    sigprocmask(SIG_SETMASK, mask, NULL);

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG,"\
%s: The original signal mask has been restored", fname);
    }

}

static void
setUnkwnValues(void)
{
    int i;

    for(i = 0; i < allInfo.numUsrIndx; i++)
        myHostPtr->loadIndex[NBUILTINDEX + i] = INFINIT_LOAD;

    for (i = 0; i < myHostPtr->numInstances; i++) {

        if (myHostPtr->instances[i]->updateTime == 0
            || myHostPtr->instances[i]->updHost == NULL)
            continue;

        if (myHostPtr->instances[i]->updHost == myHostPtr) {
            strcpy (myHostPtr->instances[i]->value, "-");
            myHostPtr->instances[i]->updHost = NULL;
            myHostPtr->instances[i]->updateTime = 0;
        }
    }
}

static int
saveSBValue(char *name, char *value)
{
    int i;
    int indx;
    int j;
    int myHostNo = -1;
    int updHostNo = -1;

    if ((indx = getResEntry(name)) < 0)
        return (-1);

    if (!(allInfo.resTable[indx].flags & RESF_DYNAMIC))
        return -1 ;

    if (allInfo.resTable[indx].valueType == LS_NUMERIC){
        if (!isanumber_(value)){
            return -1;
        }
    }

    if (myHostPtr->numInstances <= 0)
        return (-1);

    for (i = 0; i < myHostPtr->numInstances; i++) {

        if (strcmp(myHostPtr->instances[i]->resName, name))
            continue;

        if (masterMe) {

            for (j = 0; j < myHostPtr->instances[i]->nHosts; j++) {

                if (myHostPtr->instances[i]->updHost
                    && (myHostPtr->instances[i]->updHost
                        == myHostPtr->instances[i]->hosts[j]))
                    updHostNo = j;

                if (myHostPtr->instances[i]->hosts[j] == myHostPtr)
                    myHostNo = j;

                if (myHostNo >= 0
                    && (updHostNo >= 0
                        || myHostPtr->instances[i]->updHost == NULL))
                    break;
            }
            if (updHostNo >= 0
                && (myHostNo < 0
                    || ((updHostNo < myHostNo)
                        && strcmp(myHostPtr->instances[i]->value, "-"))))
                return 0;
        }

        FREEUP(myHostPtr->instances[i]->value);
        myHostPtr->instances[i]->value = strdup(value);
        if (myHostPtr->instances[i]->value == NULL) {
            ls_syslog(LOG_ERR, "\
%s: strdup() %d bytes for %s failed, %m.", __func__,
                      strlen(value), value);
            return -1;
        }
        myHostPtr->instances[i]->updateTime = time(NULL);
        myHostPtr->instances[i]->updHost = myHostPtr;

        ls_syslog(LOG_DEBUG, "\
%s: i %d resName %s value %s updHost %s",
                  __func__, i, myHostPtr->instances[i]->resName,
                  myHostPtr->instances[i]->value,
                  myHostPtr->instances[i]->updHost->hostName);
        return 0;
    }
    return -1;
}

void
initConfInfo(void)
{
    char *sp;

    if((sp = getenv("LSF_NCPUS")) != NULL)
        myHostPtr->statInfo.maxCpus = atoi(sp);
    else
        myHostPtr->statInfo.maxCpus = numCPUs();

    if (myHostPtr->statInfo.maxCpus <= 0) {
        ls_syslog(LOG_ERR, "\
%s: Invalid num of CPUs %d. Default to 1", __func__, myHostPtr->statInfo.maxCpus);
        myHostPtr->statInfo.maxCpus = 1;
    }

    myHostPtr->statInfo.portno = lim_tcp_port;
    myHostPtr->statInfo.hostNo = myHostPtr->hostNo;
    myHostPtr->infoValid = TRUE;
}

static char *
getElimRes(void)
{
    int i;
    int numEnv;
    int resNo;
    char *resNameString;

    resNameString = malloc((allInfo.nRes) * MAXLSFNAMELEN);
    if (resNameString == NULL) {
        ls_syslog(LOG_ERR, "\
%s: failed allocate %d bytes %m", __func__, allInfo.nRes * MAXLSFNAMELEN);
        lim_Exit("getElimRes");
    }

    numEnv = 0;
    resNameString[0] = '\0';
    for (i = 0; i < allInfo.numIndx; i++) {
        if (allInfo.resTable[i].flags & RESF_EXTERNAL)
            continue;
        if (numEnv != 0)
            strcat (resNameString, " ");
        strcat(resNameString, allInfo.resTable[i].name);
        numEnv++;
    }

    for (i = 0; i < myHostPtr->numInstances; i++) {
        resNo = resNameDefined (myHostPtr->instances[i]->resName);
        if (allInfo.resTable[resNo].flags & RESF_EXTERNAL)
            continue;
        if (allInfo.resTable[resNo].interval > 0) {
            if (numEnv != 0)
                strcat(resNameString, " ");
            strcat (resNameString, myHostPtr->instances[i]->resName);
            numEnv++;
        }
    }

    if (numEnv == 0)
        return NULL;

    return resNameString;
}

static int
callElim(void)
{
    static int runit = FALSE;
    static int lastTimeMasterMe = FALSE;

    if (masterMe && !lastTimeMasterMe) {
        lastTimeMasterMe = TRUE ;
        if (runit){
            termElim() ;
            if (myHostPtr->callElim || defaultRunElim)
                return TRUE ;

            runit = FALSE ;
            return FALSE ;
        }
    }

    if (!masterMe && lastTimeMasterMe){
        lastTimeMasterMe = FALSE ;

        if (runit){
            termElim() ;

            if (myHostPtr->callElim || defaultRunElim)
                return TRUE ;

            runit = FALSE ;
            return FALSE ;
        }
    }

    if (masterMe)
        lastTimeMasterMe = TRUE ;
    else
        lastTimeMasterMe = FALSE ;

    if (runit) {
        if (!myHostPtr->callElim && !defaultRunElim){
            termElim();
            runit = FALSE ;
            return FALSE ;
        }
    }

    if (defaultRunElim) {
        runit = TRUE;
        return TRUE;
    }

    if (myHostPtr->callElim) {
        runit = TRUE ;
        return TRUE ;
    }
    runit = FALSE ;
    return FALSE ;
}

static int
startElim(void)
{
    static int notFirst=FALSE, startElim=FALSE;
    int i;

    if (!notFirst){

        for (i = 0; i < allInfo.nRes; i++) {
            if (allInfo.resTable[i].flags & RESF_EXTERNAL)
                continue;
            if ((allInfo.resTable[i].flags & RESF_DYNAMIC)
                && !(allInfo.resTable[i].flags & RESF_BUILTIN)){
                startElim = TRUE;
                break;
            }
        }
        notFirst = TRUE;
    }

    return startElim;
}

static void
termElim(void)
{
    if (elim_pid == -1)
        return;

    kill(elim_pid, SIGTERM);
    elim_pid = -1;

}

static int
isResourceSharedInAllHosts(char *resName)
{
    struct sharedResourceInstance *tmpSharedRes;

    for (tmpSharedRes=sharedResourceHead;
         tmpSharedRes ;
         tmpSharedRes=tmpSharedRes->nextPtr ){

        if (strcmp(tmpSharedRes->resName, resName)) {
            continue;
        }
        if (tmpSharedRes->nHosts == myClusterPtr->numHosts) {
            return(1);
        }
    }

    return(0);

}

