The public headers, each one a part of the standard: the declarations, and in their comments the contract of every call, which is where a language implementation reads its behaviour. The conventions every header follows (sized structs that only grow, refusals as data, who frees what) are [the conventions doc](../../../conventions.md)'s, the manifest and its validation are [the manifests doc](../../../manifests.md)'s, and each service's meaning is in [the services doc](../../../services.md). `standard_api.h` is the one place the export macro is decided, so the static, shared and consumer builds differ in nothing else.

<!-- generated: source-children -->
- [description.h](../../../../include/weivel/description.h) - the description types, their kinds, roles and forms, and the check every binding runs first
- [descriptions.h](../../../../include/weivel/descriptions.h) - the descriptions of the standard's definitions other definitions nest, and how a library serving one describes it
- [host.h](../../../../include/weivel/host.h) - the host service: the plan a run follows, the outcome of each plugin, when to tick, and the order the program follows
- [identity.h](../../../../include/weivel/identity.h) - a repository identity taken apart, parsed and compared
- [json.h](../../../../include/weivel/json.h) - the JSON reader and its tree, with what it refuses beyond the grammar
- [layout.h](../../../../include/weivel/layout.h) - discovery: the plugins and failures one scan of a directory answers
- [library.h](../../../../include/weivel/library.h) - the library declaration: services, functions, their kinds and the contracts it publishes
- [mirrors.h](../../../../include/weivel/mirrors.h) - the mirror list, expanding a template for a hash
- [scope.h](../../../../include/weivel/scope.h) - the scope service: one plugin's own config, secrets, paths and log
- [services.h](../../../../include/weivel/services.h) - the config, secrets, paths and log definitions, the service refusal and the conventions of every definition
- [sha256.h](../../../../include/weivel/sha256.h) - SHA-256, and the check that a download is the file that was asked for
- [standard.h](../../../../include/weivel/standard.h) - the refusal, the manifest read and its validation, the conventions every declaration follows
- [standard_api.h](../../../../include/weivel/standard_api.h) - the export macro, decided once for static, shared and consuming builds
- [value.h](../../../../include/weivel/value.h) - the typed value tree a definition hands data in
<!-- end generated -->
