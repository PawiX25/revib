# Translation units

## MAIN_T.EXE

`MAIN_T.EXE` (title screen / menus) is linked from the C++ files below, in this
order. The split follows the original object files rather than arbitrary
address ranges, because the compiler's per-file output (out-of-line STL
template instances, static constructors/destructors, string literals) only
comes out right when each source file covers exactly one original object.

| text start | file | how the boundary is known |
|---|---|---|
| 0x8001D558 | VideoSys.cpp | `VideoSys.cpp` asserts; ends with STL instances for `vector<void (*)()>` and the file's static destructor/constructor |
| 0x8001E7E8 | CdSys.cpp | `CdSys.cpp` asserts; static constructor at the end |
| 0x8001F0D4 | AudioSys.cpp | `AudioSys.cpp` asserts; static destructor/constructor at the end |
| 0x800200AC | InputSys.cpp | `InputSys.cpp` asserts; STL instances for `string` and `set<string>`; static destructor/constructor |
| 0x800216FC | MemorySys.cpp | `MemorySys.cpp` asserts |
| 0x80021AC0 | FileSys.cpp | `FileSys.cpp` assert; `string` instances; static destructor/constructor |
| 0x80022EE8 | unk_22EE8.cpp | static constructor, then the inline functions it uses; name unknown (projection / clipping helpers) |
| 0x80023210 | PackedFiles.cpp | `PackedFiles.cpp` assert string; archive lookup used by `FileSys` |
| 0x800233C0 | TitleMain.cpp | `main()` ("Title main()"); name assumed. Ends with its template instances and the synthesized constructors of `GameParams`, `ShellOpen` and `DiscInfo` (the loader's `GameParams` constructor is emitted the same way) |
| 0x80023B20 | InputControl.cpp | `InputControl.cpp` asserts; `string` instances |
| 0x80024A48 | TmdLineObj.cpp | `TmdLineObj.cpp` assert; static constructor |
| 0x80025590 | unk_25590.cpp | `Movie` and `ModelMovie` (`ModelMovie.h`); ends with the out-of-line copies of the `Coord` functions its constructor calls |
| 0x80025B68 | CameraMovie.cpp | `CameraMovie`; `MAIN_G` has this file but none of the next one |
| 0x80025EBC | unk_25B68.cpp | the scenes of `Movie.h` (the title, the kiosk demo) and their elements; static constructor builds the kiosk tables, followed by the out-of-line inline functions, whose vtables close the file's `.rodata`. The kiosk version builds it with `KIOSK` |
| 0x80028F98 | unk_28A88.cpp | menu cursor and pad input (repeat, the "Play" pad); `string` instances; static constructor |
| 0x80029B0C | unk_29B0C.cpp | logo sequence (`logos/*.tim`) |
| 0x8002AA14 | Message.cpp | `Message.cpp` assert |
| 0x8002AE5C | FontHack.cpp | `FontHack.cpp` asserts |
| 0x8002B054 | unk_2B054.cpp | language / mode select menus (`LanguageSelect`); static destructor/constructor, then an inline function of the file (0x8002BF70) |

Everything from 0x8002BF78 (`__SN_ENTRY_POINT`) on is the PSY-Q SDK and
libgcc.

### Evidence

- **Static constructors/destructors.** The constructor table that `__main`
  runs, at 0x8003F91C, lists ten static constructors (`_GLOBAL_.I.` here)
  and five static destructors (`_GLOBAL_.D.`). egcs emits them after the
  rest of their file's functions and template instances; only out-of-line
  copies of inline functions come after them.
- **STL template instances.** The game was built against SGI STL 3.2
  (`include/stl`). Its out-of-line instances (`vector::_M_insert_aux`,
  `basic_string::append`, `_Rb_tree::insert_unique`, the allocator's
  `_S_oom_malloc`, ...) are emitted, as file-local copies, right after the last
  user function of each file that needs them, so a cluster of them marks the
  end of a file.
- **`.rodata` order.** String literals sit in `.rodata` in link order, so the
  `__FILE__` strings of `assert` (`C:/psj/dev/locale/game/<File>.cpp`) and the
  strings each file's code uses give the same sequence of files.
- **Header strings.** Some literals are not referenced by any code
  ("Info: status=%d, type=%d, size=%d", "Analog: Left(...)", `Movie.h`
  asserts, "MemPtr: file %s not found."). They come from inline functions in
  shared headers and appear once in every file that includes them.
- **The other executables.** `MAIN_K.EXE` and `MAIN_G.EXE` link most of these
  files too; runs of functions that stay adjacent there confirm the groupings.

The files that the asserts' `__FILE__` names keep those names. The names of
the others are assumed: CameraMovie, Font, Format, Game, GameMain, KioskMain,
Meters, NumberVoice, Parameter, RandomPicks, Receiver, Score, ScoreMeter,
SignalAnalyzer, SignalSys, SoundSequence, Title and TitleMain are named after
what they hold, and the rest `unk_<address>.cpp`. For two of those the
address, from an earlier split, is not where the file starts:
`unk_25B68.cpp` starts at 0x80025EBC and `unk_28A88.cpp` at 0x80028F98.

## MAIN_K.EXE and MAIN_G.EXE

The same sources build the other two executables: `include/common.h` defines
`TITLE` for `MAIN_T` (when none of `KIOSK`, `GAME` and `LOADER` is defined),
`make kiosk` builds `MAIN_K` with `KIOSK` and `make game` builds `MAIN_G` with
`GAME`.

`MAIN_K.EXE` (the kiosk demo) links the title's files in the same order, with
`KioskMain.cpp` in place of `TitleMain.cpp`, reduced versions of
`unk_28A88.cpp` and `unk_2B054.cpp` (no menus) and no `unk_29B0C.cpp` (the
logo sequence); `CdSys.cpp` and `AudioSys.cpp` have its CD audio functions.
Its text starts at 0x8001D364 (`VideoSys.cpp`); the PSY-Q libraries follow
from 0x800294B4 (`__SN_ENTRY_POINT`).

`MAIN_G.EXE` (the game itself) names about half of its files in its asserts.
In link order (text start; shared files in bold):

| text start | file |
|---|---|
| 0x800219E4 | FbStream.cpp (`CSpuFeedbackStream`) |
| 0x80022130 | **VideoSys.cpp** |
| 0x800233CC | **CdSys.cpp** (all its receivers) |
| 0x800241BC | **AudioSys.cpp** |
| 0x80025194 | Thread.cpp |
| 0x80025550 | **InputSys.cpp** |
| 0x80026C80 | **MemorySys.cpp** |
| 0x80027044 | **FileSys.cpp** |
| 0x8002846C | **unk_22EE8.cpp** |
| 0x80028794 | Format.cpp (`ToString`) |
| 0x80028B10 | GameMain.cpp (`main`) |
| 0x8002952C | AudioStream.cpp (the CD's sound read back from the SPU; `StaticQueue.h`) |
| 0x80029FE4 | SignalModule.cpp (onset detection) |
| 0x8002ADE4 | SignalAnalyzer.cpp |
| 0x8002AFCC | SignalSys.cpp (`StaticQueue.h`) |
| 0x8002B62C | **InputControl.cpp** |
| 0x8002C554 | Parameter.cpp (debug-menu parameters) |
| 0x8002FB94 | Game.cpp (`Game`: starts the music, its analysis and the gameplay, and updates them together) |
| 0x8002FD7C | Receiver.cpp |
| 0x80030048 | FastAllocator.cpp |
| 0x8003042C | Forms.cpp |
| 0x80035768 | **unk_28A88.cpp** (menu cursor; `LookAlongZ`) |
| 0x800362EC | NumberVoice.cpp (numbers said in Japanese; its static initializer from asm) |
| 0x8003730C | TitleDraw.cpp (the title's screens shown) |
| 0x8003F1FC | Font.cpp |
| 0x8003F894 | Title.cpp (the title's screens, one after the other) |
| 0x80040310 | AnalysisScript.cpp (the tracks' script) |
| 0x80043F2C | Gameplay.cpp (`Effect::operator=`, `Update()` and `AddEvent()` from asm) |
| 0x8004BE74 | Score.cpp |
| 0x8004C0FC | SceneDraw.cpp |
| 0x8004DBC8 | Meters.cpp |
| 0x8004E6B0 | RoadSelect.cpp |
| 0x8004EC8C | Glue.cpp (between the title's screens and the courses; `Update()` from asm) |
| 0x80050538 | **TmdLineObj.cpp** |
| 0x80051208 | **unk_25590.cpp** |
| 0x800517E0 | **CameraMovie.cpp** |
| 0x80051B34 | ScoreMeter.cpp (the score as a row of marks) |
| 0x80052280 | RandomPicks.cpp |
| 0x800524F4 | **PackedFiles.cpp** |
| 0x800526A4 | GameCamera.cpp (`CGameCamera`) |
| 0x80053788 | **Message.cpp** |
| 0x80053BD0 | **FontHack.cpp** |
| 0x80053DC8 | SoundSequence.cpp |
| 0x80053FB4 | **unk_2B054.cpp** |

The PSY-Q libraries follow from 0x8005405C (`__SN_ENTRY_POINT`).

All of these files are C++ but for the five functions marked "from asm",
which are linked from the game's assembly; their C++ is in the files, under
`NON_MATCHING`.

## SCES_028.73

The loader the CD boots is a single file, `loader.cpp` (its assert names it),
built with `LOADER` by `make loader`: it loads `main_t.exe`, `main_g.exe` or
`main_k.exe`, whichever `LoaderData::next` says, and runs it with the
`LoaderData` as its argv. Its code runs from 0x800106BC to 0x80010AF0; all
of the rest is the PSY-Q SDK (libetc, libcd, libapi and the startup code).
