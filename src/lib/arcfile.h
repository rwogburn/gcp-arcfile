/*
 * arcfile.h - read arc files, extracting requested registers
 *             as time streams.
 *
 * RWO 090402
 *
 */

#ifndef ARCFILE_ARCFILE_H_
#define ARCFILE_ARCFILE_H_

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "reglist.h"
#include "namelist.h"
#include "dataset.h"

#define HAVE_GZ		1
#define HAVE_BZ2	0

#if HAVE_GZ == 1
#  include <zlib.h>
#endif
#if HAVE_BZ2 == 1
#  include <bzlib.h>
#endif
#define ARC_FILE_PLAIN	0
#define ARC_FILE_GZ	1
#define ARC_FILE_BZ2	2

#define TIME time_t *
#define DO_DEBUG_ARCFILE 0

/* Select which method to use for reading frames. */
/*        1 - straightforward fread               */
/*        2 - buffer frame-by-frame with fread    */
/*      + 3 - buffer N frames with fread          */
/*       (4 - straightforward mmap)               */
#define arcfile_read_frames arcfile_read_frames_3

struct arcfile {
    FILE * f;
#if HAVE_GZ == 1
    gzFile g;
#endif
#if HAVE_BZ2 == 1
    BZFILE * b;
#endif
    int file_type;
    uint32_t fsize;
    int do_swap_header, do_swap_data;

    uint32_t header[6];
    uint32_t version;
    uint32_t frame_len;
    uint32_t frame0_ofs;
    uint32_t numframes;
};

int arcfile_open (char * fname, struct arcfile * af);
int arcfile_close (struct arcfile * af);
int arcfile_read_regmap (struct arcfile * af, struct reglist * rl);
int arcfile_read_regmap_namelist (struct arcfile * af, struct namelist * nl, struct reglist * rl);
int arcfile_skip_regmap (struct arcfile * af);
int arcfile_read_frames (struct arcfile * af, struct reglist * rl, struct dataset * ds);
int arcfile_read_frames_utc (struct arcfile * af, struct reglist * rl, uint32_t t1[2], uint32_t t2[2], struct dataset * ds);

#endif
