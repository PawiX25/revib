#ifndef FORMAT_H
#define FORMAT_H

#include "common.h"

#include <stdio.h>

/*
 * Number formatting functions nothing calls. Their format strings are
 * compiled into each file that includes this header, as the original
 * executables have them; the functions are reconstructed around those
 * strings.
 */
inline int FormatInt(char* buffer, int value)
{
    return sprintf(buffer, "%d", value);
}

inline int FormatUInt(char* buffer, u_int value)
{
    return sprintf(buffer, "%u", value);
}

#endif
