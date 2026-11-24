# Descriptions

A language that cannot read a C header still has to call a library's definitions. The description is how: a static,
read-only value a library answers for one definition, listing its structs and enums, every member with its offset and
size, and every operation with its parameters, so a binding reconstructs the layout from data. The types are in
`include/weivel/description.h`, the check in `src/description.c`, and the program that writes descriptions in
`tools/describe/`.

## What a description says

A description is the definition's struct, the structs and enums its operations reach, and its operations, each with its
parameters in call order and what each is to the call: an input read during the call, an answer filled on success and
freed by a named release, or the refusal. Every parameter and member has a kind (a scalar, a text, texts, bytes, a
struct, a pointer, a counted array, a value tree, a handle, a context, an answer's storage or a definition). The kinds,
roles and return forms are append-only, and a binding that meets one it does not know refuses the whole description
rather than binding part of it.

Two things a binding needs beyond layout. An operation that may block is named in `blocking`, so a binding whose callers
must not wait calls it away from its calling thread. A header marks such an operation with the `@blocking` tag
([weivel-describe](#weivel-describe)); among the standard's own services only the host's `tick` carries it
([services.md](services.md#host-what-a-program-runs)). What a language does with `blocking` is its own, stated in each
language implementation's own docs and gathered in one table in weivel's `docs/languages/reaching-c.md`. A member or a parameter that holds a definition names the
definition's own description, nested whole inside this one, so an answer can carry a definition and the binding binds it
in the same pass.

A struct may point at its own type, or at one reaching it, through a pointer or an array, as a tree of blocks holds its
blocks, so a binding following one walks a struct it may already be inside. A struct holding itself by value has no
finite size and is refused.

## Trust: the check

A binding calls `weivel_standard_description_check` before it reads anything else of a description, and binds nothing
of one the check refuses. The check is the one place the rules of a well-formed description live: every name is set
and none repeats, every offset lies inside its struct with the size its kind has, every count names a size beside it,
every release names an operation that answers nothing, every operation takes the context first, and nested descriptions
are checked as a whole and refused when they nest too deeply, so a cycle of descriptions is refused instead of
followed. It reads only, and stops at the first fault, whose path it gives (as `types[2].members[1].count`). Putting
the rules here once means no language implementation has to rediscover them, and a malformed description from a
library cannot reach a binding as a wrong pointer arithmetic.

## weivel-describe

`weivel-describe` writes a description from a library's own header, so a description is never typed by hand and cannot
drift from the struct it describes. It is not a C parser: it reads the few forms a weivel definition is written in and
refuses everything else, so a header it accepts is described whole or not at all. The accepted forms, the count and
enum binding rules and the `@blocking` tag are stated at the head of `tools/describe/reader.h`. It takes a definition
described elsewhere by its symbol, so a library that nests one of the standard's definitions declares that symbol and
does not describe it again.

The program writes one C source per service. Every offset and size in it is `offsetof` or `sizeof` of the header's own
type, so the library's compiler fills them and a member the header no longer declares is a compile error. Running a
second time over unchanged headers leaves the source untouched, its modification time included, so nothing built from
it is rebuilt. A refused header exits non-zero, prints the refusal as `<file>:<line>: <message>` and removes any source
an earlier run left, so a stale source never outlives a run that could not replace it.

## In a build

`cmake/weivel_describe.cmake` provides `weivel_describe`, which runs the program at build time over named headers and
adds the generated source to a target. The standard uses it for its own definitions: `include/weivel/descriptions.h`
exports `weivel_standard_config_description`, `weivel_standard_secrets_description`, `weivel_standard_paths_description`,
`weivel_standard_log_description`, `weivel_standard_scopes_description` (nesting the four before it) and
`weivel_standard_host_description`. A library serving one of those services declares the matching symbol and never
describes it again. The program runs on the build machine, so a cross-compiled library cannot be described.
