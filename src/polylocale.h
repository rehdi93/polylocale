/*
    polylocale: polyfill for C *_l functions
    
    by Pedro Oliva Rodrigues
*/

#if !defined(_POLY_LOCALE)
#define _POLY_LOCALE

#include <stdio.h>
#include <stdarg.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct poly_impl;
struct poly_locale
{
    struct poly_impl* impl;
    const char* name;
};
typedef struct poly_locale* poly_locale_s;



poly_locale_s poly_newlocale(int category_mask, const char* localename, poly_locale_s base);
void poly_freelocale(poly_locale_s loc);
poly_locale_s poly_duplocale(poly_locale_s loc);

#define POLY_GLOBAL_LOCALE    ((poly_locale_s) -1L)

double poly_strtod_l(const char* str, char** endptr, poly_locale_s loc);

int poly_printf_l(const char* fmt, poly_locale_s locale, ...);
int poly_vprintf_l(const char* fmt, poly_locale_s locale, va_list args);
int poly_fprintf_l(FILE* stream, const char* format, poly_locale_s locale, ...);
int poly_vfprintf_l(FILE* cfile, const char* fmt, poly_locale_s locale, va_list args);
int poly_sprintf_l(char* buffer, const char* fmt, poly_locale_s loc, ...);
int poly_vsprintf_l(char* buffer, const char* format, poly_locale_s locale, va_list args);
int poly_snprintf_l(char* buffer, size_t count, const char* fmt, poly_locale_s loc, ...);
int poly_vsnprintf_l(char* buffer, size_t count, const char* fmt, poly_locale_s loc, va_list args);

#ifdef __cplusplus
}
#endif

#ifdef POLYLOC_UNDECORATED

#define locale_t        poly_locale_s
#define newlocale       poly_newlocale
#define freelocale      poly_freelocale
#define duplocale       poly_duplocale
#define strtod_l        poly_strtod_l
#define printf_l        poly_printf_l
#define vprintf_l       poly_vprintf_l
#define fprintf_l       poly_fprintf_l
#define vfprintf_l      poly_vfprintf_l
#define sprintf_l       poly_sprintf_l
#define vsprintf_l      poly_vsprintf_l
#define snprintf_l      poly_snprintf_l
#define vsnprintf_l     poly_vsnprintf_l

// LC_*_MASK names
#define LC_CTYPE_MASK       (1 << LC_CTYPE)
#define LC_NUMERIC_MASK     (1 << LC_NUMERIC)
#define LC_TIME_MASK        (1 << LC_TIME)
#define LC_COLLATE_MASK     (1 << LC_COLLATE)
#define LC_MONETARY_MASK    (1 << LC_MONETARY)
#define LC_ALL_MASK         (LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | LC_COLLATE_MASK | LC_MONETARY_MASK)

#define LC_GLOBAL_LOCALE    POLY_GLOBAL_LOCALE

#endif // POLYLOC_UNDECORATED


#endif