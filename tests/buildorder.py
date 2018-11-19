#!/usr/bin/env python

import auracle_test


class TestBuildOrder(auracle_test.TestCase):

    def testSinglePackage(self):
        p = self.Auracle(['buildorder', 'ocaml-configurator'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
            'SATISFIEDREPOS ocaml',
            'REPOS dune',
            'AUR ocaml-sexplib0 ocaml-sexplib0',
            'AUR ocaml-base ocaml-base',
            'AUR ocaml-stdio ocaml-stdio',
            'AUR ocaml-configurator ocaml-configurator',
       ])


    def testMultiplePackage(self):
        p = self.Auracle([
            'buildorder', 'ocaml-configurator', 'ocaml-cryptokit'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
            'SATISFIEDREPOS ocaml',
            'REPOS dune',
            'AUR ocaml-sexplib0 ocaml-sexplib0',
            'AUR ocaml-base ocaml-base',
            'AUR ocaml-stdio ocaml-stdio',
            'AUR ocaml-configurator ocaml-configurator',
            'REPOS zlib',
            'REPOS gmp',
            'REPOS ocaml-findlib',
            'AUR ocaml-zarith ocaml-zarith',
            'REPOS ocamlbuild',
            'AUR ocaml-cryptokit ocaml-cryptokit',
       ])


    def testDuplicatePackage(self):
        p = self.Auracle(['buildorder'] + 2 * ['ocaml-configurator'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
            'SATISFIEDREPOS ocaml',
            'REPOS dune',
            'AUR ocaml-sexplib0 ocaml-sexplib0',
            'AUR ocaml-base ocaml-base',
            'AUR ocaml-stdio ocaml-stdio',
            'AUR ocaml-configurator ocaml-configurator',
        ])


    def testOverlappingSubtrees(self):
        p = self.Auracle([
            'buildorder', 'google-drive-ocamlfuse', 'ocaml-configurator'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
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
            'AUR ocaml-configurator ocaml-configurator',
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
            'AUR google-drive-ocamlfuse google-drive-ocamlfuse',
        ])


if __name__ == '__main__':
    auracle_test.main()
