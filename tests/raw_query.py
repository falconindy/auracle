#!/usr/bin/env python

import auracle_test
import json


class TestRawQuery(auracle_test.TestCase):

    def testRawInfo(self):
        p = self.Auracle(['rawinfo', 'auracle-git'])
        self.assertEqual(p.returncode, 0)

        parsed = json.loads(p.stdout)
        self.assertEqual(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git',
        ])


    def testRawSearch(self):
        p = self.Auracle(['rawsearch', 'aura'])
        self.assertEqual(p.returncode, 0)

        parsed = json.loads(p.stdout)
        self.assertGreater(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
        ])


    def testMultipleRawSearch(self):
        p = self.Auracle(['rawsearch', 'aura', 'systemd'])
        self.assertEqual(p.returncode, 0)

        for line in p.stdout.splitlines():
            parsed = json.loads(line)
            self.assertGreater(parsed['resultcount'], 1)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
            '/rpc?v=5&type=search&by=name-desc&arg=systemd',
        ])


    def testRawSearchBy(self):
        p = self.Auracle(['rawsearch', '--searchby=maintainer', 'falconindy'])
        self.assertEqual(p.returncode, 0)

        parsed = json.loads(p.stdout)
        self.assertGreaterEqual(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=maintainer&arg=falconindy',
        ])


    def testLiteralSearch(self):
        p = self.Auracle(['rawsearch', '--literal', '^aurac.+'])
        self.assertEqual(p.returncode, 0)

        self.assertListEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=%5Eaurac.%2B',
        ])


if __name__ == '__main__':
    auracle_test.main()
