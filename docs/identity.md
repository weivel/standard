# Repository identity

An identity is the text that says which repository a plugin or a dependency comes from, taken apart into parts a host
can compare. It is declared in `include/weivel/identity.h` and matched by `src/identity.c`.

## Two kinds

A `local` identity is `local:<name>`, optionally followed by path segments, and is for a repository no forge hosts. A
forge identity is `<forge>[@host]:<number>`, then path segments: an opaque forge word, an optional host for a forge
that is not one place, and the repository's number on that forge. The number is the identity, never the repository's
name, so renaming a repository does not change who it is. Segments after the root name a path inside the repository and
are never `.` or `..`.

## One grammar in three places

The identity grammar is the schema's `pattern` for the `provider` of a consumed service and for a dependency's `id`, so
parsing accepts exactly the strings the schema accepts. The schema carries the pattern as text and `src/patterns.c`
holds the hand-written matcher for each pattern; the schema parity test pins each pattern string to its matcher, so a
changed pattern is red until its matcher is reviewed. A refusal for a text that is not an identity gives the byte
offset where it stopped being one, and a number that does not fit 64 bits is a refusal of its own, since a number is
held as a number.

## Comparing

Identities are ordered by their parts, not their text: kind first (local before forge), then for a local one its name
bytes, for a forge one the forge word, the host (none before any) and the number as a number, then the segments in
order, a shorter list first when it is a prefix of the other. Leading zeros do not matter, so `forge:007` and `forge:7`
are one repository and compare equal, and `forge:9` sorts before `forge:10`. Discovery uses this order to list plugins
(see [layout.md](layout.md)).
