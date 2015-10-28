/*
 * RWO 100106 - read a GCP arc file, and
 *              return the register map
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mex.h"
#include "arcfile.h"
#include "utcrange.h"

#ifndef HAVE_OCTAVE
#  include "matrix.h"
#endif

#define PR(args...) mexPrintf(args); mexEvalString("0;")

#define DEBUG_MEX_LISTARC 0
#if DEBUG_MEX_LISTARC
#  define DEBUG(args...) PR(args)
#else
#  define DEBUG(args...)
#endif

/* Initialize a Matlab structure to hold the output:  */
/* Loop through all the register blocks we have read, */
/* building a three-level hierarchy of structs, with  */
/* the form map.board.regblock.  Don't actually fill  */
/* in any values now, just create the structs here.   */
int init_struct (struct reglist *rl, mxArray ** D)
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

  if (rl->num_regblocks == 0)
  {
    DEBUG ("No registers in data set, returning empty structure.\n");
    *D = mxCreateStructMatrix (0, 0, 0, NULL);
    return 0;
  }

  boards = malloc ((rl->num_regblocks) * sizeof (mxArray *));
  maps = malloc ((rl->num_regblocks) * sizeof (mxArray *));
  map_names = malloc ((rl->num_regblocks) * sizeof (char *));
  board_names = malloc ((rl->num_regblocks) * sizeof (char *));
  regblock_names = malloc ((rl->num_regblocks) * sizeof (char *));
  last_map = rl->r[0].rb.map;
  last_board = rl->r[0].rb.board;
  map_names[0] = last_map;
  nm = 1;
  board_names[0] = last_board;
  nb = 1;
  regblock_names[0] = rl->r[0].rb.regblock;
  nr = 1;
  DEBUG ("About to try initializing structure.\n");
  DEBUG ("On %s.%s.%s\n", map_names[0], board_names[0], regblock_names[0]);
  for (i=1; i<rl->num_regblocks; i++)
  {
    DEBUG ("On %s.%s.%s\n", rl->r[i].rb.map, rl->r[i].rb.board, rl->r[i].rb.regblock);
    if (0 != strcmp (rl->r[i].rb.map, last_map))
    {
      boards[nb-1] = mxCreateStructMatrix (1, 1, nr, (const char **)regblock_names);
      maps[nm-1] = mxCreateStructMatrix (1, 1, nb, (const char **)board_names);
      for (j=0; j<nb; j++)
        mxSetFieldByNumber (maps[nm-1], 0, j, boards[j]);
      regblock_names[0] = rl->r[i].rb.regblock;
      nr = 1;
      last_board = rl->r[i].rb.board;
      board_names[0] = last_board;
      nb = 1;
      last_map = rl->r[i].rb.map;
      map_names[nm] = last_map;
      nm++;
      continue;
    }
    if (0 != strcmp (rl->r[i].rb.board, last_board))
    {
      boards[nb-1] = mxCreateStructMatrix (1, 1, nr, (const char **)regblock_names);
      regblock_names[0] = rl->r[i].rb.regblock;
      nr = 1;
      last_board = rl->r[i].rb.board;
      board_names[nb] = last_board;
      nb++;
      continue;
    }
    regblock_names[nr] = rl->r[i].rb.regblock;
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

const char * fieldnames[] = {"class", "spf", "nchan", "is_fast", "do_arc"};
   
int mat_fill_info (struct reglist * rl, mxArray ** D)
{
  int i;
  mxArray * map;
  mxArray * board;
  mxArray * regblock;
  mxArray * tmp;
  mxClassID mat_class;
  int numchan;

  init_struct (rl, D);

  for (i=0; i<rl->num_regblocks; i++)
  {
    map = mxGetField (*D, 0, rl->r[i].rb.map);
    if (map == NULL)
      return -1;
    board = mxGetField (map, 0, rl->r[i].rb.board);
    if (board == NULL)
      return -1;

    regblock = mxCreateStructMatrix(1, 1, sizeof(fieldnames)/sizeof(char *), fieldnames);

    switch (rl->r[i].rb.typeword & GCP_REG_TYPE)
    {
      case GCP_REG_UINT:
        tmp = mxCreateString("uint32");
        break;
      case GCP_REG_INT:
	tmp = mxCreateString("int32");
        break;
      case GCP_REG_USHORT:
        tmp = mxCreateString("uint16");
        break;
      case GCP_REG_SHORT:
        tmp = mxCreateString("int16");
        break;
      case GCP_REG_UCHAR:
	tmp = mxCreateString("uint8");
        break;
      case GCP_REG_CHAR:
	tmp = mxCreateString("int8");
        break;
      case GCP_REG_FLOAT:
	tmp = mxCreateString("single");
        break;
      case GCP_REG_DOUBLE:
	tmp = mxCreateString("double");
        break;

      /* Just treat UTC times as a UINT64, for now. */
      case GCP_REG_UTC:
	tmp = mxCreateString("utc");
        break;

      default: 
        PR ("Skipping register of type 0x%lx.\n", rl->r[i].rb.typeword & GCP_REG_TYPE);
	tmp = mxCreateString("unknown");
    }
    mxSetField (regblock, 0, "class", tmp);
    tmp = mxCreateDoubleScalar (rl->r[i].rb.spf);
    mxSetField (regblock, 0, "spf", tmp);
    tmp = mxCreateDoubleScalar (rl->r[i].rb.nchan);
    mxSetField (regblock, 0, "nchan", tmp);
    tmp = mxCreateLogicalScalar (rl->r[i].rb.spf > 1);
    mxSetField (regblock, 0, "is_fast", tmp);
    tmp = mxCreateLogicalScalar (rl->r[i].rb.do_arc);
    mxSetField (regblock, 0, "do_arc", tmp);

    mxSetField (board, 0, rl->r[i].rb.regblock, regblock);
  }
  return 0;
}


void mexFunction (int nlhs, mxArray * plhs[], int nrhs, const mxArray * prhs[])
{
    char * fname;
    mxClassID inpt_class;
    struct arcfile af;
    struct reglist rl;
    mxArray * D;
    int r;

    if (nrhs != 1)
        mexErrMsgTxt ("list_arc takes one argument.");

    inpt_class = mxGetClassID (prhs[0]);
    if (inpt_class != mxCHAR_CLASS)
        mexErrMsgTxt ("list_arc takes a string argument.");
    fname = mxArrayToString (prhs[0]);
    if (fname == 0)
        mexErrMsgTxt ("could not get file name.");

    r = arcfile_open(fname, &af);
    if (r != 0)
    {
       PR ("Could not open arc file %s:\n", fname);
       mxFree (fname);
       mexErrMsgTxt ("list_arc failed.");
    }

    r = arcfile_read_regmap (&af, &rl);
    arcfile_close (&af);
    if (r != 0)
    {
      PR ("Error reading register map from arc file %s.\n", fname);
      mxFree (fname);
      mexErrMsgTxt ("list_arc failed.");
    }

    r = mat_fill_info (&rl, &D);
    if (r == 0)
    {
        plhs[0] = D;
    }
    else
    {
        PR ("Reading arc file %s:\n", fname);
        mxFree (fname);
        mexErrMsgTxt ("Error packaging register list for output.\n");
    }

    free_reglist (&rl);
    mxFree (fname);
    return;
}

