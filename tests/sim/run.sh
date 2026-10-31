#!/usr/bin/env bash
# Simulates the mod's release math (div_round ... axis_out, taken from native/trmousefix.c as it is)
# against the game as measured in the client (2026-10-31): the game turns by each mouse message's
# distance from the centre, also when several messages come before it puts the pointer back (a
# burst of two messages at 384 and 400 px from the centre turned 784 px). The pointer positions
# are raw, as Windows sends them: they add up until the game re-centres, after 1 to 8 messages.
# The mod's side is fix_move's: the movement since the last known position goes into the remainder.
# The game reads a move from the middle of its root window (virt / 2), also for an odd window size
# (measured: 641 and 645 wide). Window sizes 800 ... 3840 wide (root 1280 / 1706), 720 ... 1600
# high (root 960) and five odd sizes (both centre pixels next to the middle), random steps
# of up to 1 ... 8 pixels. Fails if the total turn is more than 2.5 units away from the real
# movement, or if more than 2 units are ever held back.
# Needs gcc and python3.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
python3 -I - "$HERE/../../native/trmousefix.c" "$TMP/core.c" <<'EOF'
import sys
s = open(sys.argv[1]).read()
open(sys.argv[2], 'w').write(s[s.index('static int div_round'):s.index('static int in_mouselook')])
EOF
cat > "$TMP/sim.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
static int g_min_units = 2;
#include "core.c"

static int bad = 0;

/* k: mouse messages before the game puts the pointer back on the centre. */
static void run(int pixels, int virt, int center, int k, int step, int frames, unsigned seed)
{
    int rest = 0, passed, f, m;
    int g0 = virt / 2; /* the game reads from the middle of its root window, also for an odd size */
    long turned = 0, real = 0, err, held, max_held = 0;
    srand(seed);
    for (f = 0; f < frames; f++) {
        int raw = center, phys = center; /* re-centred: the pointer and the mod's last known place */
        for (m = 0; m < k; m++) {
            int mv = (rand() % (2 * step + 1)) - step, o, u;
            if (f < frames / 2)
                mv = abs(mv); /* first half one way (slow turns), then back and forth */
            raw += mv;
            real += mv;
            rest += (raw - phys) * virt; /* fix_move */
            phys = raw;
            o = axis_out(&rest, center, pixels, virt, &passed);
            held = labs((long)rest) * 100 / pixels;
            if (held > max_held)
                max_held = held;
            u = units_at(o, pixels, virt) - g0; /* the game: this message's distance from the centre */
            if (u >= 2 || u <= -2)
                turned += u;
        }
    }
    err = (turned * pixels - real * virt) * 100 / pixels;
    if (err > 250 || err < -250 || max_held > 200) {
        printf("FAIL %d px / %d units, centre %d, %d messages a re-centre, step %d: total off by %ld/100, held up to %ld/100 units\n",
               pixels, virt, center, k, step, err, max_held);
        bad++;
    }
}

int main(void)
{
    static const int sizes[][2] = {{800, 1280}, {1024, 1280}, {1280, 1706}, {1920, 1706}, {2560, 1706},
                                   {3840, 1706}, {720, 960}, {1080, 960}, {1600, 960},
                                   {641, 1280}, {645, 1280}, {481, 960}, {1001, 1706}, {767, 960}};
    int i, c, k, step, runs = 0;
    for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++)
        for (c = 0; c <= sizes[i][0] % 2; c++) /* odd sizes: the game centres at 320 for 641, 323 for 645 */
            for (k = 1; k <= 8; k *= 2)
                for (step = 1; step <= 8; step *= 2) {
                    run(sizes[i][0], sizes[i][1], sizes[i][0] / 2 + c, k, step, 4000, i * 100 + k * 10 + step);
                    runs++;
                }
    printf("%d runs, %d failed\n", runs, bad);
    return bad != 0;
}
EOF
gcc -O2 -Wall -o "$TMP/sim" "$TMP/sim.c"
"$TMP/sim"
