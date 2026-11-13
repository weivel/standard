# The rules the validator applies beside the schema

`schema/plugin.schema.json` carries every rule draft-07 can express. The two rules here cannot be
expressed there, so `weivel_standard_validate` applies them alongside the schema's rules, in the same
walk: a manifest is not first checked by the schema and only then by these. Every wrong field is
listed, whichever rule found it, each once, in the order the validator reached it; the first of them is
also the validation's refusal. The conformance kit's `validate` vectors hold the validator to both.

Each rule names the path its refusal reports, its code, and the `fix` it carries, because a refusal is
data and an author reads the fix.

## V1. A service is listed at most once on each side

A service id appears at most once in `services.provides` and at most once in `services.consumes`. An
entry is either the id as a string or an object carrying `id`, and **the two forms name the same
service**, so `["a:x", { "id": "a:x" }]` lists `a:x` twice.

- **Why it is not in the schema:** the arrays mix strings and objects, and `uniqueItems` compares whole
  items, so it cannot see that a string and an object carry the same id.
- **Refusal:** `DUPLICATE` at the path of the later occurrence, for example `services.provides[1]`, with
  the fix `remove the duplicate entry`. Every later occurrence is listed.

## V2. A provided service id is namespaced or well-known

An id in `services.provides` is legal when the host lists it as well-known, or when it is namespaced by
the plugin's own `importName`: `<importName>:<name>`, both parts non-empty. Anything else is refused.

An `importName` is chosen by the plugin and nothing makes it unique, so two plugins can both be legal
under `a:x`. What decides which one a consumer reaches is `provider` on its `services.consumes` entry,
which names the providing repository's identity.

- **Why it is not in the schema:** the well-known list is the host's, given at validation time as
  `weivel_standard_validation_options.well_known_services`, so one manifest can be legal on one host
  and refused on another.
- **When it is skipped:** given no well-known list, the validator does not apply V2. An empty list is a
  list, and refuses every bare id.
- **Refusal:** `NOT_WELL_KNOWN` at the path of the offending entry, for example `services.provides[0]`,
  with a fix that names the namespaced form, `<importName>:<name>`, and the host's well-known ids.
