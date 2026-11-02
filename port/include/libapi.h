/* PSY-Q's libapi.h, without its rename(), which C's (stdio.h) clashes with */
#define rename psyq_rename
#include_next <libapi.h>
#undef rename
