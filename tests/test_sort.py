#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test


class SortTest(auracle_test.TestCase):

    def testSortInfoByPopularity(self):
        r = self.Auracle([
            '--sort', 'popularity', 'info', 'auracle-git', 'pkgfile-git',
            'nlohmann-json'
        ])
        self.assertEqual(0, r.process.returncode)

        v = []
        for line in r.process.stdout.decode().splitlines():
            if line.startswith('Popularity'):
                v.append(float(line.rsplit(':')[-1].strip()))

        self.assertTrue(all(v[i] <= v[i + 1] for i in range(len(v) - 1)))

    def testRSortInfoByPopularity(self):
        r = self.Auracle([
            '--rsort', 'popularity', 'search', 'auracle-git', 'pkgfile-git',
            'nlohmann-json'
        ])
        self.assertEqual(0, r.process.returncode)

        v = []
        for line in r.process.stdout.decode().splitlines():
            if line.startswith('Popularity'):
                v.append(float(line.rsplit(':')[-3].strip()))

        self.assertTrue(all(v[i] >= v[i + 1] for i in range(len(v) - 1)))

    def testSortSearchByVotes(self):
        r = self.Auracle([
            '--sort', 'votes', 'search', '--searchby=maintainer', 'falconindy'
        ])
        self.assertEqual(0, r.process.returncode)

        v = []
        for line in r.process.stdout.decode().splitlines():
            if line.startswith('aur/'):
                v.append(int(line.split()[2][1:-1]))

        self.assertTrue(all(v[i] <= v[i + 1] for i in range(len(v) - 1)))

    def testRSortSearchByVotes(self):
        r = self.Auracle([
            '--rsort', 'votes', 'search', '--searchby=maintainer', 'falconindy'
        ])
        self.assertEqual(0, r.process.returncode)

        v = []
        for line in r.process.stdout.decode().splitlines():
            if line.startswith('aur/'):
                v.append(int(line.split()[2][1:-1]))

        self.assertTrue(all(v[i] >= v[i + 1] for i in range(len(v) - 1)))

    def testSortByInvalidKey(self):
        r = self.Auracle(['--sort', 'nonsense', 'search', 'aura'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertCountEqual([], r.requests_sent)

    def testRSortByInvalidKey(self):
        r = self.Auracle(['--rsort', 'nonsense', 'search', 'aura'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertCountEqual([], r.requests_sent)


if __name__ == '__main__':
    auracle_test.main()
