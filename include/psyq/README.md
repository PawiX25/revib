# PSY-Q headers

The C headers of Sony's PlayStation SDK, PSY-Q, Run-time Library Release
4.5 (each file has `$PSLibId: Run-time Library Release 4.5$`). They are
Sony's (Sony Corporation, Sony Computer Entertainment), not under a free
licence.

They come from open-ribbon's `include/psyq`, with the names of the files
and of `sys/` lower-cased, and one change:

- `stdlib.h`: `exit()` is declared `__attribute__((noreturn))`. The build
  does not use this header: `include/stl_psx/stdlib.h` is found first.

The game's files include them for the libraries' types and functions
(libgpu, libgte, libgs, libcd, libds, libspu, libsnd, libetc, libapi, ...);
`include/stl_psx` stands in for some of the C library headers.
