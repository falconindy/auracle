#!/usr/bin/env python

import auracle_test


class TestDownload(auracle_test.TestCase):

    def testDownloadSingle(self):
        r = self.Auracle(['download', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')

        # We can assert ordering here because the RPC call must necessarily be
        # made prior to the tarball request.
        self.assertListEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git',
            '/cgit/aur.git/snapshot/auracle-git.tar.gz'
        ])


    def testDownloadNotFound(self):
        r = self.Auracle(['download', 'packagenotfound'])
        self.assertNotEqual(r.process.returncode, 0)
        self.assertIn('no results found', r.process.stderr.decode())


    def testSendsDifferentAcceptEncodingHeaders(self):
        r = self.Auracle(['download', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')

        accept_encoding = r.requests_sent[0].headers['accept-encoding']
        self.assertIn('deflate', accept_encoding)
        self.assertIn('gzip', accept_encoding)

        accept_encoding = r.requests_sent[1].headers['accept-encoding']
        self.assertEqual(accept_encoding, 'identity')


    def testDownloadMultiple(self):
        r = self.Auracle(['download', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('pkgfile-git')

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git&arg[]=pkgfile-git',
            '/cgit/aur.git/snapshot/auracle-git.tar.gz',
            '/cgit/aur.git/snapshot/pkgfile-git.tar.gz'
        ])


    def testDownloadRecursive(self):
        r = self.Auracle(['download', '-r', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('nlohmann-json')

        self.assertGreater(len(r.request_uris), 2)
        self.assertIn('/rpc?v=5&type=info&arg[]=auracle-git',
                r.request_uris)
        self.assertIn('/cgit/aur.git/snapshot/auracle-git.tar.gz',
                r.request_uris)


    def testRecurseMany(self):
        r = self.Auracle(['download', '-r', 'google-drive-ocamlfuse'])
        self.assertEqual(r.process.returncode, 0)

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
        r = self.Auracle(['download', 'yaourt'])
        self.assertNotEqual(r.process.returncode, 0)
        self.assertIn('failed to extract tarball', r.process.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
