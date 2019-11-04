#!/usr/bin/env python

import auracle_test


class TestDownload(auracle_test.TestCase):

    def testOutdatedFindsPackagesNeedingUpgrade(self):
        r = self.Auracle(['update', '--quiet'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('pkgfile-git')


    def testOutdatedFiltersUpdatesToArgs(self):
        r = self.Auracle(['update', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')


    def testExitsNonZeroWithoutUpgrades(self):
        r = self.Auracle(['update', 'ocaml'])
        self.assertEqual(r.process.returncode, 1)


if __name__ == '__main__':
    auracle_test.main()
