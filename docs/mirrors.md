# Mirrors and downloads

Fetching the core's binary by its content is how a host is meant to obtain it, and this doc covers the two things the
standard does for that: the list of places to fetch it from, and the check that what came back is what was asked for.
The declarations are in `include/weivel/mirrors.h` and `include/weivel/sha256.h`; the code is `src/mirrors.c` and
`src/sha256.c`.

Nothing fetches the core's binary today. The core publishes no binary, and no language implementation calls these
functions to obtain it: Java's and TypeScript's standard modules wrap them for a caller and nothing more. Each language
implementation builds the core with CMake from the core checkout it pins and loads it from a path, the TypeScript
package by default from the `native/` folder its own build fills.

## The mirror list

A mirror list is `{ "mirrors": [ "<template>", ... ] }`: an ordered, non-empty list of URL templates, each holding the
placeholder `{sha256}` exactly once. The standard ships no list. The list is a user's file, so a user, not weivel,
decides where the core's binary comes from and can replace every entry; no mirror is required and none is privileged. A
field the format does not define is ignored. Loading from a path reads a UTF-8 file of at most 1 MiB and does not follow
a link; a file that is absent is a refusal of its own (not found), distinct from one that cannot be read.

## Expanding a template

The binary is addressed by its content, so `weivel_standard_mirror_expand` puts a hash into a template: the hash must be
64 lowercase hex characters and the template must hold the placeholder exactly once. Uppercase is refused rather than
folded, so one binary has one name on every mirror.

## Verifying a download

The standard never fetches. The caller downloads, then asks `weivel_standard_verify_download` whether the bytes hash to
the expected SHA-256 (FIPS 180-4, implemented in `src/sha256.c` so the check needs no library). A mismatch is a refusal
whose detail names both hashes. Keeping fetching out of the standard is what lets a host choose its own transport and
its own policy about retries and trust, while the one check that makes a mirror safe to use lives here once.
