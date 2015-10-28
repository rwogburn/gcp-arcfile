#include "handlesig.h"

/* See "signal basics" for more information */
/* http://orchard.wccnet.org/~chasselb/linux275/ClassNotes/process/sigbasics.htm */

/* Set of signals with only SIGINT selected. */
/* Prepare once, use each time we block,     */
/* unblock, or check for SIGINT.             */
sigset_t signals_sigint;

/* Sigaction structure for SIGINT. Make global   */
/* so it can be used inside the handler as well. */
struct sigaction act_sigint, old_sigint;

/* Flag gets set when we receive sigint. */
int got_sigint = 0;

void handle_sigint (int sig)
{
  if (sig == SIGINT)
  {
    printf("SIGINT received. Terminate readarc at user request.\n");
    got_sigint = 1;
  }
}

int block_sigint ()
{
  sigprocmask (SIG_BLOCK, &signals_sigint, NULL);

  return 0;
}

int check_sigint (int do_blocks)
{
  if (do_blocks)
  {
    /* Hope that brief span of unblocking will be enough to */
    /* catch any signal.                                    */
    sigprocmask (SIG_UNBLOCK, &signals_sigint, NULL);
    sigprocmask (SIG_BLOCK, &signals_sigint, NULL);
  }

  return got_sigint;
}

int unblock_sigint ()
{
  sigprocmask (SIG_UNBLOCK, &signals_sigint, NULL);

  return 0;
}

int set_up_sigint ()
{
  /* No SIGINT received so far. */
  got_sigint = 0;

  /* Prepare signals list with only SIGINT. */
  sigemptyset (&signals_sigint);
  sigaddset (&signals_sigint, SIGINT);

  /* Set up SIGINT handler. */
  act_sigint.sa_flags = 0;
  sigemptyset (&act_sigint.sa_mask);
  sigaddset (&act_sigint.sa_mask, SIGINT);
  act_sigint.sa_handler = handle_sigint;
  sigaction (SIGINT, &act_sigint, &old_sigint);

  return 0;
}

int clean_up_sigint ()
{
  sigaction (SIGINT, &old_sigint, 0);
}

