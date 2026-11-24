# The library declaration

A C library that plugins call declares itself in a JSON file, and `weivel_standard_read_library` reads and checks it.
The struct and its parts are in `include/weivel/library.h`; the code is `src/library.c`.

## What it declares

A declaration names the library by the `importName` plugins import it by, its version, the services its functions
offer and the contracts it publishes. It must declare something, so it lists at least one service or at least one
contract: a library that only publishes a contract has no service to invent, and its `services` may be empty or left
out, while a declaration listing neither is refused. Each service has an id plugins request and a list of functions, each
with the name a plugin calls it by, the exported C symbol that implements it, and its kind. A service id is lowercase
words joined by single hyphens or dots, so a dotted id such as `weivel.log` is legal, and an `importName` is refused
when it names a Windows device name such as `con` or `com1`, since a name that cannot be a file name cannot be the
`<importName>.json` the layout expects (see [layout.md](layout.md)).

## Two kinds of function

A **bytes** function takes opaque bytes and answers opaque bytes: status 0 answers the result, any other status refuses
and its body, when there is one, says why in English. Every body is the library's, freed by the library's one `release`
symbol, so a declaration holding a bytes function must name that symbol. A **definition** function answers one
service's whole definition, a static struct of function pointers that begins with its `size` and lives until the
library unloads; the struct's layout is the library's own, published in its own header, and a service that declares one
declares it as its only function. A definition carries its own context, so it declares no `context` and reads no facts
of the calling process; a bytes function may list the facts it reads (`environment`, `home`, `arguments`), each once.
The value 1 is retired and never reused, as every kind is append-only.

## Descriptions and contracts

A service whose only function is a definition may declare a `description` symbol, so a language that cannot read the
library's C header can bind the struct from [the description](descriptions.md) the symbol answers. That description
names the function answering the service's definition, and a host loads the service only when it answers the very
definition the service's symbol answered, so a description cannot describe one definition while another is served.
A library may also publish a description it implements nowhere, as a `contracts` entry naming a capability id and the
description's symbol: plugins that provide that capability implement one definition, and every language binds it
alike. A capability id has one grammar wherever it is written, lowercase words joined by single hyphens or dots
(`^[a-z0-9]+([-.][a-z0-9]+)*$`), the same one a manifest's `capabilities` lists are held to (see
[manifests.md](manifests.md)), so a plugin can provide every capability a contract can describe and no other. Each
capability is published once, and a contract's description answers no definition, so its answering
function is not compared.

## Reading it

The reader refuses the first field that breaks a rule, naming its path (as `services[0].functions[1].symbol`) and a
fix. A field the format does not define is ignored at every level, `$schema` among them, so a declaration written for a
later version still loads. How a language binds the functions is the language layer's business and not the
declaration's.
