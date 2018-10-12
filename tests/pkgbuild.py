
import auracle_test


class TestPkgbuild(auracle_test.TestCase):

    def testSinglePkgbuild(self):
        p = self.Auracle(['pkgbuild', 'auracle-git'])
        self.assertEqual(p.returncode, 0)

        pkgbuild = p.stdout.decode()

        self.assertIn('pkgname=auracle-git', pkgbuild)

        self.assertCountEqual(self.request_uris, [
            '/rpc?type=info&v=5&arg[]=auracle-git',
            '/cgit/aur.git/plain/PKGBUILD?h=auracle-git'
        ])


    def testPkgbuildNotFound(self):
        p = self.Auracle(['pkgbuild', 'totesnotfoundpackage'])
        self.assertNotEqual(p.returncode, 0)

        self.assertCountEqual(self.request_uris, [
            '/rpc?type=info&v=5&arg[]=totesnotfoundpackage',
        ])


if __name__ == '__main__':
    auracle_test.main()
