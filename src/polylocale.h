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

struct poly_locale;

typedef struct poly_locale* poly_locale_t;

// locale_t management
poly_locale_t poly_newlocale(int category_mask, const char* localename, poly_locale_t base);
void poly_freelocale(poly_locale_t loc);
poly_locale_t poly_duplocale(poly_locale_t loc);

// deserialization
double poly_strtod_l(const char* str, char** endptr, poly_locale_t loc);

// printf family
int poly_printf_l(const char* fmt, poly_locale_t locale, ...);
int poly_vprintf_l(const char* fmt, poly_locale_t locale, va_list args);
int poly_sprintf_l(char* buffer, const char* fmt, poly_locale_t loc, ...);
int poly_vsprintf_l(char* buffer, const char* fmt, poly_locale_t locale, va_list args);
int poly_snprintf_l(char* buffer, size_t count, const char* fmt, poly_locale_t loc, ...);
int poly_vsnprintf_l(char* buffer, size_t count, const char* fmt, poly_locale_t loc, va_list args);
int poly_fprintf_l(FILE* cfile, const char* fmt, poly_locale_t locale, ...);
int poly_vfprintf_l(FILE* cfile, const char* fmt, poly_locale_t locale, va_list args);

// polyloc specific
const char* polyloc_getname(poly_locale_t l);


enum poly_lc_masks
{
    POLY_COLLATE_MASK = 1 << 0,
    POLY_CTYPE_MASK = 1 << 1,
    POLY_MONETARY_MASK = 1 << 2,
    POLY_NUMERIC_MASK = 1 << 3,
    POLY_TIME_MASK = 1 << 4,

    POLY_ALL_MASK = POLY_COLLATE_MASK | POLY_CTYPE_MASK | POLY_MONETARY_MASK | POLY_NUMERIC_MASK | POLY_TIME_MASK
};

#define POLY_GLOBAL_LOCALE    ((poly_locale_t) -1L)

#ifdef __cplusplus
}
#endif

#ifdef POLYLOC_UNDECORATED

#define locale_t        poly_locale_t
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

#define LC_GLOBAL_LOCALE    POLY_GLOBAL_LOCALE

// LC_*_MASK names
#define LC_COLLATE_MASK     POLY_COLLATE_MASK
#define LC_CTYPE_MASK       POLY_CTYPE_MASK
#define LC_MONETARY_MASK    POLY_MONETARY_MASK
#define LC_NUMERIC_MASK     POLY_NUMERIC_MASK
#define LC_TIME_MASK        POLY_TIME_MASK
#define LC_ALL_MASK         POLY_ALL_MASK


#endif // POLYLOC_UNDECORATED

#endif // _POLY_LOCALE