/*
 * trmousefix.pyd: Tabula Rasa mouselook fix.
 *
 * The problem (measured in the client, 2026-10-30). In mouselook the game reads the mouse from
 * WM_MOUSEMOVE, turns the client pixel position into whole logical UI units, turns the camera by
 * the difference from the window centre, and puts the pointer back on the centre (SetCursorPos).
 *   - The logical UI is the game's root window: 1280 x 960 (4:3 windows) or 1706 x 960
 *     (widescreen); a pixel is root width / window width units across, root height / window
 *     height units down. Pixel p reads as round(p * root / window) units, a half down.
 *   - The difference is whole units, so what is less than a unit is lost at each re-centre.
 *   - A difference of 1 unit is ignored (a dead zone); 2 and more count (0.09 degrees a unit).
 * When the window has more pixels than units (wider than 1706 px for widescreen), slow movement
 * turns the camera late or not at all: at 3840 px a pixel is 0.44 units. With fewer pixels than
 * units (1024 x 768: 1.25 units a pixel) a slow 1-pixel move often reads as 1 unit and is lost.
 *
 * The fix. Python 2.4 imports this DLL as the module trmousefix (mod/generated/__init__.py), which
 * also tells it the game's root window size (set_virtual) once the UI exists. On import it points
 * four entries in the import table of tabula_rasa.exe (user32 PeekMessageA, GetMessageA,
 * SetCursorPos, ShowCursor) at the functions here. While the game hides the pointer and puts it
 * back on one centre point (mouselook), on each axis:
 *   - the real movement since the last message goes into a remainder, kept exactly (in 1/window
 *     of a unit, integers only);
 *   - once the remainder holds at least 1.5 units (MinUnits - 1/2), the message gets a pixel the
 *     game reads as a move of MinUnits (2) or more, close to the remainder (see axis_out), and
 *     exactly the units the game reads there come off the remainder;
 *   - below that the message gets the centre (the game turns 0), and the movement stays.
 * A release is counted from the centre plus what this mod already gave since the game last put the
 * pointer back (so a game that reads only the last message of a frame still gets all of it), and
 * where the pointer really lands after a re-centre is read back (GetCursorPos), so a landing 1 px
 * off the centre (Windows display scaling) is not taken for a mouse move.
 * Nothing else is changed: not the pointer while it is shown, not any message before the root
 * window size is known.
 *
 * Settings, optional, in trmousefix.ini in the game folder:
 *   [mousefix]
 *   Enabled=1      0 turns the fix off (the hooks then only pass the calls on)
 *   MinUnits=2     the smallest move given to the game, in units (2 or more; it ignores 1)
 *   Log=0          1 writes a line for each changed message to trmousefix.log (for tests)
 *
 * No Python headers, no import library, no C runtime: kernel32 and user32 only; the Python C API
 * functions are looked up in python24.dll at run time. Build: native/build.sh (32-bit x86).
 */
#include <windows.h>

#define VERSION "1.2.0"
#define PYTHON_API_VERSION 1012 /* Python 2.4 */
#define METH_NOARGS 0x0004
#define METH_VARARGS 0x0001

typedef void PyObject;
typedef PyObject *(*PyCFunction)(PyObject *, PyObject *);
typedef struct {
    const char *ml_name;
    PyCFunction ml_meth;
    int ml_flags;
    const char *ml_doc;
} PyMethodDef;
typedef PyObject *(*Py_InitModule4_t)(const char *, PyMethodDef *, const char *, PyObject *, int);
typedef PyObject *(*PyString_FromString_t)(const char *);
typedef int (*PyArg_ParseTuple_t)(PyObject *, const char *, ...);
typedef PyObject *(*Py_BuildValue_t)(const char *, ...);
static PyString_FromString_t pPyString_FromString;
static PyArg_ParseTuple_t pPyArg_ParseTuple;
static Py_BuildValue_t pPy_BuildValue;

static char g_log_path[MAX_PATH], g_ini_path[MAX_PATH];

/* The game folder (the folder of tabula_rasa.exe) + name. */
static void game_path(char *out, const char *name)
{
    int i, n = (int)GetModuleFileNameA(NULL, out, MAX_PATH);
    for (i = n - 1; i >= 0 && out[i] != '\\' && out[i] != '/'; i--)
        ;
    if (i < 0 || i + 1 + lstrlenA(name) >= MAX_PATH) {
        lstrcpyA(out, name);
        return;
    }
    lstrcpyA(out + i + 1, name);
}

static void log_line(const char *text)
{
    char line[1024];
    SYSTEMTIME t;
    DWORD written;
    HANDLE f = CreateFileA(g_log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    GetLocalTime(&t);
    wsprintfA(line, "%04d-%02d-%02d %02d:%02d:%02d.%03d %s\r\n", t.wYear, t.wMonth, t.wDay, t.wHour,
              t.wMinute, t.wSecond, t.wMilliseconds, text);
    WriteFile(f, line, lstrlenA(line), &written, NULL);
    CloseHandle(f);
}

/* The import table slot of function `name` from `dll` in `module`, or NULL. */
static void **import_slot(HMODULE module, const char *dll, const char *name)
{
    BYTE *base = (BYTE *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp;
    if (dir.VirtualAddress == 0)
        return NULL;
    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); imp->Name != 0; imp++) {
        IMAGE_THUNK_DATA *names, *addrs;
        if (lstrcmpiA((const char *)(base + imp->Name), dll) != 0)
            continue;
        if (imp->OriginalFirstThunk == 0)
            continue; /* no name list (bound imports only): not this exe */
        names = (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
        addrs = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData != 0; names++, addrs++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;
            if (lstrcmpA((const char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name, name) == 0)
                return (void **)&addrs->u1.Function;
        }
    }
    return NULL;
}

static int write_slot(void **slot, void *value)
{
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old))
        return 0;
    *slot = value;
    VirtualProtect(slot, sizeof(void *), old, &old);
    return 1;
}

typedef BOOL(WINAPI *SetCursorPos_t)(int, int);
typedef int(WINAPI *ShowCursor_t)(BOOL);
typedef BOOL(WINAPI *PeekMessageA_t)(LPMSG, HWND, UINT, UINT, UINT);
typedef BOOL(WINAPI *GetMessageA_t)(LPMSG, HWND, UINT, UINT);
static SetCursorPos_t real_SetCursorPos;
static ShowCursor_t real_ShowCursor;
static PeekMessageA_t real_PeekMessageA;
static GetMessageA_t real_GetMessageA;

/* Settings. */
static int g_enabled = 1, g_log_each = 0, g_min_units = 2;
static volatile LONG g_virtual_w = 0, g_virtual_h = 0;   /* the game's root window size; 0: not known yet */
static int g_hooked = 0, g_inited = 0;

/*
 * State. The hooks run on the game's window thread (the thread that pumps messages and moves the
 * pointer); set_virtual is called from Python on the same thread. No locks.
 */
static int g_cursor_count = 0;   /* ShowCursor display count: < 0 while the pointer is hidden */
static HWND g_hwnd;              /* the window the mouse messages are for */
static int g_center_known = 0;
static POINT g_center;           /* the game's centre point, client coordinates */
static POINT g_phys;             /* where the pointer really is now, client coordinates */
static int g_phys_known = 0;
/* Movement not yet given to the game, per axis, in 1/(window pixels) of a unit:
 * a pixel of movement adds the root size (a pixel is root/window units). */
static int g_rest_x, g_rest_y;
static int g_last_w = 1, g_last_h = 1; /* the window size of the last mouselook message */
/* Since the game last put the pointer back on the centre, per axis: the units this mod gave it
 * (the game reads the pointer against the centre) and the pixel it gave last. */
static int g_given_x, g_given_y, g_out_x, g_out_y;

/* Back on the centre: nothing given since. */
static void recentred(void)
{
    g_given_x = g_given_y = 0;
    g_out_x = g_center.x;
    g_out_y = g_center.y;
}

/* Start again: no remainder, nothing given. */
static void reset_axes(void)
{
    g_rest_x = g_rest_y = 0;
    recentred();
}
static volatile LONG n_changed, n_held, n_passed;

/* round(n / d) for d > 0, halves down (measured in the client at 1024 x 768: pixels 502, 506, 510,
 * 514, 518 read 627, 632, 637, 642, 647 for 627.5 ... 647.5; the same down). Negative n mirrors. */
static int div_round(int n, int d)
{
    int q, r;
    if (n < 0)
        return -div_round(-n, d);
    q = n / d;
    r = n % d;
    if (2 * r > d)
        q++;
    return q;
}

/* The units the game reads at client pixel p (p >= 0) of an axis of `pixels` pixels and `virt` units. */
static int units_at(int p, int pixels, int virt)
{
    return div_round(p * virt, pixels);
}

/* How far p * virt / pixels is from a half, in 1/pixels of a unit (0: exactly a half). */
static int half_margin(int p, int pixels, int virt)
{
    int m = 2 * ((p * virt) % pixels) - pixels;
    return m < 0 ? -m : m;
}

/*
 * One axis. `rest` is the remainder (in 1/pixels of a unit); returns the client pixel to give the
 * game and takes off `rest` exactly what the game reads there.
 *   - Less than MinUnits - 1/2 units (1.5): the game gets the centre (nothing); the movement stays.
 *   - Else the aim is the nearest whole count (at least MinUnits). Of the pixels near that place,
 *     the one is taken whose reading (at least MinUnits, same direction) is closest to the exact
 *     remainder, but never more than 1 unit past it: an overshoot that large could make the next
 *     small move turn back. Equal: the pixel furthest from a half.
 * What stays is then within about half a unit after each move (more pixels than units), and the
 * most held is 2 units, or 3 when the game reads only the last message of a frame (tests/sim).
 */
static int axis_out(int *rest, int *given, int *out, int center, int pixels, int virt, int *passed)
{
    int c0 = units_at(center, pixels, virt) + *given; /* what the game reads at the last pixel given */
    int r = *rest, lim = (2 * g_min_units - 1) * pixels;
    int want, p, mid, best = *out, best_off = 0x7fffffff, best_margin = -1, got;
    *passed = 0;
    if (2 * r < lim && 2 * r > -lim)
        return *out;
    want = div_round(r, pixels);
    if (want < g_min_units && want > -g_min_units)
        want = r > 0 ? g_min_units : -g_min_units;
    if (*given + want == 1 || *given + want == -1)
        want += want > 0 ? 1 : -1; /* a total of 1 unit from the centre is ignored: aim past it */
    mid = *out + (want * pixels) / virt;
    for (p = mid - 4; p <= mid + 4; p++) {
        int units, off, margin;
        if (p < 0 || p >= pixels)
            continue;
        units = units_at(p, pixels, virt) - c0;
        if ((want > 0 && units < g_min_units) || (want < 0 && units > -g_min_units))
            continue;
        if (*given + units == 1 || *given + units == -1)
            continue;             /* the game would read 1 unit from the centre: it ignores that */
        off = units * pixels - r; /* reading minus remainder, in 1/pixels of a unit */
        if (want < 0)
            off = -off;           /* > 0: past the movement, < 0: short of it */
        if (off > pixels)
            continue;             /* more than a unit past it */
        if (off < 0)
            off = -off;
        margin = half_margin(p, pixels, virt);
        if (off < best_off || (off == best_off && margin > best_margin)) {
            best_off = off;
            best_margin = margin;
            best = p;
        }
    }
    if (best_margin < 0)
        return *out; /* nothing suitable inside the window: give nothing now */
    got = units_at(best, pixels, virt) - c0;
    *rest -= got * pixels;
    *given += got;
    *out = best;
    *passed = got;
    return best;
}

static int in_mouselook(HWND hwnd)
{
    return g_enabled && g_virtual_w > 0 && g_virtual_h > 0 && g_cursor_count < 0 && g_center_known &&
           g_phys_known && hwnd == g_hwnd;
}

static void fix_move(MSG *msg)
{
    RECT rc;
    int w, h, x, y, vw = (int)g_virtual_w, vh = (int)g_virtual_h;
    int out_x, out_y, px = 0, py = 0;
    char line[512];

    x = (short)LOWORD(msg->lParam);
    y = (short)HIWORD(msg->lParam);
    if (!GetClientRect(msg->hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) {
        g_phys.x = x;
        g_phys.y = y;
        return;
    }
    w = rc.right;
    h = rc.bottom;
    if (w != g_last_w || h != g_last_h) {
        /* A new window size: the remainder is in 1/(old size) units. Start again. */
        reset_axes();
        g_last_w = w;
        g_last_h = h;
    }

    /* Real movement since the pointer was last known, into the remainder. */
    g_rest_x += (x - g_phys.x) * vw;
    g_rest_y += (y - g_phys.y) * vh;
    g_phys.x = x;
    g_phys.y = y;
    /* More than half the window at once is not a mouse move (a window change): start again. */
    if (g_rest_x > w / 2 * vw || g_rest_x < -(w / 2 * vw) || g_rest_y > h / 2 * vh || g_rest_y < -(h / 2 * vh)) {
        reset_axes();
        return;
    }

    out_x = axis_out(&g_rest_x, &g_given_x, &g_out_x, g_center.x, w, vw, &px);
    out_y = axis_out(&g_rest_y, &g_given_y, &g_out_y, g_center.y, h, vh, &py);

    if (px == 0 && py == 0)
        InterlockedIncrement(&n_held);
    else
        InterlockedIncrement(&n_passed);
    if (out_x != x || out_y != y) {
        POINT screen;
        InterlockedIncrement(&n_changed);
        msg->lParam = MAKELPARAM((WORD)(short)out_x, (WORD)(short)out_y);
        screen.x = out_x;
        screen.y = out_y;
        if (ClientToScreen(msg->hwnd, &screen))
            msg->pt = screen;
    }
    if (g_log_each) {
        wsprintfA(line, "move at %d,%d (centre %d,%d, window %dx%d, root %dx%d): units %d,%d -> %d,%d, rest %d/%d, %d/%d",
                  x, y, g_center.x, g_center.y, w, h, vw, vh, px, py, out_x, out_y, g_rest_x, w, g_rest_y, h);
        log_line(line);
    }
}

/* A mouse message the game takes off the queue. */
static void on_mouse_message(MSG *msg)
{
    if (msg->message != WM_MOUSEMOVE)
        return;
    if (g_hwnd != msg->hwnd) {
        g_hwnd = msg->hwnd;
        g_center_known = g_phys_known = 0;
        reset_axes();
    }
    if (in_mouselook(msg->hwnd)) {
        fix_move(msg);
    } else {
        g_phys.x = (short)LOWORD(msg->lParam);
        g_phys.y = (short)HIWORD(msg->lParam);
        g_phys_known = 1;
    }
}

static BOOL WINAPI my_PeekMessageA(LPMSG msg, HWND hwnd, UINT lo, UINT hi, UINT remove)
{
    BOOL got = real_PeekMessageA(msg, hwnd, lo, hi, remove);
    if (got && msg && (remove & PM_REMOVE))
        on_mouse_message(msg);
    return got;
}

static BOOL WINAPI my_GetMessageA(LPMSG msg, HWND hwnd, UINT lo, UINT hi)
{
    BOOL got = real_GetMessageA(msg, hwnd, lo, hi);
    if (got > 0 && msg)
        on_mouse_message(msg);
    return got;
}

static BOOL WINAPI my_SetCursorPos(int x, int y)
{
    BOOL ok = real_SetCursorPos(x, y);
    if (ok && g_hwnd != NULL) {
        POINT p, at;
        p.x = x;
        p.y = y;
        if (ScreenToClient(g_hwnd, &p)) {
            if (!g_center_known || p.x != g_center.x || p.y != g_center.y) {
                /* A new centre (window moved or resized): the old remainder is meaningless. */
                g_center = p;
                g_center_known = 1;
                reset_axes();
            }
            recentred();
            /* Where the pointer really is now: with display scaling it can land a pixel off the
             * centre asked for, and that pixel is no mouse movement. More than a pixel off is a real
             * move that came in between: that stays counted (the centre asked for). */
            if (GetCursorPos(&at) && ScreenToClient(g_hwnd, &at) && at.x - p.x <= 1 && p.x - at.x <= 1 &&
                at.y - p.y <= 1 && p.y - at.y <= 1) {
                if ((at.x != p.x || at.y != p.y) && g_log_each) {
                    char line[128];
                    wsprintfA(line, "re-centre to %d,%d landed at %d,%d", p.x, p.y, at.x, at.y);
                    log_line(line);
                }
                g_phys = at;
            } else {
                g_phys = p;
            }
            g_phys_known = 1;
        }
    }
    return ok;
}

static int WINAPI my_ShowCursor(BOOL show)
{
    int count = real_ShowCursor(show);
    if ((count < 0) != (g_cursor_count < 0)) {
        /* Into or out of a hidden pointer: no remainder, and a centre only once the game sets one
         * (a hidden pointer without re-centring, a loading screen say, is not mouselook). */
        reset_axes();
        g_center_known = 0;
    }
    g_cursor_count = count;
    return count;
}

static char g_stats[512];

static void format_stats(char *out)
{
    wsprintfA(out, "trmousefix %s: %s, root %ldx%ld, min %d units; mouselook messages: %ld changed, %ld held, %ld passed",
              VERSION, !g_hooked ? "not hooked" : g_enabled ? "on" : "off (Enabled=0)", g_virtual_w, g_virtual_h,
              g_min_units, n_changed, n_held, n_passed);
}

static DWORD WINAPI stats_thread(LPVOID unused)
{
    char now[512], previous[512];
    (void)unused;
    previous[0] = '\0';
    for (;;) {
        Sleep(30000);
        format_stats(now);
        if (lstrcmpA(previous, now) != 0) {
            log_line(now);
            lstrcpyA(previous, now);
        }
    }
    return 0;
}

static PyObject *py_stats(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    format_stats(g_stats);
    return pPyString_FromString(g_stats);
}

/* held(): (rest x, window width, rest y, window height): the movement not yet given to the game is
 * rest x / window width units across (and the same down). For tests. */
static PyObject *py_held(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    return pPy_BuildValue("(iiii)", g_rest_x, g_last_w, g_rest_y, g_last_h);
}

/* set_virtual(width, height): the game's root window size (GetUIManager().GetRootWindow().GetSize()). */
static PyObject *py_set_virtual(PyObject *self, PyObject *args)
{
    int w = 0, h = 0;
    char line[128];
    (void)self;
    if (!pPyArg_ParseTuple(args, "ii", &w, &h))
        return NULL;
    if (w > 0 && h > 0 && w <= 8192 && h <= 8192 && (w != g_virtual_w || h != g_virtual_h)) {
        reset_axes();
        g_virtual_w = w;
        g_virtual_h = h;
        wsprintfA(line, "trmousefix: game root window %dx%d", w, h);
        log_line(line);
    }
    return pPy_BuildValue("");
}

/* enable(0|1): turn the fix off or on while the game runs (tests; Enabled= in the ini at start). */
static PyObject *py_enable(PyObject *self, PyObject *args)
{
    int on = 1;
    (void)self;
    if (!pPyArg_ParseTuple(args, "i", &on))
        return NULL;
    reset_axes();
    g_enabled = on != 0;
    return pPy_BuildValue("");
}

static PyMethodDef methods[] = {
    {"stats", py_stats, METH_NOARGS, "Settings and counts of the mouselook fix, as one string."},
    {"enable", py_enable, METH_VARARGS, "enable(0|1): the fix off or on (tests)."},
    {"held", py_held, METH_NOARGS, "held(): (rest x, window width, rest y, window height), for tests."},
    {"set_virtual", py_set_virtual, METH_VARARGS, "set_virtual(width, height): the game's root window size."},
    {NULL, NULL, 0, NULL},
};

struct hook {
    const char *name;
    void *mine;
    void **real;
    void **slot;
};

__declspec(dllexport) void inittrmousefix(void)
{
    static struct hook hooks[] = {
        {"PeekMessageA", (void *)my_PeekMessageA, (void **)&real_PeekMessageA, NULL},
        {"GetMessageA", (void *)my_GetMessageA, (void **)&real_GetMessageA, NULL},
        {"SetCursorPos", (void *)my_SetCursorPos, (void **)&real_SetCursorPos, NULL},
        {"ShowCursor", (void *)my_ShowCursor, (void **)&real_ShowCursor, NULL},
    };
    int i, n = sizeof(hooks) / sizeof(hooks[0]), all = 1;
    char line[512];
    HMODULE exe = GetModuleHandleA(NULL);
    HMODULE python = GetModuleHandleA("python24.dll");
    Py_InitModule4_t pPy_InitModule4;
    HANDLE thread;

    game_path(g_log_path, "trmousefix.log");
    game_path(g_ini_path, "trmousefix.ini");
    if (python == NULL)
        return;
    pPy_InitModule4 = (Py_InitModule4_t)GetProcAddress(python, "Py_InitModule4");
    pPyString_FromString = (PyString_FromString_t)GetProcAddress(python, "PyString_FromString");
    pPyArg_ParseTuple = (PyArg_ParseTuple_t)GetProcAddress(python, "PyArg_ParseTuple");
    pPy_BuildValue = (Py_BuildValue_t)GetProcAddress(python, "Py_BuildValue");
    if (!pPy_InitModule4 || !pPyString_FromString || !pPyArg_ParseTuple || !pPy_BuildValue)
        return;
    if (g_inited) {
        /* A second import (only after a Python restart): the hooks are in place already. */
        pPy_InitModule4("trmousefix", methods, "Tabula Rasa mouselook fix " VERSION ".", NULL, PYTHON_API_VERSION);
        return;
    }
    g_inited = 1;

    g_enabled = GetPrivateProfileIntA("mousefix", "Enabled", 1, g_ini_path) != 0;
    g_log_each = GetPrivateProfileIntA("mousefix", "Log", 0, g_ini_path) != 0;
    g_min_units = GetPrivateProfileIntA("mousefix", "MinUnits", 2, g_ini_path);
    if (g_min_units < 2)
        g_min_units = 2; /* the game ignores a 1-unit move */
    if (g_min_units > 20)
        g_min_units = 20;

    /* Find every slot first; the real functions are stored before any slot points here, so a hook
     * never runs without its real function. All or none: the fix needs all four. */
    for (i = 0; i < n; i++) {
        hooks[i].slot = import_slot(exe, "user32.dll", hooks[i].name);
        if (hooks[i].slot == NULL)
            all = 0;
    }
    if (all) {
        for (i = 0; i < n; i++)
            *hooks[i].real = *hooks[i].slot;
        for (i = 0; i < n && all; i++) {
            if (!write_slot(hooks[i].slot, hooks[i].mine)) {
                int k;
                for (k = 0; k < i; k++)
                    write_slot(hooks[k].slot, *hooks[k].real);
                all = 0;
            }
        }
    }
    g_hooked = all;
    if (!all)
        log_line("trmousefix: the import table of tabula_rasa.exe could not be changed; no fix");
    format_stats(line);
    log_line(line);

    thread = CreateThread(NULL, 0, stats_thread, NULL, 0, NULL);
    if (thread != NULL)
        CloseHandle(thread);
    pPy_InitModule4("trmousefix", methods, "Tabula Rasa mouselook fix " VERSION ".", NULL, PYTHON_API_VERSION);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
