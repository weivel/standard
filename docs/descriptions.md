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
([weivel-describe](#weivel-describe)); among the standard's own services every operation of config and of secrets but
a release carries it, and the host's `tick` ([services.md](services.md#config-secrets-paths-and-log)). What a language does with
`blocking` is its own, stated in each language implementation's own docs and gathered in one table in weivel's `docs/languages/reaching-c.md`. A member or a parameter that holds a definition names the
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

## Which definition: the answering function

A definition is a struct of function pointers that carries no name, so nothing in it says which description is its
own: a library declaring one definition under another's description would load, and a binding would read the one
through the other's layout. So a description a service declares names the function that answers its definition, in
`answered_by`, the same symbol the service declares as its definition. After the check passes, a host calls
`weivel_standard_description_answers` with the definition the service's symbol answered, and loads no service whose
description names no function or one answering another definition. It compares the two definitions' addresses, never
the two functions': a definition is a static struct living until the library unloads, so one function answers the same
pointer every time, while a function's own address may differ between the export table and the library's code.

So one description describes one definition. Two definitions of the same struct served by one library each have their
own description, one more service in the generator's call. `answered_by` is not read when a description is nested in
another's `definitions` or declared by a `contracts` entry, which answer no definition of their own, and the check does
not police it.

## weivel-describe

`weivel-describe` writes a description from a library's own header, so a description is never typed by hand and cannot
drift from the struct it describes. It is not a C parser: it reads the few forms a weivel definition is written in and
refuses everything else, so a header it accepts is described whole or not at all. The accepted forms, the count and
enum binding rules and the `@blocking` tag are stated at the head of `tools/describe/reader.h`. It takes a definition
described elsewhere by its symbol, so a library that nests one of the standard's definitions declares that symbol and
does not describe it again. Given the function answering the definition (`--answered-by`), it names it in the
description and declares it, with the export macro, unless a header it read declares it, so a misspelt function is a
link error rather than a NULL at run time; given none, the description names none, as one only ever nested does.

The program writes one C source per service. Every offset and size in it is `offsetof` or `sizeof` of the header's own
type, so the library's compiler fills them and a member the header no longer declares is a compile error. Running a
second time over unchanged headers leaves the source untouched, its modification time included, so nothing built from
it is rebuilt. A refused header exits non-zero, prints the refusal as `<file>:<line>: <message>` and removes any source
an earlier run left, so a stale source never outlives a run that could not replace it.

## In a build

`cmake/weivel_describe.cmake` provides `weivel_describe`, which runs the program at build time over named headers and
adds the generated source to a target, each `SERVICE <struct> SYMBOL <symbol>` optionally followed by
`ANSWERED_BY <function>`. The standard uses it for the definitions other definitions nest:
`include/weivel/descriptions.h` exports `weivel_standard_config_description`, `weivel_standard_secrets_description`,
`weivel_standard_paths_description` and `weivel_standard_log_description`, each naming no answering function, and a
library describing a definition that holds one names it as described elsewhere and never describes it again.

A library serving one of the standard's services describes it itself, since a description shared by every library
serving the service could name the function of none of them. It runs `weivel_describe` over the standard's header for
that definition, `ANSWERED_BY` its own function and `DESCRIBED_ELSEWHERE` the definitions it nests: a library serving
`weivel.scope` names `weivel_standard_config`, `weivel_standard_secrets`, `weivel_standard_paths` and
`weivel_standard_log`. The bytes are generated from the same header by `offsetof` and `sizeof`, so its copy cannot
drift from the standard's struct. The program runs on the build machine, so a cross-compiled library cannot be
described.
