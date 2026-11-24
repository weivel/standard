# Services

The standard defines six services a host provides, each an id and a C struct of function pointers that every language
translates. A provider fills the struct and serves it as its service object; a consumer calls through it. The standard
ships the ids, the structs and their descriptions, and no provider (see [replacing.md](replacing.md)). The structs are
in `include/weivel/services.h`, `scope.h`, `host.h` and `value.h`, the ids' accessor functions in `src/services.c`, and
the conventions every one follows are [conventions.md](conventions.md)'s.

A plugin gets a service object by listing its id under `services.consumes` in its manifest (see
[manifests.md](manifests.md)), and the core refuses a service the plugin does not consume; resolving the id to the provider's object is the core's job and the language layer's, never the standard's.

No value crossing these definitions is JSON: JSON is a file format of the standard's defaults, and a service reached
from any language hands its data over as typed structs and the value tree below. An id is also a function, for a binding
that cannot read a macro.

## Config, secrets, paths and log

- **`weivel.config`** answers settings. `all` merges the provider's defaults with what the host stores, a stored value
  winning, and `get` answers one setting as text, a number or a boolean. `get_tree` answers any setting whole as a value
  tree, so a setting that is a list or a record is read as the host stores it. `set` stores a value, replacing any, and
  `settings_file` names the file `set` writes, the one a user edits by hand, so a consumer can tell a user which file
  to change. A value of kind none, or a number that is not finite, is never a value to store. Every operation but a
  release is marked blocking (see [descriptions.md](descriptions.md)), since a provider reads and rewrites its
  settings files under a lock.
- **`weivel.secrets`** keeps secrets as bytes, any bytes at all, under a name. `get` answers the bytes under a name,
  released with `release_secret`, `set` stores them, replacing any, and `remove` deletes them; removing a name that
  holds nothing succeeds. A provider should overwrite a secret's bytes before freeing them. The operation is `remove`, not `delete`,
  so the header stays includable from C++. Every operation but a release is marked blocking, since an OS credential
  store may prompt the user.
- **`weivel.paths`** answers absolute directories the provider owns and keeps the same for the definition's life, so
  nothing is released: where rebuildable files are kept, where settings are kept, and the directory offered as a
  plugin's own. Whether a directory exists yet is not promised.
- **`weivel.log`** takes entries at a level (least important first, append-only). Where an entry goes, how it is
  formatted and which levels are kept are the provider's. `write` is not blocking: a provider buffers an entry or
  hands it off rather than wait on I/O.

## Scope: each plugin sees its own

None of the four above names a plugin, and one definition cannot tell its callers apart, because every plugin that
resolves a service holds the same object. A host that wants each plugin to see its own settings, secrets, directories
or log does it outside those definitions: **`weivel.scope`** (`scope.h`) opens, for one plugin chosen once by its
manifest path, a scope holding that plugin's own config, secrets, paths and log, each NULL where the provider offers
that plugin none. The plugin is chosen when the scope is opened and never again in a call, so the rule that no
operation names a plugin still holds.

## Host: what a program runs

**`weivel.host`** (`host.h`) is how a program asks its provider what to run and tells it what happened. The program
decides nothing of its own: the provider's `start` answers a plan, which libraries to load, which plugins to activate
in which order, and whether to serve or to run one action. Time reaches C only through `tick`'s `now`, which keeps the
core's rule that it reads no clock. `tick` is marked blocking (see [descriptions.md](descriptions.md)), since a provider
may wait on I/O in it, as when it checks for updates.

The plan is followed in a fixed order: the program loads the libraries in order, then activates the plugins in order,
opening each plugin's scope immediately before that plugin's activation. It reads and checks a plugin's manifest before
opening its scope, so a plugin whose manifest cannot be read gets no scope and is told as refused. The scope is asked of
whichever library the program loaded serves a described `weivel.scope`, the library it was started with included; when
two do, the first loaded wins, the library it was started with first, and a program none of them serves opens none and
offers the plugin nothing. The program offers each definition the scope answers to that plugin alone, under the
standard's id for it, so the plugin resolves `weivel.config` and the rest by the same ids whether or not a scope
stands behind them. A program never offers a plugin `weivel.host` or `weivel.scope`, since a scope is made for one
plugin and handed to no other, and a library serving `weivel.scope` offers config, secrets, paths and log through the
scope alone.

An action run calls either an action the plugin's entry offers, by name, or a method of a service the plugin itself
provides, as the plan names it (`action_service` and `action_method`, both set or both unset; one without the other
ends the run). In serve mode the program calls `tick` at once and again at the time the provider answered, until a
stop request. Once `start` has answered ok the program calls `stop` last, once, whatever happened, and exits with the
code `stop` answered (0 to 255), or 1 when `stop` does not answer ok. A stop request takes effect once the call it arrived during has returned, never in the
middle of one. Every plugin the plan named is attempted before a stop that arrived during activation takes effect, and
each is told as activated, refused or failed with the reason.

## The value tree

`value.h` defines one typed tree for data a definition only stores or hands back and whose shape belongs to someone
else, such as a setting's value or a plugin's props. A field whose shape the definition decides is typed by its own
struct, never by this. It is the tree a description's `value` kind names, so every binding knows one shape for it. A
number is a finite double, so an integer is exact only up to 2^53; a text is well-formed UTF-8 with no NUL; a list or a
record holds its nodes in order, each record key once; and every node is reached exactly once from its root, so no node
is shared and none lies on a cycle. Nesting is bounded, which also bounds every walk over a tree. A provider owns a tree
until the consumer hands it to the definition's release for it.
