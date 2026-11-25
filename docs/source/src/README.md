The implementation of the library, one file per part and a few helpers they share. The parts are the JSON reader (`json.c`), the manifest read from it (`manifest.c`) and validated (`validate.c`) with the pattern matchers the schema needs (`patterns.c`), repository identities (`identity.c`), discovery (`layout.c`), the library declaration (`library.c`), the description check (`description.c`), the mirror list (`mirrors.c`), SHA-256 (`sha256.c`) and the service ids (`services.c`). The helpers are the single allocator, the refusal builders, the sized-struct copy, the C-locale number reader and the file module, with one implementation per platform. Why each part behaves as it does is in the descent docs: [manifests](../../manifests.md), [identity](../../identity.md), [layout](../../layout.md), [library](../../library.md), [descriptions](../../descriptions.md), [mirrors](../../mirrors.md) and [conventions](../../conventions.md). `files.h` is the library's only reach into the file system, and a link is never followed through it.

<!-- generated: source-children -->
- [allocation.c](../../../src/allocation.c) - the one allocator, and in the testing build its block count and failure injection
- [allocation.h](../../../src/allocation.h) - the allocator's functions and the testing build's controls
- [c_locale.c](../../../src/c_locale.c) - an object for the C locale per platform, so a number is read the same whatever the host's locale
- [c_locale.h](../../../src/c_locale.h) - making, releasing and reading a number through that locale
- [description.c](../../../src/description.c) - the description check: every rule a binding relies on, applied in order, stopping at the first fault
- [file_names.c](../../../src/file_names.c) - the list of names a directory listing builds
- [files.h](../../../src/files.h) - the library's only reach into the file system: listing, reading and the kind of a path, never following a link
- [files_posix.c](../../../src/files_posix.c) - the file module on POSIX
- [files_windows.c](../../../src/files_windows.c) - the file module on Windows, which converts from UTF-16 and skips every reparse point
- [identity.c](../../../src/identity.c) - parsing and comparing identities, with the offset where a text stops matching
- [json.c](../../../src/json.c) - the JSON reader, its refusals beyond the grammar and its number handling
- [json_equality.h](../../../src/json_equality.h) - equality of two JSON values as `uniqueItems` counts it
- [layout.c](../../../src/layout.c) - discovery: scanning a directory, loading manifests and identity files, ordering and refusing duplicates
- [library.c](../../../src/library.c) - reading a library declaration and checking it field by field
- [manifest.c](../../../src/manifest.c) - converting a JSON manifest into the core's manifest structure
- [mirrors.c](../../../src/mirrors.c) - reading and loading a mirror list, and expanding a template
- [patterns.c](../../../src/patterns.c) - the hand-written matchers for the schema's `pattern` strings, the capability id's shared with the library declaration, whose service ids use the same grammar
- [patterns.h](../../../src/patterns.h) - the matchers' declarations and what each decides
- [report.c](../../../src/report.c) - building refusals: field paths, formatted texts and the filled refusal
- [report.h](../../../src/report.h) - the refusal builders and the field path they write from
- [services.c](../../../src/services.c) - the service ids as functions
- [sha256.c](../../../src/sha256.c) - SHA-256 and the download check
- [sized.c](../../../src/sized.c) - copying a caller's sized struct into the library's full copy and back
- [sized.h](../../../src/sized.h) - the size-prefixed struct helpers
- [validate.c](../../../src/validate.c) - the validator: the schema's rules and V1 and V2, walked together, every wrong field listed once
<!-- end generated -->
