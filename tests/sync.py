#!/usr/bin/env python

import auracle_test


class TestDownload(auracle_test.TestCase):

    def testSyncFindsPackagesNeedingUpgrade(self):
        r = self.Auracle(['sync', '--quiet'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout.decode().strip(), 'auracle-git')

        # TODO: build this dynamically from the filesystem?
        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git&arg[]=ocaml&arg[]=pkgfile-git'])


    def testSyncFiltersUpdatesToArgs(self):
        r = self.Auracle(['sync', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git'])


    def testExitsNonZeroWithoutUpgrades(self):
        r = self.Auracle(['sync', '--quiet', 'ocaml'])
        self.assertEqual(r.process.returncode, 1)


if __name__ == '__main__':
    auracle_test.main()
