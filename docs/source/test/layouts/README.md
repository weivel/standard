The layouts program and the script that checks it. The program prints the layout of every public struct as this compiler lays it out, so a language that declares the same layouts by hand can compare its own against the C compiler's; the script holds the program to the headers, so a struct or field added to a header and forgotten in the program fails, and a second script plants a fault for each way the first can refuse.

<!-- generated: source-children -->
- [layouts.c](../../../../test/layouts/layouts.c) - prints every public struct's layout as this compiler lays it out
- [layouts_check.cmake](../../../../test/layouts/layouts_check.cmake) - checks the printed layouts against the headers, struct by struct and field by field
- [layouts_check_test.cmake](../../../../test/layouts/layouts_check_test.cmake) - plants one fault per way the check can refuse, and expects each refusal
<!-- end generated -->
