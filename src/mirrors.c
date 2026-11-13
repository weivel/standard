#include "weivel/mirrors.h"

#include "allocation.h"
#include "files.h"
#include "report.h"
#include "sized.h"
#include "weivel/sha256.h"

#include <string.h>

typedef struct mirror_storage {
    weivel_json_value *document;
    const char **templates;
} mirror_storage;

static weivel_status refuse(weivel_standard_refusal *refusal, uint32_t code, char *path, char *detail, char *fix) {
    weivel_standard_refusal_texts texts;
    texts.detail = detail;
    texts.fix = fix;
    texts.path = path;
    if (refusal == NULL) {
        weivel_standard_release_texts(&texts);
        return detail != NULL && fix != NULL && path != NULL ? WEIVEL_STATUS_REFUSED : WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    return weivel_standard_fill_refusal(refusal, code, texts) ? WEIVEL_STATUS_REFUSED : WEIVEL_STATUS_OUT_OF_MEMORY;
}

static weivel_status refuse_at(weivel_standard_refusal *refusal, uint32_t code, const weivel_standard_field_path *at,
                               const char *what, const char *fix) {
    char *path = weivel_standard_path_text(at);
    char *detail = path == NULL ? NULL
                                : weivel_standard_format_text("%s %s", path[0] == '\0' ? "the mirror list" : path, what);
    return refuse(refusal, code, path, detail, weivel_standard_format_text("%s", fix));
}

static size_t count_placeholders(const char *text) {
    const size_t placeholder_length = strlen(WEIVEL_STANDARD_MIRROR_PLACEHOLDER);
    size_t count = 0;
    const char *found = strstr(text, WEIVEL_STANDARD_MIRROR_PLACEHOLDER);
    while (found != NULL) {
        count++;
        found = strstr(found + placeholder_length, WEIVEL_STANDARD_MIRROR_PLACEHOLDER);
    }
    return count;
}

#define TEMPLATE_FIX "write the entry as a URL holding " WEIVEL_STANDARD_MIRROR_PLACEHOLDER " exactly once"

static weivel_status check_entry(weivel_standard_refusal *refusal, const weivel_standard_field_path *at,
                                 const weivel_json_value *entry) {
    const char *text;
    if (weivel_json_kind_of(entry) != WEIVEL_JSON_STRING) {
        return refuse_at(refusal, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, at, "must be a string", TEMPLATE_FIX);
    }
    text = weivel_json_as_string(entry);
    if (strlen(text) != weivel_json_string_length(entry)) {
        return refuse_at(refusal, WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, at, "must not hold a NUL character",
                         TEMPLATE_FIX);
    }
    if (count_placeholders(text) != 1) {
        return refuse_at(refusal, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, at,
                         "must hold " WEIVEL_STANDARD_MIRROR_PLACEHOLDER " exactly once", TEMPLATE_FIX);
    }
    return WEIVEL_STATUS_OK;
}

static weivel_status check_mirrors(weivel_standard_refusal *refusal, const weivel_json_value *document) {
    weivel_standard_field_path field = {NULL, "mirrors", 0};
    const weivel_json_value *mirrors;
    size_t count;
    size_t at;
    if (weivel_json_kind_of(document) != WEIVEL_JSON_OBJECT) {
        return refuse_at(refusal, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, NULL, "must be an object",
                         "write the mirror list as {\"mirrors\": [\"<url holding " WEIVEL_STANDARD_MIRROR_PLACEHOLDER
                         ">\"]}");
    }
    mirrors = weivel_json_object_get(document, "mirrors");
    if (mirrors == NULL) {
        return refuse_at(refusal, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, &field, "is required",
                         "add a \"mirrors\" array of URL templates");
    }
    if (weivel_json_kind_of(mirrors) != WEIVEL_JSON_ARRAY) {
        return refuse_at(refusal, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, &field, "must be an array",
                         "make \"mirrors\" an array of URL templates");
    }
    count = weivel_json_array_length(mirrors);
    if (count == 0) {
        return refuse_at(refusal, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, &field, "must not be empty",
                         "list at least one mirror");
    }
    for (at = 0; at < count; at++) {
        weivel_standard_field_path entry_path = {&field, NULL, at};
        weivel_status status = check_entry(refusal, &entry_path, weivel_json_array_at(mirrors, at));
        if (status != WEIVEL_STATUS_OK) {
            return status;
        }
    }
    return WEIVEL_STATUS_OK;
}

static void release_storage(mirror_storage *storage) {
    if (storage == NULL) {
        return;
    }
    weivel_json_free(storage->document);
    weivel_standard_memory_release((void *)storage->templates);
    weivel_standard_memory_release(storage);
}

/** Takes `document`, which has passed check_mirrors, whatever it answers. */
static weivel_status build_list(weivel_json_value *document, weivel_standard_mirror_list *list) {
    const weivel_json_value *mirrors = weivel_json_object_get(document, "mirrors");
    size_t count = weivel_json_array_length(mirrors);
    mirror_storage *storage = weivel_standard_memory_allocate_zeroed(1, sizeof *storage);
    size_t at;
    if (storage == NULL) {
        weivel_json_free(document);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    storage->document = document;
    storage->templates = weivel_standard_memory_allocate_zeroed(count, sizeof *storage->templates);
    if (storage->templates == NULL) {
        release_storage(storage);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    for (at = 0; at < count; at++) {
        storage->templates[at] = weivel_json_as_string(weivel_json_array_at(mirrors, at));
    }
    list->templates = storage->templates;
    list->count = count;
    list->storage = storage;
    return WEIVEL_STATUS_OK;
}

static weivel_status refuse_malformed_call(const char *json, const weivel_standard_mirror_list *list,
                                           weivel_standard_refusal *refusal) {
    if (list == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_read_mirrors was given no list to fill",
                                           "pass the address of an empty weivel_standard_mirror_list");
    }
    if (json == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_read_mirrors was given no JSON text",
                                           "pass the mirror list's text");
    }
    if (list->size < sizeof *list) {
        return weivel_standard_refuse_call(refusal,
                                           "weivel_standard_read_mirrors was given a list smaller than its first shape",
                                           "set the list's size, as WEIVEL_INIT does");
    }
    return weivel_standard_refuse_call(refusal, "weivel_standard_read_mirrors was given a list still holding one",
                                       "release the list first, or pass an empty one");
}

weivel_status weivel_standard_read_mirrors(const char *json, size_t length, weivel_standard_mirror_list *list,
                                           weivel_standard_refusal *refusal) {
    weivel_json_error error = {.size = (uint32_t)sizeof(weivel_json_error)};
    weivel_json_value *document;
    weivel_status status;
    if (weivel_standard_refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (json == NULL || list == NULL || list->size < sizeof *list || list->storage != NULL) {
        return refuse_malformed_call(json, list, refusal);
    }
    weivel_standard_mirror_list_release(list);
    document = weivel_json_parse(json, length, &error);
    if (document == NULL) {
        return error.out_of_memory ? WEIVEL_STATUS_OUT_OF_MEMORY
                                   : weivel_standard_refuse_malformed_json(refusal, &error);
    }
    status = check_mirrors(refusal, document);
    if (status != WEIVEL_STATUS_OK) {
        weivel_json_free(document);
        return status;
    }
    return build_list(document, list);
}

static weivel_status refuse_file(weivel_standard_refusal *refusal, uint32_t code, const char *path, const char *what,
                                 const char *fix) {
    return refuse(refusal, code, weivel_standard_format_text("%s", ""),
                  weivel_standard_format_text("%s %s", path, what), weivel_standard_format_text("%s", fix));
}

static weivel_status refuse_file_outcome(weivel_standard_refusal *refusal, const char *path,
                                         weivel_standard_files_outcome outcome) {
    switch (outcome) {
    case WEIVEL_STANDARD_FILES_OUT_OF_MEMORY:
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    case WEIVEL_STANDARD_FILES_MISSING:
        return refuse_file(refusal, WEIVEL_STANDARD_REFUSAL_NOT_FOUND, path, "does not exist",
                           "create the mirror list or correct its path");
    case WEIVEL_STANDARD_FILES_TOO_LARGE:
        return refuse_file(refusal, WEIVEL_STANDARD_REFUSAL_UNREADABLE, path, "is larger than 1 MiB",
                           "shorten the mirror list; it is a few lines");
    default:
        return refuse_file(refusal, WEIVEL_STANDARD_REFUSAL_UNREADABLE, path, "cannot be read",
                           "make the mirror list a regular file this process can read");
    }
}

/** The file module reports a file that is not there as unreadable, so absence is asked of it separately. */
static int is_absent(const char *path) {
    weivel_standard_file_kind kind;
    return weivel_standard_files_kind(path, &kind) == WEIVEL_STANDARD_FILES_DONE && kind == WEIVEL_STANDARD_FILE_NOTHING;
}

weivel_status weivel_standard_load_mirrors(const char *path, weivel_standard_mirror_list *list,
                                           weivel_standard_refusal *refusal) {
    weivel_standard_files_outcome outcome;
    weivel_status status;
    char *bytes = NULL;
    size_t length = 0;
    if (weivel_standard_refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (path == NULL || list == NULL || list->size < sizeof *list || list->storage != NULL) {
        return refuse_malformed_call(path, list, refusal);
    }
    outcome = weivel_standard_files_read(path, &bytes, &length);
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        return refuse_file_outcome(refusal, path, outcome == WEIVEL_STANDARD_FILES_UNREADABLE && is_absent(path)
                                                      ? WEIVEL_STANDARD_FILES_MISSING
                                                      : outcome);
    }
    status = weivel_standard_read_mirrors(bytes, length, list, refusal);
    weivel_standard_memory_release(bytes);
    return status;
}

void weivel_standard_mirror_list_release(weivel_standard_mirror_list *list) {
    weivel_standard_mirror_list empty = WEIVEL_INIT(weivel_standard_mirror_list);
    if (list == NULL || list->size < sizeof *list) {
        return;
    }
    release_storage(list->storage);
    weivel_standard_sized_write(list, &empty, sizeof empty);
}

static weivel_status refuse_expansion(weivel_standard_refusal *refusal, const char *what, const char *fix) {
    return refuse(refusal, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, weivel_standard_format_text("%s", ""),
                  weivel_standard_format_text("%s", what), weivel_standard_format_text("%s", fix));
}

static char *splice_hash(const char *template_text, const char *hash_hex) {
    const char *placeholder = strstr(template_text, WEIVEL_STANDARD_MIRROR_PLACEHOLDER);
    size_t before = (size_t)(placeholder - template_text);
    size_t after_length = strlen(placeholder + strlen(WEIVEL_STANDARD_MIRROR_PLACEHOLDER));
    char *url = weivel_standard_memory_allocate(before + WEIVEL_STANDARD_SHA256_HEX_LENGTH + after_length + 1);
    if (url == NULL) {
        return NULL;
    }
    memcpy(url, template_text, before);
    memcpy(url + before, hash_hex, WEIVEL_STANDARD_SHA256_HEX_LENGTH);
    memcpy(url + before + WEIVEL_STANDARD_SHA256_HEX_LENGTH, placeholder + strlen(WEIVEL_STANDARD_MIRROR_PLACEHOLDER),
           after_length + 1);
    return url;
}

weivel_status weivel_standard_mirror_expand(const char *template_text, const char *hash_hex, char **url,
                                            weivel_standard_refusal *refusal) {
    if (url != NULL) {
        *url = NULL;
    }
    if (weivel_standard_refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (template_text == NULL || hash_hex == NULL || url == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_mirror_expand was given a NULL argument",
                                           "pass a template, a hash and the address of a string to fill");
    }
    if (!weivel_standard_is_sha256_hex(hash_hex)) {
        return refuse_expansion(refusal, "the hash is not 64 lowercase hexadecimal characters",
                                "pass the SHA-256 as 64 characters of 0 to 9 and a to f, lowercase");
    }
    if (count_placeholders(template_text) != 1) {
        return refuse_expansion(refusal, "the template does not hold " WEIVEL_STANDARD_MIRROR_PLACEHOLDER " exactly once",
                                TEMPLATE_FIX);
    }
    *url = splice_hash(template_text, hash_hex);
    return *url == NULL ? WEIVEL_STATUS_OUT_OF_MEMORY : WEIVEL_STATUS_OK;
}

void weivel_standard_text_release(char *text) {
    weivel_standard_memory_release(text);
}
