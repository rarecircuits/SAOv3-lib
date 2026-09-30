#include "saoh/consts/err.h"

#define countof(x) (sizeof(x) / sizeof(*(x)))

#define X(val, ident) [-(val)] = #ident,
static const char *const saoh_error_table[] = { SAOH_ERROR_DEFS };
#undef X

const char *saoh_err_str(saoh_err_t code)
{
    if (code > 0) {
        return "<Invalid Positive Error>";
    }

    code = -code;
    if ((unsigned int) code >= (countof(saoh_error_table))) {
        return "<Invalid Error Code>";
    }

    return saoh_error_table[code];
}
