/**
 * A small wrapper around ncurses.
 */

#include "../../src/long.h"
#include <ncurses.h>
#include <assert.h>
#include <stdbool.h>

bool window_eql(void*, void*);

void window_free(void*, const sv_allocator_t*);
bool window_get_field(void*, uint32_t index, value_t* out, const vm_ctx_t*);
value_t window_to_str(void*, const vm_ctx_t*);
uint32_t window_hash(void*);

static const user_value_vtable_t window_vtable = {
    .eql_fn = window_eql,
    .free_fn = window_free,
    .hash_fn = window_hash,
    .get_field_fn = window_get_field,
    .to_string_fn = window_to_str,
};

static value_t ntv_box(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    WINDOW* w = long_as_user(args[0], &window_vtable);
    if (w == NULL)
        return long_type_error(args[0], "Window", vm);

    box(w, 0, 0);
    return long_value_nil();
}

static native_fn_t fns[] = {
    { ntv_box, 1, "box" },
};

static const native_module_t module = { "curses", fns, sizeof(fns) / sizeof(fns[0]) };

const native_module_t* long_module_init(void)
{
    return &module;
}
