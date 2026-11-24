`weivel-describe`, which writes the description a C library answers for a definition from the header that declares it. It is built from four layers: reading (`tokens.c` splits a header into tokens, `parse.c` reads declarations from them, `resolve.c` resolves each to what a description holds and applies the rules a description needs), the model that holds the result (`model.c`), writing (`writer.c` emits one C source per service from the model alone) and the program (`program.c` reads the command line, `main.c` and the `platform_*` files give it its arguments as UTF-8 and write the output). The reader and the writer are separate static libraries so the tests can reach each alone. What the program accepts and refuses, and why it writes what it writes, is [the descriptions doc](../../../descriptions.md)'s; the grammar is stated at the head of `reader.h`.

<!-- generated: source-children -->
- [CMakeLists.txt](../../../../tools/describe/CMakeLists.txt) - the reader and writer libraries, their testing builds and the program
- [main.c](../../../../tools/describe/main.c) - the program's entry point
- [model.c](../../../../tools/describe/model.c) - the model: releasing it, looking a type or enum up by name, and reaching every struct and definition a definition holds
- [model.h](../../../../tools/describe/model.h) - the model of a library's headers: structs, enums, handles, members, operations and where each was declared
- [parse.c](../../../../tools/describe/parse.c) - reading declarations from tokens into the model, in the grammar's few forms
- [platform.h](../../../../tools/describe/platform.h) - the program's arguments as UTF-8 and writing its output
- [platform_posix.c](../../../../tools/describe/platform_posix.c) - the platform functions on POSIX
- [platform_windows.c](../../../../tools/describe/platform_windows.c) - the platform functions on Windows, which read the wide command line
- [program.c](../../../../tools/describe/program.c) - the command line, and the run: read, write, replace the output only when it changed
- [program.h](../../../../tools/describe/program.h) - the program apart from its command line, and its exit codes
- [reader.c](../../../../tools/describe/reader.c) - reading headers: tokens, declarations, resolution
- [reader.h](../../../../tools/describe/reader.h) - the grammar of the headers it reads and refuses
- [reading.h](../../../../tools/describe/reading.h) - the functions the reading steps share
- [resolve.c](../../../../tools/describe/resolve.c) - resolving every spelled item to its kind, count, enum and release, and refusing what has none
- [tokens.c](../../../../tools/describe/tokens.c) - splitting a header into tokens, keeping docs and directives
- [tokens.h](../../../../tools/describe/tokens.h) - the token kinds and the tokenizer
- [writer.c](../../../../tools/describe/writer.c) - writing one C source per service from the model alone
- [writer.h](../../../../tools/describe/writer.h) - what the writer writes and the names it declares
<!-- end generated -->
