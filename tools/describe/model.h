#ifndef WEIVEL_DESCRIBE_MODEL_H
#define WEIVEL_DESCRIBE_MODEL_H

/**
 * @file
 * What weivel-describe read from a library's public headers: every struct, enum and opaque handle they declare, each
 * struct's members and, for a definition, its operations, every item resolved to what a weivel_standard_described_*
 * entry holds. The writer emits C from it alone, reaching back to no header.
 *
 * Every string is the model's own, allocated through the standard's allocator and freed by describe_model_release.
 * Every item carries where it was declared: the file as the caller named it and the 1-based line of its name token
 * (a struct's or an enum's tag after `struct` or `enum`, a member's, a parameter's or an operation's own name).
 *
 * C expressions the writer derives from the names here: a member's offset is `offsetof(<type name>, <member name>)`
 * and its size `sizeof(((<type name> *)0)-><member name>)`; an operation's offset is
 * `offsetof(<definition name>, <operation name>)`; a struct's size is `sizeof(<type name>)` and its first shape, for
 * a sized struct, `offsetof(<type name>, <first_shape>) + sizeof(((<type name> *)0)-><first_shape>)`, `first_shape`
 * naming a member or, in a definition, an operation; for an unsized struct the first shape is its size.
 */

#include <stddef.h>
#include <stdint.h>

#include "weivel/description.h"

typedef struct describe_location {
    /** The path the caller named the header by, owned by the model. */
    const char *file;
    uint32_t line;
} describe_location;

/** A C type as the header spells it, before it is resolved to a kind. */
typedef struct describe_spelling {
    /** The one identifier the type is spelled with: `uint32_t`, `char`, `void` or a typedef name. */
    char *base;
    /** 1 when `const` qualifies the base, written before or after it. */
    uint32_t base_const;
    /** How many `*` follow the base. */
    uint32_t pointers;
    /** Bit n set when the (n+1)th `*` is followed by `const`. */
    uint32_t const_pointers;
} describe_spelling;

/**
 * A struct member or an operation's parameter, resolved: `kind`, `role` (NO_ROLE for a member), `element`, `type`,
 * `count` and `release` are exactly what its weivel_standard_described_member or _parameter holds, NULL for a name it
 * does not hold. `doc` is its own documentation, normalized to one line: a member's `/` `**` comment, a parameter's
 * `@param` paragraph; NULL for none.
 */
typedef struct describe_item {
    char *name;
    describe_location where;
    describe_spelling spelling;
    char *doc;
    uint32_t kind;
    uint32_t role;
    uint32_t element;
    char *type;
    char *count;
    char *release;
} describe_item;

/**
 * A function pointer of a definition: an operation answering the status type (`returns` STATUS) or a release
 * (`returns` NOTHING), whose `frees` names the type its one parameter after the context points at. `blocking` when
 * its doc carries the tag `@blocking`, which only an operation may.
 */
typedef struct describe_operation {
    char *name;
    describe_location where;
    char *doc;
    describe_spelling returned;
    uint32_t returns;
    describe_item *parameters;
    size_t parameter_count;
    char *frees;
    int blocking;
} describe_operation;

/**
 * A `typedef struct <name> { ... } <name>;`. `sized` when its first member is `uint32_t size`, which then names its
 * first shape in `first_shape`; `definition` when it carries function pointers, its members then being `size` and
 * `context` alone and its function pointers `operations`, in declaration order. `elsewhere` for a definition named as
 * described elsewhere, which another library's `<name>_description` describes, though the headers declare it.
 */
typedef struct describe_type {
    char *name;
    describe_location where;
    char *doc;
    int sized;
    int definition;
    int elsewhere;
    char *first_shape;
    describe_item *members;
    size_t member_count;
    describe_operation *operations;
    size_t operation_count;
} describe_type;

typedef struct describe_entry {
    char *name;
    describe_location where;
    uint32_t number;
} describe_entry;

typedef struct describe_enum {
    char *name;
    describe_location where;
    char *doc;
    describe_entry *entries;
    size_t entry_count;
} describe_enum;

/**
 * An opaque type: a `typedef struct <name> <name>;` no scanned header gives a body. `release` names the one release,
 * across every definition read, that frees it, NULL when none does; a handle member's `release` is that name.
 */
typedef struct describe_handle {
    char *name;
    describe_location where;
    char *release;
} describe_handle;

/**
 * Everything read, each array in declaration order across the headers in the order they were read. `functions`
 * names every function the headers declare, which the reader otherwise skips.
 */
typedef struct describe_model {
    char **files;
    size_t file_count;
    char **functions;
    size_t function_count;
    describe_type *types;
    size_t type_count;
    describe_enum *enums;
    size_t enum_count;
    describe_handle *handles;
    size_t handle_count;
} describe_model;

/**
 * The type names a library's definitions use by role, which a library may restate under its own names: `status` what
 * an operation answers, `refusal` the struct an operation's last parameter fills when it declines, `value` the value
 * node held by value or pointed at, and `tree` the answer tree a value is answered into. The refusal type must be a
 * struct of the scanned headers; the others are known by name alone.
 *
 * `elsewhere` names the definitions another library describes, known by name alone as the value type is: a pointer
 * to one is a DEFINITION, whose description is that library's `<name>_description`. One the scanned headers also
 * declare is read from them and still described elsewhere, never written again.
 */
typedef struct describe_names {
    const char *status;
    const char *refusal;
    const char *value;
    const char *tree;
    const char *const *elsewhere;
    size_t elsewhere_count;
} describe_names;

typedef enum describe_outcome {
    DESCRIBE_DONE = 0,
    /** The refusal is filled. */
    DESCRIBE_REFUSED = 1,
    /** The refusal is left empty. */
    DESCRIBE_OUT_OF_MEMORY = 2
} describe_outcome;

/**
 * Why a header was refused, printed as `<file>:<line>: <message>`. A form the grammar does not read says
 * `<what> is not in weivel's description grammar`; every other refusal says what is wrong in its own words. A file
 * that cannot be read is refused at line 0; a refusal of no file has `file` NULL.
 */
typedef struct describe_refusal {
    char *file;
    uint32_t line;
    char *message;
} describe_refusal;

/** weivel/standard's own names: weivel_status, weivel_standard_service_refusal, weivel_standard_value and _tree. */
describe_names describe_names_of_the_standard(void);

void describe_model_release(describe_model *model);

void describe_refusal_release(describe_refusal *refusal);

const describe_type *describe_model_type(const describe_model *model, const char *name);
const describe_enum *describe_model_enum(const describe_model *model, const char *name);
const describe_handle *describe_model_handle(const describe_model *model, const char *name);
const describe_operation *describe_type_operation(const describe_type *type, const char *name);

/** Whether the headers declare the function `<definition>_description`. */
int describe_model_declares_description(const describe_model *model, const char *definition);

/**
 * What one definition's description holds: the definition and every struct its operations reach, through
 * parameters and then members, and every enum a reached U32 names, each in the model's declaration order; and
 * `definitions`, the name of every definition a reached DEFINITION names, each once, in the order first reached.
 * The arrays point into the model and are the reach's own.
 */
typedef struct describe_reach {
    const describe_type *definition;
    const describe_type **types;
    size_t type_count;
    const describe_enum **enums;
    size_t enum_count;
    const char **definitions;
    size_t definition_count;
} describe_reach;

/**
 * Fills `reach` for the definition named `definition`. REFUSED, at the definition's line, when a reached handle
 * member's release is not an operation of this definition freeing that handle, and with no file and line 0 when the
 * name is no definition of the model.
 */
describe_outcome describe_reach_of(const describe_model *model, const char *definition, describe_reach *reach,
                                   describe_refusal *refusal);

void describe_reach_release(describe_reach *reach);

#endif
