#ifndef STL_PSX_STDLIB_H
#define STL_PSX_STDLIB_H
#include <stddef.h>
extern "C" {
void  exit(int) __attribute__((noreturn));
void* malloc(size_t);
void  free(void*);
void* calloc(size_t, size_t);
void* realloc(void*, size_t);
int   abs(int);
int   atoi(const char*);
int   rand(void);
void  srand(unsigned int);
}
#endif
