#ifndef STL_PSX_STDIO_H
#define STL_PSX_STDIO_H
#include_next <stdio.h>
/* There is no stderr on the PlayStation; diagnostics go to the TTY. */
#define stderr 0
#define fprintf(stream, args...) printf(args)
#endif
