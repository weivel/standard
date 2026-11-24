A tiny project that calls `weivel_describe` from a directory below the one that defines its target, which the function must refuse. `describe/function_check.cmake` configures it and checks the refusal names the directory to call from.

<!-- generated: source-children -->
- [CMakeLists.txt](../../../../../test/describe/elsewhere/CMakeLists.txt) - the project that defines a target and calls the function from a subdirectory
- [inside/](inside/) - the subdirectory that holds the refused call
<!-- end generated -->
