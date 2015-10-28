/*
 * handlesig.h - handle SIGINT so we can catch a CTRL-C.
 *
 * RWO 120302
 *
 */
#ifndef ARCFILE_HANDLESIG_H_
#define ARCFILE_HANDLESIG_H_

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
/* #include <unistd.h> */

int block_sigint ();
int check_sigint (int do_blocks);
int unblock_sigint ();
int set_up_sigint ();
int clean_up_sigint ();

#endif
