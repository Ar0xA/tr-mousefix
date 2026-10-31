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
trmousefix 1.1.0: on, root 1706x960, min 2 units; mouselook messages: 120 changed, 40 held, 80 passed
```

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

Nothing gets lost and nothing gets counted twice. At most 2 units (0.18 degrees) are ever waiting
to be passed on. The Python part reads the game's UI size and tells the native part, also when you
change resolution. When the mouse cursor is visible (menus, inventory, chat), the mod does nothing.
No game files are changed: the mod only redirects the game's calls to `PeekMessageA`,
`GetMessageA`, `SetCursorPos` and `ShowCursor` in memory while the game runs.

## Results

Real client, mouse moved 240 px sideways and 120 px up/down in steps of 1, 2, 3, 4 and 8 pixels.
How much the camera turned compared to the correct amount, with the mod:

RESULTS_TABLE

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
The math was also checked in a simulation of the game's measured behaviour for 27 window sizes,
from 640 x 480 up to 7680 x 4320. It hasn't been tested on Windows yet: the Windows test plan is
in [tests/WINDOWS-TESTPLAN.md](tests/WINDOWS-TESTPLAN.md) (with an AutoHotkey script for the test
moves in `tests/windows/mousetest.ahk`).
