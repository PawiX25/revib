maspsx (MIT, see LICENSE) is vendored from mkst/maspsx 625b27b, with two
local changes, both in `maspsx/__init__.py`:

- A file's jump tables are assembled into `.subsection 1` of `.rodata`,
  after the rest of its read-only data, where ASPSX puts them (the game's
  Forms.cpp has its three jump tables after all its strings and vtables).
- Which symbols are in `.sdata` (and so addressed through `$gp`) is tracked
  more closely: what is defined between `.pushsection` and `.popsection` (the
  asm that INCLUDE_ASM and INCLUDE_RODATA bring in) is not counted, the
  section is taken from any `.section` directive rather than only
  `.section .text`, and `.extern` lines, which gcc lists at the end of the
  file, no longer end a `.sdata` section.

The test inputs in `aspsx/ASM/` have LF line endings here (CRLF upstream).
