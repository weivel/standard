#ifndef WEIVEL_STANDARD_JSON_LOCALE_H
#define WEIVEL_STANDARD_JSON_LOCALE_H

#include <stdint.h>

#ifdef WEIVEL_STANDARD_TESTING
/** How many times the reader has asked the C library for its decimal point since the process started. */
int64_t weivel_standard_json_locale_reads(void);
#endif

#endif
