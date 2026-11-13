#ifndef WEIVEL_STANDARD_C_LOCALE_H
#define WEIVEL_STANDARD_C_LOCALE_H

/**
 * The C locale as an object of its own, so a number is read with '.' as its decimal point whatever locale the host
 * has set, without reading or changing the process's locale. NULL when the C library could not make one.
 */
void *weivel_standard_c_locale_create(void);

void weivel_standard_c_locale_release(void *locale);

/** Converts the whole of `number`, a validated JSON number, as strtod does in the C locale. */
double weivel_standard_c_locale_read_real(const char *number, void *locale);

#endif
