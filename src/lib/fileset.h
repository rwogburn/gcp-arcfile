#ifndef ARCFILE_FILESET_H_
#define ARCFILE_FILESET_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utcrange.h"
#include "readarc.h"

#define TIME time_t *

#define DO_DEBUG_FILESET 0

struct fileset_file {
    char * name;
    size_t size;
};

struct fileset {
    int nf;
    struct fileset_file * files;
};

int init_fileset_utc (char * fname, uint32_t t1[2], uint32_t t2[2], struct fileset * fset);
int init_fileset (char * fname, struct fileset * fset);
int free_fileset (struct fileset * fset);
#endif
