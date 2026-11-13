#include "check.h"
#include "weivel/services.h"

#include <stdlib.h>

#define TABLE_CAPACITY 8

/** Every answer and refusal of the in-memory providers lives in one block, so `release` frees it whole. */
static void *allocate_block(size_t length) {
    return calloc(1, length == 0 ? 1 : length);
}

static char *copy_text(const char *text, size_t length) {
    char *copy = allocate_block(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length);
    }
    return copy;
}

static void release_block(void *context, void *storage) {
    (void)context;
    free(storage);
}

static int refusal_is_unusable(const weivel_standard_service_refusal *refusal) {
    return refusal != NULL && (refusal->size < sizeof *refusal || refusal->storage != NULL);
}

/** `header` zeroed bytes lead the block, so a provider whose blocks open with their own header releases them alike. */
static weivel_status refuse_after(size_t header, weivel_standard_service_refusal *refusal, const char *detail,
                                  const char *fix) {
    size_t detail_length = strlen(detail);
    char *block;
    char *texts;
    if (refusal == NULL) {
        return WEIVEL_STATUS_REFUSED;
    }
    block = allocate_block(header + detail_length + strlen(fix) + 2);
    if (block == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    texts = block + header;
    memcpy(texts, detail, detail_length);
    memcpy(texts + detail_length + 1, fix, strlen(fix));
    refusal->detail = texts;
    refusal->fix = texts + detail_length + 1;
    refusal->storage = block;
    return WEIVEL_STATUS_REFUSED;
}

static weivel_status refuse(weivel_standard_service_refusal *refusal, const char *detail, const char *fix) {
    return refuse_after(0, refusal, detail, fix);
}

typedef struct memory_config {
    weivel_standard_setting defaults[TABLE_CAPACITY];
    size_t default_count;
    weivel_standard_setting stored[TABLE_CAPACITY];
    size_t stored_count;
} memory_config;

static weivel_standard_setting *find_setting(const weivel_standard_setting *table, size_t count, const char *key) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (strcmp(table[at].key, key) == 0) {
            return (weivel_standard_setting *)&table[at];
        }
    }
    return NULL;
}

static const weivel_standard_setting *effective_setting(const memory_config *config, const char *key) {
    const weivel_standard_setting *stored = find_setting(config->stored, config->stored_count, key);
    return stored != NULL ? stored : find_setting(config->defaults, config->default_count, key);
}

static int is_known_kind(uint32_t kind) {
    return kind == WEIVEL_STANDARD_SETTING_TEXT || kind == WEIVEL_STANDARD_SETTING_NUMBER ||
           kind == WEIVEL_STANDARD_SETTING_BOOLEAN;
}

static weivel_status config_get(void *context, const char *key, weivel_standard_setting_answer *answer,
                                weivel_standard_service_refusal *refusal) {
    const weivel_standard_setting *setting;
    if (key == NULL || answer == NULL || answer->size < sizeof *answer || answer->storage != NULL ||
        refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    setting = effective_setting(context, key);
    if (setting == NULL) {
        return WEIVEL_STATUS_NONE;
    }
    answer->value = setting->value;
    if (setting->value.kind == WEIVEL_STANDARD_SETTING_TEXT) {
        char *text = copy_text(setting->value.text, setting->value.text_length);
        if (text == NULL) {
            memset(&answer->value, 0, sizeof answer->value);
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
        answer->value.text = text;
        answer->storage = text;
    }
    return WEIVEL_STATUS_OK;
}

static size_t text_bytes_of(const weivel_standard_setting *setting) {
    size_t bytes = strlen(setting->key) + 1;
    if (setting->value.kind == WEIVEL_STANDARD_SETTING_TEXT) {
        bytes += setting->value.text_length + 1;
    }
    return bytes;
}

static char *place_text(char *cursor, const char *text, size_t length, const char **placed) {
    memcpy(cursor, text, length);
    cursor[length] = '\0';
    *placed = cursor;
    return cursor + length + 1;
}

static size_t collect_settings(const memory_config *config, const weivel_standard_setting **chosen) {
    size_t count = 0;
    size_t at;
    for (at = 0; at < config->stored_count; at++) {
        chosen[count++] = &config->stored[at];
    }
    for (at = 0; at < config->default_count; at++) {
        if (find_setting(config->stored, config->stored_count, config->defaults[at].key) == NULL) {
            chosen[count++] = &config->defaults[at];
        }
    }
    return count;
}

static weivel_status config_all(void *context, weivel_standard_settings *settings,
                                weivel_standard_service_refusal *refusal) {
    const weivel_standard_setting *chosen[2 * TABLE_CAPACITY];
    weivel_standard_setting *array;
    size_t count;
    size_t text_bytes = 0;
    size_t at;
    char *cursor;
    if (settings == NULL || settings->size < sizeof *settings || settings->storage != NULL ||
        refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    count = collect_settings(context, chosen);
    for (at = 0; at < count; at++) {
        text_bytes += text_bytes_of(chosen[at]);
    }
    array = allocate_block(count * sizeof *array + text_bytes);
    if (array == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    cursor = (char *)(array + count);
    for (at = 0; at < count; at++) {
        array[at] = *chosen[at];
        cursor = place_text(cursor, chosen[at]->key, strlen(chosen[at]->key), &array[at].key);
        if (chosen[at]->value.kind == WEIVEL_STANDARD_SETTING_TEXT) {
            cursor = place_text(cursor, chosen[at]->value.text, chosen[at]->value.text_length, &array[at].value.text);
        }
    }
    settings->settings = array;
    settings->count = count;
    settings->storage = array;
    return WEIVEL_STATUS_OK;
}

static void forget_stored(weivel_standard_setting *setting) {
    free((void *)setting->key);
    free((void *)setting->value.text);
}

static weivel_status config_set(void *context, const char *key, const weivel_standard_setting_value *value,
                                weivel_standard_service_refusal *refusal) {
    memory_config *config = context;
    weivel_standard_setting *slot;
    weivel_standard_setting replacement;
    if (key == NULL || value == NULL || !is_known_kind(value->kind) || refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (strncmp(key, "locked.", 7) == 0) {
        return refuse(refusal, "the setting is locked by the host", "choose a key outside \"locked.\"");
    }
    slot = find_setting(config->stored, config->stored_count, key);
    if (slot == NULL && config->stored_count == TABLE_CAPACITY) {
        return refuse(refusal, "the store is full", "set fewer settings");
    }
    replacement.key = copy_text(key, strlen(key));
    replacement.value = *value;
    replacement.value.text =
        value->kind == WEIVEL_STANDARD_SETTING_TEXT ? copy_text(value->text, value->text_length) : NULL;
    if (replacement.key == NULL || (value->kind == WEIVEL_STANDARD_SETTING_TEXT && replacement.value.text == NULL)) {
        forget_stored(&replacement);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if (slot == NULL) {
        slot = &config->stored[config->stored_count++];
    } else {
        forget_stored(slot);
    }
    *slot = replacement;
    return WEIVEL_STATUS_OK;
}

static void config_drop(memory_config *config) {
    size_t at;
    for (at = 0; at < config->stored_count; at++) {
        forget_stored(&config->stored[at]);
    }
}

typedef struct secret_entry {
    char *name;
    uint8_t *bytes;
    size_t length;
} secret_entry;

typedef struct memory_secrets {
    secret_entry entries[TABLE_CAPACITY];
    size_t count;
    int unwritable;
} memory_secrets;

/** A secret's answer block: its length first, so the release can overwrite exactly the bytes before freeing. */
typedef struct secret_block {
    size_t length;
    uint8_t bytes[1];
} secret_block;

/** A refusal block opens with a zero length, so release_secret_block frees it like an answer. */
static weivel_status refuse_secret(weivel_standard_service_refusal *refusal, const char *detail, const char *fix) {
    return refuse_after(offsetof(secret_block, bytes), refusal, detail, fix);
}

static void release_secret_block(void *context, void *storage) {
    secret_block *block = storage;
    (void)context;
    if (block != NULL) {
        memset(block->bytes, 0, block->length);
    }
    free(block);
}

static secret_entry *find_secret(memory_secrets *secrets, const char *name) {
    size_t at;
    for (at = 0; at < secrets->count; at++) {
        if (strcmp(secrets->entries[at].name, name) == 0) {
            return &secrets->entries[at];
        }
    }
    return NULL;
}

static void forget_secret(secret_entry *entry) {
    memset(entry->bytes, 0, entry->length);
    free(entry->bytes);
    free(entry->name);
}

static weivel_status secrets_remove(void *context, const char *name, weivel_standard_service_refusal *refusal) {
    memory_secrets *secrets = context;
    secret_entry *entry;
    if (name == NULL || refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (secrets->unwritable) {
        return refuse_secret(refusal, "the secret store is read-only", "unlock the secret store");
    }
    entry = find_secret(secrets, name);
    if (entry != NULL) {
        forget_secret(entry);
        *entry = secrets->entries[--secrets->count];
    }
    return WEIVEL_STATUS_OK;
}

static weivel_status secrets_get(void *context, const char *name, weivel_standard_secret *secret,
                                 weivel_standard_service_refusal *refusal) {
    secret_entry *entry;
    secret_block *block;
    if (name == NULL || secret == NULL || secret->size < sizeof *secret || secret->storage != NULL ||
        refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    entry = find_secret(context, name);
    if (entry == NULL) {
        return WEIVEL_STATUS_NONE;
    }
    block = allocate_block(sizeof *block + entry->length);
    if (block == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    block->length = entry->length;
    memcpy(block->bytes, entry->bytes, entry->length);
    secret->bytes = block->bytes;
    secret->length = entry->length;
    secret->storage = block;
    return WEIVEL_STATUS_OK;
}

static weivel_status secrets_set(void *context, const char *name, const uint8_t *bytes, size_t length,
                                 weivel_standard_service_refusal *refusal) {
    memory_secrets *secrets = context;
    secret_entry *entry;
    secret_entry replacement;
    if (name == NULL || (bytes == NULL && length != 0) || refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (secrets->unwritable) {
        return refuse_secret(refusal, "the secret store is read-only", "unlock the secret store");
    }
    entry = find_secret(secrets, name);
    if (entry == NULL && secrets->count == TABLE_CAPACITY) {
        return refuse_secret(refusal, "the secret store is full", "remove a secret first");
    }
    replacement.name = copy_text(name, strlen(name));
    replacement.bytes = allocate_block(length);
    replacement.length = length;
    if (replacement.name == NULL || replacement.bytes == NULL) {
        free(replacement.name);
        free(replacement.bytes);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if (length != 0) {
        memcpy(replacement.bytes, bytes, length);
    }
    if (entry == NULL) {
        entry = &secrets->entries[secrets->count++];
    } else {
        forget_secret(entry);
    }
    *entry = replacement;
    return WEIVEL_STATUS_OK;
}

static void secrets_drop(memory_secrets *secrets) {
    while (secrets->count > 0) {
        forget_secret(&secrets->entries[--secrets->count]);
    }
}

typedef struct memory_paths {
    const char *cache;
    const char *config;
    const char *plugin;
} memory_paths;

static weivel_status answer_directory(const char *path, const char **directory) {
    if (directory == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *directory = path;
    return WEIVEL_STATUS_OK;
}

static weivel_status paths_cache(void *context, const char **directory) {
    return answer_directory(((const memory_paths *)context)->cache, directory);
}

static weivel_status paths_config(void *context, const char **directory) {
    return answer_directory(((const memory_paths *)context)->config, directory);
}

static weivel_status paths_plugin(void *context, const char **directory) {
    return answer_directory(((const memory_paths *)context)->plugin, directory);
}

static weivel_standard_setting_value text_value(const char *text) {
    weivel_standard_setting_value value = {WEIVEL_STANDARD_SETTING_TEXT, 0, 0.0, text, strlen(text)};
    return value;
}

static weivel_standard_setting_value number_value(double number) {
    weivel_standard_setting_value value = {WEIVEL_STANDARD_SETTING_NUMBER, 0, number, NULL, 0};
    return value;
}

static weivel_standard_setting_value boolean_value(uint32_t boolean) {
    weivel_standard_setting_value value = {WEIVEL_STANDARD_SETTING_BOOLEAN, boolean, 0.0, NULL, 0};
    return value;
}

static weivel_standard_config config_over(memory_config *config) {
    weivel_standard_config definition = WEIVEL_INIT(weivel_standard_config);
    definition.context = config;
    definition.all = config_all;
    definition.get = config_get;
    definition.set = config_set;
    definition.release = release_block;
    return definition;
}

static void check_text_setting(const weivel_standard_config *config, const char *key, const char *expected) {
    weivel_standard_setting_answer answer = WEIVEL_INIT(weivel_standard_setting_answer);
    CHECK(config->get(config->context, key, &answer, NULL) == WEIVEL_STATUS_OK);
    CHECK(answer.value.kind == WEIVEL_STANDARD_SETTING_TEXT);
    CHECK(answer.value.text_length == strlen(expected));
    check_text(answer.value.text, expected, key);
    config->release(config->context, answer.storage);
}

static const weivel_standard_setting *setting_named(const weivel_standard_settings *settings, const char *key) {
    return find_setting(settings->settings, settings->count, key);
}

static void check_merged_settings(const weivel_standard_settings *settings) {
    const weivel_standard_setting *theme = setting_named(settings, "theme");
    const weivel_standard_setting *width = setting_named(settings, "width");
    const weivel_standard_setting *wrap = setting_named(settings, "wrap");
    CHECK(settings->count == 3);
    CHECK(theme != NULL && width != NULL && wrap != NULL);
    if (theme != NULL && width != NULL && wrap != NULL) {
        check_text(theme->value.text, "light", "a stored value wins over its default");
        CHECK(width->value.kind == WEIVEL_STANDARD_SETTING_NUMBER && width->value.number == 2.5);
        CHECK(wrap->value.kind == WEIVEL_STANDARD_SETTING_BOOLEAN && wrap->value.boolean == 1);
    }
}

static void config_reads_defaults_and_writes(void) {
    memory_config memory;
    weivel_standard_config definition;
    const weivel_standard_config *config = &definition;
    weivel_standard_setting_answer answer = WEIVEL_INIT(weivel_standard_setting_answer);
    weivel_standard_settings before = WEIVEL_INIT(weivel_standard_settings);
    weivel_standard_settings after = WEIVEL_INIT(weivel_standard_settings);
    weivel_standard_setting_value value;
    memset(&memory, 0, sizeof memory);
    memory.defaults[0].key = "theme";
    memory.defaults[0].value = text_value("dark");
    memory.defaults[1].key = "width";
    memory.defaults[1].value = number_value(80.0);
    memory.default_count = 2;
    definition = config_over(&memory);

    check_text_setting(config, "theme", "dark");
    CHECK(config->get(config->context, "absent", &answer, NULL) == WEIVEL_STATUS_NONE);
    CHECK(answer.value.kind == WEIVEL_STANDARD_SETTING_NONE && answer.storage == NULL);
    CHECK(config->all(config->context, &before, NULL) == WEIVEL_STATUS_OK);
    CHECK(before.count == 2);

    value = text_value("light");
    CHECK(config->set(config->context, "theme", &value, NULL) == WEIVEL_STATUS_OK);
    check_text_setting(config, "theme", "light");
    value = boolean_value(1);
    CHECK(config->set(config->context, "wrap", &value, NULL) == WEIVEL_STATUS_OK);
    CHECK(config->get(config->context, "wrap", &answer, NULL) == WEIVEL_STATUS_OK);
    CHECK(answer.value.kind == WEIVEL_STANDARD_SETTING_BOOLEAN && answer.value.boolean == 1);
    CHECK(answer.storage == NULL);
    value = number_value(2.5);
    CHECK(config->set(config->context, "width", &value, NULL) == WEIVEL_STATUS_OK);

    CHECK(config->all(config->context, &after, NULL) == WEIVEL_STATUS_OK);
    check_merged_settings(&after);
    if (setting_named(&before, "theme") != NULL) {
        check_text(setting_named(&before, "theme")->value.text, "dark", "an answer outlives a later set");
    }
    config->release(config->context, before.storage);
    config->release(config->context, after.storage);
    config->release(config->context, NULL);
    config_drop(&memory);
}

static void config_refuses_and_rejects_malformed_calls(void) {
    memory_config memory;
    weivel_standard_config definition;
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    weivel_standard_setting_answer answer = WEIVEL_INIT(weivel_standard_setting_answer);
    weivel_standard_setting_value none = {WEIVEL_STANDARD_SETTING_NONE, 0, 0.0, NULL, 0};
    weivel_standard_setting_value unknown = {WEIVEL_STANDARD_SETTING_BOOLEAN + 1, 0, 0.0, NULL, 0};
    weivel_standard_setting_value value = text_value("x");
    memset(&memory, 0, sizeof memory);
    definition = config_over(&memory);

    CHECK(definition.set(definition.context, "locked.theme", &value, &refusal) == WEIVEL_STATUS_REFUSED);
    check_text(refusal.detail, "the setting is locked by the host", "config refusal");
    CHECK(refusal.fix != NULL && refusal.storage != NULL);
    CHECK(definition.set(definition.context, "locked.theme", &value, &refusal) == WEIVEL_STATUS_MALFORMED);
    definition.release(definition.context, refusal.storage);
    CHECK(definition.set(definition.context, "locked.theme", &value, NULL) == WEIVEL_STATUS_REFUSED);

    CHECK(definition.set(definition.context, "theme", &none, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(definition.set(definition.context, "theme", &unknown, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(definition.set(definition.context, "theme", NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(definition.get(definition.context, "theme", NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(definition.all(definition.context, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(definition.set(definition.context, "theme", &value, NULL) == WEIVEL_STATUS_OK);
    CHECK(definition.get(definition.context, "theme", &answer, NULL) == WEIVEL_STATUS_OK);
    CHECK(definition.get(definition.context, "theme", &answer, NULL) == WEIVEL_STATUS_MALFORMED);
    definition.release(definition.context, answer.storage);
    config_drop(&memory);
}

static weivel_standard_secrets secrets_over(memory_secrets *secrets) {
    weivel_standard_secrets definition = WEIVEL_INIT(weivel_standard_secrets);
    definition.context = secrets;
    definition.remove = secrets_remove;
    definition.get = secrets_get;
    definition.set = secrets_set;
    definition.release = release_secret_block;
    return definition;
}

static void secrets_keep_bytes(void) {
    static const uint8_t token[] = {'a', 0, 0xff, 'b'};
    memory_secrets memory;
    weivel_standard_secrets definition;
    const weivel_standard_secrets *secrets = &definition;
    weivel_standard_secret secret = WEIVEL_INIT(weivel_standard_secret);
    weivel_standard_secret empty = WEIVEL_INIT(weivel_standard_secret);
    weivel_standard_secret gone = WEIVEL_INIT(weivel_standard_secret);
    memset(&memory, 0, sizeof memory);
    definition = secrets_over(&memory);

    CHECK(secrets->get(secrets->context, "token", &secret, NULL) == WEIVEL_STATUS_NONE);
    CHECK(secret.bytes == NULL && secret.storage == NULL);
    CHECK(secrets->set(secrets->context, "token", token, sizeof token, NULL) == WEIVEL_STATUS_OK);
    CHECK(secrets->get(secrets->context, "token", &secret, NULL) == WEIVEL_STATUS_OK);
    CHECK(secret.length == sizeof token && memcmp(secret.bytes, token, sizeof token) == 0);
    CHECK(secrets->set(secrets->context, "token", (const uint8_t *)"new", 3, NULL) == WEIVEL_STATUS_OK);
    CHECK(secret.length == sizeof token && memcmp(secret.bytes, token, sizeof token) == 0);
    secrets->release(secrets->context, secret.storage);

    CHECK(secrets->set(secrets->context, "blank", NULL, 0, NULL) == WEIVEL_STATUS_OK);
    CHECK(secrets->get(secrets->context, "blank", &empty, NULL) == WEIVEL_STATUS_OK);
    CHECK(empty.length == 0 && empty.storage != NULL);
    secrets->release(secrets->context, empty.storage);

    CHECK(secrets->remove(secrets->context, "token", NULL) == WEIVEL_STATUS_OK);
    CHECK(secrets->remove(secrets->context, "token", NULL) == WEIVEL_STATUS_OK);
    CHECK(secrets->get(secrets->context, "token", &gone, NULL) == WEIVEL_STATUS_NONE);
    secrets_drop(&memory);
}

static void secrets_refuse_and_reject_malformed_calls(void) {
    memory_secrets memory;
    weivel_standard_secrets definition;
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    weivel_standard_service_refusal second = WEIVEL_INIT(weivel_standard_service_refusal);
    weivel_standard_service_refusal small = {(uint32_t)offsetof(weivel_standard_service_refusal, fix), NULL, NULL,
                                             NULL};
    memset(&memory, 0, sizeof memory);
    memory.unwritable = 1;
    definition = secrets_over(&memory);

    CHECK(definition.set(definition.context, "token", (const uint8_t *)"x", 1, &refusal) == WEIVEL_STATUS_REFUSED);
    check_text(refusal.detail, "the secret store is read-only", "secrets refusal");
    definition.release(definition.context, refusal.storage);
    CHECK(definition.remove(definition.context, "token", &second) == WEIVEL_STATUS_REFUSED);
    CHECK(second.detail != NULL && second.fix != NULL);
    definition.release(definition.context, second.storage);
    CHECK(definition.remove(definition.context, "token", &small) == WEIVEL_STATUS_MALFORMED);
    CHECK(small.storage == NULL);
    CHECK(definition.set(definition.context, "token", NULL, 1, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(definition.get(definition.context, NULL, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    secrets_drop(&memory);
}

static void paths_answer_their_directories(void) {
    memory_paths memory = {"/home/a/.cache/echo", "/home/a/.config/echo", "/home/a/plugins/echo"};
    weivel_standard_paths definition = WEIVEL_INIT(weivel_standard_paths);
    const weivel_standard_paths *paths = &definition;
    const char *directory = NULL;
    const char *again = NULL;
    definition.context = &memory;
    definition.cache = paths_cache;
    definition.config = paths_config;
    definition.plugin = paths_plugin;

    CHECK(paths->cache(paths->context, &directory) == WEIVEL_STATUS_OK);
    check_text(directory, "/home/a/.cache/echo", "cache");
    CHECK(paths->cache(paths->context, &again) == WEIVEL_STATUS_OK && again == directory);
    CHECK(paths->config(paths->context, &directory) == WEIVEL_STATUS_OK);
    check_text(directory, "/home/a/.config/echo", "config");
    CHECK(paths->plugin(paths->context, &directory) == WEIVEL_STATUS_OK);
    check_text(directory, "/home/a/plugins/echo", "plugin");
    CHECK(paths->plugin(paths->context, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void an_older_definition_offers_only_what_its_size_covers(void) {
    memory_config memory;
    weivel_standard_config definition;
    memset(&memory, 0, sizeof memory);
    definition = config_over(&memory);
    CHECK(WEIVEL_STANDARD_OFFERS(weivel_standard_config, &definition, set));
    CHECK(WEIVEL_STANDARD_OFFERS(weivel_standard_config, &definition, release));
    definition.size = (uint32_t)offsetof(weivel_standard_config, set);
    CHECK(WEIVEL_STANDARD_OFFERS(weivel_standard_config, &definition, get));
    CHECK(!WEIVEL_STANDARD_OFFERS(weivel_standard_config, &definition, set));
    definition.size = (uint32_t)sizeof definition;
    definition.release = NULL;
    CHECK(!WEIVEL_STANDARD_OFFERS(weivel_standard_config, &definition, release));
}

/** A published number read through a table, so a compiler that warns on a constant condition still builds it. */
typedef struct published_number {
    const char *name;
    size_t actual;
    size_t expected;
} published_number;

static const published_number PUBLISHED[] = {
    {"SETTING_NONE", WEIVEL_STANDARD_SETTING_NONE, 0},
    {"SETTING_TEXT", WEIVEL_STANDARD_SETTING_TEXT, 1},
    {"SETTING_NUMBER", WEIVEL_STANDARD_SETTING_NUMBER, 2},
    {"SETTING_BOOLEAN", WEIVEL_STANDARD_SETTING_BOOLEAN, 3},
    {"config size", offsetof(weivel_standard_config, size), 0},
    {"secrets size", offsetof(weivel_standard_secrets, size), 0},
    {"paths size", offsetof(weivel_standard_paths, size), 0},
    {"settings size", offsetof(weivel_standard_settings, size), 0},
    {"setting answer size", offsetof(weivel_standard_setting_answer, size), 0},
    {"secret size", offsetof(weivel_standard_secret, size), 0},
    {"service refusal size", offsetof(weivel_standard_service_refusal, size), 0},
};

static void the_published_numbers_and_shapes_hold(void) {
    size_t at;
    for (at = 0; at < sizeof PUBLISHED / sizeof PUBLISHED[0]; at++) {
        if (PUBLISHED[at].actual != PUBLISHED[at].expected) {
            fprintf(stderr, "%s is %u, published as %u\n", PUBLISHED[at].name, (unsigned)PUBLISHED[at].actual,
                    (unsigned)PUBLISHED[at].expected);
            check_failures++;
        }
    }
}

int main(void) {
    check_text(WEIVEL_STANDARD_SERVICE_CONFIG, "weivel.config", "config macro");
    check_text(WEIVEL_STANDARD_SERVICE_SECRETS, "weivel.secrets", "secrets macro");
    check_text(WEIVEL_STANDARD_SERVICE_PATHS, "weivel.paths", "paths macro");
    check_text(weivel_standard_service_config(), "weivel.config", "config function");
    check_text(weivel_standard_service_secrets(), "weivel.secrets", "secrets function");
    check_text(weivel_standard_service_paths(), "weivel.paths", "paths function");
    config_reads_defaults_and_writes();
    config_refuses_and_rejects_malformed_calls();
    secrets_keep_bytes();
    secrets_refuse_and_reject_malformed_calls();
    paths_answer_their_directories();
    an_older_definition_offers_only_what_its_size_covers();
    the_published_numbers_and_shapes_hold();
    return check_summary("services_test");
}
