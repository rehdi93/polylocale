// adapted from stb/tests/test_sprintf.c
#include <memory>
#include <string>
#include <array>
#include <cstddef>
#include "polylocale.h"
#include "boost/utility/string_view.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"


using boost::string_view;

template<size_t Count>
struct char_buffer : std::array<char, Count>
{
    char_buffer()
    {
        // sprintf must write the terminating null
        this->fill('\3');
    }

    operator char* () {
        return &this->front();
    }
    operator string_view() {
        return { &this->front() };
    }
};

#ifdef POLYLOC_UNDECORATED
static_assert(std::is_same_v<locale_t, poly_locale*>, "locale_t is not poly_locale*");
#endif // POLYLOC_UNDECORATED


using locale_ptr_t = std::unique_ptr<poly_locale, decltype(poly_freelocale)*>;
locale_ptr_t locale_ptr(poly_locale* ploc) { return locale_ptr_t(ploc, poly_freelocale); }


struct sprintf_test_result
{
    std::string expected, result, format;
    int returnValue;
};

template<typename ...VA>
sprintf_test_result do_test_sprintf(std::string expected, char* buf, size_t count, std::string fmt, poly_locale_t loc, VA... rest)
{
    int ret;
    auto dbgfmt = fmt.c_str();
    if (count == size_t(-1))
    {
        ret = poly_sprintf_l(buf, fmt.c_str(), loc, rest...);
    }
    else
    {
        ret = poly_snprintf_l(buf, count, fmt.c_str(), loc, rest...);
    }

    return {
        expected, buf, fmt, ret
    };
}

#define FMT_TEST_BASE(expected_, fmt, buffer, count, locp, ...) SECTION(#fmt " --> " #expected_) { \
    auto tr = do_test_sprintf(expected_, buffer, count, fmt, locp, __VA_ARGS__); \
    CAPTURE(tr.returnValue, tr.expected.size()); \
    REQUIRE(tr.expected == tr.result); }
    
#define TEST_FMT(expected_, fmt, ...) FMT_TEST_BASE(expected_, fmt, buffer, size_t(-1), ploc, __VA_ARGS__)


TEST_CASE("Formating integers", "[sprintf_l]")
{
    auto localename = "C";
    auto ploc = poly_newlocale(POLY_ALL_MASK, localename, NULL);
    char_buffer<1024> buffer;

    INFO("Locale: " << localename);

    TEST_FMT("1210", "%d1%u", 12, 0);
    TEST_FMT("0177 0", "%# +o %#o", 127, 0);
    TEST_FMT("bad fmt:  -.3M", "bad fmt: % -.3M", 321.1);
    TEST_FMT("a b     1", "%c %s     %d", 'a', "b", 1);
    TEST_FMT("abc     ", "%-8.3s", "abcdefgh");
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
}

TEST_CASE("Formating floating point", "[sprintf_l]")
{
    auto localename = "C";
    poly_locale_t ploc = poly_newlocale(POLY_ALL_MASK, localename, NULL);
    char_buffer<1024> buffer;
    const double pow_2_85 = 38685626227668133590597632.0;

    INFO("Locale: " << localename);

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
}

TEST_CASE("(new|free|dup)locale", "[polyC]") {
    string_view locname = "C";
    auto ploc = poly_newlocale(POLY_ALL_MASK, locname.data(), NULL);
    REQUIRE(polyloc_getname(ploc) == locname);

    auto ploc_copy = poly_duplocale(ploc);
    REQUIRE(polyloc_getname(ploc_copy) == string_view("C"));
    REQUIRE(ploc != ploc_copy);

    poly_freelocale(ploc_copy);

    locname = "pt_BR.UTF-8";
    auto ploc_base = poly_newlocale(POLY_NUMERIC_MASK, locname.data(), ploc);
    REQUIRE(ploc_base);
    CHECK(polyloc_getname(ploc_base) == locname);
    REQUIRE(ploc_base == ploc);

    poly_freelocale(ploc);
}

TEST_CASE("sprintf_l tests", "[sprintf_l]")
{
    char_buffer<1024> buffer;
    std::string locname = "C";
    auto ploc = poly_newlocale(POLY_ALL_MASK, locname.c_str(), NULL);

    SECTION("600 width") {
        REQUIRE(poly_sprintf_l(buffer, "%d  %600s", ploc, 3, "abc") == 603);
    }

    SECTION("% as last char") {
        int retval = poly_sprintf_l(buffer, "%", ploc, 42);
        string_view result = buffer;
        CAPTURE(retval, result);
        REQUIRE(result == "");
    }

    TEST_FMT("42.1540000", "%#0-10.3f", 42.1539);
}

TEST_CASE("Size handler bug", "[bug][.]")
{
    auto localename = "C";
    auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, localename, NULL));
    char_buffer<1024> buffer;

    intmax_t j = -1;
    int z = 2;
    ptrdiff_t t = -3;
    std::string expected = "-1 2 -3", result;

    int returnValue = poly_sprintf_l(buffer, "%ji %zi %ti", loc.get(), j, z, t);
    result = buffer;

    CAPTURE(returnValue, expected.size(), j,z,t);
    INFO("Format = \"" "%ji %zi %ti" "\"");
    REQUIRE(expected == result);
}

TEST_CASE("snprintf_l tests", "[snprintf_l]") {
    auto localename = "C";
    auto ploc = poly_newlocale(POLY_ALL_MASK, localename, NULL);
    char_buffer<1024> buffer;
    const double pow_2_75 = 37778931862957161709568.0;

    INFO("Locale: " << localename);

    SECTION(R"(" %s     %d" --> " b     123")")
    {
        auto tr = do_test_sprintf(" b     123", buffer, 100, " %s     %d", ploc, "b", 123);
        CAPTURE(tr.expected, tr.result, tr.format, tr.returnValue);
        REQUIRE(tr.expected == tr.result);
    }

    SECTION("Large float") {
        auto tr = do_test_sprintf("37778931862957161709568.000000", buffer, 100, "%f", ploc, pow_2_75);
        CAPTURE(tr.expected, tr.result, tr.format, tr.returnValue);
        CHECK(tr.returnValue == 30);
        REQUIRE(tr.expected.substr(0, 17) == tr.result.substr(0, 17));
    }

    SECTION("Truncate") {
        auto tr = do_test_sprintf("number 12", buffer, 10, "number %f", ploc, 123.456789);
        CAPTURE(tr.expected, tr.result, tr.format, tr.returnValue);
        CHECK(tr.returnValue == 17); // written vs would-be written bytes
        REQUIRE(tr.result == tr.expected);
    }

    SECTION("Don't write if count==0, return necessary size") {
        int ret = poly_snprintf_l(buffer, 0, "7 chars", ploc);
        REQUIRE(ret == 7);
    }

    SECTION("Large paddings") {
        poly_snprintf_l(buffer, 550, "%d  %600s", ploc, 3, "abc");
        string_view result = buffer;

        CHECK(result.size() == 549);
        REQUIRE(poly_snprintf_l(buffer, 600, "%510s     %c", ploc, "a", 'b') == 516);
        REQUIRE(poly_snprintf_l(NULL, 0, " %s     %d", ploc, "b", 123) == 10);
    }
}

TEST_CASE("PI to string", "[locale]")
{
    const auto PI = 3.141592653;
    char_buffer<25> buffer;
    auto fmt = "%.4f";

    CAPTURE(fmt);

    SECTION("decimal point") {
        auto en_us = locale_ptr(poly_newlocale(POLY_NUMERIC_MASK, "en_US", NULL));
        poly_snprintf_l(buffer, 25, fmt, en_us.get(), PI);
        string_view result = buffer;
        INFO("en_US");
        REQUIRE(result == "3.1416");
    }

    SECTION("decimal comma") {
        auto pt_br = locale_ptr(poly_newlocale(POLY_NUMERIC_MASK, "pt_BR", NULL));
        poly_snprintf_l(buffer, 25, fmt, pt_br.get(), PI);
        string_view result = buffer;
        INFO("pt_BR");
        REQUIRE(result == "3,1416");
    }
}

TEST_CASE("string to PI", "[strtod]")
{
    auto PI_point = "3.1416";
    auto PI_comma = "3,1416";
    const double PI =3.1416;
    double result;

    {
        auto en_us = locale_ptr(poly_newlocale(POLY_ALL_MASK, "en_US", NULL));
        result = poly_strtod_l(PI_point, NULL, en_us.get());
        INFO("en_US");
        CAPTURE(PI_point);
        REQUIRE(result == PI);
    }
    {
        auto pt_br = locale_ptr(poly_newlocale(POLY_ALL_MASK, "pt_BR", NULL));
        result = poly_strtod_l(PI_comma, NULL, pt_br.get());
        INFO("pt_BR");
        CAPTURE(PI_comma);
        REQUIRE(result == PI);
    }

}