/*
 * Included before every file of the game in the port's build (-include).
 *
 * The game was written against SGI STL 3.2, which puts its names in the
 * global namespace; the port uses the C++ standard library instead. Its
 * headers are all included here, first: MemorySys.h defines malloc and free
 * as macros, which must not reach them.
 */
#ifndef PORT_PRELUDE_H
#define PORT_PRELUDE_H

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <new>
#include <string>
#include <vector>
#include <list>
#include <forward_list>
#include <deque>
#include <set>
#include <algorithm>
#include <utility>
#include <iterator>
#include <memory>
using namespace std;

/* the PSY-Q library's malloc.h, which the system's hides */
extern "C" {
void InitHeap3(unsigned long* heap, unsigned long size);
void* malloc3(size_t size);
void free3(void* p);
}

#endif
