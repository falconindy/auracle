#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test
import os


class TestClone(auracle_test.TestCase):

    def testCloneSingle(self):
        r = self.Auracle(['clone', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')

        self.assertCountEqual(r.request_uris, ['/rpc/v5/info'])

    def testCloneNotFound(self):
        r = self.Auracle(['clone', 'packagenotfound'])
        self.assertNotEqual(r.process.returncode, 0)
        self.assertIn('no results found', r.process.stderr.decode())

    def testCloneMultiple(self):
        r = self.Auracle(['clone', 'auracle-git', 'pkgfile-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('pkgfile-git')

        self.assertCountEqual(r.request_uris, ['/rpc/v5/info'])

    def testCloneRecursive(self):
        r = self.Auracle(['clone', '-r', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildExists('nlohmann-json')

        self.assertGreater(len(r.request_uris), 1)
        self.assertIn('/rpc/v5/info', r.request_uris)

    def testCloneRecursiveWithRestrictedDeps(self):
        r = self.Auracle(
            ['clone', '-r', 'auracle-git', '--resolve-deps=^makedepends'])
        self.assertEqual(r.process.returncode, 0)
        self.assertPkgbuildExists('auracle-git')
        self.assertPkgbuildNotExists('nlohmann-json')

        self.assertGreater(len(r.request_uris), 1)
        self.assertIn('/rpc/v5/info', r.request_uris)

    def testCloneUpdatesExistingCheckouts(self):
        # Package doesn't initially exist, expect a clone.
        r = self.Auracle(['clone', 'auracle-git'])
        self.assertTrue(r.process.stdout.decode().startswith('clone'))
        self.assertTrue(
            os.path.exists(os.path.join(self.tempdir, 'auracle-git', 'clone')))

        # Package now exists, expect a pull
        r = self.Auracle(['clone', 'auracle-git'])
        self.assertTrue(r.process.stdout.decode().startswith('update'))
        self.assertTrue(
            os.path.exists(os.path.join(self.tempdir, 'auracle-git', 'pull')))

    def testCloneFailureReportsError(self):
        r = self.Auracle(['clone', 'yaourt'])
        self.assertNotEqual(r.process.returncode, 0)
        self.assertEqual(
            r.process.stderr.decode().strip(),
            ('error: clone failed for yaourt: '
             'INTERNAL: git exited with unexpected exit status 1'))


if __name__ == '__main__':
    auracle_test.main()
