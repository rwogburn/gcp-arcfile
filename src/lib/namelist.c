#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "namelist.h"
#include "readarc.h"

#if DO_DEBUG_NAMELIST
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(...)
#endif
#if DO_DEBUG_NAMELIST > 1
#  define DEBUG2(args...) printf(args)
#else
#  define DEBUG2(...)
#endif

static int count_sort_chanlist (struct chanlist * chan)
{
  int is_done;
  int i, n;
  int tmp;

  if (chan->n <= 0)
  {
    chan->ntot = 0;
    return 0;
  }
  DEBUG ("count_sort_chanlist: n=%d\n", chan->n);

  DEBUG ("Eliminating any bad 'uns\n");
  /* Eliminate any bad lists */
  n = chan->n;
  for (i=0; i<n; i++)
  {
    DEBUG ("  ... on %d.\n", i);
    if ((chan->c1[i] < 0) || (chan->c2[i] < 0) || (chan->c2[i] < chan->c1[i]))
    {
      DEBUG ("  ... range %d is gone.\n", i);
      chan->c1[i] = chan->c1[n-1];
      chan->c2[i] = chan->c2[n-1];
      n--;
    }
  }

  DEBUG ("Bubble-sorting and merging where possible.\n");
  /* Bubble sort & merge where possible!*/
  is_done = 0;
  while (is_done == 0)
  {
    is_done = 1;
    for (i=0; i<(n-1); i++)
    {
      /* Reorder */
      if (chan->c1[i] > chan->c1[i+1])
      {
        tmp = chan->c1[i];
        chan->c1[i] = chan->c1[i+1];
        chan->c1[i+1] = tmp;
        tmp = chan->c2[i];
        chan->c2[i] = chan->c2[i+1];
        chan->c2[i+1] = tmp;
        is_done = 0;
      }
      /* Merge */
      if ((chan->c1[i] <= chan->c1[i+1]) && (chan->c2[i] >= (chan->c1[i+1]-1)))
      {
        DEBUG ("  ... merging ranges %d and %d.\n", i, i+1);
        if (chan->c2[i+1] > chan->c2[i])
          chan->c2[i] = chan->c2[i+1];
        chan->c1[i+1] = chan->c1[n-1];
        chan->c2[i+1] = chan->c2[n-1];
        n--;
        is_done = 0;
        break;
      }
    }
  }
  if (chan->n != n)
  {
    DEBUG ("Reallocating buffers.\n");
    chan->c1 = realloc (chan->c1, n * sizeof (int));
    chan->c2 = realloc (chan->c2, n * sizeof (int));
    chan->n = n;
  }

  /* If we're left with no channels, set n=-1 (no channels) */
  /* rather than n=0 (all channels) to reject all channels. */
  if (chan->n <= 0)
  {
    DEBUG ("No channels remaining.\n");
    chan->n = -1;
    chan->ntot = 0;
    return 0;
  }

  /* Count */
  n = 0;
  for (i=0; i<chan->n; i++)
    n += (chan->c2[i] - chan->c1[i] + 1);
  chan->ntot = n;

  DEBUG ("Counted %d total channels in %d ranges.\n", chan->ntot, chan->n);
#if DO_DEBUG_NAMELIST
  for (i=0; i<chan->n; i++)
    DEBUG ("  %d-%d\n", chan->c1[i], chan->c2[i]);
#endif
  
  return 0;
}

static int parse_chans (char * s, struct chanlist * cl, struct chanlist * sl)
{
  int n, j, k;
  int ntmp;
  int * c1tmp;
  int * c2tmp;
  int len;

  sl->n = 0;
  sl->c1 = NULL;
  sl->c2 = NULL;

  j = 0;
  while (s[j] == ' ') j++;

  if (s[j]!='[')
  {
    cl->n = 0;
    cl->c1 = NULL;
    cl->c2 = NULL;
    return j;
  }
  j+=1;
  len = strlen (s+j);
  ntmp = (len + 1) / 2;
  cl->n = 0;
  c1tmp = malloc (ntmp * sizeof(int));
  c2tmp = malloc (ntmp * sizeof(int));
  while (j<len)
  {
    while ((s[j] == ' ') || (s[j] == ',')) j++;
    n = sscanf (s+j, "%d:%d%n", &(c1tmp[cl->n]), &(c2tmp[cl->n]), &k);
    if (n == 2)
    {
      DEBUG("Channels %d-%d.\n", c1tmp[cl->n], c2tmp[cl->n]);
      j += k;
      cl->n++;
      continue;
    }
    n = sscanf (s+j, "%d%n", &(c1tmp[cl->n]), &k);
    if (n == 1)
    {
      DEBUG("Channel %d.\n", c1tmp[cl->n]);
      j += k;
      c2tmp[cl->n] = c1tmp[cl->n];
      cl->n++;
      continue;
    }
    DEBUG("Hit character %c at %d, stopping.\n", s[j], j);
    break;
  }
  cl->c1 = realloc (c1tmp, cl->n * sizeof(int));
  cl->c2 = realloc (c2tmp, cl->n * sizeof(int));

  /* Found a channel specification, but it's empty.  Set n=-1 */
  /* to reject all channels, instead of n=0 to keep all chans.*/
  if (cl->n==0)
  {
    DEBUG("Parsed empty channel list in %d characters.\n", j);
    free (cl->c1);
    free (cl->c2);
    cl->n=-1;
  }

  DEBUG("Parsed channel numbers in %d characters.\n", j);
  return j;
}

static int parse_name_part (char * s, char ** p)
{
  int i = 0;
  int n;

  n = -1;
  while (i < MAX_NAME_LENGTH)
  {
    if ((s[i]=='\0') || (s[i]=='['))
    {
      n = i;
      break;
    }
    if (s[i]=='.')
    {
      n = i+1;
      break;
    }
    i++;
  }
  if (n == -1)
  {
    *p = NULL;
    return n;
  }

  *p = malloc (i+1);
  memcpy ((void *)(*p), (void *)s, i);
  (*p)[i] = '\0';
  DEBUG ("parsed namelist part '%s' in %d characters.\n", (*p), i);
  return n;
} 

int create_namelist (int n, char ** s, struct namelist * f)
{
  int i, j, k;

  f->s = malloc (n * sizeof (struct search_spec));
  if (f->s == NULL)
    return ARC_ERR_NOMEM;
  f->n = n;

  for (i=0; i<n; i++)
  {
    f->s[i].m = NULL;
    f->s[i].b = NULL;
    f->s[i].r = NULL;

    j = parse_name_part (s[i], &(f->s[i].m));
    if (j < 0)
      continue;

    k = parse_name_part (s[i]+j, &(f->s[i].b));
    if (k < 0)
      continue;
    j += k;

    k = parse_name_part (s[i]+j, &(f->s[i].r));
    if (k < 0)
      continue;
    j += k;

    /* FIXME: parse and handle sample selections */
    k = parse_chans (s[i]+j, &(f->s[i].chan), &(f->s[i].samp));
    count_sort_chanlist (&(f->s[i].chan));
    count_sort_chanlist (&(f->s[i].samp));
  }
#if DO_DEBUG_NAMELIST
  for (i=0; i<n; i++)
  {
    DEBUG("Filter %d: <%s> <%s> <%s>", i, f->s[i].m, f->s[i].b, f->s[i].r);
    if (f->s[i].chan.n > 0)
    {
      DEBUG(" [ ");
      for (j=0; j<(f->s[i].chan.n); j++)
        if (f->s[i].chan.c1[j] == f->s[i].chan.c2[j])
          DEBUG("%d ", f->s[i].chan.c1[j]);
        else
          DEBUG("%d-%d ", f->s[i].chan.c1[j], f->s[i].chan.c2[j]);
      DEBUG("]");
    }
    DEBUG("\n");
  }
#endif

  f->nutc = -1;

  return 0;
}

#define CHECK_FREE(x) if ((x)!=NULL) free((x));

int free_namelist (struct namelist * f)
{
  int i;
  for (i=0; i<f->n; i++)
  {
    CHECK_FREE(f->s[i].m);
    CHECK_FREE(f->s[i].b);
    CHECK_FREE(f->s[i].r);
    free_chanlist (&(f->s[i].chan));
    free_chanlist (&(f->s[i].samp));
  }
  CHECK_FREE(f->s);

  return ARC_OK;
}

/* Test a single string against name filter. */
static int test_name_part (char * s, char * f)
{
  int i, flen, slen;

  /* Null input is a match. */
  if ((f == NULL) || (s == NULL))
    return 1;

  DEBUG2 ("Testing name part %s against filter %s.\n", s, f);

  flen = strlen (f);
  slen = strlen (s);

  /* Zero-length input is a match. */
  if ((flen == 0) || (slen == 0))
    return 1;

  /* Loop over characters */
  i = 0;
  while (i < flen)
  {
    /* If we hit a wild card, it's a match. */
    if (f[i] == '*')
      return 1;
    /* If we get to the end of the test string, no match. */
    if (i == slen)
      break;
    /* If we hit a mismatch, give up on this filter string. */
    if (f[i] != s[i])
      break;
    /* If we get to the end, it's a match. */
    if ((i == flen-1) && (i == slen-1))
      return 1;

    i++;
  }
  /* Didn't match. */
  return 0;
}

/* Test whether a register m.b.r matches one of the entries */
/* in the name list.  Wild cards are supported, but only at */
/* the end of a name part.  Return -1 if no match, or index */
/* of matching entry if a match is found - don't if(result) */
int test_reg_name (char * m, char * b, char * r, struct namelist * f)
{
  int j;

  DEBUG2 ("test_reg_name against %d filters.\n", f->n);
  /* Loop over filter strings */
  for (j=0; j<f->n; j++)
  {
    /* Try each part, and jump out if no match */
    /* Treat null input as a match             */
    if ((m != NULL) && !test_name_part (m, f->s[j].m))
      continue;
    if ((b != NULL) && !test_name_part (b, f->s[j].b))
      continue;
    if ((r != NULL) && !test_name_part (r, f->s[j].r))
      continue;

    DEBUG2 ("Match.\n");
    /* If we got here, it's a match. */
    return j;
  }

  DEBUG2 ("No match.\n");
  /* Nothing matched. */
  return -1;
}

int copy_chanlist (struct chanlist * dst, struct chanlist * src)
{
  if ((src == NULL) || (src->n <= 0))
  {
    if (src == NULL)
      dst->n = 0;
    else
      dst->n = src->n;
    dst->ntot = 0;
    dst->c1 = NULL;
    dst->c2 = NULL;
    return 0;
  }
  dst->n = 0;
  dst->c1 = malloc (src->n * sizeof (int));
  if (dst->c1 == NULL)
    return ARC_ERR_NOMEM;
  dst->c2 = malloc (src->n * sizeof (int));
  if (dst->c2 == NULL)
  {
    free (dst->c1);
    return ARC_ERR_NOMEM;
  }
  memcpy (dst->c1, src->c1, src->n * sizeof(int));
  memcpy (dst->c2, src->c2, src->n * sizeof(int));
  dst->n = src->n;
  dst->ntot = src->ntot;

  return ARC_OK;
}

int free_chanlist (struct chanlist * chan)
{
  if (chan->n <= 0)
  {
    chan->c1 = NULL;
    chan->c2 = NULL;
    chan->n = 0;
    chan->ntot = 0;
    return ARC_OK;
  }
  if (chan->c1 != NULL)
    free (chan->c1);
  if (chan->c2 != NULL)
    free (chan->c2);

  chan->c1 = NULL;
  chan->c2 = NULL;
  chan->n = 0;
  chan->ntot = 0;

  return ARC_OK;
}

