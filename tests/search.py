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


if __name__ == '__main__':
    auracle_test.main()
