#!/usr/bin/env python

import auracle_test


class TestE2EClone(auracle_test.HermeticTestCase):

    def testCloneSingle(self):
        p = self.Auracle(['clone', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git', git=True)

        self.assertCountEqual(self.request_uris, [
            '/rpc?type=info&v=5&arg[]=auracle-git'
        ])


    def testCloneMultiple(self):
        p = self.Auracle(['clone', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git', git=True)
        self.assertPkgbuildExists('pkgfile-git', git=True)

        self.assertCountEqual(self.request_uris, [
            '/rpc?type=info&v=5&arg[]=auracle-git&arg[]=pkgfile-git'
        ])


    def testCloneRecursive(self):
        p = self.Auracle(['clone', '-r', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git', git=True)
        self.assertPkgbuildExists('nlohmann-json', git=True)

        self.assertGreater(len(self.request_uris), 1)
        self.assertIn('/rpc?type=info&v=5&arg[]=auracle-git',
                self.request_uris)


if __name__ == '__main__':
    auracle_test.main()
