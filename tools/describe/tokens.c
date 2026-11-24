#include "tokens.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "allocation.h"

typedef struct scanner {
    const char *file;
    const char *text;
    size_t length;
    size_t at;
    uint32_t line;
    int line_has_code;
    describe_tokens *tokens;
    size_t capacity;
    describe_refusal *refusal;
} scanner;

static char *format_arguments(const char *format, va_list arguments) {
    va_list measuring;
    int length;
    char *text;
    va_copy(measuring, arguments);
    length = vsnprintf(NULL, 0, format, measuring);
    va_end(measuring);
    if (length < 0) {
        return NULL;
    }
    text = weivel_standard_memory_allocate((size_t)length + 1);
    if (text != NULL) {
        vsnprintf(text, (size_t)length + 1, format, arguments);
    }
    return text;
}

describe_outcome describe_refuse(describe_refusal *refusal, const char *file, uint32_t line, const char *format, ...) {
    va_list arguments;
    char *message;
    char *copied_file = NULL;
    va_start(arguments, format);
    message = format_arguments(format, arguments);
    va_end(arguments);
    if (message != NULL && file != NULL) {
        size_t length = strlen(file);
        copied_file = weivel_standard_memory_allocate(length + 1);
        if (copied_file != NULL) {
            memcpy(copied_file, file, length + 1);
        }
    }
    if (message == NULL || (file != NULL && copied_file == NULL)) {
        weivel_standard_memory_release(message);
        return DESCRIBE_OUT_OF_MEMORY;
    }
    refusal->file = copied_file;
    refusal->line = line;
    refusal->message = message;
    return DESCRIBE_REFUSED;
}

static int is_identifier_start(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_';
}

static int is_identifier_part(char character) {
    return is_identifier_start(character) || (character >= '0' && character <= '9');
}

static int is_space(char character) {
    return character == ' ' || character == '\t' || character == '\r' || character == '\n' || character == '\f' ||
           character == '\v';
}

static char peek(const scanner *state, size_t ahead) {
    return state->at + ahead < state->length ? state->text[state->at + ahead] : '\0';
}

static void advance(scanner *state) {
    if (state->text[state->at] == '\n') {
        state->line++;
        state->line_has_code = 0;
    }
    state->at++;
}

static int push(scanner *state, describe_token_kind kind, size_t start, uint32_t line) {
    describe_tokens *tokens = state->tokens;
    if (tokens->count == state->capacity) {
        size_t capacity = state->capacity == 0 ? 256 : state->capacity * 2;
        describe_token *grown = weivel_standard_memory_reallocate(tokens->items, capacity * sizeof *grown);
        if (grown == NULL) {
            return 0;
        }
        tokens->items = grown;
        state->capacity = capacity;
    }
    tokens->items[tokens->count].kind = kind;
    tokens->items[tokens->count].text = state->text + start;
    tokens->items[tokens->count].length = state->at - start;
    tokens->items[tokens->count].line = line;
    tokens->count++;
    return 1;
}

/** Skips a block comment the scanner stands on; 0 when it never closes. */
static int skip_block_comment(scanner *state) {
    state->at += 2;
    while (state->at < state->length) {
        if (peek(state, 0) == '*' && peek(state, 1) == '/') {
            state->at += 2;
            return 1;
        }
        advance(state);
    }
    return 0;
}

static void skip_line_comment(scanner *state) {
    while (state->at < state->length && peek(state, 0) != '\n') {
        state->at++;
    }
}

/**
 * @implNote A block comment inside a directive may span lines, so it is skipped whole rather than to the newline. After
 * a line comment's opening the rest of the line is comment text, where no block comment opens, though a continuation
 * still joins the next line.
 */
static int skip_directive(scanner *state) {
    int in_line_comment = 0;
    while (state->at < state->length && peek(state, 0) != '\n') {
        if (peek(state, 0) == '\\' && peek(state, 1) == '\n') {
            state->at++;
            advance(state);
        } else if (peek(state, 0) == '\\' && peek(state, 1) == '\r' && peek(state, 2) == '\n') {
            state->at += 2;
            advance(state);
        } else if (!in_line_comment && peek(state, 0) == '/' && peek(state, 1) == '/') {
            in_line_comment = 1;
            state->at += 2;
        } else if (!in_line_comment && peek(state, 0) == '/' && peek(state, 1) == '*') {
            if (!skip_block_comment(state)) {
                return 0;
            }
        } else {
            state->at++;
        }
    }
    return 1;
}

static int skip_string(scanner *state) {
    state->at++;
    while (state->at < state->length && peek(state, 0) != '"' && peek(state, 0) != '\n') {
        state->at += peek(state, 0) == '\\' && state->at + 1 < state->length ? 2 : 1;
    }
    if (peek(state, 0) != '"') {
        return 0;
    }
    state->at++;
    return 1;
}

static int is_doc_opening(const scanner *state) {
    return peek(state, 2) == '*' && peek(state, 3) != '/';
}

static describe_outcome scan_one(scanner *state) {
    size_t start = state->at;
    uint32_t line = state->line;
    char character = peek(state, 0);
    if (character == '/' && peek(state, 1) == '*') {
        int doc = is_doc_opening(state);
        if (!skip_block_comment(state)) {
            return describe_refuse(state->refusal, state->file, line, "a comment that never ends");
        }
        return !doc || push(state, DESCRIBE_TOKEN_DOC, start, line) ? DESCRIBE_DONE : DESCRIBE_OUT_OF_MEMORY;
    }
    if (character == '/' && peek(state, 1) == '/') {
        skip_line_comment(state);
        return DESCRIBE_DONE;
    }
    if (character == '#' && !state->line_has_code) {
        if (!skip_directive(state)) {
            return describe_refuse(state->refusal, state->file, line, "a comment that never ends");
        }
        return push(state, DESCRIBE_TOKEN_DIRECTIVE, start, line) ? DESCRIBE_DONE : DESCRIBE_OUT_OF_MEMORY;
    }
    state->line_has_code = 1;
    if (character == '"') {
        if (!skip_string(state)) {
            return describe_refuse(state->refusal, state->file, line, "a string that never ends");
        }
        return push(state, DESCRIBE_TOKEN_STRING, start, line) ? DESCRIBE_DONE : DESCRIBE_OUT_OF_MEMORY;
    }
    if (is_identifier_start(character) || (character >= '0' && character <= '9')) {
        describe_token_kind kind = is_identifier_start(character) ? DESCRIBE_TOKEN_IDENTIFIER : DESCRIBE_TOKEN_NUMBER;
        while (state->at < state->length && is_identifier_part(peek(state, 0))) {
            state->at++;
        }
        return push(state, kind, start, line) ? DESCRIBE_DONE : DESCRIBE_OUT_OF_MEMORY;
    }
    state->at++;
    return push(state, DESCRIBE_TOKEN_PUNCTUATOR, start, line) ? DESCRIBE_DONE : DESCRIBE_OUT_OF_MEMORY;
}

describe_outcome describe_tokenize(const char *file, const char *text, size_t length, describe_tokens *tokens,
                                   describe_refusal *refusal) {
    scanner state;
    describe_outcome outcome = DESCRIBE_DONE;
    memset(&state, 0, sizeof state);
    state.file = file;
    state.text = text;
    state.length = length;
    state.line = 1;
    state.tokens = tokens;
    state.refusal = refusal;
    tokens->items = NULL;
    tokens->count = 0;
    while (outcome == DESCRIBE_DONE && state.at < length) {
        if (is_space(peek(&state, 0))) {
            advance(&state);
        } else {
            outcome = scan_one(&state);
        }
    }
    if (outcome == DESCRIBE_DONE && !push(&state, DESCRIBE_TOKEN_END, state.at, state.line)) {
        outcome = DESCRIBE_OUT_OF_MEMORY;
    }
    if (outcome != DESCRIBE_DONE) {
        describe_tokens_release(tokens);
    }
    return outcome;
}

void describe_tokens_release(describe_tokens *tokens) {
    weivel_standard_memory_release(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
}

int describe_token_is(const describe_token *token, const char *spelled) {
    size_t length = strlen(spelled);
    return (token->kind == DESCRIBE_TOKEN_IDENTIFIER || token->kind == DESCRIBE_TOKEN_PUNCTUATOR) &&
           token->length == length && memcmp(token->text, spelled, length) == 0;
}

char *describe_token_copy(const describe_token *token) {
    char *copy = weivel_standard_memory_allocate(token->length + 1);
    if (copy != NULL) {
        memcpy(copy, token->text, token->length);
        copy[token->length] = '\0';
    }
    return copy;
}

char *describe_doc_text(const describe_token *doc) {
    const char *text = doc->text + 3;
    size_t length = doc->length - 5;
    char *normal = weivel_standard_memory_allocate(length + 1);
    size_t at = 0;
    size_t written = 0;
    int line_start = 1;
    if (normal == NULL) {
        return NULL;
    }
    for (at = 0; at < length; at++) {
        char character = text[at];
        if (character == '\n') {
            line_start = 1;
            character = ' ';
        } else if (line_start && character == '*') {
            line_start = 0;
            continue;
        } else if (!is_space(character)) {
            line_start = 0;
        }
        if (is_space(character)) {
            if (written > 0 && normal[written - 1] != ' ') {
                normal[written++] = ' ';
            }
        } else {
            normal[written++] = character;
        }
    }
    while (written > 0 && normal[written - 1] == ' ') {
        written--;
    }
    normal[written] = '\0';
    return normal;
}
