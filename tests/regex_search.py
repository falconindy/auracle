#!/usr/bin/env python

import auracle_test


class TestRegexSearch(auracle_test.TestCase):

    def testFragmentTooShort(self):
        r = self.Auracle(['search', 'f'])
        self.assertNotEqual(r.process.returncode, 0)
        self.assertIn('insufficient for searching by regular expression',
                r.process.stderr.decode())


    def testInvalidRegex(self):
        r = self.Auracle(['search', '*invalid'])
        self.assertNotEqual(r.process.returncode, 0)
        self.assertIn('invalid regex', r.process.stderr.decode())


    def testMultipleSearchesWithFiltering(self):
        r = self.Auracle([
            'search', '--quiet', '^aurac.+', '.le-git$', 'auracle'
        ])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual('auracle-git', r.process.stdout.decode().strip())

        self.assertCountEqual(r.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aurac',
            '/rpc?v=5&type=search&by=name-desc&arg=le-git',
            '/rpc?v=5&type=search&by=name-desc&arg=auracle',
        ])


if __name__ == '__main__':
    auracle_test.main()
