/*
 * RWO 090403 - dump time streams to standard output as hex.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "readarc.h"

#define DEBUG_OUTPUT_HEX 1

#if DEBUG_OUTPUT_HEX == 1
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(args...)
#endif

int output_hex (struct dataset * ds)
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
    k = 0;
    for (j=0;
      j < (numchan * ds->buf[i].rb->spf * ds->buf[i].numframes *ds->buf[i].elsize);
      j++)
    {
      if (k == 2000)
      {
        printf ("\n");
        k = 0;
      }
      k++;
      printf ("%02hhx", *((char *)ds->buf[i].buf+j));
    }
    printf ("\n\n");
  }
  return 0;
}
