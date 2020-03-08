#include <memory>
#include <string>
#include <array>
#include <cstddef>
#include "polylocale.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"


struct poly_locale_deleter
{
    using pointer = locale_t;
    void operator() (locale_t p) const { freelocale(p); }
};

template<size_t Count>
using char_array = std::array<char, Count>;

static_assert(std::is_same_v<locale_t, poly_locale*>, "locale_t type mismatch");

int previous_main()
{
    double num = 123.456;
    printf("printf_l: % .3f" "\t" "%02d:%02d:%06.3f" "\n" "100%% working!\n", num, 1, 37, 11.2f);
    
    puts("-----");

    auto polyloc = std::unique_ptr<locale_t, poly_locale_deleter>(newlocale(LC_ALL_MASK, "pt_BR", NULL));
    printf_l("poly printf_l: % .3f" "\t" "%02d:%02d:%06.3f" "\n" "100%% working!\n", polyloc.get(), num, 1, 37, 11.2f);

    puts("-----");

    return 0;
}

// sprintf( s, "%02d:%02d:%06.3f", hours, mins, secs );
// sprintf( s, "%02d:%02d:%02d%c%0*d", hours, mins, secs, ':', (fps > 999 ? 4 : fps > 99 ? 3 : 2), frames );
#include "boost/utility/string_view.hpp"

TEST_CASE("(new|free|dup)locale work", "[poly C]") {
    std::string locname = "C";

    locale_t ploc = newlocale(LC_ALL_MASK, locname.c_str(), NULL);
    REQUIRE(ploc->name == locname);

    locale_t ploc_copy = duplocale(ploc);
    REQUIRE(ploc_copy->name == locname);
    REQUIRE(ploc != ploc_copy);

    freelocale(ploc);
    freelocale(ploc_copy);
}

// adapted from stb/tests/test_sprintf.c

using ssize_t = std::streamoff;
using boost::string_view;

TEST_CASE("poly sprintf_l tests", "[printf]") {

    const double pow_2_85 = 38685626227668133590597632.0;
    char_array<1024> buffer{};
    char* buffer_ptr = &buffer[0];
    string_view result, expected;
    std::string locname = "C";
    int ret;

    locale_t ploc = newlocale(LC_ALL_MASK, locname.c_str(), NULL);
    //setlocale(LC_ALL, locname.c_str());

    SECTION("Integer numbers") {
        expected = "a b     1";
        ret = sprintf_l(buffer_ptr, "%c %s     %d", ploc, 'a', "b", 1);
        result = { buffer_ptr, (size_t)ret };
        REQUIRE(ret == expected.size());
        REQUIRE(expected == result);

        expected = "+5"; ret = sprintf_l(buffer_ptr, "%+2d", ploc, 5); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size());
        REQUIRE(expected == result);

        expected = "  6"; ret = sprintf_l(buffer_ptr, "% 3i", ploc, 6); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "-7  "; ret = sprintf_l(buffer_ptr, "%-4d", ploc, -7); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "+0"; ret = sprintf_l(buffer_ptr, "%+d", ploc, 0); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "     00003:     00004"; ret = sprintf_l(buffer_ptr, "%10.5d:%10.5d", ploc, 3, 4); result = { buffer_ptr, (size_t)ret };
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "-100006789"; ret = sprintf_l(buffer_ptr, "%d", ploc, -100006789); result = { buffer_ptr, (size_t)ret };
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "20 0020"; ret = sprintf_l(buffer_ptr, "%u %04u", ploc, 20u, 20u); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "12 1e 3C"; ret = sprintf_l(buffer_ptr, "%o %x %X", ploc, 10u, 30u, 60u); result = { buffer_ptr, (size_t)ret };
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = " 12 1e 3C "; ret = sprintf_l(buffer_ptr, "%3o %2x %-3X", ploc, 10u, 30u, 60u); result = { buffer_ptr, (size_t)ret };
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "012 0x1e 0X3C"; ret = sprintf_l(buffer_ptr, "%#o %#x %#X", ploc, 10u, 30u, 60u); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = ""; ret = sprintf_l(buffer_ptr, "%.0x", ploc, 0); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "33 555"; ret = sprintf_l(buffer_ptr, "%hi %ld", ploc, (short)33, 555l); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "9888777666"; ret = sprintf_l(buffer_ptr, "%llu", ploc, 9888777666llu); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);

        expected = "-1 2 -3"; ret = sprintf_l(buffer_ptr, "%ji %zi %ti", ploc, (intmax_t)-1, (ssize_t)2, (ptrdiff_t)-3); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); REQUIRE(expected == result);


        REQUIRE(sprintf_l(buffer_ptr, "%d  %600s", ploc, 3, "abc") == 603);
    }

    SECTION("Floating point numbers") {
        expected = "-3.000000"; ret = sprintf_l(buffer_ptr, "%f", ploc, -3.0); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "-8.8888888800"; ret = sprintf_l(buffer_ptr, "%.10f", ploc, -8.88888888); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "880.0888888800"; ret = sprintf_l(buffer_ptr, "%.10f", ploc, 880.08888888); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "4.1"; ret = sprintf_l(buffer_ptr, "%.1f", ploc, 4.1); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = " 0"; ret = sprintf_l(buffer_ptr, "% .0f", ploc, 0.1); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0.00"; ret = sprintf_l(buffer_ptr, "%.2f", ploc, 1e-4); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "-5.20"; ret = sprintf_l(buffer_ptr, "%+4.2f", ploc, -5.2); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0.0       "; ret = sprintf_l(buffer_ptr, "%-10.1f", ploc, 0.); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "-0.000000"; ret = sprintf_l(buffer_ptr, "%f", ploc, -0.); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0.000001"; ret = sprintf_l(buffer_ptr, "%f", ploc, 9.09834e-07); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "38685626227668133590597632.0"; ret = sprintf_l(buffer_ptr, "%.1f", ploc, pow_2_85); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0.000000499999999999999977"; ret = sprintf_l(buffer_ptr, "%.24f", ploc, 5e-7); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0.000000000000000020000000"; ret = sprintf_l(buffer_ptr, "%.24f", ploc, 2e-17); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0.0000000100 100000000"; ret = sprintf_l(buffer_ptr, "%.10f %.0f", ploc, 1e-8, 1e+8); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "100056789.0"; ret = sprintf_l(buffer_ptr, "%.1f", ploc, 100056789.0); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = " 1.23 %"; ret = sprintf_l(buffer_ptr, "%*.*f %%", ploc, 5, 2, 1.23); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "-3.000000e+00"; ret = sprintf_l(buffer_ptr, "%e", ploc, -3.0); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "4.1E+00"; ret = sprintf_l(buffer_ptr, "%.1E", ploc, 4.1); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "-5.20e+00"; ret = sprintf_l(buffer_ptr, "%+4.2e", ploc, -5.2); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "+0.3 -3"; ret = sprintf_l(buffer_ptr, "%+g %+g", ploc, 0.3, -3.0); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "4"; ret = sprintf_l(buffer_ptr, "%.1G", ploc, 4.1); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "-5.2"; ret = sprintf_l(buffer_ptr, "%+4.2g", ploc, -5.2); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size());
        REQUIRE(expected == result);

        expected = "3e-300"; ret = sprintf_l(buffer_ptr, "%g", ploc, 3e-300); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "1"; ret = sprintf_l(buffer_ptr, "%.0g", ploc, 1.2); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size());
        REQUIRE(expected == result);

        expected = " 3.7 3.71"; ret = sprintf_l(buffer_ptr, "% .3g %.3g", ploc, 3.704, 3.706); result = { buffer_ptr, (size_t)ret };
        REQUIRE(ret == expected.size());
        REQUIRE(expected == result);

        expected = "2e-315:1e+308"; ret = sprintf_l(buffer_ptr, "%g:%g", ploc, 2e-315, 1e+308); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size());
        REQUIRE(expected == result);

        expected = "0x1.fedcbap+98"; ret = sprintf_l(buffer_ptr, "%a", ploc, 0x1.fedcbap+98); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0x1.999999999999a0p-4"; ret = sprintf_l(buffer_ptr, "%.14a", ploc, 0.1); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0x1.0p-1022"; ret = sprintf_l(buffer_ptr, "%.1a", ploc, 0x1.ffp-1023); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "0x1.0091177587f83p-1022"; ret = sprintf_l(buffer_ptr, "%a", ploc, 2.23e-308); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size());
        REQUIRE(expected == result);

        expected = "-0X1.AB0P-5"; ret = sprintf_l(buffer_ptr, "%.3A", ploc, -0X1.abp-5); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);

        expected = "(nil)"; ret = sprintf_l(buffer_ptr, "%p", ploc, (void*)NULL); result = { buffer_ptr, (size_t)ret }; 
        REQUIRE(ret == expected.size()); 
        REQUIRE(expected == result);
    }

    freelocale(ploc);
}

#undef GENTEST
#undef CHECK

TEST_CASE("poly snprintf_l tests", "[printf]") {
    string_view locname = "C", result;
    locale_t ploc = newlocale(LC_ALL_MASK, locname.data(), NULL);
    char_array<1024> buffer{}; auto* buf = &buffer[0];
    const double pow_2_75 = 37778931862957161709568.0;

    REQUIRE(snprintf_l(buf, 100, " %s     %d", ploc, "b", 123) == 10);
    REQUIRE((result = buf) == " b     123");
    REQUIRE(snprintf_l(buf, 100, "%f", ploc, pow_2_75) == 30);
    REQUIRE(strncmp(buf, "37778931862957161709568.000000", 17) == 0);

    auto n = snprintf_l(buf, 10, "number %f", ploc, 123.456789);
    REQUIRE(strcmp(buf, "number 12") == 0);
    REQUIRE(n == 17); // written vs would-be written bytes
    n = snprintf_l(buf, 0, "7 chars", ploc);
    REQUIRE(n == 7);

    REQUIRE(strlen(buf) == 603);
    snprintf_l(buf, 550, "%d  %600s", ploc, 3, "abc");
    REQUIRE(strlen(buf) == 549);
    REQUIRE(snprintf_l(buf, 600, "%510s     %c", ploc, "a", 'b') == 516);
    // length check
    REQUIRE(snprintf_l(NULL, 0, " %s     %d", ploc, "b", 123) == 10);

    freelocale(ploc);
}