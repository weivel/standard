#ifndef WEIVEL_STANDARD_PATTERNS_H
#define WEIVEL_STANDARD_PATTERNS_H

#include <stddef.h>

/**
 * Hand-written matchers for the `pattern` strings of `schema/plugin.schema.json`, each deciding exactly what its
 * ECMAScript expression decides over the whole of `text`, which is `length` bytes of UTF-8 and may hold a NUL. The
 * schema parity test pins each pattern string to its matcher, so a changed pattern is red until its matcher is
 * reviewed.
 */
typedef int (*weivel_standard_pattern_matcher)(const char *text, size_t length);

/** `importName`: lowercase words joined by single hyphens or dots, not a Windows device name. */
int weivel_standard_matches_import_name(const char *text, size_t length);

/**
 * A capability id, wherever one is written: a manifest's `capabilities.provides` and `capabilities.consumes`, and a
 * library declaration's `contracts[].capability`. Lowercase words joined by single hyphens or dots.
 */
int weivel_standard_matches_capability_id(const char *text, size_t length);

/** A library declaration's `services[].id`: the capability id grammar, which the two share by rule. */
int weivel_standard_matches_service_id(const char *text, size_t length);

/** `language`: a lowercase letter, then lowercase letters and digits, in words joined by single hyphens. */
int weivel_standard_matches_language(const char *text, size_t length);

/** `entry`: no leading separator or drive, no control or line-separator character, no `..` segment. */
int weivel_standard_matches_entry(const char *text, size_t length);

/** A repository identity: `local:<name>` or `git:<40 or 64 lowercase hex>`, then path segments, none `.` or `..`. */
int weivel_standard_matches_repository_id(const char *text, size_t length);

/** A dependency's `name`: `owner/name`, the name neither `.` nor `..`. */
int weivel_standard_matches_owner_and_name(const char *text, size_t length);

#endif
