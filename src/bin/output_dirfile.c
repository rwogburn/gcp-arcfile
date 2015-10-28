/*
 * RWO 090403 - dump time streams to standard
 *              output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "readarc.h"

#define DEBUG_OUTPUT_DIRFILE 0

#if DEBUG_OUTPUT_DIRFILE == 1
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(args...)
#endif

const char * dirfile_datatype_code (int data_type)
{
    switch (data_type)
    {
        case GCP_REG_CHAR : return "INT8";
        case GCP_REG_UCHAR : return "UINT8";
        case GCP_REG_INT : return "INT32";
        case GCP_REG_UINT : return "UINT32";
        case GCP_REG_FLOAT : return "FLOAT32";
        case GCP_REG_DOUBLE : return "FLOAT64";
	case GCP_REG_UTC : return "UINT64";
    }
    printf ("Unsupported data type %d.\n", data_type);
    return "-";
}

int get_elsize (int data_type)
{
    switch (data_type)
    {
        case GCP_REG_CHAR :
        case GCP_REG_UCHAR : return 1;
        case GCP_REG_INT :
        case GCP_REG_UINT : return 4;
        case GCP_REG_FLOAT : return 4;
        case GCP_REG_DOUBLE : return 8;
        case GCP_REG_UTC : return 8;
    }
    printf ("Unsupported data type %d.\n", data_type);
    return 1;
}

int write_dirfile_format (const char * fname, struct dataset * ds)
{
    int i, j;
    int numchan;
    FILE * f;

    f = fopen (fname, "wt");

    for (i=0; i<ds->nb; i++)
    {
        if (ds->buf[i].rb->regblock[0] == '\0')
            continue;
        if (ds->buf[i].chan.n == 0)
          numchan = ds->buf[i].rb->nchan;
        else
          numchan = ds->buf[i].chan.ntot;
        for (j=0; j<numchan; j++)
        {
          if (numchan==1)
            fprintf (f, "%s.%s.%s\t", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
          else if (isdigit(ds->buf[i].rb->regblock[strlen(ds->buf[i].rb->regblock)-1]))
            fprintf (f, "%s.%s.%s_%d\t", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);
          else
            fprintf (f, "%s.%s.%s%d\t", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);
          fprintf (f, "RAW\t");
          fprintf (f, "%s\t", dirfile_datatype_code (ds->buf[i].rb->typeword & GCP_REG_TYPE));
          fprintf (f, "%d\n", ds->buf[i].rb->spf);
        }
    }
    fclose (f);

    return 0;
}

int write_dirfile_data (const char * basedir, struct dataset * ds)
{
    int i, j;
    int numchan;
    int elsize;
    FILE * f;
    char fname[256];

    for (i=0; i<ds->nb; i++)
    {
        if (ds->buf[i].rb->regblock[0] == '\0')
            continue;
        if (ds->buf[i].chan.n == 0)
          numchan = ds->buf[i].rb->nchan;
        else
          numchan = ds->buf[i].chan.ntot;
        for (j=0; j<numchan; j++)
        {
          if (numchan==1)
            snprintf (fname, 255, "%s/%s.%s.%s", basedir, ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
          else if (isdigit(ds->buf[i].rb->regblock[strlen(ds->buf[i].rb->regblock)-1]))
            snprintf (fname, 255, "%s/%s.%s.%s_%d", basedir, ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);
          else
            snprintf (fname, 255, "%s/%s.%s.%s%d", basedir, ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);

          f = fopen (fname, "wb");
          elsize = get_elsize(ds->buf[i].rb->typeword & GCP_REG_TYPE);
          fwrite (j*ds->buf[i].rb->spf*ds->buf[i].numframes*elsize + ds->buf[i].buf, elsize, ds->buf[i].rb->spf*ds->buf[i].numframes, f);
          fclose (f);
        }
    }

    return 0;
}

int output_dirfile (const char * basedir, struct dataset * ds)
{
  int i, j, k;
  int numchan;

  write_dirfile_format ("tmp/format", ds);
  write_dirfile_data ("tmp/", ds);
  return 0;
}

