#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "fileset.h"
#include "utcrange.h"

#if DO_DEBUG_FILESET
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(...)
#endif
#if DO_DEBUG_FILESET > 1
#  define DEBUG2(args...) printf(args)
#else
#  define DEBUG2(...)
#endif


int free_fileset (struct fileset * fset)
{
  int i;
  for (i=0; i<fset->nf; i++)
  {
    if (fset->files[i].name != NULL)
      free (fset->files[i].name);
  }
  free (fset->files);
  return ARC_OK;
}

static int list_files_in_dir (char * dirname, uint32_t t1[2], uint32_t t2[2], struct fileset * fset);
static int bubble_sort_files (int numf, uint32_t * utclist, long int * flist);

int init_fileset (char * fname, struct fileset * fset)
{
  uint32_t t1[2];
  uint32_t t2[2];

  t1[0] = 0;
  t1[1] = 0;
  t2[0] = 0xFFFFFFFFUL;
  t2[1] = 0xFFFFFFFFUL;

  return init_fileset_utc (fname, t1, t2, fset);
}

int init_fileset_utc (char * fname, uint32_t t1[2], uint32_t t2[2], struct fileset * fset)
{
  struct stat st;
  int i, r;

  r = stat (fname, &st);
  if (r != 0)
    return ARC_ERR_NOFILE;

  if S_ISDIR(st.st_mode)
  {
    r = list_files_in_dir (fname, t1, t2, fset);
    if (r != 0)
      return r;
    for (i=0; i<fset->nf; i++)
    {
      r = stat (fset->files[i].name, &st);
      if (r != 0)
        return ARC_ERR_NOFILE;
      fset->files[i].size = st.st_size;
    }
    return ARC_OK;
  }
  else
  {
    fset->nf = 0;
    fset->files = malloc (sizeof (struct fileset_file));
    if (fset->files == NULL)
      return ARC_ERR_NOMEM;
    fset->files[0].name = malloc (strlen (fname) + 1);
    if (fset->files[0].name == NULL)
      return ARC_ERR_NOMEM;
    strcpy (fset->files[0].name, fname);
    fset->files[0].size = st.st_size;
    fset->nf = 1;

    return ARC_OK;
  }
}

/* Select all files in the directory that have file names */
/* indicating times between t1 and t2                     */
static int list_files_in_dir (char * dirname, uint32_t t1[2], uint32_t t2[2], struct fileset * fset)
{
  DIR * d;
  struct dirent * de;
  long int * flist = NULL;
  uint32_t * utclist;
  uint32_t utcfile[2];
  long int ffile;
  int maxf = 20;
  int numf = 0;
  long int fpre;
  uint32_t utcpre[2];
  int i, j;
  int r;

  d = opendir (dirname);
  if (d == NULL)
    return ARC_ERR_NOFILE;

  utcpre[0] = 0;
  utcpre[1] = 0;
  fpre = -1;

  flist = malloc (maxf * sizeof (long int *));
  utclist = malloc (2 * maxf * sizeof (uint32_t));
  if ((flist == NULL) || (utclist == NULL))
  {
    closedir (d);
    return ARC_ERR_NOMEM;
  }

  /* Spin through directory entries, keeping a list of the files     */
  /* whose file names are in the range of interest.  Also keep track */ 
  /* of the last file before the range of interest, since it may     */
  /* extend into it.                                                 */
  while (1)
  {
    ffile = telldir (d);
    de = readdir (d);
    if (de == NULL)
      break;
    DEBUG ("Considering file %s.\n", de->d_name);
    r = fname2utc (de->d_name, utcfile);
    if (r != 0)
      continue;
    if (NULL == strcasestr (de->d_name, ".dat"))
      continue;
    /* File starts after period of interest */
    if ((utcfile[0] > t2[0]) || ((utcfile[0] == t2[0]) && (utcfile[1] > t2[1])))
      continue;
    /* File starts in period of interest; list it */
    if ((utcfile[0] > t1[0]) || ((utcfile[0] == t1[0]) && (utcfile[1] >= t1[1])))
    {
      DEBUG ("...take it!\n");
      numf += 1;
      while (numf > maxf)
      {
        maxf *= 2;
        flist = realloc (flist, maxf * sizeof(long int *));
        utclist = realloc (utclist, 2 * maxf * sizeof (uint32_t));
        if ((flist == NULL) || (utclist == NULL))
        {
          closedir (d);
          free (flist);
          free (utclist);
          return -1;
        }
      }
      flist[numf-1] = ffile;
      utclist[2*(numf-1)] = utcfile[0];
      utclist[2*(numf-1)+1] = utcfile[1];

      DEBUG ("flist[numf-1] = flist[%d] = %ld\n", numf-1, ffile);

      continue;
    }
    /* File is last one yet seen before period of interest; make note of it just in case */
    if ((utcfile[0] > utcpre[0]) || ((utcfile[0] == utcpre[0]) && (utcfile[1] > utcpre[1])))
    {
      /* Check that it's not more than ~1 day before */
      if (t1[0] - utcfile[0] > 1)
        continue;
      DEBUG ("...save it just in case!\n");
      utcpre[0] = utcfile[0];
      utcpre[1] = utcfile[1];
      fpre = ffile;
    }
  }

  DEBUG ("Found %d files (plus pre?)\n", numf);
  /* Allocate list of file names */
  if (fpre == -1)
  {
    fset->nf = numf;
    fset->files = malloc (numf * sizeof (struct fileset_file));
  }
  else
  {
    fset->nf = numf + 1;
    fset->files = malloc ((numf+1) * sizeof (struct fileset_file));
  }
  if (fset->files == NULL)
  {
    closedir (d);
    free (flist);
    free (utclist);
    return ARC_ERR_NOMEM;
  }

  /* Copy in pre-range file name, if found */
  if (fpre != -1)
  {
    seekdir (d, fpre);
    de = readdir (d);
    DEBUG ("Copying in pre-file %s.\n", de->d_name);
    fset->files[0].name = malloc (strlen (dirname) + strlen (de->d_name) + 2);
    strcpy (fset->files[0].name, dirname);
    strcat (fset->files[0].name, "/");
    strcat (fset->files[0].name, de->d_name);
    DEBUG ("File name copied as %s.\n", fset->files[0].name);
    i = 1;
  }
  else i = 0;

  /* Bubble sort of file UTC times */
  bubble_sort_files (numf, utclist, flist);

  /* Fill in in chronological order, excluding dupes */
  if (i == 0)
  {
    utcfile[0] = 0;
    utcfile[1] = 0;
  }
  else
  {
    utcfile[0] = utcpre[0];
    utcfile[1] = utcpre[1];
  }
  for (j=0; j<numf; j++)
  {
    DEBUG ("Filling slot #%d, first file is %d, flist=%ld.\n", i, j, flist[j]);
    seekdir (d, flist[j]);
    de = readdir (d);
    if (de == NULL)
      DEBUG ("WTF?\n");
    if ((i==0) || (utclist[2*j] != utcfile[0]) || (utclist[2*j+1] != utcfile[1]))
    {
      DEBUG ("Copying in file %s.\n", de->d_name);
      fset->files[i].name = malloc (strlen (de->d_name) + strlen (dirname) + 2);
      strcpy (fset->files[i].name, dirname);
      strcat (fset->files[i].name, "/");
      strcat (fset->files[i].name, de->d_name);
      DEBUG ("File name copied as %s.\n", fset->files[i].name);
      i++;
    }
    else	/* Duplicate */
    {
      printf ("Found arc file %s, duplicate of %s.  Skipping.\n", de->d_name, fset->files[i-1].name);
    }
    utcfile[0] = utclist[2*j];
    utcfile[1] = utclist[2*j+1];
  }
  if (i < fset->nf)
  {
    fprintf (stderr, "Shrinking fset by %d to account for duplicate files.\n", fset->nf - i);
    fset->nf = i;
    fset->files = realloc (fset->files, fset->nf * sizeof (struct fileset_file));
  }

  r = closedir (d);
  free (utclist);
  free (flist);

  return ARC_OK;
}

static int bubble_sort_files (int numf, uint32_t * utclist, long int * flist)
{
  int changed = 1;
  int i;
  uint32_t tmputc[2];
  long int tmpf;

  if (numf<=0)
    return ARC_ERR_NOFILE;

  while (changed > 0)
  {
    changed = 0;
    for (i=0; i<(numf-1); i++)
    {
      if ((utclist[2*i] > utclist[2*i+2])
        || ((utclist[2*i] == utclist[2*i+2]) && (utclist[2*i+1] > utclist[2*i+3])))
      {
        tmputc[0] = utclist[2*i];
        tmputc[1] = utclist[2*i+1];
        utclist[2*i] = utclist[2*i+2];
        utclist[2*i+1] = utclist[2*i+3];
        utclist[2*i+2] = tmputc[0];
        utclist[2*i+3] = tmputc[1];
        tmpf = flist[i];
        flist[i] = flist[i+1];
        flist[i+1] = tmpf;

        changed = 1;
      }
    }
  }
  return ARC_OK;
}

