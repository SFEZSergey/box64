#ifndef __ANDROIDGLIBC_H_
#define __ANDROIDGLIBC_H_
// glibc-only functions implemented over bionic, used when the native libc lacks them.
void* AndroidGlibcCompat(const char* name);
#endif
