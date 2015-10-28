#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "arcfile.h"
#include "dataset.h"
#include "namelist.h"
#include "reglist.h"
#include "endian.h"
#include "readarc.h"

#if DO_DEBUG_ARCFILE
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(...)
#endif
#if DO_DEBUG_ARCFILE > 1
#  define DEBUG2(args...) printf(args)
#else
#  define DEBUG2(...)
#endif

/* Helper function used to get file size */
static uint32_t get_arcfile_size (char * fname)
{
  struct stat fs;
  int r;

  r = stat (fname, &fs);
  if (r != 0)
    return 0;

  return fs.st_size;
}

int arcfile_open (char * fname, struct arcfile * af)
{
  int r;

  /* Check file size */
  af->fsize = get_arcfile_size (fname);

  DEBUG ("Guessing format of file %s.\n", fname);
  r = strlen (fname);
  if (!strncmp (fname + (r-3), ".gz", 3))
    af->file_type = ARC_FILE_GZ;
  else if (!strncmp (fname + (r-4), ".bz2", 4))
    af->file_type = ARC_FILE_BZ2;
  else if (!strncmp (fname + (r-4), ".dat", 4))                                     
    af->file_type = ARC_FILE_PLAIN;                                                 
  else                                                                              
  {                                                                                 
    fprintf (stderr, "Unknown format for file %s.\n", fname);                       
    return ARC_ERR_FORMAT;                                                          
  }     

  DEBUG ("Arc file format is 0x%x\n", af->file_type);

  if (af->file_type == ARC_FILE_GZ)
  {
#if HAVE_GZ == 1
    af->g = gzopen (fname, "rb");
    if (af->g == NULL)
      return -1;

    /* Read the file header */
    r = gzread (af->g, af->header, sizeof (uint32_t) * 6);
    if (r < 6 * sizeof (uint32_t))
    {
      gzclose (af->g);
      af->g = NULL;
      return -1;
    }
#else
    return ARC_ERR_FORMAT;
#endif
  }
  else
  if (af->file_type == ARC_FILE_BZ2)
  {
#if HAVE_BZ2 == 1
    af->b = BZ2_bzopen (fname, "rb");
    if (af->b == NULL)
      return -1;

    /* Read the file header */
    r = BZ2_bzread (af->b, af->header, sizeof (uint32_t) * 6);
    if (r < 6 * sizeof (uint32_t))
    {
      BZ2_bzclose (af->b);
      af->b = NULL;
      return -1;
    }
#else
    return ARC_ERR_FORMAT;
#endif
  }
  else
  {
    /* Open file on disk */
    af->f = fopen (fname, "rb");
    if (af->f <= 0)
      return -1;
  
    /* Read the file header */
    r = fread (af->header, sizeof (uint32_t), 6, af->f);
    if (r < 6)
    {
      fclose (af->f);
      af->f = NULL;
      return -1;
    }
  }

  /* Check endianness */
  af->do_swap_header = (af->header[0] > 0x00010000U);
  if (af->do_swap_header)
    for (r=0; r<6; r++)
      swap_4 (af->header + r);
  if (check_endianness() == 0)
    af->do_swap_data = 0;
  else
  {
    printf ("Warning: endianness swap on frame data not yet supported.\n");
    af->do_swap_data = 1;
  }

  af->version = af->header[0];
  af->frame_len = af->header[2] - 8;
  af->frame0_ofs = af->header[3] + 12;
  af->numframes = (af->fsize - af->frame0_ofs) / af->frame_len;
/* Given the existence of compressed files, */
/* a check based on file size is not reliable. */
/* If there is less than one frame, the problem */
/* will be caught and handled later, during reading, */
/* so this check was redundant anyway. */
#if 0
  if (af->numframes < 1)
  {
    arcfile_close (af);
    printf("Number of frames in file < 1.\n");
    return -1;
  }
#endif

  DEBUG ("Opened arc file %s: do_swap_header=%d, version=%d.\n", fname, af->do_swap_header, af->version);

  return 0;
}

int arcfile_close (struct arcfile * af)
{
  switch (af->file_type)
  {
    case ARC_FILE_PLAIN :
      fclose (af->f);
      break;

#if HAVE_GZ == 1
    case ARC_FILE_GZ :
      gzclose (af->g);
      break;
#endif

#if HAVE_BZ2 == 1
    case ARC_FILE_BZ2 :
      BZ2_bzclose (af->b);
      break;
#endif

    default :
      return ARC_ERR_FORMAT;
  }

  return ARC_OK;
}

/* Unused, and dubious */
/* 
 * int get_buf_size (struct regblockspec * rb, int nframes)
 * {
 *   uint32_t n, m;
 * 
 *   if (!rb->do_arc)
 *     return 0;
 *
 *   n = rb->spf;
 *   if (n == 0) n = 1;
 *   n *= rb->nchan;
 *   if (n == 0) n = 1;
 *   m = element_size (rb->typeword);
 *
 *   return m * n * nframes;
 * }
 */

int arcfile_read_frames_utc (struct arcfile * af, struct reglist * rl, uint32_t t1[2], uint32_t t2[2], struct dataset * ds)
{
  return arcfile_read_frames (af, rl, ds);
}

static int af_eof (struct arcfile * af)
{
  switch (af->file_type)
  {
    case ARC_FILE_PLAIN : return feof (af->f);
#if HAVE_GZ == 1
    case ARC_FILE_GZ :
      return gzeof (af->g);
#endif
#if HAVE_BZ2 == 1
    case ARC_FILE_BZ2 :
      return (0);	/* bzlib doesn't have eof() .... */
#endif
  }
  return 0;
}

int arcfile_read_frames_3 (struct arcfile * af, struct reglist * rl, struct dataset * ds)
{
#define NBUFFRAMES 64
  int i, j, k, r, nread;
  char * buf;
  char * tmp;
  uint32_t h[2];
  int32_t ofs;

  buf = malloc (af->frame_len * NBUFFRAMES);
  if (buf == NULL)
    return ARC_ERR_NOMEM;

  j = 0;
  while (!af_eof(af))
  {
    DEBUG ("Reading from frame %d.\n", j);
    switch (af->file_type)
    {
      case ARC_FILE_PLAIN :
        nread = fread (buf, af->frame_len, NBUFFRAMES, af->f);
        break;

#if HAVE_GZ == 1
      case ARC_FILE_GZ :
        nread = gzread (af->g, buf, NBUFFRAMES * af->frame_len);
        nread = nread / af->frame_len;
        break;
#endif

#if HAVE_BZ2 == 1
      case ARC_FILE_BZ2 :
        nread = BZ2_bzread (af->b, buf, NBUFFRAMES * af->frame_len);
        nread = nread / af->frame_len;
        break;
#endif

      default: return ARC_ERR_FORMAT;
    }

    if (nread == 0)
      break;
    tmp = buf;
    for (k=0; k<nread; k++)
    {
      h[0] = *(uint32_t *)(tmp + 0);
      h[1] = *(uint32_t *)(tmp + sizeof(uint32_t));
      swap_4 (h);
      swap_4 (h+1);
      DEBUG ("Frame length is 0x%lx, Extra word is 0x%lx.\n", h[0], h[1]);
      if (h[0] != af->frame_len)
      {
        fprintf (stderr, "Corrupted file: frame %d has length %lu, should be %lu.\n", j, h[0], af->frame_len);
        free (buf);
        return -1;
      }

      if (ds->num_frames == ds->max_frames)
	dataset_resize (ds, ds->max_frames * 2);

      for (i=0; i<rl->num_regblocks; i++)
      {
	DEBUG2 ("Reading in frame %d, register block %d (%s.%s.%s).\n", j, i,
	  rl->r[i].rb.map, rl->r[i].rb.board, rl->r[i].rb.regblock);

	ofs = rl->r[i].ofs_in_frame;
	r = memcopy_to_buf (tmp+ofs, &(ds->buf[i]));
	if (r != 0)
	{
	  free (buf);
	  return -1;
	}
      }

      ds->num_frames += 1;
      j += 1;
      tmp += af->frame_len;
    }
  }
  free (buf);
  return 0;
}

int arcfile_read_frames_2 (struct arcfile * af, struct reglist * rl, struct dataset * ds)
{
  int i, j, r;
  char * buf;
  uint32_t h[2];
  int32_t ofs;

  buf = malloc (af->frame_len);
  if (buf == NULL)
    return ARC_ERR_NOMEM;

  j = 0;
  while (!feof (af->f))
  {
    DEBUG ("Reading in frame %d.\n", j);
    r = fread (buf, 1, af->frame_len, af->f);
    if (r != af->frame_len)
      break;
    h[0] = *(uint32_t *)(buf + 0);
    h[1] = *(uint32_t *)(buf + sizeof(uint32_t));
    swap_4 (h);
    swap_4 (h+1);
    DEBUG ("Frame length is 0x%lx, Extra word is 0x%lx.\n", h[0], h[1]);

    if (ds->num_frames == ds->max_frames)
      dataset_resize (ds, ds->max_frames * 2);

    for (i=0; i<rl->num_regblocks; i++)
    {
      DEBUG2 ("Reading in frame %d, register block %d (%s.%s.%s).\n", j, i,
        rl->r[i].rb.map, rl->r[i].rb.board, rl->r[i].rb.regblock);

      ofs = rl->r[i].ofs_in_frame;
      r = memcopy_to_buf (buf+ofs, &(ds->buf[i]));
      if (r != 0)
      {
        free (buf);
        return -1;
      }
    }

    ds->num_frames += 1;
    j += 1;
  }
  free (buf);
  return 0;
}

int arcfile_read_frames_1 (struct arcfile * af, struct reglist * rl, struct dataset * ds)
{
  int i, j, r;
  int32_t ofs;
  uint32_t h[2];
  uint32_t ofs0;

  j = 0;
  while (!feof (af->f))
  {
    DEBUG ("Reading in frame %d.\n", j);
    ofs=0;
    ofs0 = ftell (af->f);

    r = fread (h, sizeof(uint32_t), 1, af->f);
    if (r != 1) break;
    swap_4 (h);
    r = fread (h+1, sizeof(uint32_t), 1, af->f);
    if (r != 1) break;
    swap_4 (h+1);
    DEBUG ("Frame length is 0x%lx, Extra word is 0x%lx.\n", h[0], h[1]);
    ofs += 8;

    if (ds->num_frames == ds->max_frames)
      dataset_resize (ds, ds->max_frames * 2);
      
    for (i=0; i<rl->num_regblocks; i++)
    {
      DEBUG2 ("Reading in frame %d, register block %d (%s.%s.%s).\n", j, i,
        rl->r[i].rb.map, rl->r[i].rb.board, rl->r[i].rb.regblock);
      fseek (af->f, rl->r[i].ofs_in_frame - ofs, SEEK_CUR);
      ofs = rl->r[i].ofs_in_frame;
      r = copy_to_buf (af->f, &(ds->buf[i]), &ofs);
      if (r != 0)
        return -1;
    }

    ds->num_frames += 1;
    j += 1;

    DEBUG ("I think I've advanced 0x%lx bytes.\n", ofs);
    DEBUG ("I have really advanced 0x%lx bytes.\n", ftell(af->f)-ofs0);

    r = fseek (af->f, h[0] - ofs, SEEK_CUR);
    if (r != 0)
      break;
  }
  return 0;
}

int arcfile_read_regmap (struct arcfile * af, struct reglist * rl)
{
  void * buf;
  int buflen;
  int r;

  DEBUG ("Reading register map without namelist.\n");
  buflen = af->frame0_ofs - 24;
  buf = malloc (buflen);
  if (buf == NULL)
    return ARC_ERR_NOMEM;

  switch (af->file_type)
  {

    case ARC_FILE_PLAIN :
      r = fread (buf, buflen, 1, af->f);
      break;

#if HAVE_GZ == 1
    case ARC_FILE_GZ :
      r = gzread (af->g, buf, buflen);
      break;
#endif

#if HAVE_BZ2 == 1
    case ARC_FILE_BZ2 :
      r = BZ2_bzread (af->b, buf, buflen);
      break;
#endif
    default:
      return ARC_ERR_FORMAT;
  }
  if (r < 1)
    return ARC_ERR_EOF;
  r = parse_reglist (buf, buflen, af->do_swap_header, rl, 0);
  free (buf);

  return r;
}

int arcfile_read_regmap_namelist (struct arcfile * af, struct namelist * nl, struct reglist * rl)
{
  void * buf;
  int buflen;
  int r;

  DEBUG ("Reading register map with namelist.\n");
  buflen = af->frame0_ofs - 24;
  buf = malloc (buflen);
  if (buf == NULL)
    return ARC_ERR_NOMEM;

  switch (af->file_type)
  {
    case ARC_FILE_PLAIN :
      r = fread (buf, buflen, 1, af->f);
      break;

#if HAVE_GZ == 1
    case ARC_FILE_GZ :
      r = gzread (af->g, buf, buflen);
      break;
#endif

#if HAVE_BZ2 == 1
    case ARC_FILE_BZ2 :
      r = BZ2_bzread (af->b, buf, buflen);
      break;
#endif
    default:
      return ARC_ERR_FORMAT;
  }

  if (r < 1)
    return ARC_ERR_EOF;

  r = parse_reglist_namelist (buf, buflen, af->do_swap_header, nl, rl, 0);
  free (buf);

  return r;
}

#if HAVE_BZ2 == 1
/* bzlib has no seek function... */
static int bz2_skip_bytes (BZFILE * b, int n)
{
  #define SKIP_BUF_SIZE 100
  char buf[SKIP_BUF_SIZE];
  int j = n;
  int r;

  while (j >= SKIP_BUF_SIZE)
  {
    r = BZ2_bzread (b, buf, SKIP_BUF_SIZE);
    if (r != SKIP_BUF_SIZE)
      return ARC_ERR_EOF;
    j -= SKIP_BUF_SIZE;
  }
  if (j > 0)
  {
    r = BZ2_bzread (b, buf, j);
    if (r != j)
      return ARC_ERR_EOF;
  }

  return ARC_OK;
}
#endif

int arcfile_skip_regmap (struct arcfile * af)
{
  int r;

  switch (af->file_type)
  {
    case ARC_FILE_PLAIN :
      r = fseek (af->f, af->frame0_ofs, SEEK_SET);
      break;

#if HAVE_GZ == 1
    case ARC_FILE_GZ :
      r = gzseek (af->g, af->frame0_ofs, SEEK_SET);
      break;
#endif

#if HAVE_BZ2 == 1
    case ARC_FILE_BZ2 :
      r = bz2_skip_bytes (af->b, af->frame0_ofs - 24);
      break;
#endif
    default:
      return ARC_ERR_FORMAT;
  }

  if (r == -1)
    return ARC_ERR_EOF;

  return ARC_OK;
}

