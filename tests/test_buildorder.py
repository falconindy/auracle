#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test

import textwrap


class TestBuildOrder(auracle_test.TestCase):
    def testSinglePackage(self):
        r = self.Auracle(['buildorder', 'ocaml-configurator'])
        self.assertEqual(0, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                SATISFIEDREPOS ocaml
                REPOS dune
                AUR ocaml-sexplib0 ocaml-sexplib0
                AUR ocaml-base ocaml-base
                AUR ocaml-stdio ocaml-stdio
                TARGETAUR ocaml-configurator ocaml-configurator
            """),
            r.process.stdout.decode(),
        )

    def testMultiplePackage(self):
        r = self.Auracle(['buildorder', 'ocaml-configurator', 'ocaml-cryptokit'])
        self.assertEqual(0, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                SATISFIEDREPOS ocaml
                REPOS dune
                AUR ocaml-sexplib0 ocaml-sexplib0
                AUR ocaml-base ocaml-base
                AUR ocaml-stdio ocaml-stdio
                TARGETAUR ocaml-configurator ocaml-configurator
                REPOS zlib
                REPOS gmp
                REPOS ocaml-findlib
                AUR ocaml-zarith ocaml-zarith
                REPOS ocamlbuild
                TARGETAUR ocaml-cryptokit ocaml-cryptokit
            """),
            r.process.stdout.decode(),
        )

    def testDuplicatePackage(self):
        r = self.Auracle(['buildorder'] + 2 * ['ocaml-configurator'])
        self.assertEqual(0, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                SATISFIEDREPOS ocaml
                REPOS dune
                AUR ocaml-sexplib0 ocaml-sexplib0
                AUR ocaml-base ocaml-base
                AUR ocaml-stdio ocaml-stdio
                TARGETAUR ocaml-configurator ocaml-configurator
            """),
            r.process.stdout.decode(),
        )

    def testOverlappingSubtrees(self):
        r = self.Auracle(['buildorder', 'google-drive-ocamlfuse', 'ocaml-configurator'])
        self.assertEqual(0, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                SATISFIEDREPOS ocaml
                REPOS ocaml-findlib
                AUR camlidl camlidl
                REPOS fuse
                AUR ocamlfuse ocamlfuse
                REPOS ncurses
                REPOS gnutls
                REPOS krb5
                REPOS pcre
                REPOS dune
                AUR ocaml-sexplib0 ocaml-sexplib0
                AUR ocaml-base ocaml-base
                AUR ocaml-stdio ocaml-stdio
                TARGETAUR ocaml-configurator ocaml-configurator
                AUR ocaml-pcre ocaml-pcre
                AUR ocamlnet ocamlnet
                REPOS curl
                AUR ocaml-curl ocaml-curl
                REPOS zlib
                REPOS gmp
                AUR ocaml-zarith ocaml-zarith
                REPOS ocamlbuild
                AUR ocaml-cryptokit ocaml-cryptokit
                REPOS cppo
                AUR ocaml-extlib ocaml-extlib
                REPOS ocaml-yojson
                REPOS ocaml-topkg
                REPOS opam
                AUR ocaml-xmlm ocaml-xmlm
                AUR gapi-ocaml gapi-ocaml
                REPOS sqlite3
                REPOS jbuilder
                AUR ocaml-sqlite3 ocaml-sqlite3
                AUR ocaml-ounit ocaml-ounit
                TARGETAUR google-drive-ocamlfuse google-drive-ocamlfuse
            """),
            r.process.stdout.decode(),
        )

    def testUnsatisfiedPackage(self):
        r = self.Auracle(['buildorder', 'auracle-git'])
        self.assertEqual(1, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                UNRESOLVED pacman pacman-fancy-progress-git pacman-git pacman-pb
                UNKNOWN libarchive.so auracle-git
                REPOS libcurl.so
                UNKNOWN libsystemd.so auracle-git
                UNKNOWN meson auracle-git
                UNKNOWN git auracle-git
                UNKNOWN cmake nlohmann-json auracle-git
                AUR nlohmann-json nlohmann-json
                UNKNOWN gtest auracle-git
                UNKNOWN gmock auracle-git
                TARGETAUR auracle-git auracle-git
            """),
            r.process.stdout.decode(),
        )

    def testDependencyCycle(self):
        r = self.Auracle(['buildorder', 'python-fontpens'])
        self.assertEqual(0, r.process.returncode)
        self.assertIn(
            'warning: found dependency cycle: [ python-fontpens -> python-fontparts -> python-fontpens ]',
            r.process.stderr.decode().strip().splitlines(),
        )

    def testNoDependencies(self):
        r = self.Auracle(['buildorder', 'mingw-w64-environment'])
        self.assertEqual(0, r.process.returncode)
        self.assertIn(
            'TARGETAUR mingw-w64-environment mingw-w64-environment',
            r.process.stdout.decode().strip().splitlines(),
        )

    def testVersionedDependencyResolvedByNameMismatchedProvider(self):
        # libfoo>=2.0 isn't a real package name in the AUR; it's satisfied
        # only by libfoo-unstable, which provides libfoo=2.0.
        r = self.Auracle(['buildorder', 'needs-versioned-provider'])
        self.assertEqual(0, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                AUR libfoo libfoo-unstable
                TARGETAUR needs-versioned-provider needs-versioned-provider
            """),
            r.process.stdout.decode(),
        )

    def testAmbiguousProviderIsUnresolved(self):
        # libbar is provided by two unrelated AUR packages; auracle must not
        # guess which one to use.
        r = self.Auracle(['buildorder', 'needs-ambiguous-provider'])
        self.assertEqual(1, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                UNRESOLVED libbar libbar-a libbar-b
                TARGETAUR needs-ambiguous-provider needs-ambiguous-provider
            """),
            r.process.stdout.decode(),
        )

    def testOutdatedLocalInstallIsNotSatisfied(self):
        # verpkg>=2.0 is available in the repos (verpkg 2.0-1), but what's
        # actually installed locally is verpkg 1.0-1 -- that shouldn't be
        # reported as satisfied.
        r = self.Auracle(['buildorder', 'needs-outdated-local'])
        self.assertEqual(0, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                REPOS verpkg
                TARGETAUR needs-outdated-local needs-outdated-local
            """),
            r.process.stdout.decode(),
        )

    def testDependencyListedUnderMultipleKindsIsNotDoubleResolved(self):
        # needs-duplicate-kind-dep lists 'shared-dep' under both Depends and
        # MakeDepends -- a real pattern (e.g. ros-noetic-tf2-geometry-msgs
        # depends on orocos-kdl-python this way on the live AUR). shared-dep
        # has no exact-name AUR package and exactly one real provider
        # (dep-provider). A duplicate depstring reaching the provider
        # resolution fallback used to cause a duplicated search-by-provides
        # request, which (unlike this test's fakeserver) the real AUR RPC
        # doesn't dedupe by arg[], turning a single real provider into a
        # false-positive "multiple providers" ambiguity.
        r = self.Auracle(['buildorder', 'needs-duplicate-kind-dep'])
        self.assertEqual(0, r.process.returncode)
        self.assertMultiLineEqual(
            textwrap.dedent("""\
                AUR shared-dep dep-provider
                TARGETAUR needs-duplicate-kind-dep needs-duplicate-kind-dep
            """),
            r.process.stdout.decode(),
        )
        self.assertEqual('', r.process.stderr.decode())
        self.assertEqual(
            1,
            r.request_uris.count('/rpc/v5/search/shared-dep?by=provides'),
        )


if __name__ == '__main__':
    auracle_test.main()
