#!/usr/bin/env python

import auracle_e2e_test


class TestE2EClone(auracle_e2e_test.TestCase):

    def testCloneSingle(self):
        p = self.Auracle(['clone', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git', git=True)


    def testCloneMultiple(self):
        p = self.Auracle(['clone', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git', git=True)
        self.assertPkgbuildExists('pkgfile-git', git=True)


    def testCloneRecursive(self):
        p = self.Auracle(['clone', '-r', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git', git=True)
        self.assertPkgbuildExists('nlohmann-json', git=True)


if __name__ == '__main__':
    auracle_e2e_test.main()
