/* glibc declares strtod_l only for _GNU_SOURCE, and it must precede every include. */
#define _GNU_SOURCE

#include "c_locale.h"

#include <locale.h>
#include <stdlib.h>

#if defined(_WIN32)

void *weivel_standard_c_locale_create(void) {
    return _create_locale(LC_NUMERIC, "C");
}

void weivel_standard_c_locale_release(void *locale) {
    if (locale != NULL) {
        _free_locale((_locale_t)locale);
    }
}

double weivel_standard_c_locale_read_real(const char *number, void *locale) {
    return _strtod_l(number, NULL, (_locale_t)locale);
}

#else

#if defined(__APPLE__)
#include <xlocale.h>
#endif

void *weivel_standard_c_locale_create(void) {
    return (void *)newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
}

void weivel_standard_c_locale_release(void *locale) {
    if (locale != NULL) {
        freelocale((locale_t)locale);
    }
}

double weivel_standard_c_locale_read_real(const char *number, void *locale) {
    return strtod_l(number, NULL, (locale_t)locale);
}

#endif
