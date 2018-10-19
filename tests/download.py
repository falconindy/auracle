#!/usr/bin/env python

import auracle_test


class TestDownload(auracle_test.TestCase):

    def testDownloadSingle(self):
        p = self.Auracle(['download', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git')

        # We can assert ordering here because the RPC call must necessarily be
        # made prior to the tarball request.
        self.assertListEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git',
            '/cgit/aur.git/snapshot/auracle-git.tar.gz'
        ])


    def testSendsDifferentAcceptEncodingHeaders(self):
        p = self.Auracle(['download', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git')

        accept_encoding = self.requests_sent[0].headers['accept-encoding']
        self.assertIn('deflate', accept_encoding)
        self.assertIn('gzip', accept_encoding)

        accept_encoding = self.requests_sent[1].headers['accept-encoding']
        self.assertEqual(accept_encoding, 'identity')


    def testDownloadMultiple(self):
        p = self.Auracle(['download', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('pkgfile-git')

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git&arg[]=pkgfile-git',
            '/cgit/aur.git/snapshot/auracle-git.tar.gz',
            '/cgit/aur.git/snapshot/pkgfile-git.tar.gz'
        ])


    def testDownloadRecursive(self):
        p = self.Auracle(['download', '-r', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('nlohmann-json')

        self.assertGreater(len(self.request_uris), 2)
        self.assertIn('/rpc?v=5&type=info&arg[]=auracle-git',
                self.request_uris)
        self.assertIn('/cgit/aur.git/snapshot/auracle-git.tar.gz',
                self.request_uris)


    def testRecurseMany(self):
        p = self.Auracle(['download', '-r', 'google-drive-ocamlfuse'])
        self.assertEqual(p.returncode, 0)

        expected_packages = [
            'camlidl',
            'gapi-ocaml',
            'google-drive-ocamlfuse',
            'ocaml-base',
            'ocaml-configurator',
            'ocaml-cryptokit',
            'ocaml-curl',
            'ocaml-extlib',
            'ocaml-ounit',
            'ocaml-pcre',
            'ocaml-sexplib0',
            'ocaml-sqlite3',
            'ocaml-stdio',
            'ocaml-xmlm',
            'ocaml-zarith',
            'ocamlfuse',
            'ocamlnet'
        ]

        for pkg in expected_packages:
            self.assertPkgbuildExists(pkg)


    def testBadTarball(self):
        p = self.Auracle(['download', 'yaourt'])
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('failed to extract tarball', p.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
