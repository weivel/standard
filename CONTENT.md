## What this is

The defaults weivel ships, as one C library a host may link or ignore: a JSON reader, the JSON manifest read into
the core's manifest structure, its validation against the schema this repository publishes, repository identities,
the library declaration, plugins laid out and discovered on disk, the ids of the config, secrets and paths services,
and the mirror list the core's binary is fetched from. A host that wants another format, another layout or no files
at all builds the core's structures itself and never links this library.
