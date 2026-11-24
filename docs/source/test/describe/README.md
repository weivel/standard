The headers and scripts the `weivel-describe` tests read: headers that must be described a certain way (`clauses.h`, `cycles.h`, `enums.h`, `every_form.h`, `nested.h` and its remote, `plurals_and_owners.h`, `restated.h`, `self_reference.h`, `skipped.h`, `status_named.h`), the expected output in `golden/`, the headers that must be refused in `refused/`, a hand-written description (`remote.c`) standing in for one another library exports, and the CMake scripts that run the program and the CMake function as a user would and check what they print and leave. The grammar they hold the program to is stated at the head of `tools/describe/reader.h`.

<!-- generated: source-children -->
- [arguments/](arguments/) - a project whose calls to the function must be refused for their arguments
- [arguments_check.cmake](../../../../test/describe/arguments_check.cmake) - configures the arguments project once per scenario and checks the refusal
- [clauses.h](../../../../test/describe/clauses.h) - a header whose docs bind enums to fields clause by clause
- [cycles.h](../../../../test/describe/cycles.h) - structs that point at themselves and at each other in rings
- [elsewhere/](elsewhere/) - a project that must be refused for calling the function from the wrong directory
- [enums.h](../../../../test/describe/enums.h) - enums numbered in every way the grammar reads
- [every_form.h](../../../../test/describe/every_form.h) - a member of every kind and an operation of every parameter form
- [function_check.cmake](../../../../test/describe/function_check.cmake) - configures the refusing project and checks the refusal names the directory to call from
- [golden/](golden/) - the sources `weivel-describe` is expected to write
- [nested.h](../../../../test/describe/nested.h) - a definition answering two others, one declared here and one described elsewhere, and an operation that blocks
- [nested_remote.h](../../../../test/describe/nested_remote.h) - a definition another library describes
- [plurals_and_owners.h](../../../../test/describe/plurals_and_owners.h) - counts named for plural arrays, and handles pointed at as const
- [program_check.cmake](../../../../test/describe/program_check.cmake) - runs the program as a user would: a refused header, a missing option, a definition described elsewhere, an answering function, a second run
- [refused/](refused/) - one header per way a header falls outside the grammar
- [remote.c](../../../../test/describe/remote.c) - the description another library would export for the remote definition, written by hand
- [restated.h](../../../../test/describe/restated.h) - a service whose library restates the status, refusal and value types
- [restated_types.h](../../../../test/describe/restated_types.h) - the restated types, declared in a header read after the one that uses them
- [self_reference.h](../../../../test/describe/self_reference.h) - a struct pointing at its own type, spelled both ways C allows
- [skipped.h](../../../../test/describe/skipped.h) - preprocessor lines, comments and macros the reader must skip without losing a doc
- [status_named.h](../../../../test/describe/status_named.h) - a status type that is an enum, which a doc may name without binding it
<!-- end generated -->
