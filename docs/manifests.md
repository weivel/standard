# Manifests

A plugin's manifest is read from JSON into the core's manifest structure, and checked against the schema this
repository publishes. The declarations are in `include/weivel/standard.h` and `include/weivel/json.h`; the code is
`src/json.c`, `src/manifest.c` and `src/validate.c`.

## Reading, validating, loading

`weivel_standard_read_manifest` converts and does not validate: a field the core's manifest has no place for is ignored
(a dependency list among them), a field left out takes the core's default, and `null` is a value of the wrong type,
never an absent field. A manifest's identity is left empty because it is supplied by whatever loaded the plugin, never
written in the manifest (see [layout.md](layout.md)). `weivel_standard_validate` judges a document against the schema
and the two rules [validation.md](validation.md) states, and lists every wrong field, each once.
`weivel_standard_load_manifest` does both, validating first, so a host taking the standard's defaults cannot hand the
core a manifest that skipped validation. The two steps stay separate functions because a host that validates elsewhere
may still want the reader.

## The schema, and a validator that is not a schema engine

`schema/plugin.schema.json` is the published vocabulary: a plugin's `importName`, `api`, `entry`, `targets`,
`capabilities`, `services`, `permissions` and `dependencies`, with the draft-07 rules for each. The validator is
hand-written C, not a general schema engine, because the library has to run anywhere C does with nothing beside it.
That would let the schema and the code drift apart, so a parity test walks the schema and fails on a property no rule
of the validator covers, on a rule that names a location the schema does not hold, and on a pattern with no matcher;
each rule has a test case that proves it. A field the schema does not name validates and is ignored at any depth, which
is how a later manifest still loads on an older reader.

## The JSON reader

`weivel_json_parse` reads one JSON document into a tree the caller frees. Beyond the grammar it refuses a duplicate key
within one object, a number that overflows a double, text that is not well-formed UTF-8, a leading byte order mark and a
NUL in an object key, since each is a way for two readers to read one document two ways. `uniqueItems` equality
(`src/json_equality.h`) compares numbers as JSON Schema does, whatever their spelling, an integer equalling a real only
when the real converts exactly. Line and column in a refusal count bytes. The duplicate-key check compares each key with
the earlier ones, so it is quadratic in the members of one object, which a manifest's size makes irrelevant.

## Refusal codes

The codes a manifest can be refused with are in `standard.h` and are append-only: malformed JSON, wrong type,
unrepresentable value (an `api` outside 0 to 4294967295, which the core cannot hold), missing field, invalid value,
duplicate, not well known, conflicting field, and the three the file module adds for discovery (unreadable, not found,
name mismatch). Each carries the path of the field and a `fix`.
