#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test


class TestResolve(auracle_test.TestCase):

    def testUnversionedDependency(self):
        r = self.Auracle(['resolve', '-q', 'curl'])
        self.assertEqual(0, r.process.returncode)

        self.assertCountEqual([
            'curl-c-ares', 'curl-git', 'curl-http3-ngtcp2', 'curl-quiche-git'
        ],
                              r.process.stdout.decode().splitlines())

    def testVersionedDependencyAtLeast(self):
        r = self.Auracle(['resolve', '-q', 'curl>8'])
        self.assertEqual(0, r.process.returncode)

        # curl-c-ares provides curl, but not a versioned curl.
        self.assertCountEqual(
            ['curl-git', 'curl-http3-ngtcp2', 'curl-quiche-git'],
            r.process.stdout.decode().splitlines())

    def testVersionedDependencyEquals(self):
        r = self.Auracle(['resolve', '-q', 'curl=8.7.1.r201.gc8e0cd1de8'])
        self.assertEqual(0, r.process.returncode)

        self.assertCountEqual(['curl-git'],
                              r.process.stdout.decode().splitlines())

    def testMultipleDependencies(self):
        r = self.Auracle(['resolve', '-q', 'pacman=6.1.0', 'curl>8.7.1'])
        self.assertEqual(0, r.process.returncode)
        self.assertCountEqual(['pacman-git', 'curl-git'],
                              r.process.stdout.decode().splitlines())

    def testMultipleOverlappingDependencies(self):
        r = self.Auracle(['resolve', '-q', 'curl', 'curl>8'])
        self.assertEqual(0, r.process.returncode)
        self.assertCountEqual([
            'curl-c-ares', 'curl-git', 'curl-http3-ngtcp2', 'curl-quiche-git'
        ],
                              r.process.stdout.decode().splitlines())

    def testNoProvidersFound(self):
        r = self.Auracle(['resolve', 'curl=42'])
        self.assertEqual(0, r.process.returncode)


if __name__ == '__main__':
    auracle_test.main()
