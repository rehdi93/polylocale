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


using locale_ptr = std::unique_ptr<poly_locale, decltype(poly_freelocale)*>;

// adapted from stb/tests/test_sprintf.c


struct sprintf_test_result
{
    std::string expected, result, format;
    int returnValue;
};

template<typename ...VA>
sprintf_test_result do_test_sprintf(std::string expected, char* buf, size_t count, std::string fmt, poly_locale_s loc, VA... rest)
{
    int ret;
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


TEST_CASE("Formating integers", "[sprintf_l][format]")
{
    auto localename = "C";
    auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, localename, NULL), poly_freelocale);
    auto ploc = loc.get();
    char_buffer<1024> buffer{};

    INFO("Locale: " << localename);

    TEST_FMT("0177 0", "%# +o %#o", 127, 0);
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
    //TEST_FMT("-1 2 -3", "%ji %zi %ti", (intmax_t)-1, (ssize_t)2, (ptrdiff_t)-3);
}

TEST_CASE("Formating floating point", "[sprintf_l][format]")
{
    auto localename = "C";
    poly_locale_s ploc = poly_newlocale(POLY_ALL_MASK, localename, NULL);
    char_buffer<1024> buffer{};
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

TEST_CASE("(new|free|dup)locale work", "[poly C]") {
    std::string locname = "C";

    poly_locale_s ploc = poly_newlocale(POLY_ALL_MASK, locname.c_str(), NULL);
    REQUIRE(ploc->name == locname);

    poly_locale_s ploc_copy = poly_duplocale(ploc);
    REQUIRE(ploc_copy->name == locname);
    REQUIRE(ploc != ploc_copy);

    poly_freelocale(ploc);
    poly_freelocale(ploc_copy);
}

TEST_CASE("other sprintf_l tests", "[sprintf_l]")
{
    char_buffer<1024> buffer{};
    std::string locname = "C";

    auto ploc = poly_newlocale(POLY_ALL_MASK, locname.c_str(), NULL);
    std::locale::global(std::locale{ locname });

    REQUIRE(poly_sprintf_l(buffer, "%d  %600s", ploc, 3, "abc") == 603);
}

TEST_CASE("Size handler bug", "[sprintf_l][bug]")
{
    auto localename = "C";
    auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, localename, NULL), poly_freelocale);
    char_buffer<1024> buffer{};

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
    auto loc = locale_ptr(poly_newlocale(POLY_ALL_MASK, localename, NULL), poly_freelocale);
    char_buffer<1024> buffer{};
    auto ploc = loc.get();

    const double pow_2_75 = 37778931862957161709568.0;

    INFO("Locale: " << localename);

    SECTION(R"(" %s     %d" --> " b     123")")
    {
        auto tr = do_test_sprintf(" b     123", buffer, 100, " %s     %d", loc.get(), "b", 123);
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

    SECTION("Don't write if count==0") {
        auto tr = do_test_sprintf("", buffer, 0, "7 chars", ploc);
        CAPTURE(tr.expected, tr.result, tr.format, tr.returnValue);
        CHECK(tr.returnValue == 7);
        REQUIRE(tr.result == tr.expected);
    }

    SECTION("Large paddings") {
        poly_snprintf_l(buffer, 550, "%d  %600s", ploc, 3, "abc");
        string_view result = buffer;

        CHECK(result.size() == 549);
        REQUIRE(poly_snprintf_l(buffer, 600, "%510s     %c", ploc, "a", 'b') == 516);

        REQUIRE(poly_snprintf_l(NULL, 0, " %s     %d", ploc, "b", 123) == 10);
    }
}

