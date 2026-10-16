// SPDX-License-Identifier: MIT
// Мост AnyPS5 <-> нативные библиотеки (libaps5bridge.so, без нативного аналога).
// x86-заглушки AnyPS5 (libSceAgcDriver.prx и т.п.) загружают нативную aarch64-сборку библиотеки
// через aps5b_load/aps5b_sym и вызывают её функции через aps5b_call (до 8 целых аргументов,
// x0..x7 на aarch64). Нативный код вызывает x86-функции через указатель от aps5b_x86caller.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _GNU_SOURCE
#include <dlfcn.h>

#include "wrappedlibs.h"

#include "wrapper.h"
#include "bridge.h"
#include "librarian/library_private.h"
#include "x64emu.h"
#include "callback.h"
#include "debug.h"
#include "elfloader.h"

const char* aps5bridgeName = "libaps5bridge.so";
#define LIBNAME aps5bridge

typedef uint64_t (*native8_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*native6d4_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, double, double, double, double);

EXPORT void* my_aps5b_load(x64emu_t* emu, void* path)
{
    (void)emu;
    void* handle = dlopen((const char*)path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) printf_log(LOG_NONE, "aps5bridge: dlopen %s: %s\n", (const char*)path, dlerror());
    return handle;
}

EXPORT void* my_aps5b_sym(x64emu_t* emu, void* handle, void* name)
{
    (void)emu;
    return dlsym(handle, (const char*)name);
}

EXPORT void* my_aps5b_error(x64emu_t* emu)
{
    (void)emu;
    return dlerror();
}

EXPORT uint64_t my_aps5b_call(x64emu_t* emu, void* fn, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
{
    (void)emu;
    return ((native8_t)fn)(a0, a1, a2, a3, a4, a5, a6, a7);
}

// Функции с вещественными аргументами: 6 целых (x0..x5) и 4 double (d0..d3).
EXPORT uint64_t my_aps5b_callfd(x64emu_t* emu, void* fn, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, double d0, double d1, double d2, double d3)
{
    (void)emu;
    return ((native6d4_t)fn)(a0, a1, a2, a3, a4, a5, d0, d1, d2, d3);
}

// Вызов x86-функции из нативного кода (любой поток): до 6 целых аргументов.
static uint64_t aps5b_x86call(uintptr_t fn, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
    return RunFunctionFmt(fn, "UUUUUU", a0, a1, a2, a3, a4, a5);
}

// Native replacements of known hot functions for a module mapped without the ELF loader (tests).
EXPORT void* my_aps5b_hotpatch(x64emu_t* emu, void* path, void* base)
{
    (void)emu;
    return (void*)(uintptr_t)Aps5HotPatchModule((const char*)path, (uintptr_t)base);
}

EXPORT void* my_aps5b_x86caller(x64emu_t* emu)
{
    (void)emu;
    return (void*)&aps5b_x86call;
}

#define PRE_INIT\
    if(1)                                                           \
        lib->w.lib = dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL);    \
    else

#include "wrappedlib_init.h"
