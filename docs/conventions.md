# Conventions

The rules every declaration of the standard follows, stated once here so no other doc restates them. The headers carry
the same rules beside the declarations they bind (`include/weivel/standard.h`, `services.h`, `description.h`), and a
doc of one part names its own exceptions and nothing else.

## Sized structs that only grow

Every top-level struct a caller hands in or gets back begins with `uint32_t size`, which the caller sets to `sizeof`
the struct as it was compiled (the core's `WEIVEL_INIT` does it). Fields are only ever appended. The standard reads
only the bytes the caller's `size` covers, a field beyond it taking its default, and writes only within them, so a
caller compiled against an older header keeps working against a newer library and a newer caller against an older
library. A size below a struct's first shape, the struct as first published, is a malformed call.

Why: the standard is linked by hosts and by language implementations that are built at different times, and a library
that could only be used by callers of its own version would force every one of them to move together. The same rule is
the core's, so one habit serves both.

An element of an array the standard hands out carries no size and never grows, since the caller indexes it at its own
stride. A change to such an element arrives as a new array appended to the struct that carries the old one.

## Refusals are data

A refusal is a value shaped like the core's: an append-only code, a `detail` that says what happened, a `fix` that says
what would make the call succeed, both English UTF-8, and the `path` of the field that is wrong. A path step for a key
that is not made of letters, digits, `_` and `-` is written as a bracketed, JSON-escaped string, so a key a manifest's
author chose can never read as two steps. The codes are append-only: a published code keeps its number and its meaning,
none is renumbered or reused.

A call answers one of the core's statuses: ok, none, malformed (a NULL required pointer, a struct below its first
shape, or one still holding an answer), refused (the input was understood and declined, with the refusal filled) and
out of memory. A refusal the caller hands in that is still holding a refusal is malformed and left as it was, so a
refusal is never overwritten and never leaked.

## Whoever answers, frees

What a filled answer or refusal points at belongs to the standard, in storage private to it, until the caller hands
the struct to the release for its kind. A release frees everything, leaves the struct empty with its `size` kept, and
is a no-op for NULL or an empty struct. Text and bytes a caller hands in are borrowed for the call only. Each kind of
struct has its own release, so the standard never has to tell an answer's storage from a refusal's.

## Threads and time

The library holds no global state outside its testing build, so calls are safe concurrently on distinct objects, and
one object is never handed to two calls at once. Reading a JSON number uses the C locale as an object of its own, so a
host's locale changes nothing about how a number is read. A service a provider serves is one object shared by every
plugin that resolves it, so a provider is safe for concurrent calls from any thread, its releases included. Every
operation completes before it returns: a language whose interface is asynchronous presents a call as asynchronous.
The operations are shaped so that a service's provider may live in another process: such a provider answers once
the write has reached it.

## One allocator

Every allocation goes through one allocator (`src/allocation.c`). In the testing build it counts live blocks and can
make the nth allocation fail, which is how each parse, load and discovery is proven to fail cleanly at every point it
allocates and to leave no block behind. The other builds call the C library directly.

## Descriptions and layouts

A description records offsets and sizes the library's own compiler produced, so it is right for the platform it was
built on and for no other. [descriptions.md](descriptions.md) states what is checked and how a binding trusts it.
