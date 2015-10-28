/*
 * reglist.h - handle register maps parsed from arc file headers.
 *
 * RWO 090402
 *
 */
#ifndef ARCFILE_REGLIST_H_
#define ARCFILE_REGLIST_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "namelist.h"

#define MAX_NAME_LENGTH 100
#define DO_DEBUG_REGLIST 0

#define GCP_REG_COMPLEX    0x1
#define GCP_REG_EXC        0x100
#define GCP_REG_FAST       0x200000

#define GCP_REG_UTC        0x200
#define GCP_REG_BOOL       0x800
#define GCP_REG_CHAR       0x1000
#define GCP_REG_UCHAR      0x2000
#define GCP_REG_SHORT      0x4000
#define GCP_REG_USHORT     0x8000
#define GCP_REG_INT        0x10000
#define GCP_REG_UINT       0x20000
#define GCP_REG_FLOAT      0x40000
#define GCP_REG_DOUBLE     0x80000

#define GCP_REG_TYPE       0xFFA00

struct regblockspec {
    char map[MAX_NAME_LENGTH+1];
    char board[MAX_NAME_LENGTH+1];
    char regblock[MAX_NAME_LENGTH+1];
    uint32_t typeword;
    char is_fast, is_complex, do_arc;
    int nchan, spf;
};

struct reglist_entry {
    struct regblockspec rb;
    uint32_t ofs_in_frame;
    struct chanlist chan;
};

struct reglist {
    int max_regblocks, num_regblocks;
    struct reglist_entry * r;
    int utc_reg_num;
};

int parse_reglist (void * buf, int buflen, int do_swap, struct reglist * rm, int max_regblocks);
int parse_reglist_namelist (void * buf, int buflen, int do_swap, struct namelist * filt,
    struct reglist * rm, int max_regblocks);
int free_reglist (struct reglist * rm);

#endif
