# bin/

The compiler: egcs-2.91.66 (egcs 1.1.2), as built by
[decompals/old-gcc](https://github.com/decompals/old-gcc). The Makefile
compiles every file with `./bin/g++ -B./bin/`, which runs the three
programs next to it:

| File      | What it is                                                              |
|-----------|-------------------------------------------------------------------------|
| `g++`     | the driver, from old-gcc 0.2 `gcc-2.91.66.tar.gz` (`mips-linux-gnu`)     |
| `cpp`     | the preprocessor, from the same archive                                 |
| `cc1plus` | the C++ compiler, from old-gcc 0.17 `gcc-2.91.66-psx.tar.gz` (`mips-sony-psx`), with the change below |
| `as`      | a script: maspsx (`tools/maspsx`), then GNU `as` (`mips-linux-gnu-as`) |

`g++`, `cpp` and `cc1plus` are static 32-bit i386 Linux executables, so
they need a system that runs 32-bit Linux programs (WSL 1 does not).
`cc1plus` is the `mips-sony-psx` build because, like the PSY-Q compiler, it
clears and copies memory with `memset()` and `memcpy()` and joins generated
names with `.`.

## The cc1plus change

Under `-fno-implement-inlines`, egcs 1.1.2 leaves out every inline member
function of a class whose key method is defined in the file, virtual ones
included. The game's compiler, like GCC 2.95, still emitted the virtual ones:
the game has the inline virtual destructors of such classes (`unk_25B68.cpp`,
`Parameter.cpp`, `Forms.cpp`) but not their other inline members. GCC 2.95's
`import_export_decl` (`cp/decl2.c`) has the test egcs 1.1.2 lacks:

    DECL_NOT_REALLY_EXTERN (decl)
      = ! (CLASSTYPE_INTERFACE_ONLY (ctype)
           || (DECL_THIS_INLINE (decl) && ! flag_implement_inlines
               && ! DECL_VINDEX (decl)));

It is added to the binary by rewriting the 27 bytes at file offset 0x45107
(`import_export_decl+0x150`, vaddr 0x808D107) of the i386 `cc1plus`:

    before  8b4508 8b405c 0fb64003 83e002 84c0 740a 8b8380000000 85c0 7407
    after   8b4508 8b505c f6420302 740f 83785800 7509 83bb8000000000 7407

that is: load the decl's lang-specific data; if it is not `DECL_THIS_INLINE`,
keep the function; if `DECL_VINDEX (decl)` (at `decl+0x58`) is set, keep it;
otherwise leave it out unless `flag_implement_inlines`.

With the bytes put back, `cc1plus` is the one in old-gcc's
`gcc-2.91.66-psx.tar.gz` (the same file in releases 0.14 to 0.17); `g++` and
`cpp` are the ones in release 0.2's `gcc-2.91.66.tar.gz`.
