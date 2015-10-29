#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define MAGIC_WORD *(uint32_t  *)"abcd"
#define BACKWARDS_WORD *(uint32_t *)"dcba"

int check_endianness ()
{
    uint32_t u;
    char * c;

    c = (char *)(&u);
    c[0] = 0;
    c[1] = 0;
    c[2] = 1;
    c[3] = 0;

    /* Little-endian */
    if (u == 256*256)
        return 0;

    /* Big-endian */
    else if (u == 256)
        return 1;

    /* What? */
    else return -1;
}

void swap_8 (void * s)
{
    char tmp;
    tmp = *((char *) s + 0);
    *((char *) s + 0) = *((char *) s + 7);
    *((char *) s + 7) = tmp;
    tmp = *((char *) s + 1);
    *((char *) s + 1) = *((char *) s + 6);
    *((char *) s + 6) = tmp;
    tmp = *((char *) s + 2);
    *((char *) s + 2) = *((char *) s + 5);
    *((char *) s + 5) = tmp;
    tmp = *((char *) s + 3);
    *((char *) s + 3) = *((char *) s + 4);
    *((char *) s + 4) = tmp;
}

void swap_4 (void * s)
{
    char tmp;
    tmp = *((char *) s + 0);
    *((char *) s + 0) = *((char *) s + 3);
    *((char *) s + 3) = tmp;
    tmp = *((char *) s + 1);
    *((char *) s + 1) = *((char *) s + 2);
    *((char *) s + 2) = tmp;
}

void swap_2 (void * s)
{
    char tmp;
    tmp = *((char *) s + 0);
    *((char *) s + 0) = *((char *) s + 1);
    *((char *) s + 1) = tmp;
}


