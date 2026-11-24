The one CMake module the standard offers to libraries that depend on it: `weivel_describe`, which runs `weivel-describe` at build time over a library's headers and adds the sources it writes to a target. The usage, every keyword and the constraints are stated at the top of the file, and what a description is and why it is generated is [the descriptions doc](../../descriptions.md)'s. It refuses to run from a directory other than the one defining the target, because CMake knows a generated source as generated only to its own directory.

<!-- generated: source-children -->
- [weivel_describe.cmake](../../../cmake/weivel_describe.cmake) - the `weivel_describe` function: runs the program over headers at build time and adds its sources to a target
<!-- end generated -->
