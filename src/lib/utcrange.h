/*
 * utcrange.h - handle date and time calculations.
 *
 * RWO 090402
 *
 */
#ifndef ARCFILE_UTCRANGE_H_
#define ARCFILE_UTCRANGE_H_

#define DO_DEBUG_UTCRANGE 0

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

int fname2utc (char * fname, uint32_t utc[2]);
int txt2utc (char * txt, uint32_t utc[2]);

#endif
