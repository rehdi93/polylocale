/*
    polylocale: polyfill for C *_l functions
    
    by Pedro Oliva Rodrigues
*/

#if !defined(_POLY_LOCALE)
#define _POLY_LOCALE

#include <locale.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct poly_impl;
struct poly_locale
{
    struct poly_impl* impl;
    const char* name;
};
typedef struct poly_locale* locale_t;


// LC_*_MASK names
#define LC_CTYPE_MASK       (1 << LC_CTYPE)
#define LC_NUMERIC_MASK     (1 << LC_NUMERIC)
#define LC_TIME_MASK        (1 << LC_TIME)
#define LC_COLLATE_MASK     (1 << LC_COLLATE)
#define LC_MONETARY_MASK    (1 << LC_MONETARY)
#define LC_ALL_MASK         (LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | LC_COLLATE_MASK | LC_MONETARY_MASK)

locale_t newlocale(int category_mask, const char* localename, locale_t base);
void freelocale(locale_t loc);
locale_t duplocale(locale_t loc);

#define LC_GLOBAL_LOCALE    ((locale_t) -1L)

#define HAVE_STRTOD_L

double strtod_l(const char* str, char** endptr, locale_t loc);

int printf_l(const char* fmt, locale_t locale, ...);
int fprintf_l(FILE* stream, const char* format, locale_t locale, ...);
int sprintf_l(char* buffer, const char* fmt, locale_t loc, ...);
int snprintf_l(char* buffer, size_t count, const char* fmt, locale_t loc, ...);
int vsnprintf_l(char* buffer, size_t count, const char* fmt, locale_t loc, va_list args);
int vfprintf_l(FILE* cfile, const char* fmt, locale_t locale, va_list args);

#ifdef __cplusplus
}
#endif

#endif