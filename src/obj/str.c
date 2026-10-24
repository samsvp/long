#include "str.h"
#include "../value.h"
#include "../obj.h" // IWYU pragma: keep

static sv_rc_t(obj_t) str_obj(sv_str_t* s)
{
    return (sv_rc_t(obj_t)){ .cell = (sv_rc_cell_t(obj_t)*)s };
}

static int64_t utf8_len(char lead)
{
    uint8_t b = (uint8_t)lead;
    if ((b & 0x80) == 0x00) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;
}

str_iter_t str_iter_init(sv_str_t* s)
{
    sv_rc_borrow(str_obj(s));
    return (str_iter_t){ .str = s, .i = 0 };
}

void str_iter_deinit(str_iter_t* it, const sv_allocator_t* a)
{
    sv_rc_t(obj_t) obj = str_obj(it->str);
    sv_rc_deinit(&obj, a);
}

value_t str_iter_next(str_iter_t* it, const sv_allocator_t* a)
{
    if (it->i >= it->str->size)
        return (value_t){ .kind = VALUE_NIL };

    int64_t len = utf8_len(it->str->chars[it->i]);
    sv_str_t slice = sv_str_slice(*it->str, it->i, it->i + len);
    it->i += len;
    return value_init_str(slice, a);
}

static int64_t iterable_str_len(const void* self)
{
    const str_iter_t* it = self;
    return str_count(sv_str_slice(*it->str, it->i, it->str->size));
}

static sv_opt_t(value_t) iterable_str_next(void* self, const sv_allocator_t* a)
{
    value_t c = str_iter_next(self, a);
    return IS_NIL(c) ? sv_opt_none_t(value_t) : sv_opt_some_t(value_t, c);
}

static void iterable_str_deinit(void* self, const sv_allocator_t* a)
{
    str_iter_deinit(self, a);
}

static const iterable_vtable_t str_iterable_vtable = {
    .name = "string iterator",
    .len = iterable_str_len,
    .next = iterable_str_next,
    .deinit = iterable_str_deinit,
};

iterable_t str_iterable_init(str_iter_t* it)
{
    return (iterable_t){ .vtable = &str_iterable_vtable, .self = it };
}

int64_t str_count(sv_str_t s)
{
    int64_t n = 0;
    for (int64_t i = 0; i < s.size; i += utf8_len(s.chars[i]))
        n++;
    return n;
}

/* Byte offset n unicode characters after the byte offset i, capped at the string size. */
static int64_t str_offset(sv_str_t s, int64_t i, int64_t n)
{
    for (; n > 0 && i < s.size; n--)
        i += utf8_len(s.chars[i]);
    return i < s.size ? i : s.size;
}

sv_str_t str_get(sv_str_t s, int64_t i)
{
    if (i < 0)
        i += str_count(s);
    int64_t start = i < 0 ? s.size : str_offset(s, 0, i);
    return sv_str_slice(s, start, str_offset(s, start, 1));
}

sv_str_t str_slice(sv_str_t s, int64_t from, int64_t to)
{
    if (from < 0 || to < 0) {
        int64_t n = str_count(s);
        from = from < 0 ? from + n : from;
        to = to < 0 ? to + n : to;
    }
    if (from < 0)
        from = 0;
    if (to <= from)
        return sv_str_slice(s, 0, 0);
    int64_t start = str_offset(s, 0, from);
    return sv_str_slice(s, start, str_offset(s, start, to - from));
}
