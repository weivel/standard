A neutral C library of two definition services, the mirror and the walker, described by `weivel-describe` and built as a shared library for tests only, with its declaration (`library.json`) and three faulty copies of it. It is how the standard proves a description can be bound as a host binds it: the tests load the library at run time through `loader.c`, so its symbols are proven exported across a shared-library boundary rather than resolved at link time, call every member kind through the descriptions, and check that each broken description is refused. Nothing here is linked into the standard or exported by it.

<!-- generated: source-children -->
- [broken.c](../../../../test/described/broken.c) - three copies of the walker's description, each with one fault, written by hand
- [described.c](../../../../test/described/described.c) - the mirror and the walker, the two definitions the tests bind
- [described.h](../../../../test/described/described.h) - their declarations, in every member kind
- [loader.c](../../../../test/described/loader.c) - loading the shared library at run time on each platform
- [loader.h](../../../../test/described/loader.h) - the loader's functions
<!-- end generated -->
