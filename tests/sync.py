#!/usr/bin/env python

import auracle_test


class TestDownload(auracle_test.TestCase):

    def testSyncFindsPackagesNeedingUpgrade(self):
        p = self.Auracle(['sync', '--quiet'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode().strip(), 'auracle-git')

        # TODO: build this dynamically from the filesystem?
        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git&arg[]=ocaml&arg[]=pkgfile-git'])


    def testSyncFiltersUpdatesToArgs(self):
        p = self.Auracle(['sync', 'auracle-git'])
        self.assertEqual(p.returncode, 0)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git'])


    def testExitsNonZeroWithoutUpgrades(self):
        p = self.Auracle(['sync', '--quiet', 'ocaml'])
        self.assertEqual(p.returncode, 1)


if __name__ == '__main__':
    auracle_test.main()
