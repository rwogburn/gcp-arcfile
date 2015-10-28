/*
 * namelist.h - handle lists of requested arcfile registers,
 *              including wild cards and specified channels.
 *
 * RWO 090402
 *
 */

#ifndef ARCFILE_NAMELIST_H_
#define ARCFILE_NAMELIST_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_NAME_LENGTH 100
#define DO_DEBUG_NAMELIST 0

struct chanlist {
    int n, ntot;
    int * c1;
    int * c2;
};

struct search_spec {
    char * m;
    char * b;
    char * r;
    struct chanlist chan;
    struct chanlist samp;
};

struct namelist {
    int n;
    struct search_spec * s;
    int nutc;
};

int create_namelist (int n, char ** s, struct namelist * f);
int test_reg_name (char * m, char * b, char * r, struct namelist * f);
int free_namelist (struct namelist * f);
int copy_chanlist (struct chanlist * dst, struct chanlist * src);
int free_chanlist (struct chanlist * chan);

#endif
