#ifndef WEIVEL_DESCRIBE_TOKENS_H
#define WEIVEL_DESCRIBE_TOKENS_H

#include <stddef.h>
#include <stdint.h>

#include "model.h"

typedef enum describe_token_kind {
    DESCRIBE_TOKEN_END = 0,
    DESCRIBE_TOKEN_IDENTIFIER = 1,
    DESCRIBE_TOKEN_NUMBER = 2,
    DESCRIBE_TOKEN_STRING = 3,
    /** One character that is none of the above, `{` and `*` among them. */
    DESCRIBE_TOKEN_PUNCTUATOR = 4,
    /** A `/` `**` comment, its text spanning the delimiters. */
    DESCRIBE_TOKEN_DOC = 5,
    /** A whole preprocessor line, its continuations included. */
    DESCRIBE_TOKEN_DIRECTIVE = 6
} describe_token_kind;

typedef struct describe_token {
    describe_token_kind kind;
    const char *text;
    size_t length;
    uint32_t line;
} describe_token;

/** The tokens of one header, ending in one END token; they point into the text, which must outlive them. */
typedef struct describe_tokens {
    describe_token *items;
    size_t count;
} describe_tokens;

/**
 * Splits `text`; REFUSED, filling `refusal` at `file`, for a comment or a string left open, or OUT_OF_MEMORY.
 */
describe_outcome describe_tokenize(const char *file, const char *text, size_t length, describe_tokens *tokens,
                                   describe_refusal *refusal);

void describe_tokens_release(describe_tokens *tokens);

/** 1 when the token is the identifier or punctuator `spelled`. */
int describe_token_is(const describe_token *token, const char *spelled);

/** A copy of the token's text; NULL when memory runs out. */
char *describe_token_copy(const describe_token *token);

/**
 * A doc comment's text without its delimiters or leading `*`s, every run of white space one space; NULL when memory
 * runs out.
 */
char *describe_doc_text(const describe_token *doc);

/** Fills `refusal` with a formatted message at `file`:`line`; REFUSED, or OUT_OF_MEMORY with it left empty. */
describe_outcome describe_refuse(describe_refusal *refusal, const char *file, uint32_t line, const char *format, ...);

#endif
