#include <stdio.h> 
#include <stdlib.h>
#include <stdint.h>

#include "fileset.h"
#include "namelist.h"
#include "reglist.h"
#include "dataset.h"
#include "arcfile.h"
#include "readarc.h"
#include "handlesig.h"

/* When Matlab is running in the desktop, Mathworks chooses to
 * thoroughly break printf.  We have to work around this
 * silliness.
 */
#ifdef MATLAB_MEX_FILE
#  include "mex.h"
#  define PR(args...) mexPrintf(args); mexEvalString("0;")
#else
#  define PR(args...) printf(args)
#endif

#define DO_DEBUG_READARC 1
#ifndef DO_LIST_FILES
#  define DO_LIST_FILES 1
#endif

#if DO_DEBUG_READARC
#  define DEBUG(args...) PR(args)
#else
#  define DEBUG(...)
#endif
#if DO_DEBUG_READARC > 1
#  define DEBUG2(args...) PR(args)
#else
#  define DEBUG2(...)
#endif

#if DO_LIST_FILES == 1
#  define LISTFILES(args...) PR(args)
#elif DO_LIST_FILES == 2
#  define LISTFILES(args...) fprintf(stderr,args);
#else
#  define LISTFILES(...)
#endif

static int readarc_onefile (struct arcfilt * filt, struct fileset * fset, int fnum, struct dataset * ds);
static int readarc_multifile (struct arcfilt * filt, struct fileset * fset, struct dataset * ds);
static int readarc_multifile_utc (struct arcfilt * filt, struct fileset * fset, struct dataset * ds);
static int read_frames_utc_helper (char * fname, struct reglist * rl, uint32_t t1[2], uint32_t t2[2], struct dataset * ds);
static int read_frames_helper (char * fname, struct reglist * rl, struct dataset * ds);

int readarc (struct arcfilt * filt, struct dataset * ds)
{
  int r;
  struct fileset fset;

  DEBUG ("Entering readarc.\n");
  ds->buf = NULL;
  if (filt->use_utc)
  {
    /* Get list of files to read, based on path & UTC time */
    r = init_fileset_utc (filt->fname, filt->t1, filt->t2, &fset);
  }
  else
  {
    DEBUG ("Getting list of all files matching %s.\n", filt->fname);
    r = init_fileset (filt->fname, &fset);
    if (r != 0)
      DEBUG ("init_fileset returned %d.\n", r);
  }
  if (r != 0)
    return r;

  DEBUG ("Found %d files.\n", fset.nf);

  /* If no files, return an empty buffer */
  if (fset.nf == 0)
  {
    PR ("Selected zero files.\n");
    free_fileset (&fset);
    init_dataset (ds, NULL, 0);
    PR ("returning empty buffer.\n");
    return 0;
  }

  /* Set up signal handling so user can terminate readarc if he gets bored. */
  set_up_sigint ();

  /* Handle one-file, multi-file (no UTC), and multi-file (with UTC range) cases separately. */
  DEBUG ("fset.nf = %d.\n", fset.nf);
  if (fset.nf == 1)
    r = readarc_onefile (filt, &fset, 0, ds);
  else if (filt->use_utc == 0)
    r = readarc_multifile (filt, &fset, ds);
  else
    r = readarc_multifile_utc (filt, &fset, ds);
  DEBUG ("Specific reader returned %d.\n", r);

  /* Unregister our SIGINT handler */
  clean_up_sigint ();

  DEBUG ("About to free fileset.\n");
  free_fileset (&fset);

  DEBUG ("About to shrink buffers.\n");
  /* Shrink data buffers if any space is unused. */
  if (ds->buf != NULL)
  {
    if (r == 0)
      r = dataset_tight_size (ds);
    if (r != 0)
      free_dataset (ds);
  }

  DEBUG ("Returning %d.\n", r);
  return r;
}

static int readarc_onefile (struct arcfilt * filt, struct fileset * fset, int fnum, struct dataset * ds)
{
  int r;
  struct reglist rl;
  struct arcfile af;
  int nframes;

  DEBUG ("readarc_onefile.\n");

  r = arcfile_open (fset->files[fnum].name, &af);
  if (r != 0)
    return r;
  DEBUG ("Opened arcfile.\n");

  DEBUG ("Reading namelist.\n");
  if (filt->nl.n == 0)
    r = arcfile_read_regmap (&af, &rl);
  else
    r = arcfile_read_regmap_namelist (&af, &(filt->nl), &rl);
  if (r != 0)
  {
    arcfile_close (&af);
    return r;
  }
  DEBUG ("Finished reading namelist.\n");

#ifndef STANDARD_FILE_NFRAMES
  DEBUG ("No value for standard file number of frames.\n");
  nframes = (fset->files[fnum].size - af.frame0_ofs) / af.frame_len;
  DEBUG ("File %s: size=%d, frame0_ofs=%d, frame_len=%d, nframes=%d..\n", fset->files[fnum].name, fset->files[fnum].size, af.frame0_ofs, af.frame_len, nframes);

  if (nframes * af.frame_len + af.frame0_ofs != fset->files[fnum].size)
  {
    PR ("Ignoring %d extra bytes at end of file %s.\n",
      (fset->files[fnum].size - af.frame0_ofs - nframes * af.frame_len),
      fset->files[fnum].name);
  }
#else
  nframes = STANDARD_FILE_NFRAMES;
#endif

  DEBUG ("Initializing dataset buffer.\n");
  r = init_dataset (ds, &rl, nframes);
  if (r != 0)
  {
    arcfile_close (&af);
    free_reglist (&rl);
    return r;
  }

  if (filt->use_utc == 0)
    r = arcfile_read_frames (&af, &rl, ds);
  else
    r = arcfile_read_frames_utc (&af, &rl, filt->t1, filt->t2, ds);

  /* See if the user did a Ctrl-break.  We can't actually interrupt in the middle */
  /* of reading a single arc file, but we can pass on the to Matlab .             */
  if (check_sigint(0))
  {
    printf("Caught sigint in readarc_onefile.\n");
    r = ARC_ERR_SIGINT;
  }

  free_reglist (&rl);
  arcfile_close (&af);

  return r;
}

static int readarc_multifile (struct arcfilt * filt, struct fileset * fset, struct dataset * ds)
{
  int r;
  struct reglist rl;
  struct arcfile af;
  int nframes;
  int i;

  /* Get register list from file #0, the first one in the list */
  r = arcfile_open (fset->files[0].name, &af);
  if (r != 0)
    return r;
  if (filt->nl.n == 0)
    r = arcfile_read_regmap (&af, &rl);
  else
    r = arcfile_read_regmap_namelist (&af, &(filt->nl), &rl);
  if (r != 0)
  {
    arcfile_close (&af);
    return r;
  }

  /* Estimate total # frames in all files */
  nframes = 0;
  for (i=0; i<fset->nf; i++)
  {
#ifndef STANDARD_FILE_NFRAMES
    int tmp = (fset->files[i].size - af.frame0_ofs) / af.frame_len;
    DEBUG ("File %s: size=%d, frame0_ofs=%d, frame_len=%d, nframes=%d..\n", fset->files[i].name, fset->files[i].size, af.frame0_ofs, af.frame_len, tmp);
    if (tmp * af.frame_len + af.frame0_ofs != fset->files[i].size)
      PR ("Ignoring %d extra bytes at end of file %s.\n",
        (fset->files[i].size - af.frame0_ofs - tmp * af.frame_len),
        fset->files[i].name);
    nframes += tmp;
#else
    nframes += STANDARD_FILE_NFRAMES;
#endif
  }

  /* Initialize buffers as big as expected data set */
  r = init_dataset (ds, &rl, nframes);
  if (r != 0)
  {
    arcfile_close (&af);
    free_reglist (&rl);
    return r;
  }

  /* Read frames into buffer from first (and currently open) file */
  LISTFILES ("File 1 of %d: %s.\n", fset->nf, fset->files[0].name);
  DEBUG ("Reading frames from file %s.\n", fset->files[0].name);
  r = arcfile_read_frames (&af, &rl, ds);
  arcfile_close (&af);
  if (r != 0)
  {
    free_reglist (&rl);
    return r;
  }

  /* Now read frames from subsequent files into the buffer */
  for (i=1; i<fset->nf; i++)
  {
    if (check_sigint(0))
    {
      r = ARC_ERR_SIGINT;
      free_reglist (&rl);
      return r;
    }
    LISTFILES ("File %d of %d: %s.\n", i, fset->nf, fset->files[i].name);
    DEBUG ("Reading frames from file %s.\n", fset->files[i].name);
    r = read_frames_helper (fset->files[i].name, &rl, ds);
    if (r != 0)
      break;
  }

  DEBUG ("About to return, r=%d.\n", r);
  free_reglist (&rl);

  return r;
}

static int readarc_multifile_utc (struct arcfilt * filt, struct fileset * fset, struct dataset * ds)
{
  int r;
  struct reglist rl;
  struct arcfile af;
  int nframes;
  int i;
  struct dataset ds0, dsN;
  int frame0_ofs, frame_len;

  /* Get register list from file #1, the first one fully within the UTC range */
  r = arcfile_open (fset->files[1].name, &af);
  if (r != 0)
    return r;
  if (filt->nl.n == 0)
    r = arcfile_read_regmap (&af, &rl);
  else
    r = arcfile_read_regmap_namelist (&af, &(filt->nl), &rl);
  frame0_ofs = af.frame0_ofs;
  frame_len = af.frame_len;
  arcfile_close (&af);
  if (r != 0)
    return r;

  /* Read first & last files into separate buffers */
  DEBUG ("About to read frames from file #1, %s.\n", fset->files[0].name);
#ifndef STANDARD_FILE_NFRAMES
  nframes = (fset->files[0].size - frame0_ofs) / frame_len;
#else
  nframes = STANDARD_FILE_NFRAMES;
#endif
  DEBUG ("File %s: size=%d, frame0_ofs=%d, frame_len=%d, nframes=%d.\n", fset->files[0].name, fset->files[0].size, frame0_ofs, frame_len, nframes);
  LISTFILES ("File 1 of %d: %s.\n", fset->nf, fset->files[0].name);
  init_dataset (&ds0, &rl, nframes);
  r = read_frames_utc_helper (fset->files[0].name, &rl, filt->t1, filt->t2, &ds0);

  DEBUG ("About to read frames from file #N, %s.\n", fset->files[fset->nf-1].name);
#ifndef STANDARD_FILE_NFRAMES
  nframes = (fset->files[fset->nf-1].size - frame0_ofs) / frame_len;
#else
  nframes = STANDARD_FILE_NFRAMES;
#endif
  DEBUG ("File %s: size=%d, frame0_ofs=%d, frame_len=%d, nframes=%d.\n", fset->files[fset->nf-1].name, fset->files[fset->nf-1].size, frame0_ofs, frame_len, nframes);
  LISTFILES ("File 2 of %d: %s.\n", fset->nf, fset->files[fset->nf-1].name);
  init_dataset (&dsN, &rl, nframes);
  r = read_frames_utc_helper (fset->files[fset->nf-1].name, &rl, filt->t1, filt->t2, &dsN);

  /* Estimate total # frames in all other files */
  nframes = 0;
  for (i=1; i<((fset->nf)-1); i++)
  {
#ifndef STANDARD_FILE_NFRAMES
    int tmp = (fset->files[i].size - frame0_ofs) / frame_len;
    DEBUG ("File %s: size=%d, frame0_ofs=%d, frame_len=%d, nframes=%d.\n", fset->files[i].name, fset->files[i].size, frame0_ofs, frame_len, tmp);

    /* On inconsistency, should preferably switch to a different read method */
    if (tmp * frame_len + frame0_ofs != fset->files[i].size)
      PR ("Ignoring %d extra bytes at end of file %s.\n",
        (fset->files[i].size - frame0_ofs - tmp * frame_len),
        fset->files[i].name);
    nframes += tmp;
#else
    nframes += STANDARD_FILE_NFRAMES;
#endif
  }

  /* Initialize buffers as big as expected total data set */
  DEBUG ("Initialize big buffer with %d frames.\n", nframes + ds0.num_frames + dsN.num_frames);
  r = init_dataset (ds, &rl, nframes + ds0.num_frames + dsN.num_frames);
  if (r != 0)
  {
    free_dataset (&ds0);
    free_dataset (&dsN);
    free_reglist (&rl);
    return r;
  }

  DEBUG ("Copy data from file 0 into big buffer.\n");
  r = copy_dataset (&ds0, ds);
  free_dataset (&ds0);

  /* Now read frames into the buffer */
  for (i=1; i<((fset->nf)-1); i++)
  {
    if (check_sigint(0))
    {
      r = ARC_ERR_SIGINT;
      free_dataset (&dsN);
      free_reglist (&rl);
      return r;
    }
    DEBUG ("Read data from file %d into big buffer.\n", i);
    LISTFILES ("File %d of %d: %s.\n", i+2, fset->nf, fset->files[i].name);
    r = read_frames_helper (fset->files[i].name, &rl, ds);
    if (r != 0)
      break;
  }

  DEBUG ("Copy data from file N into big buffer.\n");
  r = copy_dataset (&dsN, ds);
  DEBUG ("Free dataset N.\n");
  free_dataset (&dsN);

  free_reglist (&rl);
  DEBUG ("Return %d.\n", r);

  return r;
}

/* Should make read_frames_helper return an error code if register maps don't match, */
/* or if number of frames is not as expected.                                        */
static int read_frames_helper (char * fname, struct reglist * rl, struct dataset * ds)
{
  struct arcfile af;
  int r;

  r = arcfile_open (fname, &af);
  if (r != 0)
    return r;

  r = arcfile_skip_regmap (&af);
  if (r != 0)
  {
    arcfile_close (&af);
    return r;
  }

  r = arcfile_read_frames (&af, rl, ds);
  arcfile_close (&af);

  return r;
}

static int read_frames_utc_helper (char * fname, struct reglist * rl, uint32_t t1[2], uint32_t t2[2], struct dataset * ds)
{
  struct arcfile af;
  int r;

  r = arcfile_open (fname, &af);
  if (r != 0)
    return r;

  r = arcfile_skip_regmap (&af);
  if (r != 0)
  {
    arcfile_close (&af);
    return r;
  }

  r = arcfile_read_frames_utc (&af, rl, t1, t2, ds);
  arcfile_close (&af);

  return r;
}
