# C++ environment headers for the PlayStation build

The game is C++ and uses the SGI STL (`include/stl`). The PSY-Q C headers in
`include/psyq` predate C++ (K&R prototypes, no `ptrdiff_t`, no `wint_t`), so
the STL cannot be compiled against them directly. The headers here supply
what the STL and the game's files need, with ANSI prototypes that resolve to
the same PSY-Q functions, and are searched before `include/psyq`:

- `stddef.h`: `size_t`, `ptrdiff_t`, `NULL` and `offsetof`.
- `stdlib.h`: `exit()` (as `noreturn`), `malloc()`, `free()`, `calloc()`,
  `realloc()`, `abs()`, `atoi()`, `rand()` and `srand()`, in place of
  PSY-Q's `stdlib.h`.
- `string.h`: `memcpy()`, `memmove()`, `memset()`, `memcmp()`, `memchr()`,
  `strlen()`, `strcmp()`, `strncmp()`, `strcpy()`, `strncpy()`, `strcat()`
  and `strchr()`, in place of PSY-Q's `string.h`.
- `stdio.h`: includes PSY-Q's (`#include_next`) and sends the STL's
  `fprintf(stderr, ...)` diagnostics (the allocator's "out of memory"
  message) to `printf`, as on the console.
- `wchar.h`: `wint_t`, which `char_traits<wchar_t>` needs; the game has no
  wide strings.
- `iostream.h`: only declares the stream classes the STL names in its
  string stream helpers; the game has no iostreams.
- `new.h`: placement new.

The STL configuration itself (malloc allocator, no `<exception>`) is set with
`-D` flags in the Makefile.
