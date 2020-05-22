#!/usr/bin/env python

import auracle_test


class TestBuildOrder(auracle_test.TestCase):

    def testSinglePackage(self):
        r = self.Auracle(['buildorder', 'ocaml-configurator'])
        self.assertEqual(r.process.returncode, 0)
        self.assertListEqual(r.process.stdout.decode().strip().splitlines(), [
            'SATISFIEDREPOS ocaml',
            'REPOS dune',
            'AUR ocaml-sexplib0 ocaml-sexplib0',
            'AUR ocaml-base ocaml-base',
            'AUR ocaml-stdio ocaml-stdio',
            'TARGETAUR ocaml-configurator ocaml-configurator',
       ])


    def testMultiplePackage(self):
        r = self.Auracle([
            'buildorder', 'ocaml-configurator', 'ocaml-cryptokit'])
        self.assertEqual(r.process.returncode, 0)
        self.assertListEqual(r.process.stdout.decode().strip().splitlines(), [
            'SATISFIEDREPOS ocaml',
            'REPOS dune',
            'AUR ocaml-sexplib0 ocaml-sexplib0',
            'AUR ocaml-base ocaml-base',
            'AUR ocaml-stdio ocaml-stdio',
            'TARGETAUR ocaml-configurator ocaml-configurator',
            'REPOS zlib',
            'REPOS gmp',
            'REPOS ocaml-findlib',
            'AUR ocaml-zarith ocaml-zarith',
            'REPOS ocamlbuild',
            'TARGETAUR ocaml-cryptokit ocaml-cryptokit',
       ])


    def testDuplicatePackage(self):
        r = self.Auracle(['buildorder'] + 2 * ['ocaml-configurator'])
        self.assertEqual(r.process.returncode, 0)
        self.assertListEqual(r.process.stdout.decode().strip().splitlines(), [
            'SATISFIEDREPOS ocaml',
            'REPOS dune',
            'AUR ocaml-sexplib0 ocaml-sexplib0',
            'AUR ocaml-base ocaml-base',
            'AUR ocaml-stdio ocaml-stdio',
            'TARGETAUR ocaml-configurator ocaml-configurator',
        ])


    def testOverlappingSubtrees(self):
        r = self.Auracle([
            'buildorder', 'google-drive-ocamlfuse', 'ocaml-configurator'])
        self.assertEqual(r.process.returncode, 0)
        self.assertListEqual(r.process.stdout.decode().strip().splitlines(), [
            'SATISFIEDREPOS ocaml',
            'REPOS ocaml-findlib',
            'AUR camlidl camlidl',
            'REPOS fuse',
            'AUR ocamlfuse ocamlfuse',
            'REPOS ncurses',
            'REPOS gnutls',
            'REPOS krb5',
            'REPOS pcre',
            'REPOS dune',
            'AUR ocaml-sexplib0 ocaml-sexplib0',
            'AUR ocaml-base ocaml-base',
            'AUR ocaml-stdio ocaml-stdio',
            'TARGETAUR ocaml-configurator ocaml-configurator',
            'AUR ocaml-pcre ocaml-pcre',
            'AUR ocamlnet ocamlnet',
            'REPOS curl',
            'AUR ocaml-curl ocaml-curl',
            'REPOS zlib',
            'REPOS gmp',
            'AUR ocaml-zarith ocaml-zarith',
            'REPOS ocamlbuild',
            'AUR ocaml-cryptokit ocaml-cryptokit',
            'REPOS cppo',
            'AUR ocaml-extlib ocaml-extlib',
            'REPOS ocaml-yojson',
            'REPOS ocaml-topkg',
            'REPOS opam',
            'AUR ocaml-xmlm ocaml-xmlm',
            'AUR gapi-ocaml gapi-ocaml',
            'REPOS sqlite3',
            'REPOS jbuilder',
            'AUR ocaml-sqlite3 ocaml-sqlite3',
            'AUR ocaml-ounit ocaml-ounit',
            'TARGETAUR google-drive-ocamlfuse google-drive-ocamlfuse',
        ])


    def testUnsatisfiedPackage(self):
        r = self.Auracle(['buildorder', 'auracle-git'])
        self.assertEqual(r.process.returncode, 1)
        self.assertListEqual(r.process.stdout.decode().strip().splitlines(), [
            'UNKNOWN pacman auracle-git',
            'UNKNOWN libarchive.so auracle-git',
            'REPOS libcurl.so',
            'UNKNOWN libsystemd.so auracle-git',
            'UNKNOWN meson auracle-git',
            'UNKNOWN git auracle-git',
            'UNKNOWN cmake nlohmann-json auracle-git',
            'AUR nlohmann-json nlohmann-json',
            'UNKNOWN gtest auracle-git',
            'UNKNOWN gmock auracle-git',
            'TARGETAUR auracle-git auracle-git'
        ])


    def testDependencyCycle(self):
        r = self.Auracle(['buildorder', 'python-fontpens'])
        self.assertEqual(r.process.returncode, 0)
        self.assertIn(
                'warning: found dependency cycle: [ python-fontpens -> python-fontparts -> python-fontpens ]',
                r.process.stderr.decode().strip().splitlines())


if __name__ == '__main__':
    auracle_test.main()
