![GitHub Actions status | falconindy/auracle](https://github.com/falconindy/auracle/workflows/arch.x86_64/badge.svg)
![GitHub Actions status | falconindy/auracle](https://github.com/falconindy/auracle/workflows/arch.x86_64.asan/badge.svg)

## What is Auracle?

Auracle is a command line tool used to interact with Arch Linux's User
Repository, commonly referred to as the [AUR](https://aur.archlinux.org).

### Features

Auracle has a number of actions it can perform:

* `search`: find packages in the AUR by regular expression.
* `info`: return detailed information about packages.
* `show`: show the contents of a source file for a package (e.g. the PKGBUILD)
* `raw{info,search}`: similar to info and search, but output raw json responses
  rather than formatting them.
* `clone`: clone the git repository for packages.
* `buildorder`: show the order and origin of packages that need to be built for
  a given set of AUR packages.
* `outdated`: attempt to find updates for installed AUR packages.
* `update`: clone out of date foreign packages

### Non-goals

Auracle does not currently, and will probably never:

* Build packages for you.
* Look at upstream VCS repos to find updates.

### Building and Testing

Building auracle requires:

* A C++17 compiler
* meson
* libsystemd
* libalpm
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
