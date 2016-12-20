
/* Test if PIM catches me.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main(int argc, char **argv)
{
    pid_t pid;
    int cc;

    /* Simple fork
     */
    pid = fork();
    if (pid == 0) {
        printf("Child 1 %d %d\n", getpid(), getppid());
        while (1)
            ++cc;
        exit(0);
    }

    printf("Parent %d forked %d\n", getpid(), pid);
    sleep(10);
    /* Fork and have the child to
     * set a new process groupID.
     */
    pid = fork();
    if (pid == 0) {
        setpgid(getpid(), getpid());
        printf("Child 2 %d %d\n", getpid(), getppid());
        sleep(3600);
        exit(0);
    }
    printf("Parent %d forked %d\n", getpid(), pid);
    sleep(10);
    /* Fork and detach the child.
     */
    pid = fork();
    if (pid == 0) {
        pid = fork();
        if (pid == 0) {
            setsid();
            printf("Child 3 %d %d\n", getpid(), getppid());
            sleep(86400);
            exit(0);
        }
        if (pid > 0) {
            printf("Parent %d forked %d\n", getpid(), pid);
            exit(0);
        }
    }
    printf("Parent %d forked %d\n", getpid(), pid);
    while (1) {
        pid = wait(NULL);
        printf("Parent %d child %d gone\n", getpid(), pid);
    }

    return 0;
}
