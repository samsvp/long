/**
 * A small wrapper around ncurses.
 */

#include "long.h"
#define SV_IMPLEMENTATION
#include "../../src/std/string.h"
#include <locale.h>
#include <ncurses.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct window_t {
    WINDOW* w;
} window_t;

// just a random number
static const uint32_t WINDOW_TAG = 51235;
static const char* WINDOW_NAME = "ncurses_window";

static int pair_idx = 0;

#define ASSERT_WINDOW(w_name)                                                                                 \
    window_t* w_name##_value = long_as_user(args[0], &window_vtable);                                         \
    if (w_name##_value == NULL || w_name##_value->w == NULL)                                                  \
        return long_type_error(args[0], "Window", vm);                                                        \
    WINDOW* w_name = w_name##_value->w

static bool window_eql(void* w1, void* w2)
{ return ((window_t*)w1)->w == ((window_t*)w2)->w; }

static uint32_t window_hash(void* w)
{ return (uint32_t)((uintptr_t)((window_t*)w)->w >> 2); }

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

    setlocale(LC_ALL, "");

    WINDOW* window = initscr();
    if (window == NULL)
        return long_value_oom();

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
            return long_type_error(args[i], "Number", vm);

    int x = (int)long_as_number(args[0]);
    int y = (int)long_as_number(args[1]);
    int w = (int)long_as_number(args[2]);
    int h = (int)long_as_number(args[3]);
    WINDOW* window = newwin(h, w, y, x);
    if (window == NULL)
        return long_value_oom();
    WRAP_WIN(w_window, window);

    value_t ret = long_value_user(w_window, &window_vtable, WINDOW_TAG, WINDOW_NAME, long_allocator(vm));

    return ret;
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
            return long_type_error(args[i], "Number", vm);

    value_t formated_str = long_call_name(vm, &args[3], 2, sv_str_init("sprintf"), sv_str_init(""));
    if (long_is_error(formated_str))
        return formated_str;

    wmove(w, long_as_number(args[2]), long_as_number(args[1]));

    const sv_str_t* s = long_as_str(formated_str);
    waddnstr(w, s->chars, (int)s->size);

    long_value_free(&formated_str, long_allocator(vm));

    return long_value_borrow(args[0]);
}

static value_t ntv_input(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    ASSERT_WINDOW(w);

    sv_str_builder buffer = sv_strb_init();
    const sv_allocator_t* a = long_allocator(vm);
    int c;
    while ((c = wgetch(w)), c != '\n' && c != ERR) {
        if (c > 255) continue;

        if (sv_strb_add_char(&buffer, c, a) < 0)
            goto oom;
    }

    if (sv_strb_add_char(&buffer, '\0', a) < 0)
        goto oom;

    value_t ret = long_value_str(buffer.arr, a);
    sv_strb_deinit(&buffer, a);
    return ret;
oom:
    sv_strb_deinit(&buffer, a);
    return long_value_oom();
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
        return long_type_error(args[0], "Window", vm);

    if (w->w != stdscr && w->w != NULL) {
        delwin(w->w);
        w->w = NULL;
    }

    return long_value_nil();
}

#define ATTR(attr)                                                                                            \
static value_t ntv_##attr(const value_t* args, uint8_t n, vm_t* vm)                                           \
{                                                                                                             \
    (void)args, (void) n, (void)vm;                                                                           \
    return long_value_number(attr);                                                                           \
}

ATTR(A_NORMAL)
ATTR(A_STANDOUT)
ATTR(A_UNDERLINE)
ATTR(A_REVERSE)
ATTR(A_BLINK)
ATTR(A_DIM)
ATTR(A_BOLD)
ATTR(A_PROTECT)
ATTR(A_INVIS)
ATTR(A_ALTCHARSET)
ATTR(A_CHARTEXT)

#define COLOR(attr)                                                                                           \
static value_t ntv_##attr(const value_t* args, uint8_t n, vm_t* vm)                                           \
{                                                                                                             \
    (void)args, (void) n, (void)vm;                                                                           \
    return long_value_number(attr);                                                                           \
}

COLOR(COLOR_BLACK)
COLOR(COLOR_RED)
COLOR(COLOR_GREEN)
COLOR(COLOR_YELLOW)
COLOR(COLOR_BLUE)
COLOR(COLOR_MAGENTA)
COLOR(COLOR_CYAN)
COLOR(COLOR_WHITE)

static value_t ntv_color_pair(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    for (int i = 0; i < 2; i++)
        if (!long_is_number(args[i]))
            return long_type_error(args[i], "Number", vm);

    init_pair(++pair_idx, long_as_number(args[0]), long_as_number(args[1]));

    return long_value_number(COLOR_PAIR(pair_idx));
}

static value_t ntv_wattr_on(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_number(args[1]))
        return long_type_error(args[1], "Number", vm);

    wattr_on(w, long_as_number(args[1]), NULL);
    return long_value_borrow(args[0]);
}

static value_t ntv_wattr_off(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    ASSERT_WINDOW(w);

    if (!long_is_number(args[1]))
        return long_type_error(args[1], "Number", vm);

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
    { ntv_input, 1, "input" },

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
};

static const native_module_t module = { "curses", fns, sizeof(fns) / sizeof(fns[0]) };

const native_module_t* long_module_init(void)
{
    return &module;
}
