#ifndef STL_PSX_NEW_H
#define STL_PSX_NEW_H
#include <stddef.h>
inline void* operator new(size_t, void* __place) { return __place; }
#endif
