The sources `weivel-describe` is expected to write, byte for byte. `describe_write_test` writes each from its headers and compares, keeping the actual output beside the build when they differ. A change to the writer is a change to these files, reviewed as such, and the `unchanged` scenario of `describe/program_check.cmake` holds that a second run over the same headers rewrites none of them.

<!-- generated: source-children -->
- [config.c](../../../../../test/describe/golden/config.c) - the config definition described
- [nested.c](../../../../../test/describe/golden/nested.c) - the nested definition, with its remote definition described elsewhere
- [paths.c](../../../../../test/describe/golden/paths.c) - the paths definition described, with the export macro
- [probe.c](../../../../../test/describe/golden/probe.c) - the every-form fixture described together with the services header
- [restated.c](../../../../../test/describe/golden/restated.c) - the restated service, described with its own status and refusal names
- [scanned.c](../../../../../test/describe/golden/scanned.c) - the nested definition with the remote header scanned too
- [secrets.c](../../../../../test/describe/golden/secrets.c) - the secrets definition described
<!-- end generated -->
