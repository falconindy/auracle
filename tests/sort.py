#!/usr/bin/env python

import auracle_test

class SortTest(auracle_test.TestCase):

    def testSortInfoByPopularity(self):
        p = self.Auracle(['--sort', 'popularity', 'info', 'auracle-git', 'pkgfile-git', 'nlohmann-json'])
        self.assertEqual(p.returncode, 0)

        v = []
        for line in p.stdout.decode().splitlines():
            if line.startswith('Popularity'):
                v.append(float(line.rsplit(':')[-1].strip()))

        self.assertTrue(all(v[i] <= v[i+1] for i in range(len(v) -1)))


    def testRSortInfoByPopularity(self):
        p = self.Auracle(['--rsort', 'popularity', 'search', 'auracle-git', 'pkgfile-git', 'nlohmann-json'])
        self.assertEqual(p.returncode, 0)

        v = []
        for line in p.stdout.decode().splitlines():
            if line.startswith('Popularity'):
                v.append(float(line.rsplit(':')[-1].strip()))

        self.assertTrue(all(v[i] >= v[i+1] for i in range(len(v) -1)))


    def testSortSearchByVotes(self):
        p = self.Auracle(['--sort', 'votes', 'search', '--searchby=maintainer', 'falconindy'])
        self.assertEqual(p.returncode, 0)

        v = []
        for line in p.stdout.decode().splitlines():
            if line.startswith('aur/'):
                v.append(int(line.split()[-2][1:-1]))

        self.assertTrue(all(v[i] <= v[i+1] for i in range(len(v) -1)))


    def testRSortSearchByVotes(self):
        p = self.Auracle(['--rsort', 'votes', 'search', '--searchby=maintainer', 'falconindy'])
        self.assertEqual(p.returncode, 0)

        v = []
        for line in p.stdout.decode().splitlines():
            if line.startswith('aur/'):
                v.append(int(line.split()[-2][1:-1]))

        self.assertTrue(all(v[i] >= v[i+1] for i in range(len(v) -1)))


if __name__ == '__main__':
    auracle_test.main()
