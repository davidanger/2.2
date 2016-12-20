/*
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) 2014 David Bigagli

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
#include "../lsf.h"
#include "../lib/lsi18n.h"

static void
usage(const char *cmd)
{
    fprintf(stderr, "%s: [-h] [-V] [-q] [-r] [-s] task_name\n", cmd);
    exit(-1);
}

int
main(int argc, char **argv)
{
    static char fname[] = "lseligible/main";
    char resreq[MAXLINELEN];
    char mode = LSF_LOCAL_MODE;
    int quiet = 0;
    int	cc;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }
    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    opterr = 0;
    while ((cc = getopt(argc, argv, "Vqrhs")) != EOF) {
        switch (cc) {
            case 'q':
                quiet = 1;
                break;
            case 's':
                quiet = 2;
                break;
            case 'r':
                mode = LSF_REMOTE_MODE;
                break;
            case 'V':
                fputs(_LS_VERSION_, stderr);
                exit(0);
            case 'h':
            default:
                usage(argv[0]);
	}
    }

    if ((optind != (argc - 1))
	|| (argv[optind] == NULL)) {
	usage(argv[0]);
    }

    lserrno = LSE_NO_ERR;
    if (!ls_eligible(argv[optind], resreq, mode))
    {
	if (quiet == 0)
	    printf("NON-ELIGIBLE\n");
        if (lserrno == LSE_NO_ERR)
	    exit(1);
        else
            exit(-10);
    }
    if (quiet == 0)
	printf("ELIGIBLE ");

    if (quiet < 2)
        puts(resreq);

    return 0;
}
