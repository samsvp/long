#include "native_lib.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef const native_module_t* (*module_init_fn)(void);

#if defined(_WIN32)
static char open_err[256];

static const char* open_error(void)
{
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                             GetLastError(), 0, open_err, sizeof(open_err), NULL);
    while (n > 0 && (open_err[n - 1] == '\n' || open_err[n - 1] == '\r'))
        open_err[--n] = '\0';
    return n > 0 ? open_err : "unknown error";
}

static void unload(void* handle)
{
    FreeLibrary((HMODULE)handle);
}
#else
static const char* open_error(void)
{
    return dlerror();
}

static void unload(void* handle)
{
    dlclose(handle);
}
#endif

sv_opt_t(error_t) native_lib_open(const char* path, const char* file, int64_t line, native_lib_t* out,
                                  const sv_allocator_t* a)
{
    char local[FILENAME_MAX];
#if defined(_WIN32)
    snprintf(local, sizeof(local), ".\\%s", path);
    HMODULE handle = LoadLibraryA(strpbrk(path, "/\\") != NULL ? path : local);
    module_init_fn init = handle == NULL ? NULL
        : (module_init_fn)(void (*)(void))GetProcAddress(handle, "long_module_init");
#else
    snprintf(local, sizeof(local), "./%s", path);
    void* handle = dlopen(strchr(path, '/') != NULL ? path : local, RTLD_NOW | RTLD_LOCAL);
    void* sym = handle == NULL ? NULL : dlsym(handle, "long_module_init");
    module_init_fn init;
    memcpy(&init, &sym, sizeof(init));
#endif
    if (handle == NULL)
        return sv_opt_some_t(error_t, error_fmt(NATIVE_LIB_ERR_OPEN, file, line, a,
                                                "Cannot load native library '%s': %s", path, open_error()));
    if (init == NULL) {
        unload((void*)handle);
        return sv_opt_some_t(error_t, error_fmt(NATIVE_LIB_ERR_NO_INIT, file, line, a,
                                                "Native library '%s' has no long_module_init", path));
    }
    const native_module_t* module = init();
    if (module == NULL) {
        unload((void*)handle);
        return sv_opt_some_t(error_t, error_fmt(NATIVE_LIB_ERR_NO_MODULE, file, line, a,
                                                "Native library '%s' returned no module", path));
    }
    *out = (native_lib_t){ .handle = (void*)handle, .module = *module };
    return sv_opt_none_t(error_t);
}

void native_lib_close(native_lib_t* lib)
{
    unload(lib->handle);
}
