#include "error.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

static const error_t oom = {
    .msg = { .chars = "Out of memory", .size = 13 },
};

error_t error_oom(int code)
{
    error_t e = oom;
    e.error_code = code;
    return e;
}

error_t error_init(void)
{
    return (error_t){
        .error_code = 0,
        .msg = sv_str_init(""),
        .trace = sv_str_init(""),
    };
}

void error_reset(error_t* e)
{
    e->error_code = 0;
    e->msg = sv_str_init("");
    e->trace = sv_str_init("");
}

bool error_set(error_t* e, int code, const char* msg, const sv_allocator_t* a)
{
    e->error_code = code;
    e->msg = sv_str_copy(sv_str_init(msg), a);
    return false;
}

static int write_loc(char* buf, int size, const char* file, int64_t line)
{
    int at = 0;
    if (file != NULL && line > 0)
        at = snprintf(buf, size, ERROR_LOC_FMT, file, line);
    else if (file != NULL)
        at = snprintf(buf, size, "%s: ", file);
    return at >= size ? size - 1 : at;
}

error_t error_fmt(int code, const char* file, int64_t line, const sv_allocator_t* a, const char* fmt, ...)
{
    int bufsize = sizeof(char) * 256;
    char* buf = sv_malloc(a, bufsize);
    if (buf == NULL)
        return error_oom(code);

    int at = write_loc(buf, bufsize, file, line);

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + at, bufsize - at, fmt, args);
    va_end(args);
    if (n < 0)
        return (error_t){ .error_code = code, .msg = sv_str_init("") };

    sv_str_t msg = sv_str_init(buf);
    return (error_t){ .error_code = code, .msg = msg };
}

void error_free(error_t* e, const sv_allocator_t* a)
{
    sv_str_deinit(&e->trace, a);
    e->trace = sv_str_init("");
    if (e->msg.chars != oom.msg.chars)
        sv_str_deinit(&e->msg, a);
    e->msg = sv_str_init("");
}
