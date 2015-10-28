/*
 * RWO 090403 - dump time streams to standard
 *              output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "readarc.h"
#include "tarfile.h"

#define DEBUG_OUTPUT_DIRBALL 0

#if DEBUG_OUTPUT_DIRBALL == 1
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

int write_dirfile_format (struct tarfile * tf, struct dataset * ds)
{
    int i, j;
    int numchan;

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
            tfprintf (tf, "%s.%s.%s\t", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
          else if (isdigit(ds->buf[i].rb->regblock[strlen(ds->buf[i].rb->regblock)-1]))
            tfprintf (tf, "%s.%s.%s_%d\t", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);
          else
            tfprintf (tf, "%s.%s.%s%d\t", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);
          tfprintf (tf, "RAW\t");
          tfprintf (tf, "%s\t", dirfile_datatype_code (ds->buf[i].rb->typeword & GCP_REG_TYPE));
          tfprintf (tf, "%d\n", ds->buf[i].rb->spf);
        }
    }

    return 0;
}

int write_dirfile_data (struct tarfile * tf, struct dataset * ds)
{
    int i, j;
    int numchan;
    int elsize;
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
            snprintf (fname, 255, "%s.%s.%s", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
          else if (isdigit(ds->buf[i].rb->regblock[strlen(ds->buf[i].rb->regblock)-1]))
            snprintf (fname, 255, "%s.%s.%s_%d", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);
          else
            snprintf (fname, 255, "%s.%s.%s%d", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock, j);

          elsize = get_elsize(ds->buf[i].rb->typeword & GCP_REG_TYPE);
          tarfile_binary (tf, fname, elsize * ds->buf[i].rb->spf * ds->buf[i].numframes, j*ds->buf[i].rb->spf*ds->buf[i].numframes*elsize + ds->buf[i].buf);
        }
    }

    return 0;
}

int output_dirball (const char * basedir, struct dataset * ds, int ftype)
{
  int i, j, k;
  int numchan;
  struct tarfile tf;

  printf ("Opening dirfile tarball.\n");
  tarfile_open (&tf, basedir, ftype);
  tarfile_set_user (&tf, "reuben", 0767, "staff", 0024);
  printf ("Starting format portion.\n");
  tarfile_start_txt (&tf, "format");
  printf ("Writing format portion.\n");
  write_dirfile_format (&tf, ds);
  printf ("CLosing format portion.\n");
  tarfile_stop_txt (&tf, "format");
  printf ("Writing dirfile data.\n");
  write_dirfile_data (&tf, ds);
  printf ("Closing dirfile tarball.\n");
  tarfile_close (&tf);
  printf ("All done.\n");

  return 0;
}

