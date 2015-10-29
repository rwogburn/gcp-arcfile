#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "reglist.h"
#include "arc_endian.h"
#include "namelist.h"
#include "readarc.h"

#define DEFAULT_MAX_REGBLOCKS 200
#if DO_DEBUG_REGLIST
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(...)
#endif

struct mem_buf {
  void * buf;
  int len, ofs;
};

/* Helper function: read n bytes, and check they're all zeros */
static int check_zeros (struct mem_buf * b, int n)
{
  int i;
  if (b->ofs + n > b->len)
    return -1;
  for (i=0; i<n; i++)
  {
    if (*(unsigned char *)(b->buf + b->ofs) != 0)
      return -1;
    b->ofs += sizeof (unsigned char);
  }
  return 0;
}

/* Helper function: read a uint16, swapping ends if needed */
static int parse_uint16 (struct mem_buf * b, int do_swap, uint16_t * n)
{
  if (b->ofs + sizeof(uint16_t) > b->len)
    return -1;

  *n = *(uint16_t *)(b->buf + b->ofs);
  if (do_swap)
    swap_2 (n);

  b->ofs += sizeof(uint16_t);

  return 0;
}

/* Helper function: parse a character string with its
 * length encoded as a uint16.
 */
static int parse_name (struct mem_buf * b, int do_swap, char * s, int buflen)
{
  int r;
  uint16_t l;

  r = parse_uint16 (b, do_swap, &l);
  if (r != 0)
    return r;
  if (l > buflen)
    return -1;
  if (b->ofs + l > b->len)
    return -1;
  strncpy (s, b->buf + b->ofs, l);
  b->ofs += l;
  s[l] = '\0';

  return 0;
}

/* Helper function: read an array of size uint32s,
 * swapping ends if needed.
 */
static int read_regblock_spec (struct mem_buf * b, int do_swap, uint32_t s[6])
{
  int r;
  if (b->ofs + 6 * sizeof (uint32_t) > b->len)
    return -1;
  memcpy (s, b->buf + b->ofs, 6 * sizeof (uint32_t));
  b->ofs += 6 * sizeof (uint32_t);
  if (do_swap)
    for (r=0; r<6; r++)
      swap_4 (s + r);

  return 0;
}

/* Number of bytes used by a register block, from 6-word spec  */
static int regblock_size_fast (uint32_t regblock_spec[6])
{
  int n, m;

  /* If not archived, it's zero */
  if (regblock_spec[0] & GCP_REG_EXC)
    return 0;

  /* Find number of elements per frame */
  n = regblock_spec[5];  /* Samples per frame */
  if (n == 0) n = 1;
  n *= regblock_spec[4]; /* Number of channels */
  if (n == 0) n = 1;

  /* Find size of each element */
  m = element_size (regblock_spec[0]);

  return m * n;
}

/* In case we need to allocate a bigger buffer of
 * register blocks.
 */
static int realloc_regblock_buf (struct reglist * rm, int n)
{
  int new_max = n;
  int i;
  struct reglist_entry * tmp;

  if (new_max <= 0)
    new_max = rm->num_regblocks;
  if (n < rm->num_regblocks)
    return -1;

  tmp = malloc (new_max * sizeof(struct reglist_entry));
  if (tmp == NULL)
    return -1;

  for (i=0; i<rm->num_regblocks; i++)
    tmp[i] = rm->r[i];

  free (rm->r);
  rm->r = tmp;
  rm->max_regblocks = new_max;

  return 0;
}

/* Add a single new register block to the register map rm, */
/* re-allocating a larger buffer in rm if needed.          */
static int add_regblock (struct reglist * rm,
    char * on_map, char * on_board, char * on_regblock,
    uint32_t regblock_spec[6], int ofs, struct chanlist * chan)
{
  int r, n;

  /* Make more space for the new reg block if needed */
  if (rm->num_regblocks >= rm->max_regblocks)
  {
    int new_max = 2 * rm->max_regblocks;
    if (new_max <= 0)
      new_max = DEFAULT_MAX_REGBLOCKS;
    r = realloc_regblock_buf (rm, new_max);
    if (r != 0)
      return r;
  }

  DEBUG ("Adding %s.%s.%s as a match.\n", on_map, on_board, on_regblock);
  n = rm->num_regblocks;
  rm->r[n].rb.typeword = regblock_spec[0];
  rm->r[n].rb.nchan = regblock_spec[4];
  rm->r[n].rb.spf = regblock_spec[5];
  rm->r[n].rb.is_fast = (rm->r[n].rb.typeword & 0x200000) > 0;
  rm->r[n].rb.is_complex = (rm->r[n].rb.typeword & 0x1) > 0;
  rm->r[n].rb.do_arc = (rm->r[n].rb.typeword & 0x100) == 0;
  rm->r[n].ofs_in_frame = ofs;
  r = copy_chanlist (&(rm->r[n].chan), chan);
  if (r != 0)
    return ARC_ERR_NOMEM;
  if (rm->r[n].rb.is_fast && (rm->r[n].rb.spf==0))
  {
    rm->r[n].rb.spf = rm->r[n].rb.nchan;
    rm->r[n].rb.nchan = 1;
  }

  if (rm->r[n].rb.is_fast == 0)
    rm->r[n].rb.spf = 1;

  strcpy (rm->r[n].rb.map, on_map);
  strcpy (rm->r[n].rb.board, on_board);
  strcpy (rm->r[n].rb.regblock, on_regblock);

  rm->num_regblocks = n+1;

  return 0;
}

/* Each board has a special "status" register that's not given  */
/* in the file's register map.  Add it separately.              */
static int add_status_regblock (struct reglist * rm,
    char * on_map, char * on_board,
    struct namelist * filt, int is_board_match, uint32_t * ofs)
{
  uint32_t regblock_spec[6];
  int regblock_match_num = 0;
  int r;

  regblock_spec[0] = GCP_REG_UINT;
  regblock_spec[1] = 0x0F;
  regblock_spec[2] = 0;
  regblock_spec[3] = 0;
  regblock_spec[4] = 1;
  regblock_spec[5] = 0;

  if (filt != NULL)
  {
    if (is_board_match)
      regblock_match_num = test_reg_name (on_map, on_board, "status", filt);
    else
      regblock_match_num = -1;
  }

  DEBUG ("On board 'frame', block %s, offset=%ld.\n", regblocks[i], *ofs);
  if (regblock_match_num >= 0)
  {
    if (filt == NULL)
      r = add_regblock (rm, on_map, (char *)on_board, "status", regblock_spec, *ofs, NULL);
    else
      r = add_regblock (rm, on_map, (char *)on_board, "status", regblock_spec, *ofs,
        &(filt->s[regblock_match_num].chan));
  }

  (*ofs) += regblock_size_fast (regblock_spec);
  
  return 0;
}

/* Each map has a special "frame" board that's not given in the */
/* file register map.                                           */
static int add_frame_board (struct reglist * rm, char * on_map,
    struct namelist * filt, int is_map_match, uint32_t * ofs)
{
  const char on_board[] = "frame";
  const char * regblocks[] = {"received", "nsnap", "record", "utc", "lst", "features", "markSeq"};
  const int types[] = {GCP_REG_UCHAR, GCP_REG_UINT, GCP_REG_UINT, GCP_REG_UTC, GCP_REG_UINT, GCP_REG_UINT, GCP_REG_UINT};
  int is_board_match = 1;
  int regblock_match_num = 0;
  uint32_t regblock_spec[6];
  int i;
  int r;

  if (filt != NULL)
    is_board_match = is_map_match && (test_reg_name (on_map, (char *)on_board, NULL, filt) >= 0);

  regblock_spec[1] = 0x0F;
  regblock_spec[2] = 0;
  regblock_spec[3] = 0;
  regblock_spec[4] = 1;
  regblock_spec[5] = 0;

  add_status_regblock (rm, on_map, on_board, filt, is_board_match, ofs);
  
  for (i=0; i<sizeof(types)/sizeof(int); i++)
  {
    regblock_spec[0] = types[i];
    if (filt != NULL)
    {
      if (is_board_match)
        regblock_match_num = test_reg_name (on_map, (char *)on_board, (char *)regblocks[i], filt);
      else
        regblock_match_num = -1;
    }

    DEBUG ("On board 'frame', block %s, offset=%ld.\n", regblocks[i], *ofs);
    if (regblock_match_num >= 0)
    {
      if (filt == NULL)
        r = add_regblock (rm, on_map, (char *)on_board, (char *)regblocks[i], regblock_spec, *ofs, NULL);
      else
        r = add_regblock (rm, on_map, (char *)on_board, (char *)regblocks[i], regblock_spec, *ofs,
          &(filt->s[regblock_match_num].chan));
    }

    (*ofs) += regblock_size_fast (regblock_spec);
  }

  return 0;
}

/* Ordinary boards are read from the arc file register map. */
static int add_board (struct reglist * rm, char * on_map,
    struct namelist * filt, int is_map_match, struct mem_buf * b, int do_swap, uint32_t * ofs)
{
  char on_board[MAX_NAME_LENGTH];
  char on_regblock[MAX_NAME_LENGTH];
  int is_board_match = 1;
  int regblock_match_num = 0;
  uint32_t regblock_spec[6];
  uint16_t num_regblocks;
  int ir;
  int r;

  r = parse_name (b, do_swap, on_board, MAX_NAME_LENGTH-1);
  if (r != 0)
    return -1;
  if (filt != NULL)
    is_board_match = is_map_match && (test_reg_name (on_map, on_board, NULL, filt) >= 0);

  add_status_regblock (rm, on_map, on_board, filt, is_board_match, ofs);

  r = parse_uint16 (b, do_swap, &num_regblocks);
  if (r != 0)
    return -1;

  on_regblock[0] = '\0';

  DEBUG ("On board %s, %d blocks.\n", on_board, num_regblocks);
  for (ir=0; ir<num_regblocks; ir++)
  {
    r = parse_name (b, do_swap, on_regblock, MAX_NAME_LENGTH-1);
    if (r != 0)
      return -1;
    if (filt != NULL)
    {
      if (is_board_match)
        regblock_match_num = test_reg_name (on_map, on_board, on_regblock, filt);
      else
        regblock_match_num = -1;
    }

    DEBUG ("On block %s, offset=%d.\n", on_regblock, *ofs);
    r = read_regblock_spec (b, do_swap, regblock_spec);
    if (r != 0)
      return -1;

    if (regblock_match_num >= 0)
    {
      if (filt == NULL)
        r = add_regblock (rm, on_map, on_board, on_regblock, regblock_spec, *ofs, NULL);
      else
        r = add_regblock (rm, on_map, on_board, on_regblock, regblock_spec, *ofs,
          &(filt->s[regblock_match_num].chan));
    }

    *ofs += regblock_size_fast (regblock_spec);
  }

  /* Zero padding at the end of each board specification */
  r = check_zeros (b, 4*sizeof(uint32_t));
  if (r != 0)
    return -1;

  return 0;
}

/* The heart of the beast! */
/* Read a register map from the arc file, adding to our register map any
 * register blocks that match the filter specification in filt.
 * if filt is NULL, keep all the register blocks.
 *
 * if max_regblocks is specified, the array of register blocks will be
 * initialized to this value.  Otherwise, a default DEFAULT_MAX_REGBLOCKS
 * will be used.
 */
int parse_reglist_namelist (void * buf, int buflen, int do_swap,
    struct namelist * filt, struct reglist * rm, int max_regblocks)
{
  int r;
  char on_map[MAX_NAME_LENGTH];
  char on_board[MAX_NAME_LENGTH];
  uint16_t num_maps, num_boards;
  int im, ib;
  int is_map_match = 1;
  uint32_t ofs = 8;
  struct mem_buf b;

  DEBUG ("entering read_reglist_namelist.\n");
  if (max_regblocks <= 0)
    rm->max_regblocks = DEFAULT_MAX_REGBLOCKS;
  else
    rm->max_regblocks = max_regblocks;

  b.ofs = 0;
  b.buf = buf;
  b.len = buflen;

  rm->num_regblocks = 0;
  rm->r = malloc ((rm->max_regblocks) * sizeof(struct reglist_entry));
  if (rm->r == 0)
    return -1;

  on_map[0] = '\0';
  on_board[0] = '\0';

  r = parse_uint16 (&b, do_swap, &num_maps);
  if (r != 0)
  {
    printf ("Error reading number of register maps.\n");
    num_maps = -1;
  }

  DEBUG ("Number of maps = %d.\n", num_maps);
  for (im=0; im<num_maps; im++)
  {
    r = parse_name (&b, do_swap, on_map, MAX_NAME_LENGTH-1);
    if (r != 0)
      break;
    if (filt != NULL)
      is_map_match = (test_reg_name (on_map, NULL, NULL, filt) >= 0);

    /* ofs += 4; */
    r = add_frame_board (rm, on_map, filt, is_map_match,  &ofs);
    if (r != 0)
    {
      printf ("Error adding 'frame' board.\n");
      break;
    }

    r = parse_uint16 (&b, do_swap, &num_boards);
    if (r != 0)
      break;

    DEBUG ("On map %s, with %d boards.\n", on_map, num_boards);
    for (ib=0; ib<num_boards; ib++)
    {
      /* ofs += 4; */
      r = add_board (rm, on_map, filt, is_map_match, &b, do_swap, &ofs);
    }

    if (r != 0)
      break;
  }

  if (r != 0)
  {
    DEBUG ("Error parsing through register specifications.\n");
    free (rm->r);
    return -1;
  }

  DEBUG ("List of register blocks to load:\n");
  for (ib=0; ib<rm->num_regblocks; ib++)
    DEBUG ("    %s.%s.%s\n", rm->r[ib].rb.map, rm->r[ib].rb.board, rm->r[ib].rb.regblock);

  DEBUG ("Final offset is 0x%lx.\n", ofs);
  return 0;
}

int parse_reglist (void * buf, int buflen, int do_swap,
    struct reglist * rm, int max_regblocks)
{
  return parse_reglist_namelist (buf, buflen, do_swap, NULL, rm, max_regblocks);
}

int free_reglist (struct reglist * rm)
{
  free (rm->r);
  /* free (rm); */

  return 0;
}

