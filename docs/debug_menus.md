# The debug menus

The game, `MAIN_G.EXE`, still has its developers' debug menus, but nothing in
it opens them. This branch adds a way in: built with `make game DEBUG_MENU=1`,
the select button opens the menu that gives the first controller another
name, and with it the debug menus.

## Trying it

```sh
make game DEBUG_MENU=1
# on a copy of your disc image:
python3 tools/mod/psxdisc.py patch "copy of Track 1.bin" MAIN_G.EXE build/main_g/MAIN_G.EXE
```

This is not the original game: `make game-check DEBUG_MENU=1` fails. Without
`DEBUG_MENU`, `make game` and `make game-check` build `InputControl.cpp`
again, and the check passes. The code that opens the menu is
shorter than the game's code it replaces, and the rest is filled, so that
nothing else in the executable moves (the game does not run if it does).

## How the menus are reached

The game names its controllers (`InputSys`): both are called "Play" (the
debug text shows names in capitals), and it plays with the first one so
named. Each debug menu listens to a controller
of its own name. The menu that renames the first controller
(`InputControl`) shows

```
ESCAPE
  CONTROL = PLAY
```

Up and down go through the names, select takes the one shown. While the
first controller has a menu's name, the game listens to the second one
(still "Play"), so a second controller can play on; with one controller,
change the settings, then choose "PLAY" again: they stay as they are.

The names appear as the game first asks for them: MAIN, PLAY, SCRIPT and
TITLE from the start, BARABARA once a course has begun.

## TITLE: the menus' screens (Title.cpp)

Up and down choose a line, left and right change it. The cursor starts below
the list: press up first.

| Line | What it does |
|------|--------------|
| `Subline` | was to set how many times the menus are drawn; it is read once at start-up, so changing it does nothing |
| `Offset` | more jitter for the menus' lines (less below 0) |
| `title` | goes to one of the screens: OFF, MAIN, PSJ MODE SELECT, UNKNOWN MODE SELECT, MODE SPEED SELECT, CD CHANGE, TRK SELECT, ALERT VIB CD IN, ALERT AUDIO CD IN, CD CHECK, OPTION, PSJ HISCORE, UNKNOWN HISCORE, CLEAR, AGAIN, AGAIN SELECT, DONE; outside the game's own way through them, so the results show whatever they hold |
| `exit` | names the first controller "Play" again |

## SCRIPT: the road (AnalysisScript.cpp)

The same controls; left or right sends an effect. While a track plays and
`Manual?` is off, two numbers are shown above the menu: the seconds into the
track, and the second of the script it is at.

| Line | What it does |
|------|--------------|
| `Manual?` | the road's parts come from the lines below rather than from the script |
| `Road Part Period` | the length of a part, in seconds (0.5) |
| `Select Period` | how long the music is listened to for the place of the next part, in seconds (0.75) |
| `Selector Type` | none (no parts), pattern (one of the script's sequences of roads) or distribution (roads at random, with the script's odds) |
| `Pattern Index`, `Distribution Index` | which of the script's patterns, or distributions |
| `Effect Parameter` | for the tempo effect, its pattern: 2, parts 1.25 or 0.5 times as long, at random; 3, 1.25, 1 / 1.5 or 0.5 times, at random; 22, 0.5 and 1.25 times in turn; any other, 1 / 1.5, 0.5 and 1.25 times in turn. For the camera, loops × 10 + the camera (1 and 2 behind, 3 and 4 at the side, 5 to 7 in front) |
| `Effect Duration` | how long an effect lasts, in seconds (20) |
| `Send Random Effect` | the tempo effect, with the pattern above; it changes the parts only while `Manual?` is set (the game ignores the effect it sends) |
| `Send Reverse Effect` | sends the reverse effect, which the game ignores |
| `Send Screw Effect` | the road's pieces lean as the camera passes them (not on the speed courses) |
| `Send Camera Effect` | makes the camera above the next one: the game turns to it at its next change of camera |

## BARABARA: the look of a course (SceneDraw.cpp)

Up and down choose a line, circle adds, square takes away; held, they repeat,
faster and faster.

| Line | What it does |
|------|--------------|
| `sublines` | times Vibri is drawn each frame, each time jittered anew (1; 0: not at all) |
| `deviation` | nothing reads it |
| `bb mode` | set back to 0 at once |
| `blur on`, `blur` | trails: the picture on display is drawn under the next one, at `blur` percent of its brightness. At 50 (the default) it is copied under a half-transparent black box; at any other value it is drawn from frame buffer 0, which holds the picture on display only every other frame |
| `markers on` | the two markers of `ROAD/MARK.TMD`: one at Vibri, one at each piece of the road and at its two ends |

## MAIN

Not a menu: the CD's track and its number of tracks, `track =   5 /12`.
