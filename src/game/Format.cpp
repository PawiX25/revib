#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <libmath.h> /* sprintf2(), which can print floating point numbers */

#include "MemorySys.h"
#include <string>

#include "Format.h"
#include "Fixed.h"

/* sprintf2() writes into this literal, as long as the longest number */
static char* const s_digits = "000000000000000";

string ToString(float value)
{
    sprintf2(s_digits, "%.3f", value);
    return string(s_digits);
}

string ToString(int value)
{
    sprintf2(s_digits, "%d", value);
    return string(s_digits);
}

/* not called; its format string is in the original's data */
inline string ToString(long value)
{
    sprintf2(s_digits, "%ld", value);
    return string(s_digits);
}

string ToString(u_long value)
{
    sprintf2(s_digits, "%lu", value);
    return string(s_digits);
}

/* not called; its strings are in the original's data */
inline string ToString(bool value)
{
    return string(value ? "true" : "false");
}

string ToString(Fixed value)
{
    return ToString(value.Raw() * (1.0f / 65536));
}
