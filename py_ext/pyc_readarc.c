/*
 * RWO 090331 - read a GCP arc file, or
 *              read specified time streams from
 *              a directory of arc files.
 * RWO 131218 - translation from Matlab mex
 *              to Python C extension
 */

#include <Python.h>  /* Greedy -- must go first */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "numpy/arrayobject.h"
#include "readarc.h"
#include "utcrange.h"

/* #define PR(args...) mexPrintf(args); mexEvalString("0;")
 */
#define PR(args...) printf(args); printf("\n");

#define DEBUG_MEX_READARC 0
#if DEBUG_MEX_READARC
#  define DEBUG(args...) PR(args)
#else
#  define DEBUG(args...)
#endif

/* Initialize a Python dictionary to hold the output:  */
/* Loop through all the register blocks we have read,  */
/* building a three-level hierarchy of dictionaries    */
/* with the form map.board.regblock.  Don't actually   */
/* fill in any values now, just create the dictionary. */
int init_dict (struct dataset * ds, PyObject ** D)
{
  int i, j;
  int nm, nb, nr;
  char * last_map;
  char * last_board;
  char ** map_names;
  char ** board_names;
  char ** regblock_names;
  PyObject ** boards;
  PyObject ** maps;

  if (ds->nb == 0)
  {
    DEBUG ("No registers in data set, returning empty structure.\n");
    *D = PyDict_New();
    return 0;
  }

  boards = malloc ((ds->nb) * sizeof (PyObject *));
  maps = malloc ((ds->nb) * sizeof (PyObject *));
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
      boards[nb-1] = PyDict_New();
      maps[nm-1] = PyDict_New();
      for (j=0; j<nb; j++)
        PyDict_SetItemString(maps[nm-1],board_names[j],boards[j]);
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
      boards[nb-1] = PyDict_New();
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
  boards[nb-1] = PyDict_New();
  maps[nm-1] = PyDict_New();
  for (j=0; j<nb; j++)
    PyDict_SetItemString (maps[nm-1], board_names[j], boards[j]);

  (*D) = PyDict_New();
  for (j=0; j<nm; j++)
    PyDict_SetItemString (*D, map_names[j], maps[j]);

  DEBUG ("Done allocating, now free the temp. arrays.\n");
  free (map_names);
  free (board_names);
  free (regblock_names);
  free (maps);
  free (boards);

  return 0;
}
   
int pyc_wrap_timestreams (struct dataset * ds, PyObject ** D)
{
  int i;
  PyObject * map;
  PyObject * board;
  PyObject * tmp;
  int typenum;
  int numchan;
  npy_intp dims[2];

  init_dict (ds, D);

  for (i=0; i<ds->nb; i++)
  {
    map = PyDict_GetItemString (*D, ds->buf[i].rb->map);
    if (map == NULL)
      return -1;
    board = PyDict_GetItemString (map, ds->buf[i].rb->board);
    if (board == NULL)
      return -1;
    
    switch (ds->buf[i].rb->typeword & GCP_REG_TYPE)
    {
      case GCP_REG_UINT:
        typenum = NPY_UINT32;
        break;
      case GCP_REG_INT:
        typenum = NPY_INT32;
        break;
      case GCP_REG_UCHAR:
        typenum = NPY_UINT8;
        break;
      case GCP_REG_CHAR:
        typenum = NPY_INT8;
        break;
      case GCP_REG_FLOAT:
        typenum = NPY_FLOAT32;
        break;
      case GCP_REG_DOUBLE:
        typenum = NPY_FLOAT64;
        break;

      /* Just treat UTC times as a UINT64, for now. */
      case GCP_REG_UTC:
	typenum = NPY_UINT64;
        break;

      default: 
        PR ("Skipping register of type 0x%lx.\n", (long int)(ds->buf[i].rb->typeword & GCP_REG_TYPE));
        continue;
    }
    numchan = ds->buf[i].rb->nchan;
    if (ds->buf[i].chan.n != 0)
      numchan = ds->buf[i].chan.ntot;
    if (!ds->buf[i].rb->do_arc)
    {
      DEBUG ("Initializing empty matrix %s.%s.%s\n",
        ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock);
      tmp = PyArray_New (&PyArray_Type, 0, NULL, typenum, NULL, NULL, 0, 0, NULL);
      PyDict_SetItemString (board, ds->buf[i].rb->regblock, tmp);
      continue;
    }
    DEBUG ("Copying %s.%s.%s, %dx%d, numframes=%d, length=%ld.\n",
      ds->buf[i].rb->map, ds->buf[i].rb->board, ds->buf[i].rb->regblock,
      ds->buf[i].rb->spf * ds->num_frames, numchan,
      ds->buf[i].numframes, (long int)ds->buf[i].bufsize);
    dims[1] = ds->buf[i].rb->spf * ds->num_frames;
    dims[0] = numchan;
    tmp = PyArray_New (&PyArray_Type, 2, dims, typenum, NULL, NULL, 0, 0, NULL);
    if (tmp == NULL) {
      PR ("Failed!");
      return -1;
    }
    memcpy ((void *)PyArray_DATA(tmp), (void *)ds->buf[i].buf,
      (numchan * ds->buf[i].rb->spf * ds->buf[i].numframes * ds->buf[i].elsize));
    PyDict_SetItemString (board, ds->buf[i].rb->regblock, tmp);
  }
  return 0;
}

static PyObject * pyc_readarc (PyObject * self, PyObject * args)
{
    char * fname = NULL;
    char * utcstr1 = NULL;
    char * utcstr2 = NULL;
    PyObject * regspec = NULL;
    struct arcfilt filt;
    struct dataset ds;
    PyObject * D;
    int r;

    PR ("readarc - a portable arc file reader\n");
    r = PyArg_ParseTuple (args, "|sssO", &fname, &utcstr1, &utcstr2, &regspec);
    if (!r || !fname) {
        PyErr_SetString (PyExc_RuntimeError, "readarc (file or directory, utc1, utc2, registers)");
        return NULL;
    }

    if (!utcstr1 || !utcstr1[0] || !utcstr2 || !utcstr2[0]) {
      filt.use_utc = 0; }
    else
    {
      r = txt2utc (utcstr1, filt.t1);
      if (r != 0) {
        PyErr_SetString (PyExc_RuntimeError, "could not parse UTC time 1!");
        return NULL;
      }
      r = txt2utc (utcstr2, filt.t2);
      if (r != 0) {
        PyErr_SetString (PyExc_RuntimeError, "could not parse UTC time 2!");
        return NULL;
      }
      DEBUG ("Selecting on time range (%lu,%lu) - (%lu,%lu)\n",
        filt.t1[0], filt.t1[1], filt.t2[0], filt.t2[1]);
      filt.use_utc = 1;
    }

    if (!regspec)
    {
      PR ("No register list specified.  Loading everything.");
      filt.nl.n = 0;
      filt.nl.s = 0;
    }
    else
    {
      int nn, in;
      char ** nlist;
      if (PyList_Check (regspec))
      {
	nn = PyList_Size (regspec);
	nlist = malloc (nn * sizeof (char *));
	for (in=0; in<nn; in++)
        {
            PyObject * tmpitem = PyList_GetItem (regspec, in);
            if (PyString_Check (tmpitem))
                nlist[in] = PyString_AsString (tmpitem);
            else
                PR ("Skipping register list entry number %d, since it is not a string.", in+1);
        }
      }
      else if (PyString_Check (regspec))
      {
        nn = 1;
        nlist = malloc (sizeof (char *));
        nlist[0] = PyString_AsString (regspec);
      }
      else
      {
          PyErr_SetString (PyExc_RuntimeError, "fourth argument to readarc must be a string or cell array of strings.");
          return NULL;
      }
      create_namelist (nn, nlist, &(filt.nl));
      /* 
       * for (in=0; in<nn; in++)
       *    mxFree (nlist[in]);
       */
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
      PyErr_SetString (PyExc_RuntimeError, "Exiting at user request.\n");
      return NULL;
    }
    else if (r != 0)
    {
      PR ("Reading arc file %s:\n", fname);
      PR ("Error opening or reading files.\n");
    }
    free_namelist (&(filt.nl));

    if (r == 0)
        r = pyc_wrap_timestreams (&ds, &D);
    if (r != 0)
    {
        PR ("Reading arc file %s:\n", fname);
        PyErr_SetString (PyExc_RuntimeError, "Error packaging frames for output.\n");
        return NULL;
    }

    free_dataset (&ds);
    return D;
}

static PyMethodDef arcfileMethods[] = {
    {"readarc", pyc_readarc, METH_VARARGS,
     "Read in an arc file."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC initarcfile(void)
{
    (void) Py_InitModule("arcfile", arcfileMethods);
    import_array();
}



