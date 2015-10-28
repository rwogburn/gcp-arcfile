#ifndef ARCFILE_DATABUF_H_
#define ARCFILE_DATABUF_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "reglist.h"

#define DO_DEBUG_DATABUF 0

struct databuf {
    int elsize;
    struct regblockspec * rb;
    int numframes, maxframes;
    int bufsize;
    void * buf;
    struct chanlist chan;
};

int element_size (uint32_t typeword);
int allocate_databuf (struct regblockspec * rb, struct chanlist * chan, int numframes, struct databuf * ts);
int change_databuf_numframes (struct databuf * ts, int numframes);
int change_databuf_nchan (struct databuf * ts, int nchan);
int check_promote_databuf (struct databuf * ts, uint32_t typeword);
int copy_to_buf (FILE * f, struct databuf * ts, int32_t * ofs);
int memcopy_to_buf (void * m, struct databuf * ts);

#endif
