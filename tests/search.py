#!/usr/bin/env python

import auracle_test
import json


class TestSearch(auracle_test.TestCase):

    def testExitSuccessOnNoResults(self):
        r = self.Auracle(['search', 'wontfindanypackages'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout, b"")


    def testExitFailureOnAurError(self):
        r = self.Auracle(['search', 'git'])
        self.assertNotEqual(r.process.returncode, 0)


    def testSearchByDimensions(self):
        dimensions = [
          'name',
          'name-desc',
          'maintainer',
          'depends',
          'makedepends',
          'optdepends',
          'checkdepends',
        ]

        for dim in dimensions:
            r = self.Auracle(['search', '--searchby', dim, 'somesearchterm'])
            self.assertEqual(r.process.returncode, 0)

            self.assertEqual(1, len(r.requests_sent))
            self.assertIn('by={}'.format(dim), r.requests_sent[0].path)


    def testSearchByInvalidDimension(self):
        r = self.Auracle(['search', '--searchby=notvalid', 'somesearchterm'])
        self.assertNotEqual(r.process.returncode, 0)


    def testResultsAreUnique(self):
        # search once to get the expected count
        r1 = self.Auracle(['search', '--quiet', 'aura'])
        packagecount = len(r1.process.stdout.decode().splitlines())
        self.assertGreater(packagecount, 0)

        # search again with the term duplicated. two requests are made, but the
        # resultcount is the same as the results are deduped.
        r2 = self.Auracle(['search', '--quiet', 'aura', 'aura'])
        self.assertCountEqual(r2.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
        ])
        self.assertEqual(packagecount,
                len(r2.process.stdout.decode().splitlines()))


    def testLiteralSearch(self):
        r = self.Auracle(['search', '--literal', '^aurac.+'])
        self.assertEqual(r.process.returncode, 0)

        self.assertListEqual(r.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=%5Eaurac.%2B',
        ])



    def testLiteralSearchWithShortTerm(self):
        r = self.Auracle(['search', '--literal', 'a'])
        self.assertEqual(r.process.returncode, 1)

        self.assertListEqual(r.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=a',
        ])


if __name__ == '__main__':
    auracle_test.main()
