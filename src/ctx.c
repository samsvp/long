#include "ctx.h"

_Noreturn void ctx_fail(ctx_t* ctx, int code, int64_t line, const char* msg)
{
    ctx->err = error_fmt(code, ctx->file, line, &ctx->alloc, "%s", msg);
    longjmp(*ctx->on_error, 2);
}

_Noreturn void ctx_fail_oom(ctx_t* ctx, int code, int64_t line)
{
    error_set_oom(&ctx->err, code, ctx->file, line, &ctx->alloc);
    longjmp(*ctx->on_error, 2);
}
