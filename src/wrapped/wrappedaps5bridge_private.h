// SPDX-License-Identifier: MIT
#if !(defined(GO) && defined(GOM) && defined(GO2) && defined(DATA))
#error Meh...
#endif

// Мост AnyPS5: x86-заглушки вызывают нативные (aarch64) библиотеки AnyPS5 и наоборот.
GOM(aps5b_load, pFEp)
GOM(aps5b_sym, pFEpp)
GOM(aps5b_error, pFEv)
GOM(aps5b_call, UFEpUUUUUUUU)
GOM(aps5b_callfd, UFEpUUUUUUdddd)
GOM(aps5b_x86caller, pFEv)
GOM(aps5b_hotpatch, pFEpp)
