#ifndef LONG_NATIVE_LIB_H
#define LONG_NATIVE_LIB_H

#include "long.h"
#include "error.h"
#include "std/option.h"
#include "std/vector.h"

#if defined(_WIN32)
#define NATIVE_LIB_EXT ".dll"
#else
#define NATIVE_LIB_EXT ".so"
#endif

typedef enum {
    NATIVE_LIB_ERR_OPEN = 1,
    NATIVE_LIB_ERR_NO_INIT,
    NATIVE_LIB_ERR_NO_MODULE,
} native_lib_error_kind;

/**
 * A native module loaded from a shared library.
 */
typedef struct {
    void* handle;
    native_module_t module;
} native_lib_t;

sv_vec_def(native_lib_t);

/**
 * Loads the shared library at the path and reads its module. A path without a directory is
 * looked up in the current directory. On failure, returns the error located at file and line.
 */
sv_opt_t(error_t) native_lib_open(const char* path, const char* file, int64_t line, native_lib_t* out,
                                  const sv_allocator_t*);
/**
 * Unloads the library.
 */
void native_lib_close(native_lib_t*);

#endif
