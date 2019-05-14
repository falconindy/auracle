#!/usr/bin/env python

import auracle_test
import json


class TestRawQuery(auracle_test.TestCase):

    def testRawInfo(self):
        r = self.Auracle(['rawinfo', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)

        parsed = json.loads(r.process.stdout)
        self.assertEqual(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=info&arg[]=auracle-git',
        ])


    def testRawSearch(self):
        r = self.Auracle(['rawsearch', 'aura'])
        self.assertEqual(r.process.returncode, 0)

        parsed = json.loads(r.process.stdout)
        self.assertGreater(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
        ])


    def testMultipleRawSearch(self):
        r = self.Auracle(['rawsearch', 'aura', 'systemd'])
        self.assertEqual(r.process.returncode, 0)

        for line in r.process.stdout.splitlines():
            parsed = json.loads(line)
            self.assertGreater(parsed['resultcount'], 1)

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aura',
            '/rpc?v=5&type=search&by=name-desc&arg=systemd',
        ])


    def testRawSearchBy(self):
        r = self.Auracle(['rawsearch', '--searchby=maintainer', 'falconindy'])
        self.assertEqual(r.process.returncode, 0)

        parsed = json.loads(r.process.stdout)
        self.assertGreaterEqual(parsed['resultcount'], 1)

        names = (r['Name'] for r in parsed['results'])
        self.assertIn('auracle-git', names)

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=search&by=maintainer&arg=falconindy',
        ])


    def testLiteralSearch(self):
        r = self.Auracle(['rawsearch', '--literal', '^aurac.+'])
        self.assertEqual(r.process.returncode, 0)

        self.assertListEqual(r.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=%5Eaurac.%2B',
        ])


if __name__ == '__main__':
    auracle_test.main()
