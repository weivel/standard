# Plugin layout and discovery

How plugins lie on disk by the standard's layout, and how one scan of a directory finds them. The function is
`weivel_standard_discover` in `include/weivel/layout.h`, implemented in `src/layout.c` over the platform file module.

## The layout

A plugin is a file `<importName>.json` in a directory: its manifest. Beside it may sit `<importName>_identity.json`,
holding `{"id": "<identity>", "name": "<repository name>"}`, and the file the manifest's `entry` names, resolved
against the same directory. The identity is a separate file because the manifest is the author's, while the identity is
where the plugin was obtained from, which only whatever put it in the directory knows. An importName holds no
underscore, so no identity file can be read as a manifest. A language layer therefore never writes a file named like a
manifest into a plugin directory.

## One scan, no hiding

A scan reads one directory, not its subdirectories. Every regular file that is a manifest is loaded and validated
through [the manifest load](manifests.md); every one that does not load is a failure naming its file and carrying a
refusal, so one bad file never hides the rest. A manifest whose importName is not its file name is a failure of its
own, and so is an identity file that is not a regular file, not JSON, or lacks a usable `id` or `name`. An identity
file that is there and cannot be trusted refuses the plugin; it never leaves it reading as local. A missing identity
file is not a failure: the plugin is local, and giving it an identity or none is the host's call.

## Links are never followed

A link is never followed, and on Windows every reparse point is skipped, a cloud placeholder file among them. The same
holds for an entry: it is reported present only when it is a regular file reached through real directories at every
component, since a link at `lib` in `lib/main.js` could lead outside the directory. A file over 1 MiB is unreadable and
refused alone, so one huge file cannot exhaust memory.

## Order

Plugins come ordered by identity (see [identity.md](identity.md)), then every plugin with no identity file by
importName. A plugin whose identity an earlier one already holds is refused as a duplicate, naming its identity file,
and such failures come last; the others come in the order of their file names. The order never depends on the order the
file system listed the entries, so two scans of one directory agree on every platform.

## What the answer carries

A directory with no plugin answers ok with nothing found. A directory that does not exist is a refusal (not found), so
a host that treats a missing directory as empty maps that itself. Everything the answer holds lives until the
discovery's release.
