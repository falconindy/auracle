#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test
import json


class TestRawQuery(auracle_test.TestCase):

    def testRawInfo(self):
        r = self.Auracle(['rawinfo', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)

        parsed = json.loads(r.process.stdout)
        self.assertEqual(1, parsed['resultcount'])

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual(['/rpc/v5/info'], r.request_uris)

    def testRawSearch(self):
        r = self.Auracle(['rawsearch', 'aura'])
        self.assertEqual(0, r.process.returncode)

        parsed = json.loads(r.process.stdout)
        self.assertGreater(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual([
            '/rpc/v5/search/aura?by=name-desc',
        ], r.request_uris)

    def testMultipleRawSearch(self):
        r = self.Auracle(['rawsearch', 'aura', 'systemd'])
        self.assertEqual(0, r.process.returncode)

        for line in r.process.stdout.splitlines():
            parsed = json.loads(line)
            self.assertGreater(parsed['resultcount'], 1)

        self.assertCountEqual([
            '/rpc/v5/search/aura?by=name-desc',
            '/rpc/v5/search/systemd?by=name-desc',
        ], r.request_uris)

    def testRawSearchBy(self):
        r = self.Auracle(['rawsearch', '--searchby=maintainer', 'falconindy'])
        self.assertEqual(0, r.process.returncode)

        parsed = json.loads(r.process.stdout)
        self.assertGreaterEqual(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual([
            '/rpc/v5/search/falconindy?by=maintainer',
        ], r.request_uris)

    def testLiteralSearch(self):
        r = self.Auracle(['rawsearch', '--literal', '^aurac.+'])
        self.assertEqual(0, r.process.returncode)

        self.assertListEqual([
            '/rpc/v5/search/^aurac.+?by=name-desc',
        ], r.request_uris)


if __name__ == '__main__':
    auracle_test.main()
