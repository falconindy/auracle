
import auracle_test


class TestPkgbuild(auracle_test.TestCase):

    def testSinglePkgbuild(self):
        p = self.Auracle(['pkgbuild', 'auracle-git'])
        self.assertEqual(p.returncode, 0)

        pkgbuild = p.stdout.decode()

        self.assertIn('pkgname=auracle-git', pkgbuild)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git',
            '/cgit/aur.git/plain/PKGBUILD?h=auracle-git'
        ])


    def testMultiplePkgbuilds(self):
        p = self.Auracle(['pkgbuild', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(p.returncode, 0)

        pkgbuilds = p.stdout.decode()

        self.assertIn('### BEGIN auracle-git/PKGBUILD', pkgbuilds)
        self.assertIn('### BEGIN pkgfile-git/PKGBUILD', pkgbuilds)


    def testPkgbuildNotFound(self):
        p = self.Auracle(['pkgbuild', 'totesnotfoundpackage'])
        self.assertNotEqual(p.returncode, 0)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=totesnotfoundpackage',
        ])


if __name__ == '__main__':
    auracle_test.main()
