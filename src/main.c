// SPDX-License-Identifier: MIT
#include "core.h"

#if defined(__ANDROID__) && defined(__aarch64__)
// bionic arm64 requires the executable TLS segment aligned to at least 64 bytes.
__attribute__((used, aligned(64))) __thread char box64_android_tls_align[64];
#endif
// Entry point shared by the box64 executable and libbox64.so (embedders call box64_main).
__attribute__((visibility("default"))) int box64_main(int argc, const char **argv, char **env) {

    x64emu_t* emu = NULL;
    elfheader_t* elf_header = NULL;
    if (initialize(argc, argv, env, &emu, &elf_header, 1)) {
        return -1;
    }

    return emulate(emu, elf_header);
}

int main(int argc, const char **argv, char **env) {
    return box64_main(argc, argv, env);
}
