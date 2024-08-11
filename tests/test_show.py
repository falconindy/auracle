import auracle_test
# SPDX-License-Identifier: MIT


class TestPkgbuild(auracle_test.TestCase):

    def testSinglePkgbuild(self):
        r = self.Auracle(['show', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)

        pkgbuild = r.process.stdout.decode()

        self.assertIn('pkgname=auracle-git', pkgbuild)

        self.assertCountEqual(
            ['/rpc/v5/info', '/cgit/aur.git/plain/PKGBUILD?h=auracle-git'],
            r.request_uris)

    def testMultiplePkgbuilds(self):
        r = self.Auracle(['show', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(0, r.process.returncode)

        pkgbuilds = r.process.stdout.decode()

        self.assertIn('### BEGIN auracle-git/PKGBUILD', pkgbuilds)
        self.assertIn('### BEGIN pkgfile-git/PKGBUILD', pkgbuilds)

    def testPkgbuildNotFound(self):
        r = self.Auracle(['show', 'totesnotfoundpackage'])
        self.assertNotEqual(0, r.process.returncode)

        self.assertCountEqual([
            '/rpc/v5/info',
        ], r.request_uris)

    def testFileNotFound(self):
        r = self.Auracle(['show', '--show-file=NOTAPKGBUILD', 'auracle-git'])
        self.assertNotEqual(0, r.process.returncode)

        self.assertIn('not found for package', r.process.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
