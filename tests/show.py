
import auracle_test


class TestPkgbuild(auracle_test.TestCase):

    def testSinglePkgbuild(self):
        r = self.Auracle(['show', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)

        pkgbuild = r.process.stdout.decode()

        self.assertIn('pkgname=auracle-git', pkgbuild)

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git',
            '/cgit/aur.git/plain/PKGBUILD?h=auracle-git'
        ])


    def testMultiplePkgbuilds(self):
        r = self.Auracle(['show', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(r.process.returncode, 0)

        pkgbuilds = r.process.stdout.decode()

        self.assertIn('### BEGIN auracle-git/PKGBUILD', pkgbuilds)
        self.assertIn('### BEGIN pkgfile-git/PKGBUILD', pkgbuilds)


    def testPkgbuildNotFound(self):
        r = self.Auracle(['show', 'totesnotfoundpackage'])
        self.assertNotEqual(r.process.returncode, 0)

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=totesnotfoundpackage',
        ])


    def testFileNotFound(self):
        r = self.Auracle(['show', '--show-file=NOTAPKGBUILD', 'auracle-git'])
        self.assertNotEqual(r.process.returncode, 0)

        self.assertIn('not found for package', r.process.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
