#ifndef PROBE_SKIPPED_H
#define PROBE_SKIPPED_H

/* A plain block comment, which documents nothing. */
// A line comment.

#include <stdint.h>

/** A doc a directive separates from what follows, so it documents nothing. First shape ends at `nothing`. */
#define PROBE_MACRO(a, b) \
    ((a) + /* inside */   \
     (b))

#if defined(_WIN32)
#define PROBE_API __declspec(dllexport)
#else
#define PROBE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** A function declaration, skipped whatever its export macro. */
PROBE_API const char *probe_name(void);
PROBE_API uint32_t probe_count(const char *name, uint32_t (*callback)(void *context));
void *probe_raw(void);

/**/
typedef struct probe_plain {
    uint32_t value; // a line comment after a member
    /* a block comment before a member, which leaves the member undocumented */
    uint64_t other;
} probe_plain;

#ifdef __cplusplus
}
#endif

#endif
