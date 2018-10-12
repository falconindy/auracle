#!/usr/bin/env python

import auracle_test
import os


class TestClone(auracle_test.TestCase):

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


    def testCloneUpdatesExistingCheckouts(self):
        # Package doesn't initially exist, expect a clone.
        p = self.Auracle(['clone', 'auracle-git'])
        self.assertTrue(os.path.exists(
            os.path.join(self.tempdir, 'auracle-git', 'clone')))

        # Package now exists, expect a pull
        p = self.Auracle(['clone', 'auracle-git'])
        self.assertTrue(os.path.exists(
            os.path.join(self.tempdir, 'auracle-git', 'pull')))


    def testCloneFailureReportsError(self):
        p = self.Auracle(['clone', 'yaourt'])
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(p.stderr.decode().strip(), 'error: git clone failed')



if __name__ == '__main__':
    auracle_test.main()
