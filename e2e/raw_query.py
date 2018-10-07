#!/usr/bin/env python

import auracle_e2e_test
import json


class TestE2ERawQuery(auracle_e2e_test.TestCase):

    def testRawInfo(self):
        p = self.Auracle(['rawinfo', 'auracle-git'])
        self.assertEqual(p.returncode, 0)

        parsed = json.loads(p.stdout)
        self.assertEqual(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)


    def testRawSearch(self):
        p = self.Auracle(['rawsearch', 'aura'])
        self.assertEqual(p.returncode, 0)

        parsed = json.loads(p.stdout)
        self.assertGreater(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)


    def testRawSearchBy(self):
        p = self.Auracle(
                ['rawsearch', '--searchby=maintainer', 'falconindy'])
        self.assertEqual(p.returncode, 0)

        parsed = json.loads(p.stdout)
        self.assertGreaterEqual(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)


if __name__ == '__main__':
    auracle_e2e_test.main()
