// adapted from stb/tests/test_sprintf.c

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
#include <locale>
#include <utility>

#include "polylocale.h"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"


using std::string_view;
using std::string;

#ifdef POLYLOC_UNDECORATED
static_assert(std::is_same_v<locale_t, poly_locale*>, "locale_t is not poly_locale*");
#endif // POLYLOC_UNDECORATED

using locale_ptr_t = std::unique_ptr<poly_locale, decltype(poly_freelocale)*>;
locale_ptr_t locale_ptr(poly_locale* ploc) { return locale_ptr_t(ploc, poly_freelocale); }

// look for a locale by decimal point
static auto find_num_locale(char dp) -> std::string
{
    using std::locale; using std::use_facet; 
    using std::numpunct; using std::clog;

    auto userlc = locale{ "" };
    if (use_facet<numpunct<char>>(userlc).decimal_point() == dp)
    {
        return userlc.name();
    }

    std::vector<const char*> found_locs;

    static const auto decimal_comma_locales = {
        "pt_BR", "pt_BR.utf8",
        "pt_PT", "pt_PT.utf8",
        "fr_FR", "fr_FR.utf8",
        "ru_RU", "ru_RU.utf8",
        "ca_FR", "ca_FR.utf8"
    };

    static const auto decimal_point_locales = {
        "en_US", "en_US.utf8",
        "en_GB", "en_GB.utf8",
        "en_CA", "en_CA.utf8"
        "POSIX", "C"
    };

    if (dp == ',')
    {
        found_locs.assign(decimal_comma_locales);
    }
    else if (dp == '.')
    {
        found_locs.assign(decimal_point_locales);
    }
    else
    {
        throw std::runtime_error("invalid decimal point");
    }

    for (auto name : found_locs)
    {
        try
        {
            locale l{ name };
            return name;
        }
        catch (const std::exception& e)
        {
            clog << e.what() << '\n';
        }
    }

    // none found
    throw std::runtime_error("No locale found");
}

// returns a pair of valid locale names with point and comma decimal separators respectivally
static auto get_test_locales()
{
    using std::locale; 
    using std::clog;
    using namespace std::literals;

    std::string COMMA_LC, POINT_LC;

    try
    {
        if (auto env = getenv("POINT_LC")) {
            POINT_LC = env;
            locale l{ POINT_LC };
        }
        else {
            POINT_LC = find_num_locale('.');
        }
    }
    catch (const std::exception&)
    {
        POINT_LC = "<error>";
    }

    try
    {
        if (auto env = getenv("COMMA_LC")) {
            COMMA_LC = env;
            locale l{ POINT_LC };
        }
        else {
            COMMA_LC = find_num_locale(',');
        }
    }
    catch (const std::exception&)
    {
        COMMA_LC = "<error>";
    }

    return make_pair(POINT_LC, COMMA_LC);
}

static string POINT_LC, COMMA_LC;

int main(int argc, char* argv[]) {
    // setup test locales
    auto lcpair = get_test_locales();
    POINT_LC = lcpair.first;
    COMMA_LC = lcpair.second;

    std::clog << "POINT_LC=" << POINT_LC << "; " "COMMA_LC=" << COMMA_LC << ";\n";

    auto session = Catch::Session();
    int result = session.run(argc, argv);

    return result;
}


template<typename ...VA>
auto test_print_format(string expected, char* buffer, string format, poly_locale_t loc, VA... args)
{
    int retval = poly_sprintf_l(buffer, format.c_str(), loc, args...);
    string_view result = buffer;

    CAPTURE(format, expected.size(), result.size());
    CHECK(expected == result);
}

template<typename ...VA>
auto test_print_format_n(string expected, char* buffer, size_t count, string format, poly_locale_t loc, VA... args)
{
    int retval = poly_snprintf_l(buffer, count, format.c_str(), loc, args...);
    string_view result = buffer;

    CAPTURE(format, expected.size(), result.size());
    CHECK(expected == result);

    return retval;
}


TEST_CASE("Formating integers", "[sprintf][format]")
{
    auto ploc = locale_ptr(poly_newlocale(POLY_ALL_MASK, "C", NULL));
    auto buffer = std::vector<char>(100, '\3');

    auto TEST_FMT = [&](const char* expected, const char* format, auto... args) {
        test_print_format(expected, buffer.data(), format, ploc.get(), args...);
    };

    TEST_FMT("1210", "%d1%u", 12, 0);
    TEST_FMT("0177 0", "%# +o %#o", 127, 0);
    TEST_FMT("+5", "%+2d", 5);
    TEST_FMT("  6", "% 3i", 6);
    TEST_FMT("-7  ", "%-4d", -7);
    TEST_FMT("+0", "%+d", 0);
    TEST_FMT("     00003:     00004", "%10.5d:%10.5d", 3, 4);
    TEST_FMT("-100006789", "%d", -100006789);
    TEST_FMT("20 0020", "%u %04u", 20u, 20u);
    TEST_FMT("12 1e 3C", "%o %x %X", 10u, 30u, 60u);
    TEST_FMT(" 12 1e 3C ", "%3o %2x %-3X", 10u, 30u, 60u);
    TEST_FMT("012 0x1e 0X3C", "%#o %#x %#X", 10u, 30u, 60u);
    TEST_FMT("", "%.0x", 0);
    TEST_FMT("", "%.0d", 0);
    TEST_FMT("33 555", "%hi %ld", (short)33, 555l);
    TEST_FMT("9888777666", "%llu", 9888777666llu);
    TEST_FMT("what? 22", "what? %zi", 22);
    TEST_FMT("100% win rate!", "%d%% win rate!", 100);
    TEST_FMT("+1024 -768", "%+lli % ld", 1024ll, -768l);
    TEST_FMT("24 25", "%hu %hhd", (unsigned short)24, (char)25);
}

TEST_CASE("Formating floating point", "[sprintf][format]")
{
    auto ploc = locale_ptr(poly_newlocale(POLY_ALL_MASK, "C", NULL));
    auto buffer = std::vector<char>(100, '\3');

    auto TEST_FMT = [&](const char* expected, const char* format, auto... args) {
        test_print_format(expected, buffer.data(), format, ploc.get(), args...);
    };

    TEST_FMT("-3.000000", "%f", -3.0);
    TEST_FMT("-8.8888888800", "%.10f", -8.88888888);
    TEST_FMT("880.0888888800", "%.10f", 880.08888888);
    TEST_FMT("4.1", "%.1f", 4.1);
    TEST_FMT(" 0", "% .0f", 0.1);
    TEST_FMT("0.00", "%.2f", 1e-4);
    TEST_FMT("-5.20", "%+4.2f", -5.2);
    TEST_FMT("0.0       ", "%-10.1f", 0.);
    TEST_FMT("-0.000000", "%f", -0.);
    TEST_FMT("0.000001", "%f", 9.09834e-07);
    const auto pow_2_85 = 38685626227668133590597632.0;
    TEST_FMT("38685626227668133590597632.0", "%.1f", pow_2_85);
    TEST_FMT("0.000000499999999999999977", "%.24f", 5e-7);
    TEST_FMT("0.000000000000000020000000", "%.24f", 2e-17);
    TEST_FMT("0.0000000100 100000000", "%.10f %.0f", 1e-8, 1e+8);
    TEST_FMT("100056789.0", "%.1f", 100056789.0);
    TEST_FMT(" 1.23 %", "%*.*f %%", 5, 2, 1.23);
    TEST_FMT("-3.000000e+00", "%e", -3.0);
    TEST_FMT("4.1E+00", "%.1E", 4.1);
    TEST_FMT("-5.20e+00", "%+4.2e", -5.2);
    TEST_FMT("+0.3 -3", "%+g %+g", 0.3, -3.0);
    TEST_FMT("4", "%.1G", 4.1);
    TEST_FMT("-5.2", "%+4.2g", -5.2);
    TEST_FMT("3e-300", "%g", 3e-300);
    TEST_FMT("1", "%.0g", 1.2);
    TEST_FMT(" 3.7 3.71", "% .3g %.3g", 3.704, 3.706);
    TEST_FMT("2e-315:1e+308", "%g:%g", 2e-315, 1e+308);
    TEST_FMT("42.1540000", "%#0-10.3f", 42.1539);
}

TEST_CASE("(new|free|dup)locale", "[polyC]") {
    string locname = "C";

    auto ploc = poly_newlocale(POLY_ALL_MASK, locname.c_str(), NULL);
    REQUIRE(polyloc_getname(ploc) == locname);

    auto ploc_copy = poly_duplocale(ploc);
    REQUIRE(polyloc_getname(ploc_copy) == locname);
    REQUIRE(ploc != ploc_copy);

    poly_freelocale(ploc_copy);

    locname = COMMA_LC;

    auto ploc_base = poly_newlocale(POLY_NUMERIC_MASK | POLY_COLLATE_MASK, locname.data(), ploc);
    CAPTURE(errno);
    REQUIRE(ploc_base);
    REQUIRE(ploc_base == ploc);
    string_view ploc_name = polyloc_getname(ploc_base);
    CHECK(ploc_name.find(locname) != string_view::npos);

    poly_freelocale(ploc);
}

TEST_CASE("sprintf_l tests", "[sprintf]")
{
    auto ploc = locale_ptr(poly_newlocale(POLY_ALL_MASK, "C", NULL));
    auto buffer = std::vector<char>(1024, '\3');

    SECTION("other") {
        auto fmt_test = [&](const char* expected, const char* fmt, auto... args) {
            test_print_format(expected, buffer.data(), fmt, ploc.get(), args...);
        };

        fmt_test("bad fmt:  -.3M", "bad fmt: % -.3M", 321.1);
        fmt_test("bad fmt:  -.3M some other text", "bad fmt: % -.3M some other text", 321.1);
        fmt_test("a b     1", "%c %s     %d", 'a', "b", 1);
        fmt_test("abc     ", "%-8.3s", "abcdefgh");
    }

    SECTION("Return Nº of chars written to buffer minus null") {
        auto test_return = [&](auto format, auto... args) {
            return poly_sprintf_l(buffer.data(), format, ploc.get(), args...);
        };

        REQUIRE(test_return("Lorem ipsum %s sit amet, %s adipiscing %s.", "dolor", "consectetur", "elit") == 56);
        REQUIRE(test_return("%#o %#x %#X", 10u, 30u, 60u) == 13);
        REQUIRE(test_return("%.0x", 0) == 0);
        REQUIRE(test_return("7 chars") == 7);
        REQUIRE(test_return("number %f", 123.456789) == 17);
    }

    SECTION("Big width") {
        REQUIRE(poly_sprintf_l(buffer.data(), "%d  %600s", ploc.get(), 3, "abc") == 603);
    }

    SECTION("% as last char") {
        int retval = poly_sprintf_l(buffer.data(), "%", ploc.get(), 42);
        string_view result = buffer.data();
        CAPTURE(retval, result);
        REQUIRE(result == "");
    }
}

TEST_CASE("Size handler bug", "[bug]")
{
    auto ploc = locale_ptr(poly_newlocale(POLY_ALL_MASK, "C", NULL));
    auto buffer = std::vector<char>(50, '\3');

    intmax_t j = -1; int z = 2; ptrdiff_t t = -3;
    std::string expected = "-1 2 -3";
    auto format = "%ji %zi %ti";

    poly_sprintf_l(buffer.data(), format, ploc.get(), j, z, t);
    string result = buffer.data();

    CAPTURE(format, expected.size(), j,z,t);
    REQUIRE(expected == result);
}

TEST_CASE("snprintf_l tests", "[snprintf]") {
    auto ploc = locale_ptr(poly_newlocale(POLY_ALL_MASK, "C", NULL));
    auto buffer = std::vector<char>(1000, '\3');
    using namespace std::literals;

    SECTION("other") {
        test_print_format_n(" b     123", buffer.data(), 100, " %s     %d", ploc.get(), "b", 123);
        test_print_format_n("number 12", buffer.data(), 10, "number %f", ploc.get(), 123.456789);
    }

    SECTION("Large float") {
        const double pow_2_75 = 37778931862957161709568.0;
        auto pow_2_75_text = "37778931862957161709568.000000"sv;

        int retval = poly_snprintf_l(buffer.data(), 100, "%f", ploc.get(), pow_2_75);
        string_view result = buffer.data();

        CHECK(result.substr(0, 17) == pow_2_75_text.substr(0, 17));
        REQUIRE(result == pow_2_75_text);
    }

    SECTION("Return Nº of chars that would've been written to buffer if count was ignored minus null") {
        auto test_return = [&](size_t cnt, auto format, auto... args) {
            return poly_snprintf_l(buffer.data(), cnt, format, ploc.get(), args...);
        };

        REQUIRE(test_return(60, "Lorem ipsum %s sit amet, %s adipiscing %s.", "dolor", "consectetur", "elit") == 56);
        REQUIRE(test_return(39, "Lorem ipsum %s sit amet, %s adipiscing %s.", "dolor", "consectetur", "elit") == 56);

        REQUIRE(test_return(15, "%#o %#x %#X", 10u, 30u, 60u) == 13);
        REQUIRE(test_return(4, "%#o %#x %#X", 10u, 30u, 60u) == 13);

        REQUIRE(test_return(5, "%.0x", 0) == 0);
        REQUIRE(test_return(0, "%.0x", 0) == 0);

        REQUIRE(test_return(0, "7 chars") == 7);
        REQUIRE(test_return(10, "7 chars") == 7);

        REQUIRE(test_return(10, "number %f", 123.456789) == 17);
        REQUIRE(test_return(20, "number %f", 123.456789) == 17);
    }

    SECTION("Large paddings") {
        poly_snprintf_l(buffer.data(), 550, "%d  %600s", ploc.get(), 3, "abc");
        string_view result = buffer.data();

        CHECK(result.size() == 549);
        REQUIRE(poly_snprintf_l(buffer.data(), 600, "%510s     %c", ploc.get(), "a", 'b') == 516);
        REQUIRE(poly_snprintf_l(NULL, 0, " %s     %d", ploc.get(), "b", 123) == 10);
    }
}

TEST_CASE("PI to string", "[snprintf]")
{
    const auto PI = 3.141592653;
    auto buffer = string(25, '\3');
    auto fmt = "%.4f";

    CAPTURE(fmt);

    SECTION("decimal point") {
        auto ploc = locale_ptr(poly_newlocale(POLY_ALL_MASK, POINT_LC.c_str(), NULL));
        poly_snprintf_l(buffer.data(), 25, fmt, ploc.get(), PI);
        string_view result = buffer.c_str();
        CAPTURE(POINT_LC);
        REQUIRE(result == "3.1416");
    }

    SECTION("decimal comma") {
        auto ploc = locale_ptr(poly_newlocale(POLY_ALL_MASK, COMMA_LC.c_str(), NULL));
        poly_snprintf_l(buffer.data(), 25, fmt, ploc.get(), PI);
        string_view result = buffer.c_str();
        CAPTURE(COMMA_LC);
        REQUIRE(result == "3,1416");
    }
}

TEST_CASE("string to PI", "[strtod]")
{
    auto PI_point = "3.1416";
    auto PI_comma = "3,1416";
    const double PI= 3.1416;
    double result;

    SECTION("decimal point") {
        auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, POINT_LC.c_str(), NULL));
        result = poly_strtod_l(PI_point, NULL, loc.get());
        CAPTURE(PI_point);
        REQUIRE(result == PI);
    }

    SECTION("decimal comma") {
        auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, COMMA_LC.c_str(), NULL));
        result = poly_strtod_l(PI_comma, NULL, loc.get());
        CAPTURE(PI_comma, COMMA_LC);
        REQUIRE(result == PI);
    }

}

TEST_CASE("Wide strings", "[wide]")
{
    auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, "en_US.utf8", NULL));
    auto buffer = string(128, '\3');
    int ret;
    using namespace std::literals;

    SECTION("%S") {
        auto expected = u8"%S: áéíóú"s;
        ret = poly_snprintf_l(buffer.data(), 128, "%%S: %S", loc.get(), L"áéíóú");
        string_view result = buffer.c_str();
        CAPTURE(ret);
        REQUIRE(expected == result);
    }
    SECTION("%ls") {
        auto expected = u8"%ls: áéíóú"s;
        ret = poly_snprintf_l(buffer.data(), 128, "%%ls: %ls", loc.get(), L"áéíóú");
        string_view result = buffer.c_str();
        CAPTURE(ret);
        REQUIRE(expected == result);
    }

}

TEST_CASE("printf tests")
{
    using namespace std::literals;
    auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, "C", NULL));
    auto expected = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"sv;
    auto format = "Lorem ipsum %s sit amet, %s adipiscing %s.\n";

    // trick to intercept the output of cout
    std::ostringstream local;
    auto cout_buf = std::cout.rdbuf(local.rdbuf());
    
    auto result = poly_printf_l(format, loc.get(), "dolor", "consectetur", "elit");

    std::cout.rdbuf(cout_buf);

    REQUIRE(result == expected.size());
    REQUIRE(local.str() == expected);
}
