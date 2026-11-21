## What this is

The defaults weivel ships, as one C library a host may link or ignore: a JSON reader, the JSON manifest read into the
core's manifest structure, its validation against the schema this repository publishes, repository identities, the
library declaration, plugins laid out and discovered on disk, the config, secrets, paths and log services, each an id
and a C struct of function pointers every language translates, the value tree a definition hands data in, the
description a C library answers for a definition so that a language which cannot read its header binds it, with the
check every binding runs on a description first, and the mirror list the core's binary is fetched from.

A description names the operations that may block (`blocking`, so a binding whose callers must not wait calls them
away from its calling thread) and nests the whole description of every definition a DEFINITION member or parameter
names, so an answer can carry a definition. `weivel-describe` writes one from a header: it reads `@blocking` on an
operation, and takes a definition described elsewhere by its symbol. A struct may point at its own type, or at one
reaching it, through a POINTER or an ARRAY, as a tree of blocks holds its blocks: `weivel-describe` reads the member
as C spells it inside the struct's own body, `const struct <tag> *`, and the check refuses a struct holding itself by
value, which has no finite size. `descriptions.h` exports the descriptions of the
standard's own definitions, so a library serving one declares the symbol and never describes it again.

Three more contracts are defined for a host program: `weivel.log` (`services.h`), `weivel.scope` (`scope.h`), which
opens each plugin a scope of its own services, and `weivel.host` (`host.h`), through which a program asks its provider
what to run and tells it what happened. An action run calls either an action the plugin's entry offers or a method of
a service the plugin itself provides, as the plan names it. A library's service id may be dotted, as `weivel.log` is,
and its `importName` is refused when it names a Windows device name such as `con` or `com1`. A host that wants another
format, another layout or no files at all builds the core's structures itself and never links this library.
