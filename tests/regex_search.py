#!/usr/bin/env python

import auracle_test


class TestRegexSearch(auracle_test.TestCase):

    def testFragmentTooShort(self):
        p = self.Auracle(['search', 'f'])
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('insufficient for searching by regular expression',
                p.stderr.decode())


    def testInvalidRegex(self):
        p = self.Auracle(['search', '*invalid'])
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('invalid regex', p.stderr.decode())


    def testMultipleSearchesWithFiltering(self):
        p = self.Auracle([
            'search', '--quiet', '^aurac.+', '.le-git$', 'auracle'
        ])
        self.assertEqual(p.returncode, 0)
        self.assertEqual('auracle-git', p.stdout.decode().strip())

        self.assertCountEqual(self.request_uris, [
            '/rpc?v=5&type=search&by=name-desc&arg=aurac',
            '/rpc?v=5&type=search&by=name-desc&arg=le-git',
            '/rpc?v=5&type=search&by=name-desc&arg=auracle',
        ])


if __name__ == '__main__':
    auracle_test.main()
