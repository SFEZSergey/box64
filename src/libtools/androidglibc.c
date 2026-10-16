// glibc-only entry points for wrapped libc/libpthread when the host libc is bionic (box64 embedded
// in an Android app). Used as a last-resort fallback when dlsym on the native library fails.
#if defined(ANDROID) || defined(TERMUX)
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include "androidglibc.h"

// glibc ctype tables: indexable from -128 to 255, bit layout of <ctype.h> on little-endian.
#define GLIBC_ISBIT(bit) ((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
static unsigned short ctype_b[384];
static int32_t ctype_toupper[384];
static int32_t ctype_tolower[384];
static const unsigned short* ctype_b_ptr = ctype_b + 128;
static const int32_t* ctype_toupper_ptr = ctype_toupper + 128;
static const int32_t* ctype_tolower_ptr = ctype_tolower + 128;
static pthread_once_t ctype_once = PTHREAD_ONCE_INIT;

static void ctype_init(void)
{
    for (int i = -128; i < 256; ++i) {
        int c = i < 0 ? -1 : i; // only 0..127 is classified in the C locale
        unsigned short v = 0;
        if (c >= 0 && c < 128) {
            if (isupper(c)) v |= GLIBC_ISBIT(0);
            if (islower(c)) v |= GLIBC_ISBIT(1);
            if (isalpha(c)) v |= GLIBC_ISBIT(2);
            if (isdigit(c)) v |= GLIBC_ISBIT(3);
            if (isxdigit(c)) v |= GLIBC_ISBIT(4);
            if (isspace(c)) v |= GLIBC_ISBIT(5);
            if (isprint(c)) v |= GLIBC_ISBIT(6);
            if (isgraph(c)) v |= GLIBC_ISBIT(7);
            if (isblank(c)) v |= GLIBC_ISBIT(8);
            if (iscntrl(c)) v |= GLIBC_ISBIT(9);
            if (ispunct(c)) v |= GLIBC_ISBIT(10);
            if (isalnum(c)) v |= GLIBC_ISBIT(11);
        }
        ctype_b[i + 128] = v;
        ctype_toupper[i + 128] = (c >= 'a' && c <= 'z') ? c - 32 : i;
        ctype_tolower[i + 128] = (c >= 'A' && c <= 'Z') ? c + 32 : i;
    }
}

// glibc struct __locale_struct layout (libstdc++ "gnu" locale model reads the ctype fields).
typedef struct glibc_locale_s {
    void* locales[13];
    const unsigned short* ctype_b;
    const int32_t* ctype_tolower;
    const int32_t* ctype_toupper;
    const char* names[13];
} glibc_locale_t;

static glibc_locale_t* make_locale(void)
{
    pthread_once(&ctype_once, ctype_init);
    glibc_locale_t* l = calloc(1, sizeof(glibc_locale_t));
    if (!l) return NULL;
    l->ctype_b = ctype_b_ptr;
    l->ctype_tolower = ctype_tolower_ptr;
    l->ctype_toupper = ctype_toupper_ptr;
    for (int i = 0; i < 13; ++i) l->names[i] = "C";
    return l;
}

#define GLIBC_LC_GLOBAL_LOCALE ((void*)-1L)
static __thread void* current_locale = GLIBC_LC_GLOBAL_LOCALE;

static int* my_errno_location(void) { return &errno; }
static int my_bcmp(const void* a, const void* b, size_t n) { return memcmp(a, b, n); }
static const unsigned short** my_ctype_b_loc(void) { pthread_once(&ctype_once, ctype_init); return &ctype_b_ptr; }
static const int32_t** my_ctype_toupper_loc(void) { pthread_once(&ctype_once, ctype_init); return &ctype_toupper_ptr; }
static const int32_t** my_ctype_tolower_loc(void) { pthread_once(&ctype_once, ctype_init); return &ctype_tolower_ptr; }
static int my_pthread_setcanceltype(int type, int* old) { (void)type; if (old) *old = 0; return 0; }
static void my_assert_fail(const char* assertion, const char* file, unsigned line, const char* function)
{
    __assert2(file, (int)line, function ? function : "?", assertion);
}
static void* my_newlocale(int mask, const char* name, void* base) { (void)mask; (void)name; return base ? base : make_locale(); }
static void my_freelocale(void* l) { if (l && l != GLIBC_LC_GLOBAL_LOCALE) free(l); }
static void* my_duplocale(void* l)
{
    glibc_locale_t* d = make_locale();
    if (d && l && l != GLIBC_LC_GLOBAL_LOCALE) memcpy(d, l, sizeof(*d));
    return d;
}
static void* my_uselocale(void* l)
{
    void* old = current_locale;
    if (l) current_locale = l;
    return old;
}
static const char* my_nl_langinfo_l(int item, void* l)
{
    (void)l;
    switch (item) {
        case 14: return "UTF-8";                // CODESET
        case 0x10000: return ".";               // RADIXCHAR / DECIMAL_POINT
        case 0x10001: return "";                // THOUSEP
        case 0x10002: return "";                // GROUPING
        default: return "";
    }
}
static int my_strcoll_l(const char* a, const char* b, void* l) { (void)l; return strcoll(a, b); }
static size_t my_strxfrm_l(char* d, const char* s, size_t n, void* l) { (void)l; return strxfrm(d, s, n); }
static size_t my_strftime_l(char* s, size_t n, const char* f, const struct tm* t, void* l) { (void)l; return strftime(s, n, f, t); }
static double my_strtod_l(const char* s, char** e, void* l) { (void)l; return strtod(s, e); }
static float my_strtof_l(const char* s, char** e, void* l) { (void)l; return strtof(s, e); }
static int my_wcscoll_l(const wchar_t* a, const wchar_t* b, void* l) { (void)l; return wcscoll(a, b); }
static size_t my_wcsxfrm_l(wchar_t* d, const wchar_t* s, size_t n, void* l) { (void)l; return wcsxfrm(d, s, n); }
static size_t my_wcsftime_l(wchar_t* s, size_t n, const wchar_t* f, const struct tm* t, void* l) { (void)l; return wcsftime(s, n, f, t); }
static int my_iswctype_l(wint_t c, wctype_t t, void* l) { (void)l; return iswctype(c, t); }
static wctype_t my_wctype_l(const char* n, void* l) { (void)l; return wctype(n); }
static wint_t my_towlower_l(wint_t c, void* l) { (void)l; return towlower(c); }
static wint_t my_towupper_l(wint_t c, void* l) { (void)l; return towupper(c); }
static int my_xpg_strerror_r(int e, char* buf, size_t n)
{
    const char* s = strerror(e);
    if (!n) return ERANGE;
    snprintf(buf, n, "%s", s);
    return strlen(s) >= n ? ERANGE : 0;
}
static char* my_gettext(const char* id) { return (char*)id; }
static char* my_dgettext(const char* d, const char* id) { (void)d; return (char*)id; }
static char* my_bindtextdomain(const char* d, const char* dir) { (void)d; return (char*)(dir ? dir : "/usr/share/locale"); }
static char* my_bind_textdomain_codeset(const char* d, const char* c) { (void)d; return (char*)c; }
static char* my_secure_getenv(const char* n) { return getenv(n); }
static double my_strtof64(const char* s, char** e) { return strtod(s, e); }
static int my_strfromf64(char* s, size_t n, const char* f, double v) { return snprintf(s, n, f, v); }

typedef struct { const char* name; void* addr; } compat_entry_t;
static const compat_entry_t compat[] = {
    {"__errno_location", my_errno_location},
    {"bcmp", my_bcmp},
    {"__ctype_b_loc", my_ctype_b_loc},
    {"__ctype_toupper_loc", my_ctype_toupper_loc},
    {"__ctype_tolower_loc", my_ctype_tolower_loc},
    {"pthread_setcanceltype", my_pthread_setcanceltype},
    {"__assert_fail", my_assert_fail},
    {"__newlocale", my_newlocale},
    {"newlocale", my_newlocale},
    {"__freelocale", my_freelocale},
    {"freelocale", my_freelocale},
    {"__duplocale", my_duplocale},
    {"duplocale", my_duplocale},
    {"__uselocale", my_uselocale},
    {"uselocale", my_uselocale},
    {"__nl_langinfo_l", my_nl_langinfo_l},
    {"nl_langinfo_l", my_nl_langinfo_l},
    {"__strcoll_l", my_strcoll_l},
    {"__strxfrm_l", my_strxfrm_l},
    {"__strftime_l", my_strftime_l},
    {"__strtod_l", my_strtod_l},
    {"__strtof_l", my_strtof_l},
    {"__wcscoll_l", my_wcscoll_l},
    {"__wcsxfrm_l", my_wcsxfrm_l},
    {"__wcsftime_l", my_wcsftime_l},
    {"__iswctype_l", my_iswctype_l},
    {"__wctype_l", my_wctype_l},
    {"__towlower_l", my_towlower_l},
    {"__towupper_l", my_towupper_l},
    {"__xpg_strerror_r", my_xpg_strerror_r},
    {"gettext", my_gettext},
    {"dgettext", my_dgettext},
    {"bindtextdomain", my_bindtextdomain},
    {"bind_textdomain_codeset", my_bind_textdomain_codeset},
    {"secure_getenv", my_secure_getenv},
    {"strtof64", my_strtof64},
    {"strfromf64", my_strfromf64},
};

void* AndroidGlibcCompat(const char* name)
{
    for (size_t i = 0; i < sizeof(compat) / sizeof(compat[0]); ++i)
        if (!strcmp(compat[i].name, name)) return compat[i].addr;
    return NULL;
}
#endif
