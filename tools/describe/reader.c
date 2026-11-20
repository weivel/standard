#include "reader.h"

#include <string.h>

#include "files.h"
#include "reading.h"
#include "tokens.h"

static describe_outcome read_header(describe_model *model, const char *path, describe_refusal *refusal) {
    char *bytes = NULL;
    size_t length = 0;
    describe_outcome outcome;
    switch (weivel_standard_files_read(path, &bytes, &length)) {
    case WEIVEL_STANDARD_FILES_DONE:
        break;
    case WEIVEL_STANDARD_FILES_OUT_OF_MEMORY:
        return DESCRIBE_OUT_OF_MEMORY;
    case WEIVEL_STANDARD_FILES_TOO_LARGE:
        return describe_refuse(refusal, path, 0, "the header is larger than any weivel-describe reads");
    default:
        return describe_refuse(refusal, path, 0, "the header cannot be read");
    }
    outcome = describe_parse(model, path, bytes, length, refusal);
    weivel_standard_memory_release(bytes);
    return outcome;
}

static describe_outcome finish(describe_model *model, describe_outcome outcome, const describe_names *names,
                               describe_refusal *refusal) {
    if (outcome == DESCRIBE_DONE) {
        outcome = describe_resolve(model, names, refusal);
    }
    if (outcome != DESCRIBE_DONE) {
        describe_model_release(model);
    }
    return outcome;
}

describe_outcome describe_read(describe_model *model, const char *const *paths, size_t path_count,
                               const describe_names *names, describe_refusal *refusal) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t at;
    memset(model, 0, sizeof *model);
    memset(refusal, 0, sizeof *refusal);
    for (at = 0; outcome == DESCRIBE_DONE && at < path_count; at++) {
        outcome = read_header(model, paths[at], refusal);
    }
    return finish(model, outcome, names, refusal);
}

describe_outcome describe_read_texts(describe_model *model, const char *const *paths, const char *const *texts,
                                     const size_t *lengths, size_t count, const describe_names *names,
                                     describe_refusal *refusal) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t at;
    memset(model, 0, sizeof *model);
    memset(refusal, 0, sizeof *refusal);
    for (at = 0; outcome == DESCRIBE_DONE && at < count; at++) {
        outcome = describe_parse(model, paths[at], texts[at], lengths[at], refusal);
    }
    return finish(model, outcome, names, refusal);
}
