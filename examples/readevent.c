
#include <lsf.h>
#include <lsbatch.h>

static void
usage(void)
{
    fprintf(stderr, "usage: readevent: eventfile\n");
}
extern char *getNextWord_(char **);

int
main(int argc,
     char **argv)
{
    struct eventRec *er;
    FILE *fp;
    char *file;
    char *p;
    char *buf;
    int cc;

    file = argv[1];
    if (!file) {
        usage();
        return -1;
    }

    cc = lsb_init((char *)__func__);
    if (cc < 0) {
        fprintf(stderr, "%s\n", lsb_sysmsg());
        return -1;
    }

    fp = fopen(file, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    buf = calloc(BUFSIZ/2, sizeof(char));
    while ((fgets(buf, BUFSIZ, fp))) {
        while ((p = getNextWord_(&buf)))
            printf("%s\n", p);
    }

    while ((er = lsb_geteventrec(fp, &cc))) {
        printf("%s %d\n", er->version, er->type);
    }

    fclose(fp);

    return 0;

} /* main() */
