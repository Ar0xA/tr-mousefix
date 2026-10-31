# Windows test plan

This checks the mouselook fix on a real Windows PC. It takes about 45 minutes. You need nothing
except the game, this repository and AutoHotkey. Write your results in the table at the end.

## 0. Preparation

1. Install the mod (see "How to install" in `README.md`).
2. Install AutoHotkey v2 from <https://www.autohotkey.com>.
3. Mouse settings (Windows Settings → Bluetooth & devices → Mouse → Additional mouse settings →
   Pointer Options):
   - turn **off** "Enhance pointer precision";
   - set the pointer speed slider to the middle (6th of 11 marks).

   Write down what they were, so you can set them back afterwards. Without this, the test moves
   are not exactly 1 pixel.
4. Write down your Windows version and your display scale (Settings → System → Display →
   Scale, for example 100 %, 125 %, 150 %).
5. Make a file `trmousefix.ini` in the game folder (next to `tabula_rasa.exe`) with:
   ```ini
   [mousefix]
   Enabled=1
   Log=0
   ```
   You change `Enabled` and `Log` between tests. The game reads the file when it starts, so
   restart the game after each change.
6. Double-click `tests\windows\mousetest.ahk`. A green "H" icon appears in the tray.

## 1. Does it load?

Start the game and log in. Close it again. Open `trmousefix.log` in the game folder.

- It must contain `mod: native module loaded`, `trmousefix: game root window 1706x960` (or
  `1280x960` for a 4:3 window) and `trmousefix 1.2.2: on`.
- If it says `not loaded` or `not hooked`, or if the file is missing: stop here and send me the
  log. Also check whether Windows Security / Smart App Control showed a warning about
  `trmousefix.pyd`.

## 2. The round-trip test

This is the main test. You repeat it in every setup of section 3.

1. Stand still somewhere with a sharp landmark (a pole, a door edge, a building corner). Put the
   crosshair exactly on it.
2. Press **F6** (slow: 240 × 1 pixel to the right). Wait for the beep.
3. Press **F7** (one jump of 240 pixels to the left).
4. Look where the crosshair is now, compared to the landmark:
   - **back on the landmark** (within about a crosshair's width): nothing was lost. ✔
   - **far left of it**: the slow moves were lost (this is what happens without the mod).
5. Do the same up and down: crosshair on the landmark, **F8** (slow down), **F9** (jump up).
6. And with larger steps: **F10** then **F7**, and **F11** then **F7**.

Do the whole test once with `Enabled=1` and once with `Enabled=0` (restart the game in between).
With `Enabled=0` you see the game's own behaviour; at most sizes the crosshair ends up clearly
left of the landmark after F6 + F7.

## 3. Setups

Set the size in the game's video options. Fullscreen uses your screen's own resolution.

| # | Mode       | Size        | Display scale |
|---|------------|-------------|---------------|
| A | windowed   | 1024 x 768  | your normal   |
| B | windowed   | 1280 x 720  | your normal   |
| C | windowed   | 1600 x 900  | your normal   |
| D | windowed   | largest that fits | your normal |
| E | fullscreen | screen size | your normal   |
| F | windowed   | 1280 x 720  | 100 %         |
| G | windowed   | 1280 x 720  | 125 %         |
| H | windowed   | 1280 x 720  | 150 %         |
| I | fullscreen | screen size | 150 % (or the highest you have) |

Skip F to I if your display only offers one scale. After F to I, set the scale back.

## 4. Window changes (in setup B)

Do the round-trip test (F6 + F7) after each of these:

1. **Move the window:** open a menu so the mouse cursor shows, drag the window by its title bar to
   another place, close the menu, test.
2. **Resize the window:** drag a window edge to make it clearly bigger, test. Then clearly
   smaller, test.
3. **Change the resolution** in the game's video options (for example to 1600 x 900), test.
4. **Switch** windowed → fullscreen → windowed in the game options, test after each switch.
5. **Alt-Tab** away while in crosshair mode, wait 5 seconds, come back. The camera must not jump.
   Test.
6. **Minimise** the game (Windows key + D), wait, restore it. The camera must not jump. Test.
7. **Zone:** travel to another map (or log out and back in), then change the resolution once
   more, then test. Open `trmousefix.log`: there must be a new `game root window` line after the
   change.

## 5. Precision check with the log (setup B and setup H)

1. Set `Log=1` in `trmousefix.ini` and restart the game.
2. Go into crosshair mode and press **F6** once.
3. Close the game and send me `trmousefix.log` (it is large; zip it).

In the log, every line reads `move at X,Y (centre CX,CY, ...)`. F6 only moves sideways, so each
`Y` must be equal to `CY`. If Y is 1 or 2 away from CY on most lines, the pointer does not land
exactly on the centre after the game re-centres it. That is the problem we look for; please write
it in the table. Version 1.2 also writes `re-centre to X,Y landed at X,Y+1` when the pointer lands
off the centre: count those lines too.

Set `Log=0` again afterwards (the log grows fast).

## 6. Normal play (10 minutes, setup E or D)

Play normally: aim, fight, open the inventory and map, click in menus, chat.

- Slow, small aiming movements should now feel smooth, with no dead zone.
- Fast turns must feel the same as before.
- Menus, inventory, map: the cursor and clicks must work as before.

If you use the Infinite Rasa client mod: do this with both mods installed.

## 7. Afterwards

- Set "Enhance pointer precision" and the pointer speed back to what you wrote down.
- Close AutoHotkey (F12, or right-click the tray icon → Exit).

## Results

Copy this table and fill it in. In the round-trip columns write: ✔ (back on the landmark),
"left" / "right" / "up" / "down" (where it ended up, and roughly how far), or "-" (not tested).

| Setup / step | F6+F7 on | F8+F9 on | F10+F7 on | F11+F7 on | F6+F7 off | F8+F9 off | Notes |
|--------------|----------|----------|-----------|-----------|-----------|-----------|-------|
| A            |          |          |           |           |           |           |       |
| B            |          |          |           |           |           |           |       |
| C            |          |          |           |           |           |           |       |
| D            |          |          |           |           |           |           |       |
| E            |          |          |           |           |           |           |       |
| F            |          |          |           |           |           |           |       |
| G            |          |          |           |           |           |           |       |
| H            |          |          |           |           |           |           |       |
| I            |          |          |           |           |           |           |       |
| 4.1 move     |          |          |           |           |           |           |       |
| 4.2 resize   |          |          |           |           |           |           |       |
| 4.3 res.     |          |          |           |           |           |           |       |
| 4.4 switch   |          |          |           |           |           |           |       |
| 4.5 alt-tab  |          |          |           |           |           |           |       |
| 4.6 minimise |          |          |           |           |           |           |       |
| 4.7 zone     |          |          |           |           |           |           |       |

Also write down: Windows version, display scale, the largest size, and anything that felt wrong
in normal play.
