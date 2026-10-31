#!/usr/bin/env bash
# Simulates the mod's release math (div_round ... axis_out, taken from native/trmousefix.c as it is)
# against two ways the game could read the mouse:
#   model 0: the game turns on every mouse message and puts the pointer back on the centre after it;
#   model 1: several messages come in one frame, the game reads only the last one, then re-centres.
# Window sizes 800 ... 3840 wide (root 1280 / 1706) and 720 ... 1600 high (root 960), 1 to 8
# messages a frame, random steps of up to 1 ... 8 pixels. Fails if the total turn is more than
# 2.5 units away from the real movement, or if more than 2 units (model 0) or 3 units (model 1)
# are ever held back.
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

static void run(int model, int pixels, int virt, int k, int step, int frames, unsigned seed)
{
    int center = pixels / 2, rest = 0, given = 0, out = center, passed, f, m;
    int c0 = units_at(center, pixels, virt);
    long turned = 0, real = 0, err, held, max_held = 0;
    srand(seed);
    for (f = 0; f < frames; f++) {
        int last = center;
        for (m = 0; m < k; m++) {
            int mv = (rand() % (2 * step + 1)) - step, o, u;
            if (f < frames / 2)
                mv = abs(mv); /* first half one way (slow turns), then back and forth */
            real += mv;
            rest += mv * virt;
            o = axis_out(&rest, &given, &out, center, pixels, virt, &passed);
            held = labs((long)rest) * 100 / pixels;
            if (held > max_held)
                max_held = held;
            if (model == 0) {
                u = units_at(o, pixels, virt) - c0;
                if (u >= 2 || u <= -2)
                    turned += u;
                given = 0;
                out = center;
            } else {
                last = o;
            }
        }
        if (model == 1) {
            int u = units_at(last, pixels, virt) - c0;
            if (u >= 2 || u <= -2)
                turned += u;
            given = 0;
            out = center;
        }
    }
    err = (turned * pixels - real * virt) * 100 / pixels;
    if (err > 250 || err < -250 || max_held > (model == 0 ? 200 : 300)) {
        printf("FAIL %d px / %d units, model %d, %d messages a frame, step %d: total off by %ld/100, held up to %ld/100 units\n",
               pixels, virt, model, k, step, err, max_held);
        bad++;
    }
}

int main(void)
{
    static const int sizes[][2] = {{800, 1280}, {1024, 1280}, {1280, 1706}, {1920, 1706}, {2560, 1706},
                                   {3840, 1706}, {720, 960}, {1080, 960}, {1600, 960}};
    int i, model, k, step, runs = 0;
    for (i = 0; i < 9; i++)
        for (model = 0; model < 2; model++)
            for (k = 1; k <= 8; k *= 2)
                for (step = 1; step <= 8; step *= 2) {
                    if (model == 0 && k > 1)
                        continue;
                    run(model, sizes[i][0], sizes[i][1], k, step, 4000, i * 100 + k * 10 + step);
                    runs++;
                }
    printf("%d runs, %d failed\n", runs, bad);
    return bad != 0;
}
EOF
gcc -O2 -Wall -o "$TMP/sim" "$TMP/sim.c"
"$TMP/sim"
