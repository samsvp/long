/**
 * A small wrapper around ncurses.
 */

#include "../../src/long.h"
#include "../../src/std/string.h"
#include <ncurses.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

// just a random number
static const uint32_t WINDOW_TAG = 51235;
static const char* WINDOW_NAME = "ncurses_window";

bool window_eql(void* w1, void* w2)
{ return w1 == w2; }

uint32_t window_hash(void* w)
{ return (uint32_t)((uintptr_t)w >> 2); }

void window_free(void* w, const sv_allocator_t* a)
{
    (void)a;
    delwin(w);
}

bool window_get_field(void* w, uint32_t index, value_t* out, const vm_ctx_t* ctx)
{
    (void)w, (void)index, (void)out, (void)ctx;
    return false;
}

value_t window_to_str(void* w, const vm_ctx_t* ctx)
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

static value_t ntv_init(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 0);
    (void)args;

    WINDOW* window = initscr();
    if (window == NULL)
        return long_value_oom();

    return long_value_user(window, &window_vtable, WINDOW_TAG, WINDOW_NAME, long_allocator(vm));
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

    WINDOW* w = long_as_user(args[0], &window_vtable);
    if (w == NULL)
        return long_type_error(args[0], "Window", vm);

    box(w, 0, 0);
    return args[0];
}

static value_t ntv_getdims(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    WINDOW* w = long_as_user(args[0], &window_vtable);
    if (w == NULL)
        return long_type_error(args[0], "Window", vm);

    int y, x;
    getmaxyx(w, y, x);

    const value_t t[] = { long_value_number(x), long_value_number(y) };
    return long_value_tuple(t, 2, long_allocator(vm));
}

static value_t ntv_new_win(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 4);
    for (int i = 0; i < 4; i++)
        if (long_is_number(args[i]))
            return long_type_error(args[i], "Number", vm);

    int x = (int)long_as_number(args[0]);
    int y = (int)long_as_number(args[1]);
    int w = (int)long_as_number(args[2]);
    int h = (int)long_as_number(args[3]);
    WINDOW* window = newwin(h, w, y, x);
    if (window == NULL)
        return long_value_oom();

    return long_value_user(window, &window_vtable, WINDOW_TAG, WINDOW_NAME, long_allocator(vm));
}

/**
 * print(window, x, y, fmt_string, values): prints the format string on the given x and y positions.
 */
static value_t ntv_wmvprint(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 5);

    WINDOW* w = long_as_user(args[0], &window_vtable);
    if (w == NULL)
        return long_type_error(args[0], "Window", vm);

    for (int i = 1; i < 3; i++)
        if (long_is_number(args[i]))
            return long_type_error(args[i], "Number", vm);

    value_t formated_str = long_call_name(vm, &args[4], 1, sv_str_init("sprintf"), sv_str_init(""));
    if (long_is_error(formated_str))
        return formated_str;

    wmove(w, long_as_number(args[1]), long_as_number(args[2]));
    wprintw(w, "%s", long_str_to_c(long_as_str(formated_str), long_allocator(vm)));
    return args[0];
}

static value_t ntv_refresh(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    WINDOW* w = long_as_user(args[0], &window_vtable);
    if (w == NULL)
        return long_type_error(args[0], "Window", vm);

    wrefresh(w);
    return args[0];
}

static value_t ntv_del_win(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    WINDOW* w = long_as_user(args[0], &window_vtable);
    if (w == NULL)
        return long_type_error(args[0], "Window", vm);

    delwin(w);
    return long_value_nil();
}

static native_fn_t fns[] = {
    { ntv_init, 0, "init" },
    { ntv_end, 0, "end" },
    { ntv_new_win, 4, "new" },
    { ntv_del_win, 1, "del" },
    { ntv_box, 1, "box" },
    { ntv_wmvprint, 5, "print" },
    { ntv_getdims, 1, "get_dims" },
    { ntv_refresh, 1, "refresh" },
};

static const native_module_t module = { "curses", fns, sizeof(fns) / sizeof(fns[0]) };

const native_module_t* long_module_init(void)
{
    return &module;
}
