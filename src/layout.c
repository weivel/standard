#include "weivel/layout.h"

#include "allocation.h"
#include "files.h"
#include "patterns.h"
#include "report.h"
#include "sized.h"
#include "weivel/json.h"

#include <stdlib.h>
#include <string.h>

#define MANIFEST_SUFFIX ".json"
#define IDENTITY_SUFFIX "_identity.json"
#define DISCOVERY_FIRST_SHAPE (offsetof(weivel_standard_discovery, refusal) + sizeof(weivel_standard_refusal))

typedef weivel_standard_discovered_plugin discovered_plugin;
typedef weivel_standard_discovery_failure discovery_failure;

typedef struct plugin_list {
    discovered_plugin *items;
    size_t count;
    size_t capacity;
} plugin_list;

typedef struct failure_list {
    discovery_failure *items;
    size_t count;
    size_t capacity;
} failure_list;

typedef struct scan {
    const char *directory;
    const weivel_standard_validation_options *options;
    plugin_list plugins;
    failure_list failures;
    /** The refusal weivel_standard_load_manifest answered a malformed call with, as malformed `options`. */
    weivel_standard_refusal call_refusal;
} scan;

static char *copy_text(const char *text) {
    size_t length = strlen(text);
    char *copy = weivel_standard_memory_allocate(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static int is_separator(char character) {
#ifdef _WIN32
    return character == '/' || character == '\\';
#else
    return character == '/';
#endif
}

static int ends_with_separator(const char *directory) {
    size_t length = strlen(directory);
    return length > 0 && is_separator(directory[length - 1]);
}

static char *join_path(const char *directory, const char *name) {
    return weivel_standard_format_text("%s%s%s", directory, ends_with_separator(directory) ? "" : "/", name);
}

static int grow(void **items, size_t *capacity, size_t count, size_t item_size) {
    size_t wanted;
    void *grown;
    if (count < *capacity) {
        return 1;
    }
    wanted = *capacity == 0 ? 8 : *capacity * 2;
    grown = weivel_standard_memory_reallocate(*items, wanted * item_size);
    if (grown == NULL) {
        return 0;
    }
    *items = grown;
    *capacity = wanted;
    return 1;
}

static void release_plugin(discovered_plugin *plugin) {
    discovered_plugin empty = {0};
    empty.identity.size = (uint32_t)sizeof empty.identity;
    weivel_standard_manifest_release((weivel_standard_manifest *)plugin->manifest);
    weivel_standard_memory_release((char *)plugin->manifest_path);
    weivel_standard_memory_release((char *)plugin->identity_path);
    weivel_standard_memory_release((char *)plugin->identity_id);
    weivel_standard_memory_release((char *)plugin->repository_name);
    weivel_standard_memory_release((char *)plugin->entry_path);
    weivel_standard_identity_release(&plugin->identity);
    *plugin = empty;
}

static void release_failure(discovery_failure *failure) {
    weivel_standard_memory_release((char *)failure->file);
    weivel_standard_refusal_release(&failure->refusal);
    failure->file = NULL;
}

static void release_lists(plugin_list *plugins, failure_list *failures) {
    size_t at;
    for (at = 0; at < plugins->count; at++) {
        release_plugin(&plugins->items[at]);
    }
    for (at = 0; at < failures->count; at++) {
        release_failure(&failures->items[at]);
    }
    weivel_standard_memory_release(plugins->items);
    weivel_standard_memory_release(failures->items);
}

/** Takes what `refusal` holds whatever it answers, leaving it empty. */
static weivel_status add_failure(scan *state, const char *file, weivel_standard_refusal *refusal) {
    failure_list *failures = &state->failures;
    char *copy = copy_text(file);
    if (copy == NULL ||
        !grow((void **)&failures->items, &failures->capacity, failures->count, sizeof *failures->items)) {
        weivel_standard_memory_release(copy);
        weivel_standard_refusal_release(refusal);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    failures->items[failures->count].file = copy;
    failures->items[failures->count].refusal = *refusal;
    failures->count++;
    weivel_standard_refusal_forget(refusal);
    return WEIVEL_STATUS_OK;
}

/** Takes `detail` and `fix`, either of which may be NULL when memory ran out building it. */
static weivel_status refuse_file(scan *state, const char *file, uint32_t code, const char *field, char *detail,
                                 char *fix) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_standard_refusal_texts texts;
    texts.detail = detail;
    texts.fix = fix;
    texts.path = copy_text(field);
    if (!weivel_standard_fill_refusal(&refusal, code, texts)) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    return add_failure(state, file, &refusal);
}

static weivel_status refuse_too_large(scan *state, const char *file) {
    return refuse_file(state, file, WEIVEL_STANDARD_REFUSAL_UNREADABLE, "",
                       weivel_standard_format_text("%s is larger than the %zu bytes a laid-out file may hold", file,
                                                   WEIVEL_STANDARD_FILES_LARGEST_READ),
                       weivel_standard_format_text("make %s at most %zu bytes", file,
                                                   WEIVEL_STANDARD_FILES_LARGEST_READ));
}

static weivel_status refuse_unreadable(scan *state, const char *file) {
    return refuse_file(state, file, WEIVEL_STANDARD_REFUSAL_UNREADABLE, "",
                       weivel_standard_format_text("%s cannot be read", file),
                       weivel_standard_format_text("make %s a regular file this process can read", file));
}

/** Reads a whole file, recording a failure for one it cannot; `*bytes` is NULL unless it answers OK with them. */
static weivel_status read_file(scan *state, const char *file, char **bytes, size_t *length) {
    weivel_standard_files_outcome outcome = weivel_standard_files_read(file, bytes, length);
    if (outcome == WEIVEL_STANDARD_FILES_OUT_OF_MEMORY) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if (outcome == WEIVEL_STANDARD_FILES_TOO_LARGE) {
        return refuse_too_large(state, file);
    }
    return outcome == WEIVEL_STANDARD_FILES_DONE ? WEIVEL_STATUS_OK : refuse_unreadable(state, file);
}

/** Loads the manifest at `file`, recording a failure for one that does not load; `*manifest` is NULL then. */
static weivel_status load_manifest(scan *state, const char *file, weivel_standard_manifest **manifest) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_status status;
    char *bytes;
    size_t length;
    *manifest = NULL;
    status = read_file(state, file, &bytes, &length);
    if (status != WEIVEL_STATUS_OK || bytes == NULL) {
        return status;
    }
    status = weivel_standard_load_manifest(bytes, length, state->options, manifest, &validation);
    weivel_standard_memory_release(bytes);
    if (status == WEIVEL_STATUS_REFUSED) {
        status = add_failure(state, file, &validation.refusal);
    } else if (status == WEIVEL_STATUS_MALFORMED) {
        state->call_refusal = validation.refusal;
        weivel_standard_refusal_forget(&validation.refusal);
    }
    weivel_standard_validation_release(&validation);
    return status;
}

static weivel_status refuse_identity_field(scan *state, const char *file, uint32_t code, const char *field,
                                           const char *what, const char *fix) {
    return refuse_file(state, file, code, field, weivel_standard_format_text("%s: \"%s\" %s", file, field, what),
                       weivel_standard_format_text("%s", fix));
}

#define IDENTITY_FIX "write the identity file as {\"id\": \"<identity>\", \"name\": \"<repository name>\"}"

/** The identity file's `field` as a string with no NUL, or NULL with a failure recorded and `*status` set. */
static const char *identity_text(scan *state, const char *file, const weivel_json_value *document, const char *field,
                                 weivel_status *status) {
    const weivel_json_value *value = weivel_json_object_get(document, field);
    const char *text = weivel_json_as_string(value);
    if (value == NULL) {
        *status = refuse_identity_field(state, file, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, field, "is missing",
                                        IDENTITY_FIX);
    } else if (text == NULL) {
        *status = refuse_identity_field(state, file, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, field, "is not a string",
                                        IDENTITY_FIX);
    } else if (strlen(text) != weivel_json_string_length(value)) {
        *status = refuse_identity_field(state, file, WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, field,
                                        "holds a NUL character", "remove the NUL character");
    } else if (text[0] == '\0') {
        *status = refuse_identity_field(state, file, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, field, "is empty",
                                        IDENTITY_FIX);
    } else {
        return text;
    }
    return NULL;
}

/** Parses `id` into the plugin's identity, recording a failure at path `id` for one that is not an identity. */
static weivel_status parse_identity(scan *state, const char *file, const char *id, discovered_plugin *plugin,
                                    int *accepted) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_status status = weivel_standard_identity_parse(id, strlen(id), &plugin->identity, &refusal);
    if (status == WEIVEL_STATUS_REFUSED) {
        status = refuse_file(state, file, refusal.code, "id",
                             weivel_standard_format_text("%s: \"id\" is \"%s\": %s", file, id, refusal.detail),
                             weivel_standard_format_text("%s", refusal.fix));
        weivel_standard_refusal_release(&refusal);
        return status;
    }
    *accepted = status == WEIVEL_STATUS_OK;
    return status;
}

static weivel_status take_identity_document(scan *state, const char *file, const weivel_json_value *document,
                                            discovered_plugin *plugin, int *accepted) {
    weivel_status status = WEIVEL_STATUS_OK;
    const char *id;
    const char *name;
    if (weivel_json_kind_of(document) != WEIVEL_JSON_OBJECT) {
        return refuse_file(state, file, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "",
                           weivel_standard_format_text("%s is not a JSON object", file),
                           weivel_standard_format_text("%s", IDENTITY_FIX));
    }
    id = identity_text(state, file, document, "id", &status);
    if (id == NULL) {
        return status;
    }
    name = identity_text(state, file, document, "name", &status);
    if (name == NULL) {
        return status;
    }
    plugin->identity_id = copy_text(id);
    plugin->repository_name = copy_text(name);
    if (plugin->identity_id == NULL || plugin->repository_name == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    return parse_identity(state, file, id, plugin, accepted);
}

static weivel_status read_identity_file(scan *state, const char *file, discovered_plugin *plugin, int *accepted) {
    weivel_json_error error = {.size = (uint32_t)sizeof(weivel_json_error)};
    weivel_json_value *document;
    weivel_status status;
    char *bytes;
    size_t length;
    status = read_file(state, file, &bytes, &length);
    if (status != WEIVEL_STATUS_OK || bytes == NULL) {
        return status;
    }
    document = weivel_json_parse(bytes, length, &error);
    weivel_standard_memory_release(bytes);
    if (document == NULL) {
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        if (error.out_of_memory) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
        status = weivel_standard_refuse_malformed_json(&refusal, &error);
        return status == WEIVEL_STATUS_REFUSED ? add_failure(state, file, &refusal) : status;
    }
    status = take_identity_document(state, file, document, plugin, accepted);
    weivel_json_free(document);
    return status;
}

/**
 * Reads `<importName>_identity.json` beside the manifest into the plugin. `*accepted` is 1 when there is none or it
 * is a sound one, and 0 when a failure was recorded instead: an identity file that is there and cannot be trusted
 * refuses the plugin, never leaves it reading as local.
 */
static weivel_status read_identity(scan *state, const char *import_name, discovered_plugin *plugin, int *accepted) {
    weivel_standard_file_kind kind;
    char *file = weivel_standard_format_text("%s%s%s%s", state->directory,
                                             ends_with_separator(state->directory) ? "" : "/", import_name,
                                             IDENTITY_SUFFIX);
    weivel_status status;
    *accepted = 0;
    if (file == NULL || weivel_standard_files_kind(file, &kind) != WEIVEL_STANDARD_FILES_DONE) {
        weivel_standard_memory_release(file);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if (kind == WEIVEL_STANDARD_FILE_NOTHING) {
        weivel_standard_memory_release(file);
        *accepted = 1;
        return WEIVEL_STATUS_OK;
    }
    status = kind == WEIVEL_STANDARD_FILE_REGULAR || kind == WEIVEL_STANDARD_FILE_UNKNOWN
                 ? read_identity_file(state, file, plugin, accepted)
                 : refuse_unreadable(state, file);
    if (*accepted) {
        plugin->identity_path = file;
    } else {
        weivel_standard_memory_release(file);
    }
    return status;
}

/** The kind of what `path` names up to `end`, a separator in it. */
static weivel_standard_files_outcome kind_up_to(char *path, size_t end, weivel_standard_file_kind *kind) {
    char kept = path[end];
    weivel_standard_files_outcome outcome;
    path[end] = '\0';
    outcome = weivel_standard_files_kind(path, kind);
    path[end] = kept;
    return outcome;
}

/**
 * Whether the entry, from `entry_start` in `path`, is a regular file reached through real directories only: a link at
 * any component, as `lib` in `lib/main.js`, could lead outside the directory, so such an entry answers not present.
 */
static weivel_status entry_is_present(char *path, size_t entry_start, uint32_t *present) {
    weivel_standard_file_kind kind = WEIVEL_STANDARD_FILE_DIRECTORY;
    size_t at;
    *present = 0;
    for (at = entry_start + 1; path[at] != '\0' && kind == WEIVEL_STANDARD_FILE_DIRECTORY; at++) {
        if (is_separator(path[at]) && kind_up_to(path, at, &kind) != WEIVEL_STANDARD_FILES_DONE) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
    }
    if (kind != WEIVEL_STANDARD_FILE_DIRECTORY) {
        return WEIVEL_STATUS_OK;
    }
    if (weivel_standard_files_kind(path, &kind) != WEIVEL_STANDARD_FILES_DONE) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    *present = kind == WEIVEL_STANDARD_FILE_REGULAR;
    return WEIVEL_STATUS_OK;
}

static weivel_status resolve_entry(scan *state, const char *entry, discovered_plugin *plugin) {
    char *path;
    if (entry == NULL) {
        return WEIVEL_STATUS_OK;
    }
    path = join_path(state->directory, entry);
    plugin->entry_path = path;
    if (path == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    return entry_is_present(path, strlen(path) - strlen(entry), &plugin->entry_present);
}

static weivel_status refuse_mismatch(scan *state, const char *file, const char *file_name_import_name,
                                     const char *import_name) {
    return refuse_file(state, file, WEIVEL_STANDARD_REFUSAL_NAME_MISMATCH, "importName",
                       weivel_standard_format_text("%s states the importName \"%s\", and its file name says \"%s\"",
                                                   file, import_name, file_name_import_name),
                       weivel_standard_format_text("rename the file to %s.json, or set its importName to \"%s\"",
                                                   import_name, file_name_import_name));
}

static weivel_status keep_plugin(scan *state, discovered_plugin *plugin) {
    plugin_list *plugins = &state->plugins;
    if (!grow((void **)&plugins->items, &plugins->capacity, plugins->count, sizeof *plugins->items)) {
        release_plugin(plugin);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    plugins->items[plugins->count++] = *plugin;
    return WEIVEL_STATUS_OK;
}

/** Completes a plugin whose manifest loaded from a file named for `import_name`, and keeps it or records why not. */
static weivel_status complete_plugin(scan *state, const char *import_name, discovered_plugin *plugin) {
    const weivel_manifest *view = weivel_standard_manifest_view(plugin->manifest);
    weivel_status status;
    int accepted;
    if (strcmp(view->import_name, import_name) != 0) {
        status = refuse_mismatch(state, plugin->manifest_path, import_name, view->import_name);
        release_plugin(plugin);
        return status;
    }
    status = read_identity(state, import_name, plugin, &accepted);
    if (status == WEIVEL_STATUS_OK && accepted) {
        status = resolve_entry(state, view->entry, plugin);
    }
    if (status != WEIVEL_STATUS_OK || !accepted) {
        release_plugin(plugin);
        return status;
    }
    return keep_plugin(state, plugin);
}

/** The importName a file name lays a manifest out for, or NULL when it is not `<importName>.json`. */
static char *import_name_of(const char *file_name, int *out_of_memory) {
    size_t length = strlen(file_name);
    size_t suffix_length = strlen(MANIFEST_SUFFIX);
    char *import_name;
    *out_of_memory = 0;
    if (length <= suffix_length || strcmp(file_name + length - suffix_length, MANIFEST_SUFFIX) != 0 ||
        !weivel_standard_matches_import_name(file_name, length - suffix_length)) {
        return NULL;
    }
    import_name = weivel_standard_memory_allocate(length - suffix_length + 1);
    if (import_name == NULL) {
        *out_of_memory = 1;
        return NULL;
    }
    memcpy(import_name, file_name, length - suffix_length);
    import_name[length - suffix_length] = '\0';
    return import_name;
}

static weivel_status take_file(scan *state, const char *file_name) {
    discovered_plugin plugin = {0};
    weivel_standard_manifest *manifest;
    weivel_status status;
    int out_of_memory;
    char *import_name = import_name_of(file_name, &out_of_memory);
    plugin.identity.size = (uint32_t)sizeof plugin.identity;
    if (import_name == NULL) {
        return out_of_memory ? WEIVEL_STATUS_OUT_OF_MEMORY : WEIVEL_STATUS_OK;
    }
    plugin.manifest_path = join_path(state->directory, file_name);
    if (plugin.manifest_path == NULL) {
        weivel_standard_memory_release(import_name);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    status = load_manifest(state, plugin.manifest_path, &manifest);
    plugin.manifest = manifest;
    if (status == WEIVEL_STATUS_OK && manifest != NULL) {
        status = complete_plugin(state, import_name, &plugin);
    } else {
        release_plugin(&plugin);
    }
    weivel_standard_memory_release(import_name);
    return status;
}

static int compare_file_names(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

static const char *import_name_at(const discovered_plugin *plugin) {
    return weivel_standard_manifest_view(plugin->manifest)->import_name;
}

static int compare_plugins(const void *left_item, const void *right_item) {
    const discovered_plugin *left = left_item;
    const discovered_plugin *right = right_item;
    int left_has_identity = left->identity_path != NULL;
    int right_has_identity = right->identity_path != NULL;
    if (left_has_identity != right_has_identity) {
        return left_has_identity ? -1 : 1;
    }
    if (left_has_identity) {
        int difference = weivel_standard_identity_compare(&left->identity, &right->identity);
        if (difference != 0) {
            return difference;
        }
    }
    return strcmp(import_name_at(left), import_name_at(right));
}

static int holds_the_same_identity(const discovered_plugin *earlier, const discovered_plugin *later) {
    return earlier->identity_path != NULL && later->identity_path != NULL &&
           weivel_standard_identity_compare(&earlier->identity, &later->identity) == 0;
}

static weivel_status refuse_repeated_identity(scan *state, const discovered_plugin *earlier,
                                              const discovered_plugin *later) {
    return refuse_file(state, later->identity_path, WEIVEL_STANDARD_REFUSAL_DUPLICATE, "id",
                       weivel_standard_format_text("%s: the identity \"%s\" is already held by %s",
                                                   later->identity_path, later->identity_id, earlier->identity_path),
                       weivel_standard_format_text("lay out each repository's plugin once, so no two identity files "
                                                   "name one repository"));
}

/** Orders the plugins and refuses every one whose identity the one before it in order already holds. */
static weivel_status order_plugins(scan *state) {
    plugin_list *plugins = &state->plugins;
    weivel_status status = WEIVEL_STATUS_OK;
    size_t kept = 0;
    size_t at;
    if (plugins->count > 1) {
        qsort(plugins->items, plugins->count, sizeof *plugins->items, compare_plugins);
    }
    for (at = 0; at < plugins->count; at++) {
        if (kept > 0 && holds_the_same_identity(&plugins->items[kept - 1], &plugins->items[at])) {
            if (status == WEIVEL_STATUS_OK) {
                status = refuse_repeated_identity(state, &plugins->items[kept - 1], &plugins->items[at]);
            }
            release_plugin(&plugins->items[at]);
        } else {
            plugins->items[kept++] = plugins->items[at];
        }
    }
    plugins->count = kept;
    return status;
}

static weivel_status scan_files(scan *state, weivel_standard_file_names *names) {
    weivel_status status = WEIVEL_STATUS_OK;
    size_t at;
    if (names->count > 1) {
        qsort(names->items, names->count, sizeof *names->items, compare_file_names);
    }
    for (at = 0; at < names->count && status == WEIVEL_STATUS_OK; at++) {
        status = take_file(state, names->items[at]);
    }
    return status == WEIVEL_STATUS_OK ? order_plugins(state) : status;
}

static weivel_status refuse_directory(weivel_standard_discovery *discovery, uint32_t code, char *detail, char *fix) {
    weivel_standard_refusal_texts texts;
    texts.detail = detail;
    texts.fix = fix;
    texts.path = copy_text("");
    return weivel_standard_fill_refusal(&discovery->refusal, code, texts) ? WEIVEL_STATUS_REFUSED
                                                                          : WEIVEL_STATUS_OUT_OF_MEMORY;
}

static weivel_status refuse_listing(weivel_standard_discovery *discovery, const char *directory,
                                    weivel_standard_files_outcome outcome) {
    if (outcome == WEIVEL_STANDARD_FILES_OUT_OF_MEMORY) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if (outcome == WEIVEL_STANDARD_FILES_MISSING) {
        return refuse_directory(discovery, WEIVEL_STANDARD_REFUSAL_NOT_FOUND,
                                weivel_standard_format_text("the directory %s does not exist", directory),
                                weivel_standard_format_text("create %s, or name the directory the plugins are laid "
                                                            "out in", directory));
    }
    return refuse_directory(discovery, WEIVEL_STANDARD_REFUSAL_UNREADABLE,
                            weivel_standard_format_text(outcome == WEIVEL_STANDARD_FILES_NOT_A_DIRECTORY
                                                            ? "%s is not a directory"
                                                            : "the directory %s cannot be listed",
                                                        directory),
                            weivel_standard_format_text("name a directory this process can list, not %s", directory));
}

/** Readies a usable discovery for its answer: its refusal sized, and what a newer caller has beyond it zero. */
static void prepare(weivel_standard_discovery *discovery) {
    weivel_standard_discovery own;
    memset(&own, 0, sizeof own);
    own.refusal.size = (uint32_t)sizeof own.refusal;
    weivel_standard_sized_write(discovery, &own, sizeof own);
}

static int holds_answer(const weivel_standard_discovery *discovery) {
    return discovery->plugins != NULL || discovery->failures != NULL || discovery->plugin_count != 0 ||
           discovery->failure_count != 0 || weivel_standard_refusal_holds_one(&discovery->refusal);
}

static weivel_status finish(weivel_standard_discovery *discovery, scan *state, weivel_status status) {
    if (status != WEIVEL_STATUS_OK) {
        release_lists(&state->plugins, &state->failures);
        if (status == WEIVEL_STATUS_MALFORMED) {
            discovery->refusal = state->call_refusal;
        }
        return status;
    }
    discovery->plugins = state->plugins.items;
    discovery->plugin_count = state->plugins.count;
    discovery->failures = state->failures.items;
    discovery->failure_count = state->failures.count;
    return WEIVEL_STATUS_OK;
}

weivel_status weivel_standard_discover(const char *directory, const weivel_standard_validation_options *options,
                                       weivel_standard_discovery *discovery) {
    weivel_standard_file_names names;
    weivel_standard_files_outcome outcome;
    weivel_status status;
    scan state;
    if (discovery == NULL || discovery->size < DISCOVERY_FIRST_SHAPE || holds_answer(discovery)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    prepare(discovery);
    if (directory == NULL || directory[0] == '\0') {
        return weivel_standard_refuse_call(&discovery->refusal, "weivel_standard_discover was given no directory",
                                           "name the directory the plugins are laid out in");
    }
    outcome = weivel_standard_files_list(directory, &names);
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        return refuse_listing(discovery, directory, outcome);
    }
    memset(&state, 0, sizeof state);
    state.directory = directory;
    state.options = options;
    status = scan_files(&state, &names);
    weivel_standard_file_names_release(&names);
    return finish(discovery, &state, status);
}

void weivel_standard_discovery_release(weivel_standard_discovery *discovery) {
    plugin_list plugins = {0};
    failure_list failures = {0};
    uint32_t size;
    if (discovery == NULL || discovery->size < DISCOVERY_FIRST_SHAPE) {
        return;
    }
    plugins.items = (discovered_plugin *)discovery->plugins;
    plugins.count = discovery->plugin_count;
    failures.items = (discovery_failure *)discovery->failures;
    failures.count = discovery->failure_count;
    release_lists(&plugins, &failures);
    weivel_standard_refusal_release(&discovery->refusal);
    size = discovery->size;
    memset(discovery, 0, DISCOVERY_FIRST_SHAPE);
    discovery->size = size;
}
