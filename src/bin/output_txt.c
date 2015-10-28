/*
 * RWO 090403 - dump time streams to standard
 *              output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "readarc.h"

#define DEBUG_OUTPUT_TXT 0

#if DEBUG_OUTPUT_TXT == 1
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(args...)
#endif

int output_txt (struct dataset * ds)
{
  int i, j, k;
  int numchan;

  printf ("dump_timestreams: n=%d.\n", ds->nb);
  for (i=0; i<ds->nb; i++)
  {
    printf ("###\n");
    printf ("%s %s %s\n", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
    switch (ds->buf[i].rb->typeword & GCP_REG_TYPE)
    {
      case GCP_REG_UINT:
        printf ("uint32\n");
        break;
      case GCP_REG_INT:
        printf ("int32\n");
        break;
      case GCP_REG_UCHAR:
        printf ("uint8\n");
        break;
      case GCP_REG_CHAR:
        printf ("int8\n");
        break;
      case GCP_REG_FLOAT:
        printf ("single\n");
        break;
      case GCP_REG_DOUBLE:
        printf ("double\n");
        break;

      /* Just treat UTC times as a UINT64, for now. */
      case GCP_REG_UTC:
        printf ("uint64\n");
        break;

      default: 
        printf ("Skipping register of type 0x%lx.\n", ds->buf[i].rb->typeword & GCP_REG_TYPE);
        continue;
    }
    if (ds->buf[i].chan.n == 0)
      numchan = ds->buf[i].rb->nchan;
    else
      numchan = ds->buf[i].chan.ntot;
    printf ("%ld %ld\n", ds->buf[i].rb->spf * ds->buf[i].numframes, numchan);
    for (j=0; j < (ds->buf[i].rb->spf * ds->buf[i].numframes); j++)
    {
      for (k=0; k < numchan; k++)
      {
        switch (ds->buf[i].rb->typeword & GCP_REG_TYPE)
        {
          case GCP_REG_UINT:
            printf("%u ", *(j + k*ds->buf[i].rb->spf*ds->buf[i].numframes + (unsigned int *)ds->buf[i].buf));
            break;
          case GCP_REG_INT:
            printf("%d ", *(j + k*ds->buf[i].rb->spf*ds->buf[i].numframes + (int *)ds->buf[i].buf));
            break;
          case GCP_REG_UCHAR:
            printf("%hhu ", *(j + k*ds->buf[i].rb->spf*ds->buf[i].numframes + (unsigned char *)ds->buf[i].buf));
            break;
          case GCP_REG_CHAR:
            printf("%c ", *(j + k*ds->buf[i].rb->spf*ds->buf[i].numframes + (char *)ds->buf[i].buf));
            break;
          case GCP_REG_FLOAT:
            printf("%f ", *(j + k*ds->buf[i].rb->spf*ds->buf[i].numframes + (float *)ds->buf[i].buf));
            break;
          case GCP_REG_DOUBLE:
            printf("%lf ", *(j + k*ds->buf[i].rb->spf*ds->buf[i].numframes + (double *)ds->buf[i].buf));
            break;

          /* Just treat UTC times as a UINT64, for now. */
          case GCP_REG_UTC:
            printf("%llu ", *(j + k*ds->buf[i].rb->spf*ds->buf[i].numframes + (long long int *)ds->buf[i].buf));
            break;
        }
      }
      printf ("\n");
    }
    printf ("\n\n");
  }
  return 0;
}

