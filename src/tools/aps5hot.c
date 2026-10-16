// SPDX-License-Identifier: MIT
// Native replacements for hot x86 functions of the PS5 game modules run through AnyPS5
// (BOX64_APS5_NATIVE, on by default). When a known module is loaded, each listed function is checked
// byte for byte (FNV-1a of its code) and its first bytes become a box64 bridge (int3 'S' 'C' wrapper
// function ret), so every caller runs the native function instead: the dynarec turns the bridge
// into a direct native call with the x86 argument registers (simple wrappers), and inlines it into
// direct `call`s of the function. A function whose bytes differ (another game build) is left alone.
#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include "debug.h"
#include "box64context.h"
#include "elfloader.h"
#include "bridge.h"
#include "wrapper.h"
#include "custommem.h"
#include "env.h"
#include "aps5hot_sincos.h"

static void Aps5SinCos(double x, double* sn, double* cs)
{
    GameSinCos(x, sn, cs);
}

// _Atomic_compare_exchange_weak_4: x86 lock cmpxchg never fails spuriously, so strong.
static int Aps5CompareExchange4(int32_t* object, int32_t* expected, int32_t desired)
{
    return __atomic_compare_exchange_n(object, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

// The game's memcpy copies overlapping ranges like memmove (forward or backward); keep that.
static void* Aps5MemCpy(void* dst, const void* src, size_t size)
{
    return memmove(dst, src, size);
}

static void* Aps5MemSet(void* dst, int value, size_t size)
{
    return memset(dst, value, size);
}

typedef struct aps5hot_s {
    const char* module;     // file name of the loaded module
    uintptr_t   offset;     // function address in the module
    size_t      size;       // function size (bytes hashed)
    uint64_t    hash;       // FNV-1a 64 of the function bytes
    wrapper_t   wrapper;
    void*       native;
    const char* name;
} aps5hot_t;

static const aps5hot_t hot[] = {
    // Plants vs. Zombies: Replanted (PPSA29802), game libc.
    {"libc.prx.guest.prx", 0x3b90, 1751, 0x0943c77a84c0daa0ull, pFppL, Aps5MemCpy, "memcpy"},
    {"libc.prx.guest.prx", 0x10430, 383, 0x9bbe0fbf4c4671e6ull, pFpiL, Aps5MemSet, "memset"},
    {"libc.prx.guest.prx", 0x4e2f0, 1213, 0xb530e8a466540be7ull, vFdpp, Aps5SinCos, "sincos"},
    {"libc.prx.guest.prx", 0x7f150, 62, 0xaae49931ef1d8a5full, iFppi, Aps5CompareExchange4, "cas4"},
};

static uint64_t Fnv1a(const uint8_t* p, size_t size)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i)
        h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

static int WriteCode(uintptr_t addr, const void* bytes, size_t size)
{
    uintptr_t first = addr & ~(uintptr_t)(box64_pagesize - 1);
    uintptr_t last = (addr + size - 1) & ~(uintptr_t)(box64_pagesize - 1);
    for (uintptr_t page = first; page <= last; page += box64_pagesize) {
        uint32_t prot = getProtection(page);
        if (!prot || mprotect((void*)page, box64_pagesize, (prot | PROT_READ | PROT_WRITE) & ~PROT_CUSTOM))
            return 0;
    }
    memcpy((void*)addr, bytes, size);
    for (uintptr_t page = first; page <= last; page += box64_pagesize)
        mprotect((void*)page, box64_pagesize, getProtection(page) & ~PROT_CUSTOM);
#ifdef DYNAREC
    // Translations of the old bytes (from DynaCache) are rechecked and dropped.
    cleanDBFromAddressRange(addr, size, 0);
#endif
    return 1;
}

// `path` is the module's file (only its name is matched), `delta` its load address.
int Aps5HotPatchModule(const char* path, uintptr_t delta)
{
    if (!BOX64ENV(aps5_native) || !path || !my_context)
        return 0;
    const char* file = strrchr(path, '/');
    file = file ? file + 1 : path;
    int patched = 0, skipped = 0;
    char names[256] = "";
    for (size_t i = 0; i < sizeof(hot) / sizeof(hot[0]); ++i) {
        if (strcmp(file, hot[i].module))
            continue;
        uintptr_t addr = delta + hot[i].offset;
        if (Fnv1a((const uint8_t*)addr, hot[i].size) != hot[i].hash) {
            ++skipped;
            continue;
        }
        uint8_t bridge[20] = {0xcc, 'S', 'C'};
        memcpy(bridge + 3, &hot[i].wrapper, sizeof(void*));
        memcpy(bridge + 11, &hot[i].native, sizeof(void*));
        bridge[19] = 0xc3;
        if (!WriteCode(addr, bridge, sizeof(bridge))) {
            ++skipped;
            continue;
        }
        ++patched;
        if (strlen(names) + strlen(hot[i].name) + 2 < sizeof(names)) {
            if (names[0]) strcat(names, " ");
            strcat(names, hot[i].name);
        }
    }
    if (patched || skipped)
        printf_log(LOG_INFO, "[aps5hot] %s: %d native (%s), %d skipped (code differs)\n", file, patched, names, skipped);
    return patched;
}

void Aps5HotPatchElf(elfheader_t* h)
{
    if (!h)
        return;
    const char* path = ElfPath(h);
    Aps5HotPatchModule(path ? path : ElfName(h), (uintptr_t)GetElfDelta(h));
}
