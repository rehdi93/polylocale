/*
    printf format constants
*/

// char, string, pointer, signed integer unsigned integer, floating point
#define FMT_CHAR          "Cc"
#define FMT_STRING        "Ss"
#define FMT_POINTER       "p"
#define FMT_SINT          "di"
#define FMT_UINT          "uoxX"
#define FMT_FLOATING_PT   "FfEeAaGg"

#define FMT_TYPE          FMT_CHAR FMT_STRING FMT_POINTER FMT_SINT FMT_UINT FMT_FLOATING_PT
#define FMT_FLAGS         "-+#0 "
#define FMT_LENGTHMOD     "hljztLI"
#define FMT_SPECIAL       "%.*"

constexpr auto FMT_START = FMT_SPECIAL[0], FMT_PREC = FMT_SPECIAL[1], FMT_VA = FMT_SPECIAL[2];
