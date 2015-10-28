/*
 * RWO 090403 - dump time streams to standard
 *              output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "readarc.h"
#include "output_dirball.h"
#include "output_txt.h"
#include "output_hex.h"

#define DEBUG_ARCFILE 1

#if DEBUG_ARCFILE == 1
#  define DEBUG(args...) printf(args)
#else
#  define DEBUG(args...)
#endif

#define OUTFORMAT_TXT     0
#define OUTFORMAT_HEX     1
#define OUTFORMAT_DIRFILE 2

/* The name of this program.  */
const char* program_name;

/* Prints usage information for this program to STREAM (typically
   stdout or stderr), and exit the program with EXIT_CODE.  Does not
   return.  */

void print_usage (FILE* stream, int exit_code)
{
  fprintf (stream, "Usage:  %s options [ inputfile ... ]\n", program_name);
  fprintf (stream,
           "  -h  --help             Display this usage information.\n"
           "  -o  --output filename  Write output to file.\n"
           "  -v  --verbose          Print verbose messages.\n");
  exit (exit_code);
}

int guess_output_filename (const char * input_fname, char ** output_fname, int format, int do_tar, int do_gzip)
{
  char * basename;
  int iext;

  *output_fname = NULL;

  if (input_fname == NULL)
  {
    iext = 0;
  }
  else
  {
    /* Find arcfile extension (if any) in input_fname */
    int ninput = strlen(input_fname);
    iext = ninput;
    for (int i=0; i<ninput; i++)
      if (!strncasecmp(input_fname+i,".dat",4) || !strncasecmp(input_fname+i,".dat.",5))
      {
        iext=i;
        break;
      }

    basename = input_fname;
  }

  if (iext==0)
  {
    basename = "out";
    iext = 3;
  }

  switch (format)
  {
    case OUTFORMAT_TXT:
      *output_fname = malloc (iext + 5);
      strncpy (*output_fname, basename, iext);
      strcpy (iext+*output_fname,".txt");
      break;
    case OUTFORMAT_HEX:
      *output_fname = malloc (iext + 5);
      strncpy (*output_fname, basename, iext);
      strcpy (iext+*output_fname,".hex");
      break;
    case OUTFORMAT_DIRFILE:
      if (!do_tar)
      {
        *output_fname = malloc (iext + 1);
        strncpy (*output_fname, basename, iext);
      }
      else if (!do_gzip)
      {
        *output_fname = malloc (iext + 5);
        strncpy (*output_fname, basename, iext);
        strcpy (iext+*output_fname,".tar");
      }
      else
      {
        *output_fname = malloc (iext + 5);
        strncpy (*output_fname, basename, iext);
        strcpy (iext+*output_fname,".tgz");
      }
      break;
  }

  return 0;
}

int main (int argc, char* argv[])
{
  int next_option;

  struct arcfilt filt;
  struct dataset ds;
  int r;
  int verbose = 0;
  char ** nlist;
  int nn;
  int format, do_tar, do_gzip;

  /* A string listing valid short options letters.  */
  const char* const short_options = "ho:r:s:e:f:tzv";
  /* An array describing valid long options.  */
  const struct option long_options[] = {
    { "help",     0, NULL, 'h' },
    { "output",   1, NULL, 'o' },
    { "register", 1, NULL, 'r' },
    { "start",    1, NULL, 's' },
    { "end",      1, NULL, 'e' },
    { "format",   1, NULL, 'f' },
    { "tar",      0, NULL, 't' },
    { "gzip",     0, NULL, 'z' },
    { "verbose",  0, NULL, 'v' },
    { NULL,       0, NULL, 0   }   /* Required at end of array.  */
  };

  /* The name of the file to receive program output, or NULL for
     standard output.  */
  const char* output_filename = NULL;

  /* Remember the name of the program, to incorporate in messages.
     The name is stored in argv[0].  */
  program_name = argv[0];

  filt.use_utc = 0;
  filt.t1[0] = 0;
  filt.t1[1] = 0;
  filt.t2[0] = 0 - 1;    /* Max uint32_t */
  filt.t2[1] = 0;
  if (argc > 0)
    nlist = malloc (argc * sizeof (char *));
  else
    nlist = NULL;
  nn = 0;
  format = OUTFORMAT_DIRFILE;
  do_tar = 1;
  do_gzip = 1;

  do {
    next_option = getopt_long (argc, argv, short_options,
                               long_options, NULL);
    switch (next_option)
    {
    case 'h':   /* -h or --help */
      /* User has requested usage information.  Print it to standard
         output, and exit with exit code zero (normal termination).  */
      print_usage (stdout, 0);

    case 'o':   /* -o or --output */
      /* This option takes an argument, the name of the output file.  */
      output_filename = optarg;
      break;

    case 's':   /* -s or --start */
      /* This option takes an argument, the starting UTC time. */
      r = txt2utc (optarg, filt.t1);
      filt.use_utc = 1;
      if (r != 0)
      {
        printf ("could not parse UTC starting time!");
        return -1;
      }
      break;

    case 'e':   /* -e or --end */
      /* This option takes an argument, the ending UTC time. */
      r = txt2utc (optarg, filt.t2);
      filt.use_utc = 1;
      if (r != 0)
      {
        printf ("could not parse UTC ending time!");
        return -1;
      }
      break;

    case 'r':   /* -r or --register */
      /* This option takes an argument, the register specification. */
      /* It can be used multiple times. */
      nlist[nn] = optarg;
      nn++;
      break;

    case 't':   /* -t or --tar */
      do_tar = 1;
      break;

    case 'z':   /* -z or --gzip */
      do_gzip = 1;
      break;

    case 'f':   /* -f or --format */
      if (!strcasecmp (optarg, "dir") || !strcasecmp (optarg, "dirfile"))
        format = OUTFORMAT_DIRFILE;
      else if (!strcasecmp (optarg, "txt") || !strcasecmp (optarg, "text"))
        format = OUTFORMAT_TXT;
      else if (!strcasecmp (optarg, "hex") || !strcasecmp (optarg, "dump"))
        format = OUTFORMAT_HEX;
      else
      {
        printf ("Unrecognized format type %s.\n", optarg);
        return -1;
      }
      break;

    case 'v':   /* -v or --verbose */
      verbose = 1;
      break;

    case '?':   /* The user specified an invalid option.  */
      /* Print usage information to standard error, and exit with exit
         code one (indicating abnormal termination).  */
      print_usage (stderr, 1);

    case -1:    /* Done with options.  */
      break;

    default:    /* Something else: unexpected.  */
      abort ();
    }
  }
  while (next_option != -1);

  /* Done with options.  OPTIND points to first non-option argument. */
  if (nn>0)
    create_namelist (nn, nlist, &(filt.nl));
  else
  {
    filt.nl.n = 0;
    filt.nl.s = 0;
  }
  if (argc > 0)
    free (nlist);

  for (int i = optind; i < argc; ++i)
  {
    char * use_output_fname;
    filt.fname = argv[i];

    DEBUG ("Number of register name specifications = %d.\n", filt.nl.n);
    DEBUG ("Calling readarc.\n");

    r = readarc (&filt, &ds); 
    if (r != 0)
    {
      free_namelist (&(filt.nl));
      return r;
    }
    DEBUG ("Returned %d.\n", r);

    use_output_fname = output_filename;
    if (use_output_fname == NULL)
      guess_output_filename (filt.fname, &use_output_fname, format, do_tar, do_gzip);

    switch (format)
    {
      case OUTFORMAT_TXT:
        r = output_txt (&ds);
        break;
      case OUTFORMAT_HEX:
        r = output_hex (&ds);
        break;
      case OUTFORMAT_DIRFILE:
        if (!do_tar)
          r = output_dirball (use_output_fname, &ds, 0);
        else if (!do_gzip)
          r = output_dirball (use_output_fname, &ds, 1);
        else
          r = output_dirball (use_output_fname, &ds, 2);
        break;
      default:
        printf ("Unknown output type code %d.\n", format);
    }

    if ((use_output_fname != NULL) && (use_output_fname != output_filename))
      free (use_output_fname);

    free_dataset (&ds);
  }
  free_namelist (&(filt.nl));

  return 0;
}
