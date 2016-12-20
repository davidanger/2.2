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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include "../lsf.h"
#include "../lib/lproto.h"
#include "../lib/lsi18n.h"


static void usage(char *);
static void print_long(struct resItem *);
static char nameInList(char **, int, char *);
static char *flagToStr(int);
static char *orderTypeToStr(enum orderType);
static char *valueTypeToStr(enum valueType);

int
main(int argc, char **argv)
{
    struct lsInfo *lsInfo;
    int i;
    int cc;
    int nnames;
    char *namebufs[256];
    char longFormat = FALSE;
    char rFlag = FALSE;
    char tFlag = FALSE;
    char mFlag = FALSE;
    char mmFlag = FALSE;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }

    while ((cc = getopt(argc, argv, "VhlrmMt")) != EOF) {
        switch(cc) {
            case 'V':
                fputs(_LS_VERSION_, stderr);
                exit(0);
            case 'l':
                longFormat = TRUE;
                break;
            case 'r':
                rFlag = TRUE;
                break;
            case 't':
                tFlag = TRUE;
                break;
            case 'm':
                mFlag = TRUE;
                break;
            case 'M':
                mFlag  = TRUE;
                mmFlag = TRUE;
                break;
            case 'h':
            default:
                usage(argv[0]);
                return -1;
        }
    }

    for (nnames = 0; optind < argc; optind++, nnames++)
        namebufs[nnames] = argv[optind];

    if ((lsInfo = ls_info()) == NULL) {
        ls_perror("lsinfo");
        exit(-10);
    }

    if (!nnames && !rFlag && !mFlag && !tFlag && !mmFlag)
        rFlag = mFlag = tFlag = TRUE;
    else if (nnames)
        rFlag = TRUE;

    if (rFlag) {
        if (!longFormat)
            printf("\
%-13.13s %7.7s  %5.5s  %s\n", "RESOURCE_NAME", "  TYPE ","ORDER","DESCRIPTION");

        for (i = 0; i < lsInfo->nRes; i++) {

            if (!nameInList(namebufs, nnames, lsInfo->resTable[i].name))
                continue;

            if (!longFormat) {
                printf("%-13.13s %7.7s %5.5s   %s\n",
                       lsInfo->resTable[i].name,
                       valueTypeToStr(lsInfo->resTable[i].valueType),
                       orderTypeToStr(lsInfo->resTable[i].orderType),
                       lsInfo->resTable[i].des);
            } else
                print_long(&(lsInfo->resTable[i]));
        }

        for (i = 0; i < nnames; i++)
            if (namebufs[i])
                printf("\
%s: Resource name not found\n", namebufs[i]);

    }

    if (tFlag) {
        if (rFlag)
            putchar('\n');
        printf("%s\n", "TYPE_NAME");
        for (i = 0; i < lsInfo->nTypes; i++)
            puts(lsInfo->hostTypes[i]);
    }

    if (mFlag) {
        if (rFlag || tFlag)
            putchar('\n');
        printf("%s\n", "MODEL_NAME      CPU_FACTOR      ARCHITECTURE");
        for (i = 0; i < lsInfo->nModels; ++i)
            printf("\
%-16s    %6.2f      %s\n", lsInfo->hostModels[i],
                   lsInfo->cpuFactor[i],
                   lsInfo->hostArchs[i]);
    }

    exit(0);
}

static void
usage(char *cmd)
{
    fprintf(stderr, "\
%s: [-h] [-V] [-l] [-r] [-m] [-M] [-t] [resource_name ...]\n", cmd);
}

static void
print_long(struct resItem *res)
{
    char tempStr[15];
    static int first = TRUE;

    if (first) {
        printf("%s:  %s\n", "RESOURCE_NAME", res->name);
        first = FALSE;
    } else
        printf("\n%s:  %s\n", "RESOURCE_NAME", res->name);
    printf("DESCRIPTION: %s\n", res->des); /* catgets  1814  */

    printf("%-7.7s ", "TYPE");
    printf("%5s  ",   "ORDER");
    printf("%9s ",    "INTERVAL");
    printf("%8s ",    "BUILTIN");
    printf("%8s ",    "DYNAMIC");
    printf("%8s\n",   "RELEASE");

    sprintf(tempStr,"%d", res->interval);
    printf("%-7.7s %5s  %9s %8s %8s %8s\n",
           valueTypeToStr(res->valueType),
           orderTypeToStr(res->orderType),
           tempStr,
           flagToStr(res->flags & RESF_BUILTIN),
           flagToStr(res->flags & RESF_DYNAMIC),
           flagToStr(res->flags & RESF_RELEASE));
}

static char *
flagToStr(int flag)
{
    if (flag)
        return "yes";
    else
        return "no";
}

static char *
valueTypeToStr(enum valueType valtype)
{
    switch (valtype) {
        case LS_NUMERIC:
            return "Numeric";
            break;
        case LS_BOOLEAN:
            return "Boolean";
            break;
        default:
            return "String";
            break;
    }
}

static char *
orderTypeToStr(enum orderType ordertype)
{
    switch (ordertype) {
        case INCR:
            return "Inc";
            break;
        case DECR:
            return "Dec";
            break;
        default:
            return "N/A";
            break;
    }
}

static char
nameInList(char **namelist, int listsize, char *name)
{
    int i, j;

    if (listsize == 0)
        return TRUE;

    for (i = 0; i < listsize; i++) {

        if (!namelist[i])
            continue;

        if (strcmp(name, namelist[i]) == 0) {
            namelist[i] = NULL;

            for (j= i + 1; j < listsize; j++) {

                if(!namelist[j])
                    continue;

                if (strcmp(name, namelist[j]) == 0)
                    namelist[j] = NULL;
            }
            return TRUE;
        }
    }
    return FALSE;
}
