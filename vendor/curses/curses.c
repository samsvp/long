/**
 * A small wrapper around ncurses.
 */

#include "long.h"
#include <locale.h>
#define NCURSES_WIDECHAR 1
#include <ncurses.h>
#include "keys.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

typedef struct window_t {
    WINDOW* w;
} window_t;

// just a random number
static const uint32_t WINDOW_TAG = 51235;
static const char* WINDOW_NAME = "Window";

static int pair_idx = 0;

#define ASSERT_WINDOW(w_name)                                                                                 \
    window_t* w_name##_value = long_as_user(args[0], &window_vtable);                                         \
    if (w_name##_value == NULL)                                                                               \
        return long_type_error(args[0], WINDOW_NAME, vm);                                                     \
    if (w_name##_value->w == NULL)                                                                            \
        return long_error(vm, "The window was deleted");                                                      \
    WINDOW* w_name = w_name##_value->w

static bool window_eql(void* w1, void* w2)
{ return w1 == w2; }

static uint32_t window_hash(void* w)
{ return (uint32_t)((uintptr_t)w >> 4); }

static void window_free(void* ptr, const sv_allocator_t* a)
{
    window_t* w = ptr;
    if (w->w && w->w != stdscr)
        delwin(w->w);
    long_free(a, ptr);
}

static bool window_get_field(void* w, uint32_t index, value_t* out, const vm_ctx_t* ctx)
{
    (void)w, (void)index, (void)out, (void)ctx;
    return false;
}

static value_t window_to_str(void* w, const vm_ctx_t* ctx)
{
    (void)w;
    return long_value_str("<ncurses window>", long_ctx_allocator(ctx));
}

static const user_value_vtable_t window_vtable = {
    .eql_fn = window_eql,
    .free_fn = window_free,
    .hash_fn = window_hash,
    .get_field_fn = window_get_field,
    .to_string_fn = window_to_str,
};

#define WRAP_WIN(name, window)                                                                                \
    window_t* name = long_alloc(long_allocator(vm), sizeof(window_t));                                        \
    if (name == NULL) {                                                                                       \
        if (window != stdscr) delwin(window);                                                                 \
        return long_value_oom();                                                                              \
    }                                                                                                         \
    *name = (window_t){ .w = window }

static value_t ntv_init(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 0);
    (void)args;
    pair_idx = 0;

    setlocale(LC_CTYPE, "");

    WINDOW* window = initscr();
    if (window == NULL)
        return long_error(vm, "Cannot initialize the terminal");

    WRAP_WIN(w, window);

    start_color();
    return long_value_user(w, &window_vtable, WINDOW_TAG, WINDOW_NAME, long_allocator(vm));
}

static value_t ntv_end(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 0);
    (void)args, (void)vm;

    endwin();

    return long_value_nil();
}

static value_t ntv_box(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    ASSERT_WINDOW(w);

    box(w, 0, 0);
    return long_value_borrow(args[0]);
}

static value_t ntv_getdims(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    ASSERT_WINDOW(w);

    int y, x;
    getmaxyx(w, y, x);

    const value_t t[] = { long_value_number(x), long_value_number(y) };
    return long_value_tuple(t, 2, long_allocator(vm));
}

static value_t ntv_new_win(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 4);
    for (int i = 0; i < 4; i++)
        if (!long_is_number(args[i]))
            return long_type_error(args[i], "number", vm);

    int x = (int)long_as_number(args[0]);
    int y = (int)long_as_number(args[1]);
    int w = (int)long_as_number(args[2]);
    int h = (int)long_as_number(args[3]);
    WINDOW* window = newwin(h, w, y, x);
    if (window == NULL)
        return long_error(vm, "Cannot create a %d x %d window at %d, %d", w, h, x, y);
    WRAP_WIN(w_window, window);

    return long_value_user(w_window, &window_vtable, WINDOW_TAG, WINDOW_NAME, long_allocator(vm));
}

/**
 * print(window, x, y, fmt_string, values): prints the format string on the given x and y positions.
 */
static value_t ntv_wmvprint(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 5);

    ASSERT_WINDOW(w);

    for (int i = 1; i < 3; i++)
        if (!long_is_number(args[i]))
            return long_type_error(args[i], "number", vm);

    int x = (int)long_as_number(args[1]);
    int y = (int)long_as_number(args[2]);
    if (wmove(w, y, x) == ERR)
        return long_error(vm, "Position %d, %d is outside the window", x, y);

    value_t formated_str = long_call_name(vm, &args[3], 2, "sprintf", "");
    if (long_is_error(formated_str))
        return formated_str;

    const sv_str_t* s = long_as_str(formated_str);
    waddnstr(w, long_str_chars(s), (int)long_str_size(s));

    long_value_free(&formated_str, long_allocator(vm));

    return long_value_borrow(args[0]);
}

/**
 * input(window, max): reads a line of at most max characters, with the erase and kill keys, in
 * any input mode. Returns nil when no line was read: end of input, or no key in nodelay or
 * timeout mode.
 */
static value_t ntv_input(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_number(args[1]))
        return long_type_error(args[1], "number", vm);
    int max = (int)long_as_number(args[1]);
    if (max < 1)
        return long_error(vm, "Input size must be at least 1, got %d", max);

    const sv_allocator_t* a = long_allocator(vm);
    wint_t* line = long_alloc(a, ((size_t)max + 1) * sizeof(wint_t));
    char* text = long_alloc(a, (size_t)max * MB_LEN_MAX + 1);
    value_t ret = long_value_oom();
    if (line == NULL || text == NULL)
        goto done;

    ret = long_value_nil();
    if (wgetn_wstr(w, line, max) == ERR)
        goto done;

    size_t size = 0;
    mbstate_t state = { 0 };
    for (int i = 0; line[i] != 0; i++) {
        size_t k = wcrtomb(text + size, (wchar_t)line[i], &state);
        if (k != (size_t)-1)
            size += k;
    }
    text[size] = '\0';
    ret = long_value_str(text, a);

done:
    if (line != NULL)
        long_free(a, line);
    if (text != NULL)
        long_free(a, text);
    return ret;
}

static value_t ntv_refresh(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    ASSERT_WINDOW(w);

    wrefresh(w);
    return long_value_borrow(args[0]);
}

static value_t ntv_del_win(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    window_t* w = long_as_user(args[0], &window_vtable);
    if (w == NULL)
        return long_type_error(args[0], WINDOW_NAME, vm);

    if (w->w != stdscr && w->w != NULL) {
        delwin(w->w);
        w->w = NULL;
    }

    return long_value_nil();
}

static value_t ntv_keypad(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_bool(args[1]))
        return long_type_error(args[1], "bool", vm);

    keypad(w, long_as_bool(args[1]));
    return long_value_borrow(args[0]);
}

static value_t ntv_getch(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    ASSERT_WINDOW(w);

    int c = wgetch(w);
    return c == ERR ? long_value_nil() : long_value_number(c);
}

// a terminal-wide mode turned on or off by a bool
#define MODE(name, on, off)                                                                                   \
static value_t ntv_##name(const value_t* args, uint8_t n, vm_t* vm)                                           \
{                                                                                                             \
    assert(n == 1);                                                                                           \
    if (!long_is_bool(args[0]))                                                                               \
        return long_type_error(args[0], "bool", vm);                                                          \
    if (long_as_bool(args[0]))                                                                                \
        on();                                                                                                 \
    else                                                                                                      \
        off();                                                                                                \
    return long_value_nil();                                                                                  \
}

MODE(cbreak, cbreak, nocbreak)
MODE(raw, raw, noraw)
MODE(echo, echo, noecho)

static value_t ntv_nodelay(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_bool(args[1]))
        return long_type_error(args[1], "bool", vm);

    nodelay(w, long_as_bool(args[1]));
    return long_value_borrow(args[0]);
}

/**
 * timeout(window, ms): getch waits at most ms milliseconds, forever if negative.
 */
static value_t ntv_timeout(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_number(args[1]))
        return long_type_error(args[1], "number", vm);

    wtimeout(w, (int)long_as_number(args[1]));
    return long_value_borrow(args[0]);
}

/**
 * chgat(window, x, y, count, attr): changes the attributes of count characters from x, y.
 */
static value_t ntv_chgat(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 5);

    ASSERT_WINDOW(w);

    for (int i = 1; i < 5; i++)
        if (!long_is_number(args[i]))
            return long_type_error(args[i], "number", vm);

    int x = (int)long_as_number(args[1]);
    int y = (int)long_as_number(args[2]);
    attr_t attr = (attr_t)long_as_number(args[4]);
    if (mvwchgat(w, y, x, (int)long_as_number(args[3]), attr & ~A_COLOR, (short)PAIR_NUMBER(attr), NULL) == ERR)
        return long_error(vm, "Position %d, %d is outside the window", x, y);
    return long_value_borrow(args[0]);
}

static value_t ntv_clear(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    ASSERT_WINDOW(w);

    wclear(w);
    return long_value_borrow(args[0]);
}

#define CONSTANT(c)                                                                                           \
static value_t ntv_##c(const value_t* args, uint8_t n, vm_t* vm)                                              \
{                                                                                                             \
    (void)args, (void)n, (void)vm;                                                                            \
    return long_value_number(c);                                                                              \
}

CONSTANT(A_NORMAL)
CONSTANT(A_STANDOUT)
CONSTANT(A_UNDERLINE)
CONSTANT(A_REVERSE)
CONSTANT(A_BLINK)
CONSTANT(A_DIM)
CONSTANT(A_BOLD)
CONSTANT(A_PROTECT)
CONSTANT(A_INVIS)
CONSTANT(A_ALTCHARSET)
CONSTANT(A_CHARTEXT)

// defined here instead of with CONSTANT, which would get key already expanded to its number
#define KEY_NATIVE(key, name)                                                                                 \
static value_t ntv_##key(const value_t* args, uint8_t n, vm_t* vm)                                            \
{                                                                                                             \
    (void)args, (void)n, (void)vm;                                                                            \
    return long_value_number(key);                                                                            \
}
CURSES_KEYS(KEY_NATIVE)
#undef KEY_NATIVE

static value_t ntv_key_f(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    if (!long_is_number(args[0]))
        return long_type_error(args[0], "number", vm);

    return long_value_number(KEY_F((int)long_as_number(args[0])));
}

CONSTANT(COLOR_BLACK)
CONSTANT(COLOR_RED)
CONSTANT(COLOR_GREEN)
CONSTANT(COLOR_YELLOW)
CONSTANT(COLOR_BLUE)
CONSTANT(COLOR_MAGENTA)
CONSTANT(COLOR_CYAN)
CONSTANT(COLOR_WHITE)

static value_t ntv_color_pair(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    for (int i = 0; i < 2; i++)
        if (!long_is_number(args[i]))
            return long_type_error(args[i], "number", vm);

    init_pair(++pair_idx, long_as_number(args[0]), long_as_number(args[1]));

    return long_value_number(COLOR_PAIR(pair_idx));
}

static value_t ntv_wattr_on(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_number(args[1]))
        return long_type_error(args[1], "number", vm);

    wattr_on(w, long_as_number(args[1]), NULL);
    return long_value_borrow(args[0]);
}

static value_t ntv_wattr_off(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_number(args[1]))
        return long_type_error(args[1], "number", vm);

    wattr_off(w, long_as_number(args[1]), NULL);
    return long_value_borrow(args[0]);
}

static native_fn_t fns[] = {
    { ntv_init, 0, "init" },
    { ntv_end, 0, "deinit" },
    { ntv_new_win, 4, "new" },
    { ntv_del_win, 1, "del" },
    { ntv_box, 1, "box" },
    { ntv_wmvprint, 5, "print" },
    { ntv_getdims, 1, "get_dims" },
    { ntv_refresh, 1, "refresh" },
    { ntv_input, 2, "input" },
    { ntv_keypad, 2, "keypad" },
    { ntv_getch, 1, "getch" },
    { ntv_cbreak, 1, "cbreak" },
    { ntv_raw, 1, "raw" },
    { ntv_echo, 1, "echo" },
    { ntv_nodelay, 2, "nodelay" },
    { ntv_timeout, 2, "timeout" },
    { ntv_chgat, 5, "chgat" },
    { ntv_clear, 1, "clear" },

    // attrs
    { ntv_A_NORMAL, 0, "a_normal" },
    { ntv_A_STANDOUT, 0, "a_standout" },
    { ntv_A_UNDERLINE, 0, "a_underline" },
    { ntv_A_REVERSE, 0, "a_reverse" },
    { ntv_A_BLINK, 0, "a_blink" },
    { ntv_A_DIM, 0, "a_dim" },
    { ntv_A_BOLD, 0, "a_bold" },
    { ntv_A_PROTECT, 0, "a_protect" },
    { ntv_A_INVIS, 0, "a_invis" },
    { ntv_A_ALTCHARSET, 0, "a_altcharset" },
    { ntv_A_CHARTEXT, 0, "a_chartext" },

    { ntv_wattr_on, 2, "attr_on" },
    { ntv_wattr_off, 2, "attr_off" },

    // colors
    { ntv_COLOR_BLACK, 0, "black" },
    { ntv_COLOR_RED, 0, "red" },
    { ntv_COLOR_GREEN, 0, "green" },
    { ntv_COLOR_YELLOW, 0, "yellow" },
    { ntv_COLOR_BLUE, 0, "blue" },
    { ntv_COLOR_MAGENTA, 0, "magenta" },
    { ntv_COLOR_CYAN, 0, "cyan" },
    { ntv_COLOR_WHITE, 0, "white" },

    { ntv_color_pair, 2, "color_pair" },

    // keys
    { ntv_key_f, 1, "key_f" },
#define KEY_ENTRY(key, name) { ntv_##key, 0, name },
    CURSES_KEYS(KEY_ENTRY)
#undef KEY_ENTRY
};

static const native_module_t module = { "curses", fns, sizeof(fns) / sizeof(fns[0]) };

const native_module_t* long_module_init(void)
{
    return &module;
}
