#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test
import json


class TestSearch(auracle_test.TestCase):

    def testExitSuccessOnNoResults(self):
        r = self.Auracle(['search', 'wontfindanypackages'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual('', r.process.stdout.decode())

    def testExitFailureOnAurError(self):
        r = self.Auracle(['search', 'git'])
        self.assertNotEqual(0, r.process.returncode)

    def testSearchByDimensions(self):
        dimensions = [
            'name',
            'name-desc',
            'maintainer',
            'comaintainers',
            'depends',
            'makedepends',
            'optdepends',
            'checkdepends',
            'provides',
            'replaces',
            'conflicts',
        ]

        for dim in dimensions:
            r = self.Auracle(['search', '--searchby', dim, 'somesearchterm'])
            self.assertEqual(0, r.process.returncode)

            self.assertEqual(1, len(r.requests_sent))
            self.assertIn(f'by={dim}', r.requests_sent[0].path)

        for dim in dimensions[2:]:
            r = self.Auracle(['search', '--searchby', dim, 'libfoo.so'])
            self.assertEqual(0, r.process.returncode)

            self.assertEqual(1, len(r.requests_sent))
            self.assertIn('/libfoo.so', r.requests_sent[0].path)
            self.assertIn(f'by={dim}', r.requests_sent[0].path)

    def testSearchByInvalidDimension(self):
        r = self.Auracle(['search', '--searchby=notvalid', 'somesearchterm'])
        self.assertNotEqual(0, r.process.returncode)

    def testResultsAreUnique(self):
        # search once to get the expected count
        r1 = self.Auracle(['search', '--quiet', 'aura'])
        packagecount = len(r1.process.stdout.decode().splitlines())
        self.assertGreater(packagecount, 0)

        # search again with the term duplicated. two requests are made, but the
        # resultcount is the same as the results are deduped.
        r2 = self.Auracle(['search', '--quiet', 'aura', 'aura'])
        self.assertCountEqual([
            '/rpc/v5/search/aura?by=name-desc',
            '/rpc/v5/search/aura?by=name-desc',
        ], r2.request_uris)
        self.assertEqual(packagecount,
                         len(r2.process.stdout.decode().splitlines()))

    def testLiteralSearch(self):
        r = self.Auracle(['search', '--literal', '^aurac.+'])
        self.assertEqual(0, r.process.returncode)

        self.assertListEqual([
            '/rpc/v5/search/^aurac.+?by=name-desc',
        ], r.request_uris)

    def testLiteralSearchWithShortTerm(self):
        r = self.Auracle(['search', '--literal', 'a'])
        self.assertEqual(1, r.process.returncode)

        self.assertListEqual([
            '/rpc/v5/search/a?by=name-desc',
        ], r.request_uris)


if __name__ == '__main__':
    auracle_test.main()
