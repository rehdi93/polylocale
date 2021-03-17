/*
    printf format constants
*/

// char, string, pointer, signed integer unsigned integer, floating point
#define FMT_CHAR          "Cc"
#define FMT_STRING        "Ss"
#define FMT_POINTER       "p"
#define FMT_SINT          "di"
#define FMT_UINT          "uoxX"
#define FMT_INTEGER       FMT_SINT FMT_UINT
#define FMT_FLOATING_PT   "FfEeAaGg"

#define FMT_TYPE          FMT_CHAR FMT_STRING FMT_POINTER FMT_INTEGER FMT_FLOATING_PT
#define FMT_FLAGS         "-+#0 "
#define FMT_LENGTHMOD     "hljztLI"
#define FMT_SPECIAL       "%.*"

constexpr auto FMT_START = FMT_SPECIAL[0], FMT_PREC = FMT_SPECIAL[1], FMT_VA = FMT_SPECIAL[2];

//namespace FMT {
//    constexpr char Start = '%', Precision = '.', FromVa = '*';
//    constexpr char Length[] = FMT_LENGTHMOD;
//    constexpr char Flags[] = FMT_FLAGS;
//    constexpr char Types[] = FMT_ALL_TYPES;
//} // FMT
