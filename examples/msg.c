#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct jinfo {
    char name[32];
    int z;
    char *p;
    int num;
    char **np;
};

struct mzgbuf {
    long mtype;
    char mtext[64];
};

int
main(int argc, char **argv)
{
    int cc;
    int msqid;
    struct mzgbuf msg;
    struct mzgbuf msg2;
    size_t len;
    struct msqid_ds buf;

    msqid = msgget(IPC_PRIVATE, IPC_CREAT|0666);
    if (msqid < 0) {
        perror("msgget()");
        return -1;
    }

    cc = msgctl(msqid, IPC_STAT, &buf);

    msg.mtype = 1;
    sprintf(msg.mtext, "ciao ciao ciao");
    len = strlen(msg.mtext) + 1;

    cc = msgsnd(msqid, &msg, len, 0);
    if (cc < 0) {
        perror("msgsnd");
        return -1;
    }

    cc = msgrcv(msqid, &msg2, 2 * len, 0, 0);
    if (cc < 0) {
        perror("msgrcv");
        return -1;
    }

    printf("%ld %s\n", msg2.mtype, msg2.mtext);

    return 0;
}
