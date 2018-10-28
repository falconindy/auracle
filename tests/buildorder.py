#!/usr/bin/env python

import auracle_test


class TestBuildOrder(auracle_test.TestCase):

    def testSinglePackage(self):
        p = self.Auracle(['buildorder', 'ocaml-configurator'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
            'BUILD ocaml-sexplib0',
            'BUILD ocaml-base',
            'BUILD ocaml-stdio',
            'BUILD ocaml-configurator',
       ])


    def testMultiplePackage(self):
        p = self.Auracle([
            'buildorder', 'ocaml-configurator', 'ocaml-cryptokit'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
            'BUILD ocaml-sexplib0',
            'BUILD ocaml-base',
            'BUILD ocaml-stdio',
            'BUILD ocaml-configurator',
            'BUILD ocaml-zarith',
            'BUILD ocaml-cryptokit',
       ])


    def testDuplicatePackage(self):
        p = self.Auracle(['buildorder'] + 2 * ['ocaml-configurator'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
            'BUILD ocaml-sexplib0',
            'BUILD ocaml-base',
            'BUILD ocaml-stdio',
            'BUILD ocaml-configurator',
        ])


    def testOverlappingSubtrees(self):
        p = self.Auracle([
            'buildorder', 'google-drive-ocamlfuse', 'ocaml-configurator'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().splitlines(), [
            'BUILD camlidl',
            'BUILD ocamlfuse',
            'BUILD ocaml-sexplib0',
            'BUILD ocaml-base',
            'BUILD ocaml-stdio',
            'BUILD ocaml-configurator',
            'BUILD ocaml-pcre',
            'BUILD ocamlnet',
            'BUILD ocaml-curl',
            'BUILD ocaml-zarith',
            'BUILD ocaml-cryptokit',
            'BUILD ocaml-extlib',
            'BUILD ocaml-xmlm',
            'BUILD gapi-ocaml',
            'BUILD ocaml-sqlite3',
            'BUILD ocaml-ounit',
            'BUILD google-drive-ocamlfuse',
        ])


if __name__ == '__main__':
    auracle_test.main()
