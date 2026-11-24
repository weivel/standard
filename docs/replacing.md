# Replacing the standard

The standard is replaceable by design: the core stands alone, and everything here is a default that a host may take,
change in part or leave. This doc says what that means and what a replacement still owes.

## What the core knows

Nothing of the standard. The core takes a manifest as a C structure and a clock as a number, and reads no file,
environment variable or format. The dependency runs one way: the standard links the core (at the commit `repo.json`
pins) for the manifest structure, the status codes and the string list type, and the core holds no reference back.

## What a host may replace

- **A host that wants another format, another layout or no files at all** builds the core's structures itself and never
  links this library. Nothing is left to honour: the core accepts what it is given.
- **A host that keeps the library and changes a service** serves its own provider under the service's id. The
  definitions in `services.h`, `scope.h` and `host.h` are the contract: a provider fills the struct of function
  pointers the header publishes and serves it, and a consumer calls through it and cannot tell whose it is. The
  standard ships no provider of them, only the ids and the structs, so every provider is a replacement in this sense.
- **A language implementation** translates the same definitions, by reading the header or by binding the description a
  library answers for one, so its plugins meet the same ids and layouts as every other language's.

## What a replacement owes

- **The ids and layouts stay as published.** A service id is a promise to every plugin that resolves it, and a
  definition's layout is the promise to every language that translated it. Fields and operations are only appended,
  and a published code, kind, level or mode keeps its number.
- **A manifest format is its own world.** Plugins written for the JSON manifest load where the JSON manifest is read.
  A replacement that reads another format produces the core's manifest structure and owes the core nothing else; it
  does not owe the schema, which is the JSON format's.
- **Behaviour the standard holds is held by vectors, not by this repository.** The standard's behaviour is judged by
  the conformance kit's vectors (the conformance kit's `docs/vectors/`), which this repository
  does not own, so a replacement can be judged by the same ones where it claims the same behaviour.
