#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "utcrange.h"

#if DO_DEBUG_UTCRANGE
#define DEBUG(args...) printf(args)
#else
#define DEBUG(args...)
#endif

/* MJD as defined here:
 * http://tycho.usno.navy.mil/mjd.html
 */
static uint32_t mjd (int d[3])
{
  const int month_start_noleap[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  uint32_t days;

  days = 48987;
  days += (d[0]-1993) * 365;
  days += (d[0]-1) / 4;
  days += month_start_noleap[d[1]-1];
  if ((d[1] > 2) && (d[0] % 4 == 0))
    days += 1;
  days += d[2]-1;

  return days;  
}

static uint32_t timeofday (int t[3])
{
  uint32_t tim;
  tim = t[0] * 60;
  tim += t[1];
  tim *= 60;
  tim += t[2];
  tim *= 1000;

  return tim;
}

int txt2utc (char * txt, uint32_t utc[2])
{
  const char * months[] = {"january",
        "february", "march", "april",
        "may", "june", "july",
        "august", "september", "october",
        "november", "december"};
  int date_parts[3];
  int time_parts[3];
  char month_name[10];
  int i, l, n;

  if (txt == NULL)
    return -1;

  DEBUG ("UTC conversion of text %s.\n", txt);

  n = sscanf (txt, "%d-%9[A-Za-z]-%d:%d:%d:%d",
    &(date_parts[0]), month_name, &(date_parts[2]),
    &(time_parts[0]), &(time_parts[1]), &(time_parts[2]));

  DEBUG ("Filled %d spots.\n", n);
  if (n < 3)
  {
    return -1;
  }
  if (n < 4)
    time_parts[0] = 0;
  if (n < 5)
    time_parts[1] = 0;
  if (n < 6)
    time_parts[2] = 0;

  l = strlen (month_name);
  date_parts[1] = -1;
  for (i=0; i<12; i++)
  {
    if (!strcasecmp (month_name, months[i]))
    {
      date_parts[1] = i+1;
      break;
    }
    if ((l==3) && (!strncasecmp (month_name, months[i], 3)))
    {
      date_parts[1] = i+1;
      break;
    }
  }
  if (date_parts[1] == -1)
    return -1;

  /* Handle dates specified like 01-feb-2010 as well as 2010-feb-01 */
  if (date_parts[0] < 1900)
  {
    int tmp = date_parts[0];
    date_parts[0] = date_parts[2];
    date_parts[2] = tmp;
  }

  utc[0] = mjd (date_parts);
  utc[1] = timeofday (time_parts);
  return 0;
}

int fname2utc (char * fname, uint32_t utc[2])
{
  int date_parts[3];
  int time_parts[3];
  int n;

  n = sscanf (fname, "%4d%2d%2d_%2d%2d%2d",
    &(date_parts[0]), &(date_parts[1]), &(date_parts[2]),
    &(time_parts[0]), &(time_parts[1]), &(time_parts[2]));

  if (n < 6)
  {
    return -1;
  }

  utc[0] = mjd (date_parts);
  utc[1] = timeofday (time_parts);

  return 0;
}

#if 0
int main (int argc, char * argv[])
{
  char ** flist;
  int i, r;

  r = select_files (argv[1], argv[2], argv[3], &flist, &i);
  DEBUG ("Result is %d, selected %d files.\n", r, i);

  return r;
}
#endif
