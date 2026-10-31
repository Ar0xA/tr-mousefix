#!/usr/bin/env bash
# Install or remove the mouselook fix in a Tabula Rasa 1.16.5.0 game folder.
#   ./install.sh "<game folder>"            install (mods/trmousefix, named first in tabula_rasa.cfg [Data])
#   ./install.sh "<game folder>" uninstall  remove the folder and its tabula_rasa.cfg line
# The first install keeps a copy of tabula_rasa.cfg as tabula_rasa.cfg.trmousefix-backup.
# If tabula_rasa.cfg is a symlink, it is replaced by a copy, so the file it points to is not changed.
# Install this mod after other mods that also add a [Data] line (for example the Infinite Rasa
# client mod): its line must be the first one in [Data].
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="${1:?usage: install.sh <game folder> [uninstall]}"
MODDIR="$GAME/mods/trmousefix"
CFG="$GAME/tabula_rasa.cfg"
# [Data] key: first in [Data], and it sorts before the other keys (AddonInfiniteRasa, GameData).
KEY="AddonAMouseFix"

if [ ! -f "$GAME/tabula_rasa.exe" ] || [ ! -f "$GAME/trpython.zip" ]; then
    echo "Not a Tabula Rasa game folder (no tabula_rasa.exe / trpython.zip): $GAME" >&2
    exit 1
fi

edit_cfg() {   # edit_cfg add|remove
    if [ ! -e "$CFG" ]; then
        [ "$1" = remove ] && return 0
        echo "No $CFG (the retail game has one); not installed." >&2
        exit 1
    fi
    if [ -L "$CFG" ]; then
        cp --remove-destination "$(readlink -f "$CFG")" "$CFG"
    fi
    [ -e "$CFG.trmousefix-backup" ] || cp "$CFG" "$CFG.trmousefix-backup"
    python3 -I - "$CFG" "$1" "$KEY" <<'EOF'
import re, sys
path, action, key = sys.argv[1:4]
text = open(path, 'rb').read().decode('latin-1')
bom = u'\u00ef\u00bb\u00bf'  # a UTF-8 byte order mark, read as latin-1
had_bom = text.startswith(bom)
if had_bom:
    text = text[len(bom):]
nl = '\r\n' if '\r\n' in text else '\n'
# Split on line ends only (splitlines would also split on characters such as \x85 inside a value).
lines = text.replace('\r\n', '\n').split('\n')
if lines and lines[-1] == '':
    lines.pop()
lines = [l for l in lines if not re.match(r'\s*%s\s*=' % re.escape(key), l, re.I)]
if action == 'add':
    try:
        i = [l.strip().lower() for l in lines].index('[data]')
    except ValueError:
        # No [Data] section: the client then uses its coded default, data/game.zip.
        lines += ['', '[Data]', 'GameData = "data/game.zip"']
        i = len(lines) - 2
    lines.insert(i + 1, '%s = "mods/trmousefix"' % key)
open(path, 'wb').write(((bom if had_bom else '') + nl.join(lines) + nl).encode('latin-1'))
EOF
}

case "${2:-}" in
"")
    if [ ! -f "$HERE/mod/trmousefix.pyd" ]; then
        echo "mod/trmousefix.pyd is missing: build it first (native/build.sh) or use a release." >&2
        exit 1
    fi
    if [ ! -e "$CFG" ]; then
        echo "No $CFG (the retail game has one); not installed." >&2
        exit 1
    fi
    mkdir -p "$MODDIR/generated"
    rm -f "$MODDIR"/generated/*.pyo "$MODDIR"/generated/*.pyc
    cp "$HERE/mod/generated/__init__.py" "$MODDIR/generated/"
    # Remove before copying: a running game has the old DLL mapped; a new file leaves it alone.
    rm -f "$MODDIR/trmousefix.pyd"
    cp "$HERE/mod/trmousefix.pyd" "$MODDIR/"
    edit_cfg add
    echo "Installed into $MODDIR, named first in $CFG [Data]. Log: $GAME/trmousefix.log"
    ;;
uninstall)
    rm -rf "$MODDIR"
    rmdir "$GAME/mods" 2>/dev/null || true
    edit_cfg remove
    echo "Removed $MODDIR and its line in $CFG (kept: $CFG.trmousefix-backup, trmousefix.log, trmousefix.ini)"
    ;;
*)
    echo "usage: install.sh <game folder> [uninstall]" >&2
    exit 2
    ;;
esac
