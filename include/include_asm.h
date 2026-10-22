#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

#ifndef PERMUTER

#ifndef INCLUDE_ASM

/*
 * Code and data from the disassembly. An included file can switch sections
 * itself (the rodata files do), so the section the compiler was in is
 * restored after it: the compiler does not know about the switch and would
 * not repeat its own directive.
 */
#define INCLUDE_ASM(FOLDER, NAME)                                              \
  __asm__(".pushsection .text\n"                                               \
          "\t.align\t2\n"                                                      \
          "\t.globl\t" #NAME "\n"                                              \
          "\t.ent\t" #NAME "\n" #NAME ":\n"                                    \
          ".include \"" FOLDER "/" #NAME ".s\"\n"                              \
          "\t.set reorder\n"                                                   \
          "\t.set at\n"                                                        \
          "\t.end\t" #NAME "\n"                                                \
          ".popsection")

#define INCLUDE_RODATA(FOLDER, NAME)                                           \
  __asm__(".pushsection .rodata\n"                                             \
          "\t.include \"" FOLDER "/" #NAME ".s\"\n"                            \
          ".popsection")

#endif

#else
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)
#endif

#endif
