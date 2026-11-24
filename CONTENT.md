## What this is

The weivel standard: the defaults weivel ships beside its core, as one C library a host may link or ignore. It holds the
JSON manifest and its validation against the schema this repository publishes, repository identities, plugins laid out
and discovered on disk, the library declaration, the config, secrets, paths, log, scope and host services, the
description a C library answers for a definition (with `weivel-describe`, which writes one from a header), and the
mirror list the core's binary is fetched from.

## What this is not

It is not the core and the core does not know it: the core reads no file, environment or format, and everything it
leaves out is a default here. A host that wants another format, another layout or no files at all builds the core's
structures itself and never links this library.

## Where to read on

Start at [docs/README.md](docs/README.md), the standard's architecture, and descend: the conventions every declaration
follows, manifests and the rules beside the schema, identities, layout and discovery, the library declaration,
descriptions, the services, mirrors, what replacing the standard means, and one doc per source folder, which also says
how to build and test.
