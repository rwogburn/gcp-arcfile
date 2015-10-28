#ifndef READARC_H
#define READARC_H

/* #include <stdio.h> */
#include <stdlib.h>
#include <stdint.h>
#include "namelist.h"
#include "dataset.h"

#define ARC_OK		0x00
#define ARC_ERR_NOFILE	0x01
#define ARC_ERR_EOF	0x02
#define ARC_ERR_REGMAP	0x10
#define ARC_ERR_NSAMP	0x11
#define ARC_ERR_NOMEM	0x20
#define ARC_ERR_FORMAT	0x40
#define ARC_ERR_SIGINT  0x80

#define STANDARD_FILE_NFRAMES 2000

struct arcfilt {
    int use_utc;
    uint32_t t1[2];
    uint32_t t2[2];
    struct namelist nl;
    char * fname;
};

int arcfilt_init (struct arcfilt * af);
int arcfilt_set_utcrange (struct arcfilt * af, char * t1str, char * t2str);
int arcfilt_set_regs (struct arcfilt * af, int n, char ** r);
int arcfilt_set_path (struct arcfilt * af, char * fn);
int arcfilt_free (struct arcfilt * af);

#define MAX_NAME_LENGTH 100

int readarc (struct arcfilt * af, struct dataset * ds);
int dataset_free (struct dataset * ds);

#endif
