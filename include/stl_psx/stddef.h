#ifndef STL_PSX_STDDEF_H
#define STL_PSX_STDDEF_H
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif
typedef int ptrdiff_t;
#ifndef NULL
#define NULL 0
#endif
#define offsetof(type, member) ((size_t) &((type*)0)->member)
#endif
