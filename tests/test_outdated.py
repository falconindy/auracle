#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test


class TestDownload(auracle_test.TestCase):
    def testOutdatedFindsPackagesNeedingUpgrade(self):
        r = self.Auracle(['outdated', '--quiet'])
        self.assertEqual(0, r.process.returncode)
        self.assertListEqual(r.process.stdout.decode().strip().splitlines(),
                             ['auracle-git', 'pkgfile-git'])

        # TODO: build this dynamically from the filesystem?
        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git&arg[]=ocaml&arg[]=pkgfile-git'
        ])

    def testOutdatedFiltersUpdatesToArgs(self):
        r = self.Auracle(['outdated', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)

        self.assertCountEqual(['/rpc?v=5&type=info&arg[]=auracle-git'],
                              r.request_uris)

    def testExitsNonZeroWithoutUpgrades(self):
        r = self.Auracle(['outdated', '--quiet', 'ocaml'])
        self.assertEqual(1, r.process.returncode)


if __name__ == '__main__':
    auracle_test.main()
