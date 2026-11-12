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
static bool open_error(error_t* err, const sv_allocator_t* a)
{
    char msg[256];
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                             GetLastError(), 0, msg, sizeof(msg), NULL);
    if (n == 0)
        return error_set(err, NATIVE_LIB_ERR_OPEN, "Native library could not be loaded", a);
    while (n > 0 && (msg[n - 1] == '\n' || msg[n - 1] == '\r'))
        msg[--n] = '\0';
    return error_set(err, NATIVE_LIB_ERR_OPEN, msg, a);
}

static void unload(void* handle)
{
    FreeLibrary((HMODULE)handle);
}
#else
static bool open_error(error_t* err, const sv_allocator_t* a)
{
    return error_set(err, NATIVE_LIB_ERR_OPEN, dlerror(), a);
}

static void unload(void* handle)
{
    dlclose(handle);
}
#endif

bool native_lib_open(const char* path, native_lib_t* out, error_t* err, const sv_allocator_t* a)
{
#if defined(_WIN32)
    char local[FILENAME_MAX];
    if (strpbrk(path, "/\\") == NULL) {
        snprintf(local, sizeof(local), ".\\%s", path);
        path = local;
    }
    HMODULE handle = LoadLibraryA(path);
    if (handle == NULL)
        return open_error(err, a);
    module_init_fn init = (module_init_fn)(void (*)(void))GetProcAddress(handle, "long_module_init");
#else
    char local[FILENAME_MAX];
    if (strchr(path, '/') == NULL) {
        snprintf(local, sizeof(local), "./%s", path);
        path = local;
    }
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL)
        return open_error(err, a);
    void* sym = dlsym(handle, "long_module_init");
    module_init_fn init;
    memcpy(&init, &sym, sizeof(init));
#endif
    if (init == NULL) {
        unload((void*)handle);
        return error_set(err, NATIVE_LIB_ERR_NO_INIT, "Native library has no long_module_init", a);
    }
    const native_module_t* module = init();
    if (module == NULL) {
        unload((void*)handle);
        return error_set(err, NATIVE_LIB_ERR_NO_MODULE, "long_module_init returned no module", a);
    }
    *out = (native_lib_t){ .handle = (void*)handle, .module = *module };
    return true;
}

void native_lib_close(native_lib_t* lib)
{
    unload(lib->handle);
    lib->handle = NULL;
}
