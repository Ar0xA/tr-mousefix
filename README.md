# Tabula Rasa mouselook fix

Small, slow mouse movements in Tabula Rasa often don't turn the camera at all, especially at high
resolutions. This mod fixes that, at any resolution, windowed or fullscreen.

You don't need to compile anything: the ready-made file `mod/trmousefix.pyd` is included.

## How to install

### Windows

1. Close the game.
2. Download this repository (green **Code** button → **Download ZIP**) and unzip it.
3. Go to your game folder (the one with `tabula_rasa.exe`). Create a folder called `mods`, and
   inside it a folder called `trmousefix`.
4. Copy everything from the `mod` folder of the download into `mods\trmousefix`. You should end up
   with:
   ```
   <game folder>\mods\trmousefix\trmousefix.pyd
   <game folder>\mods\trmousefix\generated\__init__.py
   ```
5. Open `tabula_rasa.cfg` (in the game folder) with Notepad. Find the line `[Data]` and add this
   line right below it:
   ```
   AddonAMouseFix = "mods/trmousefix"
   ```
   It has to be the first line under `[Data]`, like this:
   ```
   [Data]
   AddonAMouseFix = "mods/trmousefix"
   GameData = "data/game.zip"
   ```
   Save the file.
6. Start the game. That's it.

### Linux (Lutris, Steam/Proton, Wine)

```sh
git clone <this repository> tr-mousefix
cd tr-mousefix
./install.sh "/path/to/Tabula Rasa 1.16.5.0"
```

Or follow the Windows steps 3 to 6 by hand.

### Uninstall

- Windows: delete the `mods\trmousefix` folder and remove the `AddonAMouseFix` line from
  `tabula_rasa.cfg`.
- Linux: `./install.sh "/path/to/Tabula Rasa 1.16.5.0" uninstall`

### Using it together with the Infinite Rasa client mod

That works fine. Just make sure the `AddonAMouseFix` line stays the first line under `[Data]`,
above the Infinite Rasa line. If you update the Infinite Rasa mod, check this again afterwards.

### How do I know it works?

After starting the game, there's a file `trmousefix.log` in your game folder with lines like:

```
mod: native module loaded from ...\mods\trmousefix
mod: UI size updates registered
trmousefix: game root window 1706x960
trmousefix 1.2.1: on, root 1706x960, min 2 units; mouselook messages: 120 changed, 40 held, 80 passed
```

If the game is installed under `C:\Program Files` (or `Program Files (x86)`), Windows may
redirect the file: look in `%LOCALAPPDATA%\VirtualStore\Program Files (x86)\<game folder>\trmousefix.log`.

### Known limits

- **Display scaling (Windows 125 % / 150 %) and some Wine/Proton starts:** the game puts the pointer
  back on the window centre after each move. If the pointer lands a pixel off that centre, the mod
  could take that pixel for a mouse move, and the camera would creep slowly up or down. Since
  version 1.2 the mod reads back where the pointer really landed, which should cover the Windows
  case.
  Under Wine the read-back may not see the offset. If the camera creeps while you only move the
  mouse sideways, set `Enabled=0` in `trmousefix.ini` (see Settings) and please report it.
- Not yet tested on Windows itself (see How it was tested).

---

## Why the mouse feels bad

In mouselook (the normal crosshair mode), slow mouse movement turns the camera late, in jumps, or
not at all, and it gets worse the higher your resolution. Fast movement is fine.

Here's what the game does (measured in the client):

1. It reads the mouse position from Windows and converts it into whole **UI units**. The game's UI
   is always 1280 × 960 units (4:3 windows) or 1706 × 960 units (wide windows), whatever your
   resolution. So at 3840 pixels wide, one pixel is only 0.44 units; at 1024 pixels it is 1.25
   units.
2. It turns the camera by how far the mouse moved from the centre of the window, in whole units
   (0.09 degrees per unit), and then puts the mouse back in the centre. Anything less than a whole
   unit is thrown away.
3. On top of that, a move of exactly 1 unit is ignored (a dead zone).

So if you move the mouse slowly, one pixel at a time, the camera barely moves. We measured this by
moving the mouse 240 pixels in 1-pixel steps, without the mod:

| Window      | How much the camera turned (100% = correct) |
|-------------|---------------------------------------------|
| 1024 x 768  | 43%  |
| 1280 x 720  | 57%  |
| 1600 x 900  | 48%  |
| 3840 x 1600 | 2-4% |

## What the mod does

The mod is a small native module (`trmousefix.pyd`, a 32-bit Windows DLL that the game's built-in
Python loads) plus a small Python start file. Only while you're in mouselook, it looks at each
mouse movement before the game sees it:

- it keeps track of every bit of movement, exactly, including the parts smaller than a unit;
- once that adds up to 1.5 units or more, it hands the game a position worth 2 units or more (the
  smallest turn the game accepts), and subtracts exactly what the game will see;
- until then, it tells the game "no movement yet" and keeps the rest for the next time.

Nothing gets lost and nothing gets counted twice. At most 2 units (0.18 degrees) wait to be passed
on. The Python part reads the game's UI size and tells the native part, also when you
change resolution. When the mouse cursor is visible (menus, inventory, chat), the mod does nothing.
No game files are changed: the mod only redirects the game's calls to `PeekMessageA`,
`GetMessageA`, `SetCursorPos` and `ShowCursor` in memory while the game runs.

## Results

Real client, mouse moved 240 px sideways and 120 px up/down in steps of 1, 2, 3, 4 and 8 pixels.
How much the camera turned compared to the correct amount, with the mod:

| Window | sideways: 8 / 4 / 3 / 2 / 1 px steps | up/down: 8 / 4 / 3 / 2 / 1 px steps |
|---|---|---|
| 800x600 | 97 / 98 / 99 / 99 / 99 % | 100 / 92 / 100 / 98 / 100 % |
| 1024x768 | 100 / 100 / 98 / 99 / 100 % | 100 / 100 / 97 / 100 / 101 % |
| 1280x720 | 100 / 98 / 100 / 100 / 100 % | 100 / 100 / 100 / 100 / 99 % |
| 1600x900 | 99 / 99 / 100 / 99 / 100 % | all 100 % |
| 1800x1000 | 100 / 100 / 100 / 98 / 101 % | 100 / 100 / 100 / 100 / 99 % |
| 1920x1080 | 100 / 98 / 99 / 100 / 100 % | 99 / 99 / 99 / 99 / 98 % |
| 2560x1080 | 99 / 99 / 100 / 99 / 100 % | 99 / 99 / 99 / 97 / 98 % |
| 2560x1440 | 99 / 99 / 100 / 99 / 100 % | all 100 % |
| 3440x1440 | 97 / 100 / 100 / 99 / 100 % | all 100 % |
| 3840x1600 windowed | 99 / 100 / 100 / 99 / 101 % | all 100 % |
| 3840x1600 fullscreen | 99 / 100 / 99 / 100 / 99 % | 93 / 98.5 / 98.5 / 98.5 / 97 % |

Without the mod, 1-pixel steps gave 0-57 % at most of these sizes. These numbers come from
version 1.0 under Proton on Linux; 1.1 changed how small amounts are released (1.2 keeps that rule;
see How it was tested).

## Settings (optional)

You can create a file `trmousefix.ini` in the game folder (next to `tabula_rasa.exe`):

```ini
[mousefix]
; 0 turns the fix off (the mod then just passes everything through)
Enabled=1
; the smallest move handed to the game, in units (2 or more; the game ignores 1)
MinUnits=2
; 1 writes a line for every mouse movement it changes (for testing only: the log grows fast)
Log=0
```

The game reads this file when it starts.

## Building it yourself (optional)

`native/build.sh` builds `mod/trmousefix.pyd` from `native/trmousefix.c` with Zig or a 32-bit
MinGW gcc:

```sh
pip install ziglang
ZIG="python -m ziglang" native/build.sh      # or: CC=i686-w64-mingw32-gcc native/build.sh
```

The DLL has to be 32-bit, like `tabula_rasa.exe` and `python24.dll`. It doesn't need a C runtime:
it only uses kernel32 and user32, and looks up the Python functions in `python24.dll` when it
loads.

## How it was tested

With the real client under Proton on Linux, in windows from 800 x 600 up to 3840 x 1600 and in
fullscreen. The mouse was moved with `xdotool`, and the camera turn was read back from the client.
The math was also checked in a simulation of the game's measured behaviour (see below). It hasn't been tested on Windows yet: the Windows test plan is
in [tests/WINDOWS-TESTPLAN.md](tests/WINDOWS-TESTPLAN.md) (with an AutoHotkey script for the test
moves in `tests/windows/mousetest.ahk`).

Fast movement: the game turns by each mouse message's distance from the screen centre, also when
several messages arrive before it puts the pointer back (measured: two messages 384 and 400 px from
the centre turned the camera 784 px). The mod gives each message the centre plus only that
message's own share, so a fast flick or a 1000 Hz mouse turns the right amount. A simulation of
the release math (9 window sizes, 1 to 8 messages a frame, steps of 1 to 8 pixels) keeps the total
turn within 2.5 units of the real movement and never holds back more than 2 units. Run it with
`tests/sim/run.sh` (needs gcc and python3).
