/*
 * RWO 090403 - dump time streams to standard
 *              output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "readarc.h"
#include "output_txt.h"

#define DEBUG_ARC2TXT 0

#if DEBUG_ARC2TXT == 1
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(args...)
#endif

int main (int argc, char *argv[])
{
    char * dname;
    struct arcfilt filt;
    struct dataset ds;
    int r;

    if (argc < 2)
    {
        printf ("readarc takes at least one argument.");
        return -1;
    }

    dname = argv[1];
    if (dname == 0)
    {
         printf ("could not get file name.\n");
         return 1;
    }

    if (argc < 4)
      filt.use_utc = 0;
    else
    {
      r = txt2utc (argv[2], filt.t1);
      if (r != 0)
      {
        printf ("could not parse UTC time 1!");
        return -1;
      }
      r = txt2utc (argv[3], filt.t2);
      if (r != 0)
      {
        printf ("could not parse UTC time 2!");
        return -1;
      }
      DEBUG ("Selecting on time range (%lu,%lu) - (%lu,%lu)\n",
        filt.t1[0], filt.t1[1], filt.t2[0], filt.t2[1]);
      filt.use_utc = 1;
    }

    if (argc < 5)
    {
      filt.nl.n = 0;
      filt.nl.s = 0;
    }
    else
    {
      int nn, in;
      char ** nlist;
      nn = argc - 4;
      nlist = malloc (nn * sizeof (char *));
      for (in=0; in<nn; in++)
        nlist[in] = argv[in+4];
      create_namelist (nn, nlist, &(filt.nl));
      free (nlist);
    }
    filt.fname = dname;
    DEBUG ("Number of register name specifications = %d.\n", filt.nl.n);

    DEBUG ("Calling readarc.\n");

    r = readarc (&filt, &ds); 
    DEBUG ("Returned %d.\n", r);
    free_namelist (&(filt.nl));
    if (r != 0)
      return r;

    DEBUG ("Dumping timestreams.\n");    
    r = output_txt (&ds);

    free_dataset (&ds);

    return 0;
}
