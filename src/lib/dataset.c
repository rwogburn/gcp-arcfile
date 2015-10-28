#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dataset.h"
#include "readarc.h"

#if DO_DEBUG_DATASET
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(...)
#endif
#if DO_DEBUG_DATASET > 1
#  define DEBUG2(args...) printf(args)
#else
#  define DEBUG2(...)
#endif


int init_dataset (struct dataset * ds, struct reglist * rl, int numframes)
{
  int i;
  int r=0;

  printf ("Initializing data set.\n");
  DEBUG ("Initializing data set with %d frames, %d register blocks.\n", numframes, rl->num_regblocks);

  if (rl == NULL)
  {
    ds->buf = NULL;
    ds->nb = 0;
    ds->num_frames = 0;
    ds->max_frames = 0;
    return ARC_OK;
  }

  ds->buf = malloc (rl->num_regblocks * sizeof (struct databuf));
  if (ds->buf == NULL)
    return ARC_ERR_NOMEM;

  for (i=0; i<rl->num_regblocks; i++)
  {
    r = allocate_databuf (&(rl->r[i].rb), &(rl->r[i].chan), numframes, &(ds->buf[i]));
    if (r != 0)
      break;
  }
  if (r != 0)
  {
    ds->nb = i;
    free_dataset (ds);
    return r;
  }

  ds->max_frames = numframes;
  ds->num_frames = 0;
  ds->nb = rl->num_regblocks;

  return 0;
}

int free_dataset (struct dataset * ds)
{
  int i;

  DEBUG("Entering free_dataset.\n");
  for (i=0; i<ds->nb; i++)
  {
    if ((ds->buf[i].buf != NULL) && (ds->buf[i].maxframes > 0))
      free (ds->buf[i].buf);
    ds->buf[i].buf = NULL;
    ds->buf[i].maxframes = 0;
    free_chanlist (&(ds->buf[i].chan));
    if (ds->buf[i].rb != NULL)
      free (ds->buf[i].rb);
  }
  ds->nb = 0;
  if (ds->buf != NULL)
  {
    DEBUG("Freeing main dataset buffer.\n");
    free (ds->buf);
    ds->buf = NULL;
  }
  ds->max_frames = 0;
  ds->num_frames = 0;
  DEBUG("Returning from free_dataset, status %d.\n", ARC_OK);

  return ARC_OK;
}

int dataset_tight_size (struct dataset * ds)
{
  int i, r;

  DEBUG ("Entering dataset_tight_size\n");
  for (i=0; i<ds->nb; i++)
  {
    DEBUG ("Shrinking buffer %d/%d from %d to %d frames.\n", i, ds->nb, ds->buf[i].maxframes, ds->num_frames);
    r = change_databuf_numframes (&(ds->buf[i]), ds->num_frames);
    DEBUG ("returned %d.\n", r);
    if (r != 0)
      return r;
  }
  ds->max_frames = ds->num_frames;

  return ARC_OK;
}

int dataset_resize (struct dataset * ds, int numframes)
{
  int i, r;

  DEBUG ("Entering dataset_resize\n");
  for (i=0; i<ds->nb; i++)
  {
    DEBUG ("Resizing buffer %d from %d to %d frames.\n", i, ds->buf[i].maxframes, numframes);
    r = change_databuf_numframes (&(ds->buf[i]), numframes);
    DEBUG ("returned %d.\n", r);
    if (r != 0)
      return r;
  }
  ds->max_frames = numframes;

  return ARC_OK;
}

int copy_dataset (struct dataset * src, struct dataset * tgt)
{
  int i;
  void * src_tmp;
  void * tgt_tmp;
  long int src_chansize, tgt_chansize, copy_size;
  int j;
  int numchan;

  DEBUG ("copy_dataset: src: num_frames=%d, max_frames=%d; tgt: num_frames=%d, max_frames=%d.\n",
    src->num_frames, src->max_frames, tgt->num_frames, tgt->max_frames);

  if (tgt->num_frames + src->num_frames > tgt->max_frames)
    dataset_resize (tgt, tgt->num_frames + src->num_frames);

  for (i=0; i<src->nb; i++)
  {
    src_tmp = src->buf[i].buf;
    tgt_tmp = tgt->buf[i].buf + tgt->buf[i].numframes * tgt->buf[i].rb->spf * tgt->buf[i].elsize;
    src_chansize = src->buf[i].maxframes * src->buf[i].rb->spf * src->buf[i].elsize;
    tgt_chansize = tgt->buf[i].maxframes * tgt->buf[i].rb->spf * tgt->buf[i].elsize;
    copy_size = src->buf[i].numframes * src->buf[i].rb->spf * src->buf[i].elsize;
    if (src->buf[i].chan.n == 0)
      numchan = src->buf[i].rb->nchan;
    else
      numchan = src->buf[i].chan.ntot;
    for (j=0; j<numchan; j++)
    {
      memcpy (tgt_tmp + tgt_chansize*j, src_tmp + src_chansize*j, copy_size);
    }
    tgt->buf[i].numframes += src->buf[i].numframes;
  }
  tgt->num_frames += src->num_frames;

  return ARC_OK;
}

