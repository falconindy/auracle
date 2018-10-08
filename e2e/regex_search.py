#!/usr/bin/env python

import auracle_e2e_test


class TestE2ERegexSearch(auracle_e2e_test.TestCase):

    def testFragmentTooShort(self):
        p = self.Auracle(
                ['search', 'f'])
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('insufficient for searching by regular expression',
                p.stderr.decode())


    def testMultipleSearchesWithFiltering(self):
        p = self.Auracle(
                ['search', '--quiet', '^aurac.+', '.le-git$', 'auracle'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual('auracle-git', p.stdout.decode().strip())

        self.assertCountEqual(self.request_uris, [
            '/rpc?by=name-desc&type=search&v=5&arg=aurac',
            '/rpc?by=name-desc&type=search&v=5&arg=le-git',
            '/rpc?by=name-desc&type=search&v=5&arg=auracle',
        ])


if __name__ == '__main__':
    auracle_e2e_test.main()
