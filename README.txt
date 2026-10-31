TABULA RASA MOUSELOOK FIX
=========================

Small, slow mouse movements in Tabula Rasa often don't turn the camera at all,
especially at high resolutions. This mod fixes that, at any resolution,
windowed or fullscreen. You don't need to compile anything.


HOW TO INSTALL (WINDOWS)
------------------------

1. Close the game.

2. Go to your game folder (the one with tabula_rasa.exe). Create a folder
   called "mods", and inside it a folder called "trmousefix".

3. Copy everything from the "mod" folder of this download into
   mods\trmousefix. You should end up with:

     <game folder>\mods\trmousefix\trmousefix.pyd
     <game folder>\mods\trmousefix\generated\__init__.py

4. Open tabula_rasa.cfg (in the game folder) with Notepad. Find the line
   [Data] and add this line right below it:

     AddonAMouseFix = "mods/trmousefix"

   It has to be the first line under [Data], like this:

     [Data]
     AddonAMouseFix = "mods/trmousefix"
     GameData = "data/game.zip"

   Save the file.

5. Start the game. That's it.


HOW TO INSTALL (LINUX: LUTRIS, STEAM/PROTON, WINE)
--------------------------------------------------

Run this in the downloaded folder:

  ./install.sh "/path/to/Tabula Rasa 1.16.5.0"

Or follow the Windows steps 2 to 5 by hand.


UNINSTALL
---------

Windows: delete the mods\trmousefix folder and remove the AddonAMouseFix line
from tabula_rasa.cfg.

Linux: ./install.sh "/path/to/Tabula Rasa 1.16.5.0" uninstall


WITH THE INFINITE RASA CLIENT MOD
---------------------------------

That works fine. Just make sure the AddonAMouseFix line stays the first line
under [Data], above the Infinite Rasa line. If you update the Infinite Rasa
mod, check this again afterwards.


HOW DO I KNOW IT WORKS?
-----------------------

After starting the game, there's a file trmousefix.log in your game folder.
It should contain lines like:

  mod: native module loaded from ...\mods\trmousefix
  trmousefix: game root window 1706x960

If the game is under C:\Program Files, Windows may put the file in
%LOCALAPPDATA%\VirtualStore\Program Files (x86)\<game folder>\ instead.

KNOWN LIMITS
------------

- Display scaling (Windows 125 % / 150 %) and some Wine/Proton starts: if the
  pointer lands a pixel off the screen centre after a move, the camera could
  creep slowly up or down. Version 1.2.0 reads back where the pointer landed,
  which covers Windows; Wine may still show it. If the camera creeps while you
  move the mouse only sideways, set Enabled=0 in trmousefix.ini and report it.
- Not yet tested on Windows itself.


---------------------------------------------------------------------------
MORE DETAIL
---------------------------------------------------------------------------

Why the mouse feels bad
-----------------------

The game turns the mouse position into whole "UI units". Its UI is always
1280 x 960 or 1706 x 960 units, whatever your resolution, so at 3840 pixels
wide one pixel is only 0.44 units. After every mouse movement the game turns
the camera by whole units, throws away the rest, and puts the mouse back in the
centre. A move of exactly 1 unit is ignored on top of that. So slow mouse
movement, one pixel at a time, barely turns the camera: about half the correct
amount at 1024 x 768, and almost nothing (2-4%) at 3840 x 1600.

What the mod does
-----------------

While you're in mouselook, the mod keeps track of every bit of mouse movement,
including the parts smaller than a unit. Once it adds up to 1.5 units or more,
it hands the game a movement the game won't ignore, and keeps the exact rest
for later. Nothing gets lost and nothing gets counted twice. When the mouse
cursor is visible (menus, inventory, chat), the mod does nothing. No game files
are changed.

Settings (optional)
-------------------

Create a file trmousefix.ini in the game folder:

  [mousefix]
  Enabled=1     ; 0 turns the fix off
  MinUnits=2    ; smallest move handed to the game (2 or more)
  Log=0         ; 1 logs every changed mouse movement (testing only)

See README.md for the measurements, test results and how to build it yourself.
