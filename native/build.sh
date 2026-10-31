#!/usr/bin/env bash
# Build native/trmousefix.c into mod/trmousefix.pyd (32-bit x86 Windows DLL, like tabula_rasa.exe
# and python24.dll), without a C runtime: the DLL imports kernel32, user32 and nothing else
# (python24.dll is found at run time).
# Needs Zig (`zig` on PATH, or ZIG="python -m ziglang" after `pip install ziglang`), or an i686
# MinGW gcc: CC=i686-w64-mingw32-gcc native/build.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/../mod/trmousefix.pyd"
CFLAGS="-O2 -ffreestanding -fno-builtin -fno-stack-protector"
if [ -n "${CC:-}" ]; then
    $CC $CFLAGS -shared -s -nostdlib -Wl,-e,_DllMain@12 -o "$OUT" "$HERE/trmousefix.c" -luser32 -lkernel32
else
    ZIG=${ZIG:-zig}
    TMP="$(mktemp -d)"
    trap 'rm -rf "$TMP"' EXIT
    LIBC="$($ZIG env | sed -n 's/.*lib_dir"* *[=:] *"\([^"]*\)".*/\1/p' | head -1)/libc/mingw/lib32"
    [ -f "$LIBC/user32.def" ] || { echo "Zig's MinGW import lists not found ($LIBC)" >&2; exit 1; }
    $ZIG dlltool -m i386 -k -d "$LIBC/kernel32.def" -l "$TMP/kernel32.lib"
    $ZIG dlltool -m i386 -k -d "$LIBC/user32.def" -l "$TMP/user32.lib"
    $ZIG cc -target x86-windows-gnu $CFLAGS -c -o "$TMP/trmousefix.o" "$HERE/trmousefix.c"
    $ZIG lld-link -lldmingw /dll /entry:DllMain@12 /nodefaultlib /machine:x86 /debug:none \
        /out:"$OUT" "$TMP/trmousefix.o" "$TMP/kernel32.lib" "$TMP/user32.lib"
fi
rm -f "${OUT%.pyd}.pdb" "${OUT%.pyd}.lib"
echo "Built $OUT"
