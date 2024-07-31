#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test


class TestDownload(auracle_test.TestCase):

    def testOutdatedFindsPackagesNeedingUpgrade(self):
        r = self.Auracle(['update', '--quiet'])
        self.assertEqual(0, r.process.returncode)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('pkgfile-git')

    def testOutdatedFiltersUpdatesToArgs(self):
        r = self.Auracle(['update', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertPkgbuildExists('auracle-git')

    def testExitsNonZeroWithoutUpgrades(self):
        r = self.Auracle(['update', 'ocaml'])
        self.assertEqual(1, r.process.returncode)


if __name__ == '__main__':
    auracle_test.main()
