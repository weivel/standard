#include <stdio.h>
#include <string.h>

#include "reading.h"
#include "tokens.h"

#define GRAMMAR " is not in weivel's description grammar"

typedef struct parser {
    describe_model *model;
    const char *file;
    const describe_token *tokens;
    size_t at;
    const describe_token *doc;
    uint32_t extern_depth;
    describe_refusal *refusal;
} parser;

int describe_needs_growth(size_t count) {
    return count == 0 || (count >= 4 && (count & (count - 1)) == 0);
}

size_t describe_grown_capacity(size_t count) {
    return count == 0 ? 4 : count * 2;
}

char *describe_copy_text(const char *text) {
    size_t length = strlen(text);
    char *copy = weivel_standard_memory_allocate(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

/** The token at the cursor, past every doc comment, the last of which is kept, and every directive, which drops it. */
static const describe_token *current(parser *state) {
    for (;;) {
        const describe_token *token = &state->tokens[state->at];
        if (token->kind == DESCRIBE_TOKEN_DOC) {
            state->doc = token;
        } else if (token->kind == DESCRIBE_TOKEN_DIRECTIVE) {
            state->doc = NULL;
        } else {
            return token;
        }
        state->at++;
    }
}

static const describe_token *take(parser *state) {
    const describe_token *token = current(state);
    if (token->kind != DESCRIBE_TOKEN_END) {
        state->at++;
    }
    return token;
}

/** The doc comment standing before the declaration at the cursor, handed over and forgotten. */
static const describe_token *take_doc(parser *state) {
    const describe_token *doc;
    current(state);
    doc = state->doc;
    state->doc = NULL;
    return doc;
}

static int is(parser *state, const char *spelled) {
    return describe_token_is(current(state), spelled);
}

static describe_outcome refuse_at(parser *state, const describe_token *token, const char *what) {
    if (token->kind == DESCRIBE_TOKEN_END) {
        return describe_refuse(state->refusal, state->file, token->line, "the end of the file, before %s,%s", what,
                               GRAMMAR);
    }
    return describe_refuse(state->refusal, state->file, token->line, "`%.*s` %s%s", (int)token->length, token->text,
                           what, GRAMMAR);
}

static describe_outcome expect(parser *state, const char *spelled, const char *where) {
    const describe_token *token = current(state);
    if (!describe_token_is(token, spelled)) {
        return refuse_at(state, token, where);
    }
    state->at++;
    return DESCRIBE_DONE;
}

static describe_outcome copy_doc(const describe_token *doc, char **into) {
    if (doc == NULL) {
        return DESCRIBE_DONE;
    }
    *into = describe_doc_text(doc);
    return *into == NULL ? DESCRIBE_OUT_OF_MEMORY : DESCRIBE_DONE;
}

static describe_outcome copy_name(const describe_token *token, char **into) {
    *into = describe_token_copy(token);
    return *into == NULL ? DESCRIBE_OUT_OF_MEMORY : DESCRIBE_DONE;
}

static int token_names(const describe_token *token, const char *name) {
    return token->kind == DESCRIBE_TOKEN_IDENTIFIER && strlen(name) == token->length &&
           memcmp(token->text, name, token->length) == 0;
}

static const char *const KEYWORDS[] = {"struct",  "union",    "enum",   "unsigned", "signed",   "long",  "short",
                                       "int",     "volatile", "restrict", "static", "typedef",  "extern", "inline",
                                       "register", "_Atomic", "bool",   "_Bool",    "float"};

static int is_keyword(const describe_token *token) {
    size_t at;
    for (at = 0; at < sizeof KEYWORDS / sizeof KEYWORDS[0]; at++) {
        if (token_names(token, KEYWORDS[at])) {
            return 1;
        }
    }
    return 0;
}

static describe_outcome refuse_keyword(parser *state, const describe_token *token) {
    if (token_names(token, "union")) {
        return describe_refuse(state->refusal, state->file, token->line, "a union%s", GRAMMAR);
    }
    if (token_names(token, "struct") || token_names(token, "enum")) {
        return describe_refuse(state->refusal, state->file, token->line,
                               "a type spelled with `%.*s` inside a struct%s", (int)token->length, token->text,
                               GRAMMAR);
    }
    return refuse_at(state, token, "in a type");
}

static describe_outcome parse_spelling(parser *state, describe_spelling *spelling) {
    const describe_token *token;
    if (is(state, "const")) {
        spelling->base_const = 1;
        take(state);
    }
    token = current(state);
    if (token->kind != DESCRIBE_TOKEN_IDENTIFIER || token_names(token, "const")) {
        return refuse_at(state, token, "where a type belongs");
    }
    if (is_keyword(token)) {
        return refuse_keyword(state, token);
    }
    take(state);
    if (copy_name(token, &spelling->base) != DESCRIBE_DONE) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    if (is(state, "const")) {
        if (spelling->base_const) {
            return refuse_at(state, current(state), "twice in a type");
        }
        spelling->base_const = 1;
        take(state);
    }
    while (is(state, "*")) {
        take(state);
        if (spelling->pointers == 31) {
            return refuse_at(state, current(state), "past 31 pointers");
        }
        if (is(state, "const")) {
            spelling->const_pointers |= (uint32_t)1 << spelling->pointers;
            take(state);
        }
        spelling->pointers++;
    }
    return DESCRIBE_DONE;
}

static describe_outcome refuse_after_name(parser *state, const describe_token *name) {
    const describe_token *token = current(state);
    if (describe_token_is(token, "[")) {
        return describe_refuse(state->refusal, state->file, token->line, "the fixed array `%.*s`%s",
                               (int)name->length, name->text, GRAMMAR);
    }
    if (describe_token_is(token, ":")) {
        return describe_refuse(state->refusal, state->file, token->line, "the bit field `%.*s`%s", (int)name->length,
                               name->text, GRAMMAR);
    }
    if (describe_token_is(token, ",")) {
        return describe_refuse(state->refusal, state->file, token->line,
                               "a declaration of `%.*s` and another in one statement%s", (int)name->length,
                               name->text, GRAMMAR);
    }
    return refuse_at(state, token, "after a member's name");
}

static int names_in_items(const describe_item *items, size_t count, const describe_token *name) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (token_names(name, items[at].name)) {
            return 1;
        }
    }
    return 0;
}

static int names_in_operations(const describe_type *type, const describe_token *name) {
    size_t at;
    for (at = 0; at < type->operation_count; at++) {
        if (token_names(name, type->operations[at].name)) {
            return 1;
        }
    }
    return 0;
}

static describe_outcome refuse_repeat(parser *state, const describe_token *name, const char *what) {
    return describe_refuse(state->refusal, state->file, name->line, "%s `%.*s` repeats a name declared before it",
                           what, (int)name->length, name->text);
}

static describe_outcome take_name(parser *state, const char *what, const describe_token **name) {
    *name = current(state);
    if ((*name)->kind != DESCRIBE_TOKEN_IDENTIFIER || is_keyword(*name) || token_names(*name, "const")) {
        return refuse_at(state, *name, what);
    }
    state->at++;
    return DESCRIBE_DONE;
}

static describe_outcome parse_parameter(parser *state, describe_operation *operation) {
    describe_item *parameter;
    const describe_token *name = NULL;
    describe_outcome outcome;
    DESCRIBE_APPEND(operation->parameters, operation->parameter_count, parameter);
    if (parameter == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    outcome = parse_spelling(state, &parameter->spelling);
    if (outcome == DESCRIBE_DONE && (is(state, ",") || is(state, ")"))) {
        return describe_refuse(state->refusal, state->file, current(state)->line, "a parameter without a name%s",
                               GRAMMAR);
    }
    if (outcome == DESCRIBE_DONE) {
        outcome = take_name(state, "where a parameter's name belongs", &name);
    }
    if (outcome == DESCRIBE_DONE && names_in_items(operation->parameters, operation->parameter_count - 1, name)) {
        outcome = refuse_repeat(state, name, "the parameter");
    }
    if (outcome == DESCRIBE_DONE) {
        parameter->where.file = state->file;
        parameter->where.line = name->line;
        outcome = copy_name(name, &parameter->name);
    }
    return outcome;
}

static describe_outcome parse_parameters(parser *state, describe_operation *operation) {
    describe_outcome outcome = expect(state, "(", "where a function pointer's parameters open");
    if (outcome == DESCRIBE_DONE && is(state, "void") && describe_token_is(&state->tokens[state->at + 1], ")")) {
        take(state);
    } else {
        while (outcome == DESCRIBE_DONE) {
            outcome = parse_parameter(state, operation);
            if (outcome != DESCRIBE_DONE || !is(state, ",")) {
                break;
            }
            take(state);
        }
    }
    return outcome == DESCRIBE_DONE ? expect(state, ")", "where a function pointer's parameters close") : outcome;
}

static describe_outcome refuse_function_shape(parser *state, const describe_token *token) {
    return describe_refuse(state->refusal, state->file, token->line, "a function pointer of another shape%s",
                           GRAMMAR);
}

static describe_outcome parse_operation(parser *state, describe_type *type, describe_spelling *returned,
                                        const describe_token *doc) {
    const describe_token *opening = take(state);
    const describe_token *name;
    describe_operation *operation;
    describe_outcome outcome;
    if (!is(state, "*")) {
        return refuse_function_shape(state, opening);
    }
    take(state);
    name = current(state);
    if (name->kind != DESCRIBE_TOKEN_IDENTIFIER || is_keyword(name) ||
        !describe_token_is(&state->tokens[state->at + 1], ")")) {
        return refuse_function_shape(state, opening);
    }
    state->at += 2;
    if (names_in_items(type->members, type->member_count, name) || names_in_operations(type, name)) {
        return refuse_repeat(state, name, "the operation");
    }
    DESCRIBE_APPEND(type->operations, type->operation_count, operation);
    if (operation == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    operation->returned = *returned;
    memset(returned, 0, sizeof *returned);
    operation->where.file = state->file;
    operation->where.line = name->line;
    outcome = copy_name(name, &operation->name);
    if (outcome == DESCRIBE_DONE) {
        outcome = copy_doc(doc, &operation->doc);
    }
    if (outcome == DESCRIBE_DONE) {
        outcome = parse_parameters(state, operation);
    }
    return outcome == DESCRIBE_DONE ? expect(state, ";", "after a function pointer") : outcome;
}

static describe_outcome add_member(parser *state, describe_type *type, describe_spelling *spelling,
                                   const describe_token *doc) {
    const describe_token *name;
    describe_item *member;
    describe_outcome outcome = take_name(state, "where a member's name belongs", &name);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    if (!is(state, ";")) {
        return refuse_after_name(state, name);
    }
    take(state);
    if (type->operation_count > 0) {
        return describe_refuse(state->refusal, state->file, name->line,
                               "the member `%.*s` after the definition's function pointers%s", (int)name->length,
                               name->text, GRAMMAR);
    }
    if (names_in_items(type->members, type->member_count, name)) {
        return refuse_repeat(state, name, "the member");
    }
    DESCRIBE_APPEND(type->members, type->member_count, member);
    if (member == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    member->spelling = *spelling;
    memset(spelling, 0, sizeof *spelling);
    member->where.file = state->file;
    member->where.line = name->line;
    outcome = copy_name(name, &member->name);
    return outcome == DESCRIBE_DONE ? copy_doc(doc, &member->doc) : outcome;
}

static describe_outcome parse_member(parser *state, describe_type *type) {
    const describe_token *doc = take_doc(state);
    describe_spelling spelling;
    describe_outcome outcome;
    memset(&spelling, 0, sizeof spelling);
    outcome = parse_spelling(state, &spelling);
    if (outcome == DESCRIBE_DONE) {
        outcome = is(state, "(") ? parse_operation(state, type, &spelling, doc)
                                 : add_member(state, type, &spelling, doc);
    }
    weivel_standard_memory_release(spelling.base);
    return outcome;
}

static int names_a_declared_type(const describe_model *model, const describe_token *name) {
    size_t at;
    for (at = 0; at < model->type_count; at++) {
        if (token_names(name, model->types[at].name)) {
            return 1;
        }
    }
    for (at = 0; at < model->enum_count; at++) {
        if (token_names(name, model->enums[at].name)) {
            return 1;
        }
    }
    for (at = 0; at < model->handle_count; at++) {
        if (token_names(name, model->handles[at].name)) {
            return 1;
        }
    }
    return 0;
}

static describe_outcome expect_closing_name(parser *state, const describe_token *tag) {
    const describe_token *name = current(state);
    if (name->kind != DESCRIBE_TOKEN_IDENTIFIER || name->length != tag->length ||
        memcmp(name->text, tag->text, tag->length) != 0) {
        return describe_refuse(state->refusal, state->file, name->line,
                               "a typedef naming `%.*s` other than its tag `%.*s`%s", (int)name->length, name->text,
                               (int)tag->length, tag->text, GRAMMAR);
    }
    take(state);
    return expect(state, ";", "after a typedef's name");
}

static describe_outcome parse_struct_body(parser *state, const describe_token *tag, const describe_token *doc) {
    describe_type *type;
    describe_outcome outcome;
    DESCRIBE_APPEND(state->model->types, state->model->type_count, type);
    if (type == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    type->where.file = state->file;
    type->where.line = tag->line;
    outcome = copy_name(tag, &type->name);
    if (outcome == DESCRIBE_DONE) {
        outcome = copy_doc(doc, &type->doc);
    }
    take(state);
    while (outcome == DESCRIBE_DONE && !is(state, "}")) {
        if (current(state)->kind == DESCRIBE_TOKEN_END) {
            return refuse_at(state, current(state), "a struct's closing `}`");
        }
        outcome = parse_member(state, type);
    }
    state->doc = NULL;
    if (outcome == DESCRIBE_DONE) {
        take(state);
        outcome = expect_closing_name(state, tag);
    }
    return outcome;
}

static describe_outcome add_handle(parser *state, const describe_token *tag) {
    describe_handle *handle;
    DESCRIBE_APPEND(state->model->handles, state->model->handle_count, handle);
    if (handle == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    handle->where.file = state->file;
    handle->where.line = tag->line;
    return copy_name(tag, &handle->name);
}

static describe_outcome parse_struct(parser *state, const describe_token *doc) {
    const describe_token *tag = current(state);
    describe_outcome outcome;
    if (describe_token_is(tag, "{")) {
        return describe_refuse(state->refusal, state->file, tag->line, "an anonymous struct%s", GRAMMAR);
    }
    outcome = take_name(state, "where a struct's tag belongs", &tag);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    if (names_a_declared_type(state->model, tag)) {
        return refuse_repeat(state, tag, "the struct");
    }
    if (is(state, "{")) {
        return parse_struct_body(state, tag, doc);
    }
    outcome = expect_closing_name(state, tag);
    return outcome == DESCRIBE_DONE ? add_handle(state, tag) : outcome;
}

/** @implNote A leading zero is refused rather than read as octal, which no weivel header means. */
static describe_outcome parse_number(parser *state, const describe_token *token, uint32_t *number) {
    uint64_t value = 0;
    size_t at = 0;
    int hex = token->length > 2 && token->text[0] == '0' && (token->text[1] == 'x' || token->text[1] == 'X');
    if (token->kind != DESCRIBE_TOKEN_NUMBER || (!hex && token->length > 1 && token->text[0] == '0')) {
        return refuse_at(state, token, "as an enum's number");
    }
    for (at = hex ? 2 : 0; at < token->length; at++) {
        char digit = token->text[at];
        uint64_t worth;
        if (digit >= '0' && digit <= '9') {
            worth = (uint64_t)(digit - '0');
        } else if (hex && digit >= 'a' && digit <= 'f') {
            worth = (uint64_t)(digit - 'a' + 10);
        } else if (hex && digit >= 'A' && digit <= 'F') {
            worth = (uint64_t)(digit - 'A' + 10);
        } else {
            return refuse_at(state, token, "as an enum's number");
        }
        value = value * (hex ? 16u : 10u) + worth;
        if (value > UINT32_MAX) {
            return refuse_at(state, token, "as an enum's number, past 2^32 - 1,");
        }
    }
    *number = (uint32_t)value;
    return DESCRIBE_DONE;
}

static int names_an_entry(const describe_model *model, const describe_token *name) {
    size_t at;
    size_t entry;
    for (at = 0; at < model->enum_count; at++) {
        for (entry = 0; entry < model->enums[at].entry_count; entry++) {
            if (token_names(name, model->enums[at].entries[entry].name)) {
                return 1;
            }
        }
    }
    return 0;
}

static describe_outcome parse_entry(parser *state, describe_enum *described, uint64_t *next) {
    const describe_token *name;
    describe_entry *entry;
    describe_outcome outcome = take_name(state, "where an enum's entry belongs", &name);
    uint32_t number = 0;
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    if (names_an_entry(state->model, name)) {
        return refuse_repeat(state, name, "the entry");
    }
    if (is(state, "=")) {
        take(state);
        outcome = parse_number(state, take(state), &number);
        if (outcome != DESCRIBE_DONE) {
            return outcome;
        }
        *next = number;
    } else if (*next > UINT32_MAX) {
        return describe_refuse(state->refusal, state->file, name->line, "`%.*s`, numbered past 2^32 - 1,%s",
                               (int)name->length, name->text, GRAMMAR);
    }
    DESCRIBE_APPEND(described->entries, described->entry_count, entry);
    if (entry == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    entry->where.file = state->file;
    entry->where.line = name->line;
    entry->number = (uint32_t)*next;
    *next += 1;
    return copy_name(name, &entry->name);
}

static describe_outcome parse_entries(parser *state, describe_enum *described) {
    uint64_t next = 0;
    describe_outcome outcome = expect(state, "{", "where an enum's entries open");
    while (outcome == DESCRIBE_DONE && !is(state, "}")) {
        outcome = parse_entry(state, described, &next);
        if (outcome == DESCRIBE_DONE && !is(state, "}")) {
            outcome = expect(state, ",", "after an enum's entry");
        }
    }
    state->doc = NULL;
    if (outcome == DESCRIBE_DONE) {
        take(state);
    }
    return outcome;
}

static describe_outcome parse_enum(parser *state, const describe_token *doc) {
    const describe_token *tag = current(state);
    describe_enum *described;
    describe_outcome outcome;
    if (describe_token_is(tag, "{")) {
        return describe_refuse(state->refusal, state->file, tag->line, "an anonymous enum%s", GRAMMAR);
    }
    outcome = take_name(state, "where an enum's tag belongs", &tag);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    if (names_a_declared_type(state->model, tag)) {
        return refuse_repeat(state, tag, "the enum");
    }
    DESCRIBE_APPEND(state->model->enums, state->model->enum_count, described);
    if (described == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    described->where.file = state->file;
    described->where.line = tag->line;
    outcome = copy_name(tag, &described->name);
    if (outcome == DESCRIBE_DONE) {
        outcome = copy_doc(doc, &described->doc);
    }
    if (outcome == DESCRIBE_DONE) {
        outcome = parse_entries(state, described);
    }
    return outcome == DESCRIBE_DONE ? expect_closing_name(state, tag) : outcome;
}

static describe_outcome parse_typedef(parser *state) {
    const describe_token *doc = take_doc(state);
    const describe_token *keyword;
    take(state);
    keyword = current(state);
    if (describe_token_is(keyword, "struct")) {
        take(state);
        return parse_struct(state, doc);
    }
    if (describe_token_is(keyword, "enum")) {
        take(state);
        return parse_enum(state, doc);
    }
    return describe_refuse(state->refusal, state->file, keyword->line, "a typedef that is not of a struct or an enum%s",
                           GRAMMAR);
}

static int is_declaration_word(const describe_token *token) {
    return describe_token_is(token, "*") ||
           (token->kind == DESCRIBE_TOKEN_IDENTIFIER && !token_names(token, "typedef") &&
            !token_names(token, "struct") && !token_names(token, "union") && !token_names(token, "enum") &&
            !token_names(token, "extern") && !token_names(token, "static") && !token_names(token, "inline"));
}

/** Skips `<identifiers and *> name(...);`, a function declaration whatever its export macro and its parameters. */
static describe_outcome skip_function(parser *state) {
    const describe_token *first = current(state);
    uint32_t depth = 0;
    take_doc(state);
    while (is_declaration_word(current(state))) {
        take(state);
    }
    if (!is(state, "(")) {
        return refuse_at(state, first, "at the top level");
    }
    do {
        const describe_token *token = take(state);
        if (token->kind == DESCRIBE_TOKEN_END || describe_token_is(token, "{") || describe_token_is(token, "}") ||
            describe_token_is(token, ";")) {
            return refuse_at(state, first, "at the top level");
        }
        depth += describe_token_is(token, "(") ? 1u : 0u;
        depth -= describe_token_is(token, ")") ? 1u : 0u;
    } while (depth > 0);
    if (!is(state, ";")) {
        return refuse_at(state, first, "at the top level, a function with a body or of another shape,");
    }
    take(state);
    return DESCRIBE_DONE;
}

static describe_outcome parse_extern(parser *state) {
    const describe_token *keyword = take(state);
    const describe_token *language = current(state);
    if (language->kind != DESCRIBE_TOKEN_STRING || language->length != 3 || memcmp(language->text, "\"C\"", 3) != 0 ||
        !describe_token_is(&state->tokens[state->at + 1], "{")) {
        return refuse_at(state, keyword, "other than `extern \"C\" {`");
    }
    state->at += 2;
    state->extern_depth++;
    state->doc = NULL;
    return DESCRIBE_DONE;
}

static describe_outcome parse_top_level(parser *state) {
    const describe_token *token = current(state);
    if (describe_token_is(token, "typedef")) {
        return parse_typedef(state);
    }
    if (describe_token_is(token, "extern")) {
        return parse_extern(state);
    }
    if (describe_token_is(token, "}")) {
        if (state->extern_depth == 0) {
            return refuse_at(state, token, "closing nothing");
        }
        state->extern_depth--;
        take(state);
        state->doc = NULL;
        return DESCRIBE_DONE;
    }
    return skip_function(state);
}

static describe_outcome add_file(describe_model *model, const char *path, const char **file) {
    char **slot;
    DESCRIBE_APPEND(model->files, model->file_count, slot);
    if (slot == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    *slot = describe_copy_text(path);
    if (*slot == NULL) {
        model->file_count--;
        return DESCRIBE_OUT_OF_MEMORY;
    }
    *file = *slot;
    return DESCRIBE_DONE;
}

describe_outcome describe_parse(describe_model *model, const char *path, const char *text, size_t length,
                                describe_refusal *refusal) {
    describe_tokens tokens;
    parser state;
    describe_outcome outcome;
    memset(&state, 0, sizeof state);
    outcome = add_file(model, path, &state.file);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    outcome = describe_tokenize(state.file, text, length, &tokens, refusal);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    state.model = model;
    state.tokens = tokens.items;
    state.refusal = refusal;
    while (outcome == DESCRIBE_DONE && current(&state)->kind != DESCRIBE_TOKEN_END) {
        outcome = parse_top_level(&state);
    }
    if (outcome == DESCRIBE_DONE && state.extern_depth > 0) {
        outcome = describe_refuse(refusal, state.file, current(&state)->line,
                                  "an `extern \"C\" {` never closed%s", GRAMMAR);
    }
    describe_tokens_release(&tokens);
    return outcome;
}
