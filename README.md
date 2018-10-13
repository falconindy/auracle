## What is Auracle?

This is mainly a result of frustrations over the years with how cower has
developed, and limitations I've run into.

### Background

At the core of any AUR client is a series of HTTP requests to the AUR. cower
currently implements this using the venerable libcurl. Libcurl, generally
speaking, offers two APIs: "easy" and "multi". The easy API uses a CURL handle
to represent a session. Each handle manages approximately 1 active request at
a given time. The multi API aggregates multiple easy handles, allowing for
concurrent sessions in a non-blocking (event-like) manner.

It's also possible to achieve session concurrency through anothers means, which
is to use threads with multiple CURL handles, but solely through the easy API.
In this mode, one must be careful never to access the same CURL handle
concurrently, as the objects are not thread-safe.

### Problem

History has taught me that the handle-per-thread approach was a poor choice.
cower (loosely) implements a thread pool, and multiplexes requests across some
number of threads. Information is generally shared through light use of mutexes
around global objects (mainly, a work queue), and results from threads are
aggregated back in the main thread.

Due to my decision to write cower in C and ensuing laziness, there's other
warts.  Data structures are ill-chosen, typically using the doubly-linked list
from alpm. Encapsulation is poor, and there's numerous objects which have some
vague amount of overlap, or end up being global for the sake of sharing.

Lack of proper communication channels (or any real synchronization) between
threads has lead to relatively
[unfixable](https://github.com/falconindy/cower/issues/90) bugs. And, the
current architecture makes it difficult to implement [obvious
improvements](https://github.com/falconindy/cower/issues/90).

The cower codebase has simply become frustrating to work with.

### Solution

I've attempted to address some of these faults through refactoring. There's
some obvious (to me) abstractions to be created, but the added difficulty
presented by the current threading architecture and yearn for actual data
structures (without having to write/find them) gives me pause.

Therefore, the ideal state seems to be to move away from threads and rewrite in
a higher level language. auracle attempts to address these problems and more
through it's C++ implementation and adoption of Curl's multi API.

This code is all subject to change until a tag is pushed. If you have opinions,
feature requests, or bug reports, please file issues.

### Building and Testing

Building auracle requires:

* A C++17 capable compiler
* meson
* nlohmann-json
* libsystemd
* libalpm
* libarchive
* libcurl

Testing additionally depends on:

* gmock
* gtest
* python3

You're probably building this from the AUR, though, so just go use the
[PKGBUILD](https://aur.archlinux.org/packages/auracle-git).

If you're hacking on auracle, you can do this manually:

```sh
$ meson build
$ ninja -C build
```

And running the tests is simply a matter of:

```sh
$ meson test -C build
```
