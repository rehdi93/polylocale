#include "polylocale.h"
#include <ctime>
#include <memory>
#include <string>

struct ms_locale_deleter
{
    using pointer = _locale_t;
    void operator() (_locale_t p) const { _free_locale(p); }
};

struct poly_locale_deleter
{
    using pointer = locale_t;
    void operator() (locale_t p) const { freelocale(p); }
};

static_assert(std::is_same_v<locale_t, poly_locale*>, "locale_t type mismatch");

int main()
{
    double num = 123.456;
    printf("printf: %.3f" "\n", num);
    
    //auto msloc = std::unique_ptr<_locale_t, ms_locale_deleter>(_create_locale(LC_ALL, "pt_BR"));
    //_printf_l("MS _printf_l: %.3f" "\t" "%02d:%02d:%06.3f" "\n", msloc.get(), num, 1, 37, 11.2f);

    auto polyloc = std::unique_ptr<locale_t, poly_locale_deleter>(newlocale(LC_ALL_MASK, "pt_BR", NULL));
    printf_l("poly printf_l: % .3f" "\t" "%02d:%02d:%06.3f" "\n", polyloc.get(), num, 1, 37, 11);

    return 0;
}

// sprintf( s, "%02d:%02d:%06.3f", hours, mins, secs );
// sprintf( s, "%02d:%02d:%02d%c%0*d", hours, mins, secs, ':', (fps > 999 ? 4 : fps > 99 ? 3 : 2), frames );