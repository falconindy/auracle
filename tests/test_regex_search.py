#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test


class TestRegexSearch(auracle_test.TestCase):

    def testFragmentTooShort(self):
        r = self.Auracle(['search', 'f'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertIn('insufficient for searching by regular expression',
                      r.process.stderr.decode())

    def testInvalidRegex(self):
        r = self.Auracle(['search', '*invalid'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertIn('invalid regex', r.process.stderr.decode())

    def testMultipleSearchesWithFiltering(self):
        r = self.Auracle(
            ['search', '--quiet', '^aurac.+', '.le-git$', 'auracle'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual('auracle-git', r.process.stdout.decode().strip())

        self.assertCountEqual([
            '/rpc/v5/search/aurac?by=name-desc',
            '/rpc/v5/search/le-git?by=name-desc',
            '/rpc/v5/search/auracle?by=name-desc',
        ], r.request_uris)


if __name__ == '__main__':
    auracle_test.main()
