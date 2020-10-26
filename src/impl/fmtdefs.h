/*
    printf format constants
*/
#define FMT_CHAR          "Cc"
#define FMT_STRING        "Ss"
#define FMT_POINTER       FMT_STRING "p"
#define FMT_INTEGER       "dioxXu"
#define FMT_FLOATING_PT   "FfEeAaGg"

#define FMT_ALL_TYPES     FMT_CHAR FMT_POINTER FMT_INTEGER FMT_FLOATING_PT
#define FMT_FLAGS         "-+#0 "
#define FMT_LENGTHMOD     "hljztLI"

namespace FMT {

    constexpr char Start = '%', Precision = '.', FromVa = '*';
    constexpr char Length[] = FMT_LENGTHMOD;
    constexpr char Flags[] = FMT_FLAGS;
    constexpr char Types[] = FMT_ALL_TYPES;

    // format specifier regex
    // https://regex101.com/r/Imw6fT/2
    constexpr auto Pattern = "%"
        "([" FMT_FLAGS "]+)?"
        "(" R"([0-9]+|\*)" ")?" // field width
        "(" R"(\.[0-9]+|\.\*)" ")?" // precision
        "(" "h|hh|l|ll|j|z|t|L|I32|I64" ")?" // length
        "([" FMT_ALL_TYPES "])";
} // FMT

#undef FMT_FLAGS
#undef FMT_ALL_TYPES
#undef FMT_CHAR
#undef FMT_STRING
#undef FMT_INTEGER
#undef FMT_FLOATING_PT
#undef FMT_LENGTHMOD
