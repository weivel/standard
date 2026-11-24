#ifndef WEIVEL_STANDARD_DESCRIPTION_H
#define WEIVEL_STANDARD_DESCRIPTION_H

/**
 * @file
 * The language-neutral description a C library answers for one service's definition: the definition's structs and
 * enums, and each operation with its parameters, so a language that cannot read a C header binds the definition from
 * it. A library declares the symbol that answers it as its service's `description` in `library.json`, or, for a
 * definition it publishes for plugins to implement and serves no implementation of, as a `contracts` entry's
 * `description` beside the entry's `capability` id. The symbol is `const weivel_standard_description *f(void)`,
 * answering a static, read-only description that lives until the library is unloaded. No part of it is JSON.
 *
 * Conventions, beside `weivel/standard.h`'s and `weivel/services.h`'s:
 *
 * - Source. Every offset and size is the library's own `offsetof` and `sizeof` from the same compile as the
 *   definition, so a description is right for the platform it was built on and for no other.
 * - Size. The description begins with its `size`, the library setting it to `sizeof` the struct as it was compiled;
 *   a reader reads it up to that `size`, what lies beyond taking its default, and one below its first shape is
 *   malformed. Every other struct here is an array element: it carries no size and never grows, and anything a later
 *   version adds arrives as a new array appended to the description.
 * - Names. Every name is NUL-terminated UTF-8, the C identifier the header declares it by: a struct or an enum by its
 *   typedef name, a member, a parameter or an operation by its field name. A type, a count or a release is named, not
 *   pointed at, so a description holds no pointer into itself beyond its arrays.
 * - Kinds. The kinds below are append-only: a published kind keeps its number and none is reused. A binding that
 *   meets a kind it does not know refuses the whole description rather than binding part of it.
 * - Trust. A binding calls weivel_standard_description_check before it reads anything else of a description, and
 *   binds nothing of one the check refuses.
 */

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/standard.h"
#include "weivel/standard_api.h"

/**
 * What a member or a parameter holds. 0 is no kind, so a member left zero is refused rather than read as a `u32`.
 *
 * As a member, each kind is the C type its comment names. As a parameter, a scalar or a text is passed by value as an
 * input and through a pointer to it as an answer (`const char **` for a text); a pointer, a counted array or bytes is
 * passed as the pointer, its count as a parameter of its own; a VALUE is only ever an answer,
 * `weivel_standard_value_tree *`; a handle is its pointer as an input and a pointer to it as an answer.
 *
 * Every C type has one spelling, and the check refuses the other: a value node is held as VALUE, as STRUCT holds a
 * struct, and pointed at as a POINTER of VALUE, as a member or as an input, as a POINTER of STRUCT points at a struct;
 * an array of texts is TEXTS, never an ARRAY of TEXT.
 */
typedef enum weivel_standard_described_kind {
    WEIVEL_STANDARD_DESCRIBED_NONE = 0,
    /** `uint32_t`: a flag, a count of something other than an array, or, naming an enum type, that enum's value. */
    WEIVEL_STANDARD_DESCRIBED_U32 = 1,
    /** `uint64_t`. */
    WEIVEL_STANDARD_DESCRIBED_U64 = 2,
    /** `int64_t`. */
    WEIVEL_STANDARD_DESCRIBED_I64 = 3,
    /** `double`. */
    WEIVEL_STANDARD_DESCRIBED_F64 = 4,
    /** `size_t`, as the count of a text array, bytes or an array beside it. */
    WEIVEL_STANDARD_DESCRIBED_SIZE = 5,
    /** `const char *`: NUL-terminated UTF-8, or NULL for none. */
    WEIVEL_STANDARD_DESCRIBED_TEXT = 6,
    /** `const char *const *`: as many texts as the member its `count` names. */
    WEIVEL_STANDARD_DESCRIBED_TEXTS = 7,
    /** `const uint8_t *`: as many bytes as the member its `count` names, any bytes at all. */
    WEIVEL_STANDARD_DESCRIBED_BYTES = 8,
    /** A struct held by value, the one its `type` names, which never holds the struct holding it. */
    WEIVEL_STANDARD_DESCRIBED_STRUCT = 9,
    /** `const T *`: one `element`, a STRUCT naming its `type` or a VALUE, or NULL for none. */
    WEIVEL_STANDARD_DESCRIBED_POINTER = 10,
    /**
     * `const T *`: as many `element`s as the member its `count` names, each a scalar, a STRUCT naming its `type` or a
     * VALUE node; an array of texts is TEXTS.
     */
    WEIVEL_STANDARD_DESCRIBED_ARRAY = 11,
    /**
     * A weivel_standard_value held by value, `weivel/value.h`'s node; as a parameter, the answer tree
     * `weivel_standard_value_tree *`.
     */
    WEIVEL_STANDARD_DESCRIBED_VALUE = 12,
    /**
     * An opaque `T *` the provider made, its `type` naming T, which is never described; it lives until the operation
     * its `release` names frees it.
     */
    WEIVEL_STANDARD_DESCRIBED_HANDLE = 13,
    /** `void *`: the definition's own context, passed first to every operation and never read by a binding. */
    WEIVEL_STANDARD_DESCRIBED_CONTEXT = 14,
    /** `void *`: what an answer's storage holds, the provider's, never read by a binding. */
    WEIVEL_STANDARD_DESCRIBED_STORAGE = 15,
    /**
     * `const T *`: a definition, a size-prefixed struct of function pointers with its own context, as
     * weivel_standard_config is, or NULL for none. Its `type` names T, the `definition` of one of the description's
     * `definitions`, which describes it. Nothing releases it: one answered at the top level, as `const T **`, lives as
     * long as the library, and one carried in a struct as long as that struct, so a definition made for one call is
     * answered only inside a struct its release frees.
     */
    WEIVEL_STANDARD_DESCRIBED_DEFINITION = 16
} weivel_standard_described_kind;

/** The highest kind published, moved to each kind as it is appended. */
#define WEIVEL_STANDARD_DESCRIBED_LAST WEIVEL_STANDARD_DESCRIBED_DEFINITION

/** What a parameter is to its operation. 0 is no role. The roles are append-only. */
typedef enum weivel_standard_described_role {
    WEIVEL_STANDARD_DESCRIBED_NO_ROLE = 0,
    /** Read during the call only: what a header writes `const`, or a scalar or text passed by value. */
    WEIVEL_STANDARD_DESCRIBED_INPUT = 1,
    /**
     * The consumer's empty answer, filled on OK alone and freed by the `release` it names, when it names one; and, as
     * a release's parameter, the answer that release frees.
     */
    WEIVEL_STANDARD_DESCRIBED_ANSWER = 2,
    /**
     * The consumer's empty refusal, or NULL for none: filled on REFUSED and freed by the `release` it names; and, as a
     * release's parameter, the refusal that release frees. Its type is a struct of the description carrying `detail`
     * and `fix` texts, as weivel_standard_service_refusal does.
     */
    WEIVEL_STANDARD_DESCRIBED_REFUSAL = 3
} weivel_standard_described_role;

/** The highest role published. */
#define WEIVEL_STANDARD_DESCRIBED_ROLE_LAST WEIVEL_STANDARD_DESCRIBED_REFUSAL

/** What an operation returns. 0 is no form. The forms are append-only. */
typedef enum weivel_standard_described_return {
    WEIVEL_STANDARD_DESCRIBED_NO_RETURN = 0,
    /** A weivel_status, as every operation of a definition answers one. */
    WEIVEL_STANDARD_DESCRIBED_STATUS = 1,
    /** Nothing, as a release returns. */
    WEIVEL_STANDARD_DESCRIBED_NOTHING = 2
} weivel_standard_described_return;

/** The highest return form published. */
#define WEIVEL_STANDARD_DESCRIBED_RETURN_LAST WEIVEL_STANDARD_DESCRIBED_NOTHING

/**
 * One member of a described struct.
 *
 * `type` names, for a STRUCT, a POINTER or an ARRAY of STRUCT elements, a struct of the description; for a U32, an
 * enum of the description when the member holds that enum's value, or NULL; for a HANDLE, the opaque type; for a
 * DEFINITION, the `definition` of one of the description's `definitions`; and is NULL otherwise. `count` names, for
 * TEXTS, BYTES and an ARRAY, the SIZE member of the same struct counting it, and is NULL otherwise. `release` names,
 * for a HANDLE, the operation that frees it, and is NULL otherwise.
 *
 * A POINTER or an ARRAY of STRUCT may name the struct holding it, or one that reaches it, as a tree of blocks holds
 * its blocks: a binding following one walks a struct it may already be inside. A STRUCT never does, since a struct
 * holding itself by value has no finite size.
 */
typedef struct weivel_standard_described_member {
    const char *name;
    size_t offset;
    size_t size;
    /** A weivel_standard_described_kind. */
    uint32_t kind;
    /** For a POINTER or an ARRAY, the weivel_standard_described_kind of what it points at; else NONE. */
    uint32_t element;
    const char *type;
    const char *count;
    const char *release;
} weivel_standard_described_member;

/**
 * One struct: its `size` as `sizeof`, and `first_shape`, where the first shape its header names ends, as the offset
 * just past that field; for a struct that carries no `size`, which never grows, it equals `size`. A definition's own
 * struct lists its data members only: its function pointers are the description's operations.
 */
typedef struct weivel_standard_described_type {
    const char *name;
    size_t size;
    size_t first_shape;
    const weivel_standard_described_member *members;
    size_t member_count;
} weivel_standard_described_type;

/** One named value of a described enum, as its header numbers it. */
typedef struct weivel_standard_described_enum_entry {
    const char *name;
    uint32_t number;
} weivel_standard_described_enum_entry;

/** One enum, whose values a U32 member naming it holds. */
typedef struct weivel_standard_described_enum {
    const char *name;
    const weivel_standard_described_enum_entry *entries;
    size_t entry_count;
} weivel_standard_described_enum;

/**
 * One parameter of an operation, in the order the C prototype takes them, the context first. `type` and `count` are
 * as a member's, `count` naming a SIZE parameter of the same operation. `release` names, for an ANSWER, a REFUSAL or
 * a HANDLE, the release that frees it; it is NULL for an answer nothing frees, as a scalar or a text the provider
 * keeps, and for an input that is no handle.
 *
 * An input takes the context, a scalar, a text, texts, bytes, a POINTER, an ARRAY or a HANDLE; an answer a scalar, a
 * text, a POINTER of STRUCT, a VALUE, a HANDLE or a DEFINITION; a refusal a POINTER of STRUCT alone.
 */
typedef struct weivel_standard_described_parameter {
    const char *name;
    /** A weivel_standard_described_kind. */
    uint32_t kind;
    /** A weivel_standard_described_role. */
    uint32_t role;
    /** For a POINTER or an ARRAY, the weivel_standard_described_kind of what it points at; else NONE. */
    uint32_t element;
    const char *type;
    const char *count;
    const char *release;
} weivel_standard_described_parameter;

/**
 * One function pointer of the definition, an operation or a release, at `offset` in the definition's struct. An
 * operation of the standard's convention takes the context first, then its inputs and its answers, and at most one
 * refusal, which is its last parameter, and answers a STATUS; a release takes the context and the one thing it frees,
 * and answers NOTHING.
 */
typedef struct weivel_standard_described_operation {
    const char *name;
    size_t offset;
    /** A weivel_standard_described_return. */
    uint32_t returns;
    const weivel_standard_described_parameter *parameters;
    size_t parameter_count;
} weivel_standard_described_operation;

/**
 * One service's definition, described: `definition` names the struct of `types` the definition is, every struct and
 * enum its operations reach is in `types` and `enums`, and `operations` lists every function pointer the definition
 * carries, in the order the struct declares them. First shape ends at `operation_count`.
 */
typedef struct weivel_standard_description {
    uint32_t size;
    const char *definition;
    const weivel_standard_described_type *types;
    size_t type_count;
    const weivel_standard_described_enum *enums;
    size_t enum_count;
    const weivel_standard_described_operation *operations;
    size_t operation_count;
    /**
     * The names of the operations that may block, doing I/O or waiting, so a binding whose callers must not wait calls
     * them away from its calling thread; every other operation answers without waiting.
     */
    const char *const *blocking;
    size_t blocking_count;
    /**
     * The description of every definition a DEFINITION member or parameter of this description names, each found by
     * its own `definition`; each is a whole description, checked as this one is.
     */
    const struct weivel_standard_description *const *definitions;
    size_t definition_count;
} weivel_standard_description;

/** How deep `definitions` may nest below the description checked: a description nested deeper is refused. */
#define WEIVEL_STANDARD_DESCRIPTION_DEPTH_LIMIT 8

/**
 * Checks that a description can be bound as it says: it reaches its first shape; every name it requires is set, and
 * none repeats among the types, the enums, the operations, a type's members, an enum's entries or an operation's
 * parameters; every kind, element, role and return is one this header publishes, in the one spelling the kinds
 * allow; `definition` and every `type` it requires name a struct or an enum it holds; every member lies inside its
 * struct, with the size its kind has; no struct is filled whole by two STRUCT members, which no C struct lays over
 * each other; no struct holds itself by value, directly or through other structs, where a POINTER or an ARRAY may
 * point at it; every count names a SIZE member of the same struct, or a SIZE parameter of the same operation; every
 * release names one of its operations; every first shape lies within its struct; every operation lies inside the
 * definition's struct; every `blocking` name is set, names an operation that is no release, and is listed once;
 * every one of `definitions` is set, reaches its first shape and names its `definition`, no two naming the same; and
 * every DEFINITION's `type` is the `definition` of one of them.
 *
 * It then checks the description against the convention every binding builds on: an `element`, `type`, `count` or
 * `release` its kind or role leaves unused is NONE or NULL, as the member and parameter docs above say; every release
 * named is an operation answering NOTHING; a scalar or a text answer names no release; only the definition's struct
 * holds a CONTEXT, at most one, inside its first shape; no operation shares a name with a member of the definition's
 * struct, or a byte with one of its members or another operation; every operation takes the CONTEXT first and nowhere
 * else; each role takes only the kinds weivel_standard_described_parameter lists for it; an operation answering a
 * STATUS takes no REFUSAL but as its last parameter; a release takes exactly two parameters; every REFUSAL's
 * struct carries `detail` and `fix` TEXT members; and a DEFINITION is a member or an answer, never an input or a
 * refusal, names no release, and is no parameter of a release. Last, it checks each of `definitions` as a whole
 * description in the same way, refusing one that is a description holding it and one nested deeper than
 * WEIVEL_STANDARD_DESCRIPTION_DEPTH_LIMIT, so a cycle of descriptions is refused rather than followed. It reads only,
 * and stops at the first fault.
 *
 * @param description Required.
 * @param refusal the caller's empty refusal, or NULL for none; filled on WEIVEL_STATUS_MALFORMED with the fault's
 * `path`, as `types[2].members[1].count`, `blocking[1]` or `definitions[0].types[2].name`, a struct holding itself
 * refused at the `type` of the first member, in `types` order, holding a struct that holds its own; MISSING_FIELD
 * for a NULL name or array, DUPLICATE at the later of two names, INVALID_VALUE for any other fault, and
 * MALFORMED_CALL, at path "", for a NULL description or one below its first shape.
 * @return WEIVEL_STATUS_OK; WEIVEL_STATUS_MALFORMED at the first fault, and for a refusal below its first shape or
 * still holding one, which is left as it was; WEIVEL_STATUS_OUT_OF_MEMORY when the refusal's texts cannot be made, the
 * refusal left empty.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_description_check(const weivel_standard_description *description,
                                                                    weivel_standard_refusal *refusal);

#endif
