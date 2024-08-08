#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test


class TestInfo(auracle_test.TestCase):

    def testStringFormat(self):
        r = self.Auracle(['info', '-F', '{name} {version}', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual('auracle-git r74.82e863f-1\n',
                         r.process.stdout.decode())

    def testFloatingPointFormat(self):
        r = self.Auracle(['info', '-F', '{popularity}', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual('1.02916', r.process.stdout.decode().strip())

        r = self.Auracle(['info', '-F', '{popularity:.2f}', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual('1.03', r.process.stdout.decode().strip())

    def testDateTimeFormat(self):
        r = self.Auracle(['info', '-F', '{submitted}', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual('2017-07-02T16:40:08+00:00',
                         r.process.stdout.decode().strip())

        r = self.Auracle(['info', '-F', '{submitted::%s}', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual(':1499013608', r.process.stdout.decode().strip())

    def testListFormat(self):
        r = self.Auracle(['info', '-F', '{depends}', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual('pacman  libarchive.so  libcurl.so  libsystemd.so\n',
                         r.process.stdout.decode())

        r = self.Auracle(['info', '-F', '{depends::,,}', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)
        self.assertEqual(
            'pacman:,,libarchive.so:,,libcurl.so:,,libsystemd.so\n',
            r.process.stdout.decode())

    def testInvalidFormat(self):
        r = self.Auracle(['info', '-F', '{invalid}'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertIn('invalid arg', r.process.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
