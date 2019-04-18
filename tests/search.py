#!/usr/bin/env python

import auracle_test
import json


class TestSearch(auracle_test.TestCase):

    def testExitSuccessOnNoResults(self):
        p = self.Auracle(['search', 'wontfindanypackages'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout, b"")


    def testExitFailureOnAurError(self):
        p = self.Auracle(['search', 'git'])
        self.assertNotEqual(p.returncode, 0)


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
            p = self.Auracle(['search', '--searchby', dim, 'somesearchterm'])
            self.assertEqual(p.returncode, 0)

            self.assertEqual(1, len(self.requests_sent))
            self.assertIn('by={}'.format(dim), self.requests_sent[0].path)


    def testSearchByInvalidDimension(self):
        p = self.Auracle(['search', '--searchby=notvalid', 'somesearchterm'])
        self.assertNotEqual(p.returncode, 0)


    def testResultsAreUnique(self):
        # search once to get the expected count
        p = self.Auracle(['search', '--quiet', 'aura'])
        packagecount = len(p.stdout.decode().splitlines())
        self.assertGreater(packagecount, 0)

        # search again with the term duplicated. two requests are made, but the
        # resultcount is the same as the results are deduped.
        p2 = self.Auracle(['search', '--quiet', 'aura', 'aura'])
        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
        ])
        self.assertEqual(packagecount, len(p2.stdout.decode().splitlines()))


    def testLiteralSearch(self):
        p = self.Auracle(['search', '--literal', '^aurac.+'])
        self.assertEqual(p.returncode, 0)

        self.assertListEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=%5Eaurac.%2B',
        ])



    def testLiteralSearchWithShortTerm(self):
        p = self.Auracle(['search', '--literal', 'a'])
        self.assertEqual(p.returncode, 1)

        self.assertListEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=a',
        ])


if __name__ == '__main__':
    auracle_test.main()
