/*
 * data.h - handle data types and buffers for arc file
 *          registers.
 *
 * RWO 090402
 *
 */

#ifndef ARCFILE_DATASET_H_
#define ARCFILE_DATASET_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define DO_DEBUG_DATASET 1

#include "databuf.h"

struct dataset {
    int nb;
    int max_frames, num_frames;
    struct databuf * buf;
};

int init_dataset (struct dataset * ds, struct reglist * rl, int numframes);
int free_dataset (struct dataset * ds);
int copy_dataset (struct dataset * src, struct dataset * tgt);
int dataset_tight_size (struct dataset * ds);
int dataset_resize (struct dataset * ds, int nframes);

#endif
