/*
 * RWO 090331 - read a GCP arc file, or
 *              read specified time streams from
 *              a directory of arc files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mex.h"
#include "readarc.h"
#include "utcrange.h"

#ifndef HAVE_OCTAVE
#  include "matrix.h"
#endif

#define PR(args...) mexPrintf(args); mexEvalString("0;")

#define DEBUG_MEX_READARC 0
#if DEBUG_MEX_READARC
#  define DEBUG(args...) PR(args)
#else
#  define DEBUG(args...)
#endif

/* Initialize a Matlab structure to hold the output:  */
/* Loop through all the register blocks we have read, */
/* building a three-level hierarchy of structs, with  */
/* the form map.board.regblock.  Don't actually fill  */
/* in any values now, just create the structs here.   */
int init_struct (struct dataset * ds, mxArray ** D)
{
  int i, j;
  int nm, nb, nr;
  char * last_map;
  char * last_board;
  char ** map_names;
  char ** board_names;
  char ** regblock_names;
  mxArray ** boards;
  mxArray ** maps;

  if (ds->nb == 0)
  {
    DEBUG ("No registers in data set, returning empty structure.\n");
    *D = mxCreateStructMatrix (0, 0, 0, NULL);
    return 0;
  }

  boards = malloc ((ds->nb) * sizeof (mxArray *));
  maps = malloc ((ds->nb) * sizeof (mxArray *));
  map_names = malloc ((ds->nb) * sizeof (char *));
  board_names = malloc ((ds->nb) * sizeof (char *));
  regblock_names = malloc ((ds->nb) * sizeof (char *));
  last_map = ds->buf[0].rb->map;
  last_board = ds->buf[0].rb->board;
  map_names[0] = last_map;
  nm = 1;
  board_names[0] = last_board;
  nb = 1;
  regblock_names[0] = ds->buf[0].rb->regblock;
  nr = 1;
  DEBUG ("About to try initializing structure.\n");
  DEBUG ("On %s.%s.%s\n", map_names[0], board_names[0], regblock_names[0]);
  for (i=1; i<ds->nb; i++)
  {
    DEBUG ("On %s.%s.%s\n", ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
    if (0 != strcmp (ds->buf[i].rb->map, last_map))
    {
      boards[nb-1] = mxCreateStructMatrix (1, 1, nr, (const char **)regblock_names);
      maps[nm-1] = mxCreateStructMatrix (1, 1, nb, (const char **)board_names);
      for (j=0; j<nb; j++)
        mxSetFieldByNumber (maps[nm-1], 0, j, boards[j]);
      regblock_names[0] = ds->buf[i].rb->regblock;
      nr = 1;
      last_board = ds->buf[i].rb->board;
      board_names[0] = last_board;
      nb = 1;
      last_map = ds->buf[i].rb->map;
      map_names[nm] = last_map;
      nm++;
      continue;
    }
    if (0 != strcmp (ds->buf[i].rb->board, last_board))
    {
      boards[nb-1] = mxCreateStructMatrix (1, 1, nr, (const char **)regblock_names);
      regblock_names[0] = ds->buf[i].rb->regblock;
      nr = 1;
      last_board = ds->buf[i].rb->board;
      board_names[nb] = last_board;
      nb++;
      continue;
    }
    regblock_names[nr] = ds->buf[i].rb->regblock;
    nr++;
  }
  boards[nb-1] = mxCreateStructMatrix (1, 1, nr, (const char **)regblock_names);
  maps[nm-1] = mxCreateStructMatrix (1, 1, nb, (const char **)board_names);
  for (j=0; j<nb; j++)
    mxSetFieldByNumber (maps[nm-1], 0, j, boards[j]);

  (*D) = mxCreateStructMatrix (1, 1, nm, (const char **)map_names);
  for (j=0; j<nm; j++)
    mxSetFieldByNumber (*D, 0, j, maps[j]);

  DEBUG ("Done allocating, now free the temp. arrays.\n");
  free (map_names);
  free (board_names);
  free (regblock_names);
  free (maps);
  free (boards);

  return 0;
}
   
int mat_wrap_timestreams (struct dataset * ds, mxArray ** D)
{
  int i;
  mxArray * map;
  mxArray * board;
  mxArray * tmp;
  mxClassID mat_class;
  int numchan;

  init_struct (ds, D);

  for (i=0; i<ds->nb; i++)
  {
    map = mxGetField (*D, 0, ds->buf[i].rb->map);
    if (map == NULL)
      return -1;
    board = mxGetField (map, 0, ds->buf[i].rb->board);
    if (board == NULL)
      return -1;
    
    switch (ds->buf[i].rb->typeword & GCP_REG_TYPE)
    {
      case GCP_REG_UINT:
        mat_class = mxUINT32_CLASS;
        break;
      case GCP_REG_INT:
        mat_class = mxINT32_CLASS;
        break;
      case GCP_REG_UCHAR:
        mat_class = mxUINT8_CLASS;
        break;
      case GCP_REG_CHAR:
        mat_class = mxINT8_CLASS;
        break;
      case GCP_REG_FLOAT:
        mat_class = mxSINGLE_CLASS;
        break;
      case GCP_REG_DOUBLE:
        mat_class = mxDOUBLE_CLASS;
        break;

      /* Just treat UTC times as a UINT64, for now. */
      case GCP_REG_UTC:
        mat_class = mxUINT64_CLASS;
        break;

      default: 
        PR ("Skipping register of type 0x%lx.\n", ds->buf[i].rb->typeword & GCP_REG_TYPE);
        continue;
    }
    numchan = ds->buf[i].rb->nchan;
    if (ds->buf[i].chan.n != 0)
      numchan = ds->buf[i].chan.ntot;
    if (!ds->buf[i].rb->do_arc)
    {
      DEBUG ("Initializing empty matrix %s.%s.%s\n",
        ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
      tmp = mxCreateNumericMatrix (0, 0, mat_class, mxREAL);
      mxSetField (board, 0, ds->buf[i].rb->regblock, tmp);
      continue;
    }
    DEBUG ("Copying %s.%s.%s, %dx%d, numframes=%d, length=%ld.\n",
      ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock,
      ds->buf[i].rb->spf * ds->num_frames, numchan,
      ds->buf[i].numframes, ds->buf[i].bufsize);
    tmp = mxCreateNumericMatrix (
      (ds->buf[i].rb->spf * ds->num_frames),
      numchan,
      mat_class, mxREAL);
    if (tmp == NULL)
      return -1;
    memcpy ((void *)mxGetPr(tmp), (void *)ds->buf[i].buf,
      (numchan * ds->buf[i].rb->spf * ds->buf[i].numframes * ds->buf[i].elsize));
    mxSetField (board, 0, ds->buf[i].rb->regblock, tmp);
  }
  return 0;
}


void mexFunction (int nlhs, mxArray * plhs[], int nrhs, const mxArray * prhs[])
{
    char * fname;
    mxClassID inpt_class;
    struct arcfilt filt;
    struct dataset ds;
    mxArray * D;
    int r;

    /* PR ("readarc - a portable arc file reader\n"); */
    /* PR ("note: this is unstable and untested development code!\n"); */
    if (nrhs < 1)
        mexErrMsgTxt ("readarc takes at least one argument.");

    inpt_class = mxGetClassID (prhs[0]);
    if (inpt_class != mxCHAR_CLASS)
        mexErrMsgTxt ("readarc takes a string argument.");
    fname = mxArrayToString (prhs[0]);
    if (fname == 0)
        mexErrMsgTxt ("could not get file name.");

    if ((nrhs < 3) || (mxIsEmpty (prhs[1])) || (mxIsEmpty (prhs[2])))
      filt.use_utc = 0;
    else
    {
      char * utcstr;
      inpt_class = mxGetClassID (prhs[1]);
      if (inpt_class != mxCHAR_CLASS)
        mexErrMsgTxt ("second argument to readarc must be a string.");
      inpt_class = mxGetClassID (prhs[2]);
      if (inpt_class != mxCHAR_CLASS)
        mexErrMsgTxt ("third argument to readarc must be a string.");
      utcstr = mxArrayToString (prhs[1]);
      r = txt2utc (utcstr, filt.t1);
      if (r != 0)
        mexErrMsgTxt ("could not parse UTC time 1!");
      mxFree (utcstr);
      utcstr = mxArrayToString (prhs[2]);
      r = txt2utc (utcstr, filt.t2);
      if (r != 0)
        mexErrMsgTxt ("could not parse UTC time 2!");
      mxFree (utcstr);
      DEBUG ("Selecting on time range (%lu,%lu) - (%lu,%lu)\n",
        filt.t1[0], filt.t1[1], filt.t2[0], filt.t2[1]);
      filt.use_utc = 1;
    }
      
    if (nrhs < 4)
    {
      filt.nl.n = 0;
      filt.nl.s = 0;
    }
    else
    {
      int nn, in;
      char ** nlist;
      inpt_class = mxGetClassID (prhs[3]);
      if ((inpt_class != mxCELL_CLASS) && (inpt_class != mxCHAR_CLASS))
          mexErrMsgTxt ("second argument to readarc must be a string or cell array of strings.");
      if (inpt_class == mxCELL_CLASS)
      {
	nn = mxGetNumberOfElements (prhs[3]);
	nlist = malloc (nn * sizeof (char *));
	for (in=0; in<nn; in++)
	    nlist[in] = mxArrayToString (mxGetCell (prhs[3], in));
      }
      else
      {
        nn = 1;
        nlist = malloc (sizeof (char *));
        nlist[0] = mxArrayToString (prhs[3]);
      }
      create_namelist (nn, nlist, &(filt.nl));
      for (in=0; in<nn; in++)
          mxFree (nlist[in]);
      free (nlist);
    }
    filt.fname = fname;
    DEBUG ("Number of register name specifications = %d.\n", filt.nl.n);

    DEBUG ("Calling readarc.\n");

    r = readarc (&filt, &ds); 
    DEBUG ("Returned %d.\n", r);
    if (r == ARC_ERR_SIGINT)
    {
      free_namelist (&(filt.nl));
      mxFree (fname);
      mexErrMsgTxt ("Exiting at user request.\n");
      return;
    }
    else if (r != 0)
    {
      PR ("Reading arc file %s:\n", fname);
      PR ("Error opening or reading files.\n");
    }
    free_namelist (&(filt.nl));

    if (r == 0)
        r = mat_wrap_timestreams (&ds, &D);
    if (r == 0)
    {
        plhs[0] = D;
    }
    else
    {
        PR ("Reading arc file %s:\n", fname);
        mxFree (fname);
        mexErrMsgTxt ("Error packaging frames for output.\n");
        return;
    }

    free_dataset (&ds);
    mxFree (fname);
    return;
}

