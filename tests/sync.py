#!/usr/bin/env python

import auracle_test


class TestDownload(auracle_test.TestCase):

    def testSyncFindsPackagesNeedingUpgrade(self):
        p = self.Auracle(['sync', '--quiet'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode().strip(), 'auracle-git')

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git&arg[]=pkgfile-git'])


    def testSyncFiltersUpdatesToArgs(self):
        p = self.Auracle(['sync', 'auracle-git'])
        self.assertEqual(p.returncode, 0)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git'])


if __name__ == '__main__':
    auracle_test.main()
