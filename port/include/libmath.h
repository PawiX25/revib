/* PSY-Q's libmath.h, without the declarations C's stdlib.h already has */
#define atof psyq_atof
#define strtod psyq_strtod
#include_next <libmath.h>
#undef atof
#undef strtod
