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


class sprintf_result_matcher : public Catch::MatcherBase<std::string>
{
public:
    template<typename ...VA>
    sprintf_result_matcher(string_view fmt, locale_t loc, VA... rest)
        : format(fmt), locale_name(loc->name)
    {
        char* buf = &buffer[0];
        
        os_ret = sprintf(buf, fmt.data(), rest...);
        os_result = buf;

        poly_ret = sprintf_l(buf, fmt.data(), loc, rest...);
        poly_result = buf;
    }

    bool match(std::string const& expected) const override
    {
        return poly_result == expected && os_result == expected && os_ret == poly_ret;
    }

    std::string describe() const override
    {
        std::ostringstream ss;
        ss << "equals\n'" << poly_result<< "' and '" << os_result <<"' (poly sprintf_l and OS's sprintf).\n";
        ss << "Return values must also be the same: " << poly_ret << "==" << os_ret<<'\n';
        ss << "Format string: '" << format << "'\n";
        //ss << "Locale: '" << locale_name << '\'';

        return ss.str();
    }

private:
    char_array<1024> buffer{};
    string_view format;
    std::string os_result, poly_result, locale_name;
    int os_ret=0, poly_ret=0;
};

template<typename ...VA>
sprintf_result_matcher isResultOfFormating(string_view fmt, locale_t loc, VA... rest)
{
    return sprintf_result_matcher(fmt, loc, rest...);
}


TEST_CASE("poly sprintf_l formating tests", "[printf]") {

    std::string locname = "C";

    locale_t ploc = newlocale(LC_ALL_MASK, locname.c_str(), NULL);
    std::locale::global(std::locale{ locname });

    INFO("Locale: '" << locname << '\'');

    SECTION("Integer numbers") {
        CHECK_THAT("a b     1", isResultOfFormating("%c %s     %d", ploc, 'a', "b", 1));
        CHECK_THAT("+5", isResultOfFormating("%+2d", ploc, 5));
        CHECK_THAT("  6", isResultOfFormating("% 3i", ploc, 6));
        CHECK_THAT("-7  ", isResultOfFormating("%-4d", ploc, -7));
        CHECK_THAT("+0",isResultOfFormating("%+d", ploc, 0));
        CHECK_THAT("     00003:     00004", isResultOfFormating("%10.5d:%10.5d", ploc, 3, 4));
        CHECK_THAT("-100006789", isResultOfFormating("%d", ploc, -100006789));
        CHECK_THAT("20 0020",isResultOfFormating("%u %04u", ploc, 20u, 20u)); 
        CHECK_THAT("12 1e 3C",isResultOfFormating("%o %x %X", ploc, 10u, 30u, 60u));
        CHECK_THAT(" 12 1e 3C ",isResultOfFormating("%3o %2x %-3X", ploc, 10u, 30u, 60u));
        CHECK_THAT("012 0x1e 0X3C",isResultOfFormating("%#o %#x %#X", ploc, 10u, 30u, 60u)); 
        CHECK_THAT("",isResultOfFormating("%.0x", ploc, 0)); 
        CHECK_THAT("33 555",isResultOfFormating("%hi %ld", ploc, (short)33, 555l)); 
        CHECK_THAT("9888777666",isResultOfFormating("%llu", ploc, 9888777666llu)); 
        CHECK_THAT("-1 2 -3",isResultOfFormating("%ji %zi %ti", ploc, (intmax_t)-1, (long long)2, (ptrdiff_t)-3)); 
    }

    const double pow_2_85 = 38685626227668133590597632.0;

    SECTION("Floating point numbers") {
        CHECK_THAT(" 0", isResultOfFormating("% .0f", ploc, 0.1));
        CHECK_THAT("-3.000000", isResultOfFormating( "%f", ploc, -3.0)); 
        CHECK_THAT("-8.8888888800", isResultOfFormating( "%.10f", ploc, -8.88888888)); 
        CHECK_THAT("880.0888888800", isResultOfFormating( "%.10f", ploc, 880.08888888)); 
        CHECK_THAT("4.1", isResultOfFormating( "%.1f", ploc, 4.1));
        CHECK_THAT("0.00", isResultOfFormating( "%.2f", ploc, 1e-4)); 
        CHECK_THAT("-5.20", isResultOfFormating( "%+4.2f", ploc, -5.2)); 
        CHECK_THAT("0.0       ", isResultOfFormating( "%-10.1f", ploc, 0.)); 
        CHECK_THAT("-0.000000", isResultOfFormating( "%f", ploc, -0.)); 
        CHECK_THAT("0.000001", isResultOfFormating( "%f", ploc, 9.09834e-07)); 
        CHECK_THAT("38685626227668133590597632.0", isResultOfFormating( "%.1f", ploc, pow_2_85)); 
        CHECK_THAT("0.000000499999999999999977", isResultOfFormating( "%.24f", ploc, 5e-7)); 
        CHECK_THAT("0.000000000000000020000000", isResultOfFormating( "%.24f", ploc, 2e-17)); 
        CHECK_THAT("0.0000000100 100000000", isResultOfFormating( "%.10f %.0f", ploc, 1e-8, 1e+8)); 
        CHECK_THAT("100056789.0", isResultOfFormating( "%.1f", ploc, 100056789.0)); 
        CHECK_THAT(" 1.23 %", isResultOfFormating( "%*.*f %%", ploc, 5, 2, 1.23)); 
        CHECK_THAT("-3.000000e+00", isResultOfFormating( "%e", ploc, -3.0)); 
        CHECK_THAT("4.1E+00", isResultOfFormating( "%.1E", ploc, 4.1)); 
        CHECK_THAT("-5.20e+00", isResultOfFormating( "%+4.2e", ploc, -5.2)); 
        CHECK_THAT("+0.3 -3", isResultOfFormating( "%+g %+g", ploc, 0.3, -3.0)); 
        CHECK_THAT("4", isResultOfFormating( "%.1G", ploc, 4.1)); 
        CHECK_THAT("-5.2", isResultOfFormating( "%+4.2g", ploc, -5.2)); 
        CHECK_THAT("3e-300", isResultOfFormating( "%g", ploc, 3e-300)); 
        CHECK_THAT("1", isResultOfFormating( "%.0g", ploc, 1.2)); 
        CHECK_THAT(" 3.7 3.71", isResultOfFormating( "% .3g %.3g", ploc, 3.704, 3.706));
        CHECK_THAT("2e-315:1e+308", isResultOfFormating( "%g:%g", ploc, 2e-315, 1e+308)); 
        CHECK_THAT("0x1.fedcbap+98", isResultOfFormating( "%a", ploc, 0x1.fedcbap+98)); 
        CHECK_THAT("0x1.999999999999a0p-4", isResultOfFormating( "%.14a", ploc, 0.1)); 
        CHECK_THAT("0x1.0p-1022", isResultOfFormating( "%.1a", ploc, 0x1.ffp-1023)); 
        CHECK_THAT("0x1.0091177587f83p-1022", isResultOfFormating( "%a", ploc, 2.23e-308)); 
        CHECK_THAT("-0X1.AB0P-5", isResultOfFormating( "%.3A", ploc, -0X1.abp-5)); 
        CHECK_THAT("(nil)", isResultOfFormating( "%p", ploc, (void*)NULL)); 
    }

    freelocale(ploc);
}

TEST_CASE("poly sprintf_l tests", "[printf]")
{
    char_array<1024> buffer{}; auto* buf = &buffer[0];
    std::string locname = "C";

    locale_t ploc = newlocale(LC_ALL_MASK, locname.c_str(), NULL);
    std::locale::global(std::locale{ locname });

    REQUIRE(sprintf_l(buf, "%d  %600s", ploc, 3, "abc") == 603);
}


TEST_CASE("poly snprintf_l tests", "[printf]") {
    string_view locname = "C", result, expected;
    locale_t ploc = newlocale(LC_ALL_MASK, locname.data(), NULL);

    char_array<1024> buffer{}; auto* buf = &buffer[0]; 
    string_view bufview = { buf, 1024 };

    const double pow_2_75 = 37778931862957161709568.0;
    int ret;

    INFO("using locale '" << locname << '\'');

    CHECK_THAT(" b     123", isResultOfFormating(" %s     %d", ploc, "b", 123));

    CHECK(snprintf_l(buf, 100, "%f", ploc, pow_2_75) == 30);
    result = { buf, 17 }; expected = "37778931862957161709568.000000";
    REQUIRE(result == expected.substr(0, 17));

    auto n = snprintf_l(buf, 10, "number %f", ploc, 123.456789);
    expected = "number 12";
    result = bufview.substr(0, expected.size());
    REQUIRE(result == expected);
    REQUIRE(n == 17); // written vs would-be written bytes
    n = snprintf_l(buf, 0, "7 chars", ploc);
    REQUIRE(n == 7);

    //REQUIRE(strlen(buf) == 603);
    snprintf_l(buf, 550, "%d  %600s", ploc, 3, "abc");
    result = buf;

    CHECK(result.size() == 549);
    REQUIRE(snprintf_l(buf, 600, "%510s     %c", ploc, "a", 'b') == 516);

    // length check
    REQUIRE(snprintf_l(NULL, 0, " %s     %d", ploc, "b", 123) == 10);

    freelocale(ploc);
}

