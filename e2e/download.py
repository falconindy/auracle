#!/usr/bin/env python

import auracle_e2e_test


class TestE2EDownload(auracle_e2e_test.TestCase):

    def testDownloadSingle(self):
        p = self.Auracle(['download', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git')


    def testDownloadMultiple(self):
        p = self.Auracle(['download', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('pkgfile-git')


    def testDownloadRecursive(self):
        p = self.Auracle(['download', '-r', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('nlohmann-json')


if __name__ == '__main__':
    auracle_e2e_test.main()
