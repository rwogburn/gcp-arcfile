#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dataset.h"
#include "readarc.h"

#if DO_DEBUG_DATABUF
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(...)
#endif
#if DO_DEBUG_DATABUF > 1
#  define DEBUG2(args...) printf(args)
#else
#  define DEBUG2(...)
#endif

int element_size (uint32_t typeword)
{
  int m = 0;

  if (typeword & GCP_REG_EXC)
    return 0;

  switch (typeword & GCP_REG_TYPE)
  {
    case GCP_REG_BOOL:
    case GCP_REG_CHAR:
    case GCP_REG_UCHAR:
      m = 1;
      break;

    case GCP_REG_SHORT:
    case GCP_REG_USHORT:
      m = 2;
      break;

    case GCP_REG_INT:
    case GCP_REG_UINT:
    case GCP_REG_FLOAT:
      m = 4;
      break;

    case GCP_REG_UTC:
    case GCP_REG_DOUBLE:
      m = 8;
      break;

    default:
      printf ("Unknown register type 0x%lx\n", typeword & GCP_REG_TYPE);
  }

  if (typeword & GCP_REG_COMPLEX)
    m *= 2;

  return m;
}

int allocate_databuf (struct regblockspec * rb, struct chanlist * chan, int numframes, struct databuf * ts)
{
  ts->rb = NULL;
  ts->chan.n = 0;
  ts->buf = NULL;
  ts->bufsize = 0;
  ts->numframes = 0;
  ts->maxframes = 0;

  ts->rb = malloc (sizeof (struct regblockspec));
  if (ts->rb == NULL)
    return ARC_ERR_NOMEM;
  memcpy (ts->rb, rb, sizeof (struct regblockspec));
  if (0 != copy_chanlist (&(ts->chan), chan))
    return ARC_ERR_NOMEM;
  ts->elsize = element_size (rb->typeword);

  DEBUG ("Allocating buffer for %s.%s.%s\n", rb->map, rb->board, rb->regblock);

  if (!rb->do_arc)
  {
    ts->bufsize = 0;
    ts->buf = NULL;
    return 0;
  }

  if (ts->chan.n == 0)
    ts->bufsize = numframes * ts->elsize * rb->spf * rb->nchan;
  else
    ts->bufsize = numframes * ts->elsize * rb->spf * ts->chan.ntot;
  ts->buf = malloc (ts->bufsize);
  if (ts->buf == 0)
  {
    printf ("Malloc failed when allocating buffer for %s.%s.%s.\n", rb->map, rb->board, rb->regblock);
    return -ARC_ERR_NOMEM;
  }

  ts->maxframes = numframes;

  return 0;
}

/* Changing number of frames is tricky - since we store the channels */
/* as time streams, one after the other, changing the length of each */
/* time stream means changing the offset of all the subsequent ones. */
int change_databuf_numframes (struct databuf * ts, int numframes)
{
  int numchan;
  long int new_bufsize;
  void * new_ptr;
  int ii;
  long int new_chan_size, old_chan_size;

  if (ts->chan.n == 0)
    numchan = ts->rb->nchan;
  else
    numchan = ts->chan.ntot;

  DEBUG ("change_databuf_numframes: %s.%s.%s: numframes=%d, elsize=%d, spf=%d, nchan=%d.\n",
    ts->rb->map, ts->rb->board, ts->rb->regblock,
    numframes, ts->elsize, ts->rb->spf, numchan);

  new_bufsize = numframes * ts->elsize * ts->rb->spf * numchan;

  /* If old = new, just return. */
  if (numframes == ts->maxframes)
    return 0;

  /* If new is zero, just free. */
  if (numframes == 0)
  {
    DEBUG("Keeping zero frames -- about to free ts->buf and set ts->bufsize to 0.  Pointer was 0x%lX, size was %ld.\n", ts->buf, ts->bufsize);
    ts->bufsize = 0;
    free (ts->buf);
    ts->maxframes = 0;
    return 0;
  }

  old_chan_size = ts->maxframes * ts->rb->spf * ts->elsize;
  new_chan_size = numframes * ts->rb->spf * ts->elsize;

  /* If old < new, reallocate and then move bytes as needed */
  if (numframes > ts->maxframes)
  {
    new_ptr = realloc (ts->buf, new_bufsize);
    if (new_ptr == NULL)
      return -1;

    ts->buf = new_ptr;
    /* Copy in reverse order, and don't bother to move channel 0 */
    /* since its offset doesn't change (still zero)              */
    for (ii=numchan-1; ii>0; ii--)
      memmove ((ts->buf) + ii*new_chan_size, (ts->buf) + ii*old_chan_size, old_chan_size);

    ts->bufsize = new_bufsize;
    ts->maxframes = numframes;

    return 0;
  }

  /* If new < old, move bytes and then reallocate. */
  else
  {
    /* Copy in forward order, and don't bother to move channel 0 */
    for (ii=1; ii<numchan; ii++)
      memmove ((ts->buf) + ii*new_chan_size, (ts->buf) + ii*old_chan_size, new_chan_size);

    DEBUG ("Reallocating buffer to size %d.  Old pointer was 0x%ld, size was %d.\n", new_bufsize, ts->buf, ts->bufsize);
    new_ptr = realloc (ts->buf, new_bufsize);
    if ((new_ptr == NULL) && (new_bufsize > 0))
      return -1;

    ts->buf = new_ptr;
    ts->bufsize = new_bufsize;
    ts->maxframes = numframes;
    if (ts->numframes < numframes)
      ts->numframes = numframes;

    return 0;
  }
}

/* Changing number of channels is easy - just reallocate */
int change_databuf_nchan (struct databuf * ts, int nchan)
{
  long int new_bufsize = ts->maxframes * ts->elsize * ts->rb->spf * nchan;
  void * new_ptr;

  return -1;

  /* If old = new, just return. */
  if (nchan == ts->rb->nchan)
    return 0;

  new_ptr = realloc (ts->buf, new_bufsize);
  if (new_ptr == NULL)
    return -1;

  ts->buf = new_ptr;
  ts->bufsize = new_bufsize;
  ts->rb->nchan = nchan;

  return 0;
}

int check_promote_databuf (struct databuf * ts, uint32_t typeword)
{
  return -1;
}

int copy_to_buf (FILE * f, struct databuf * ts, int32_t * ofs)
{
  int r;
  int ichan;
  void * tmp;

  if (ts->bufsize == 0)
    return 0;
  if (ts->numframes >= ts->maxframes)
    return -1;

  if (ts->chan.n != 0)
    return -1;

  tmp = ts->buf + (ts->numframes * ts->rb->spf * ts->elsize);
  for (ichan=0; ichan<ts->rb->nchan; ichan++)
  {
    r = fread (tmp + (ichan * ts->maxframes * ts->rb->spf * ts->elsize),
      ts->elsize, ts->rb->spf, f);
    if (r < ts->rb->spf)
    {
      printf ("Tried to read %d elements, got %d.\n", ts->rb->spf, r);
      return -1;
    }
    *ofs += r * ts->elsize;
  }

  ts->numframes += 1;

  return 0;
}

int memcopy_to_buf (void * m, struct databuf * ts)
{
  int ichan;
  void * tmp;
  uint32_t j = 0;
  uint32_t chan_size, frame_chan_size;

  if (ts->bufsize == 0)
    return 0;
  if (ts->numframes >= ts->maxframes)
    return -1;

  frame_chan_size = ts->rb->spf * ts->elsize;
  chan_size = frame_chan_size * ts->maxframes;
  tmp = ts->buf + (ts->numframes * frame_chan_size);

  if (ts->chan.n == 0)
    for (ichan=0; ichan<ts->rb->nchan; ichan++)
    {
      memcpy (tmp,
        m + j, frame_chan_size);
      j += frame_chan_size;
      tmp += chan_size;
    }
  else
  {
    for (j=0; j<ts->chan.n; j++)
    {
      for (ichan=ts->chan.c1[j]; ichan<=ts->chan.c2[j]; ichan++)
      {
        memcpy (tmp,
          m + ichan * frame_chan_size, frame_chan_size);
        tmp += chan_size;
      }
    }
  }

  ts->numframes += 1;

  return 0;
}

