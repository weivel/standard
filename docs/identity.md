# Repository identity

An identity is the text that says which repository a plugin or a dependency comes from, taken apart into parts a host
can compare. It is declared in `include/weivel/identity.h` and matched by `src/identity.c`. weivel compares identities
and never resolves one: finding where a repository lives is the host's business.

## Two kinds

A `local` identity is `local:<name>`, the name holding no whitespace, NUL or `/`, optionally followed by path segments,
and is for a repository nothing hosts. A git identity is `git:<hex>`, then path segments, where the hex is the object
id of the repository's first-parent root commit: 40 lowercase hex digits for SHA-1, 64 for SHA-256. The root commit is
the identity, never the repository's name or the number a host gives it, so renaming or moving a repository does not
change who it is. Segments after the root name a path inside the repository, each `[a-z0-9._-]+` and never `.` or `..`.

## One grammar in three places

The identity grammar is the schema's `pattern` for the `provider` of a consumed service and for a dependency's `id`, so
parsing accepts exactly the strings the schema accepts. The schema carries the pattern as text and `src/patterns.c`
holds the hand-written matcher for each pattern; the schema parity test pins each pattern string to its matcher, so a
changed pattern is red until its matcher is reviewed. The conformance kit's `vectors/standard/identity` hold the same
grammar, and `test/identity_test.c` answers each of them.

A refusal for a text that is not an identity is always `invalid_value`, its detail beginning `byte <offset>:`. The
offset is 0 for a text that starts with neither `git:` nor `local:`; past `git:` it is where a run of fewer than 40 hex
digits ends, the 41st digit of a run of 41 to 63 and the 65th of a longer one; otherwise it is the first byte that
cannot continue the name or a segment, or a segment's first byte when the segment is empty, `.` or `..`.

## Comparing

Identities are ordered kind first (local before git), then a local one by its name bytes and a git one by its hex as
text, then the segments in order, a shorter list first when it is a prefix of the other. Text compares bytewise.
Discovery uses this order to list plugins (see [layout.md](layout.md)).
