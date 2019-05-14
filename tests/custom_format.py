#!/usr/bin/env python

import auracle_test


class TestInfo(auracle_test.TestCase):

    def testStringFormat(self):
        r = self.Auracle(['info', '-F', '{name} {version}', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout.decode(), 'auracle-git r74.82e863f-1\n')


    def testFloatingPointFormat(self):
        r = self.Auracle(['info', '-F', '{popularity}', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout.decode().strip(), '1.02916')

        r = self.Auracle(['info', '-F', '{popularity:.2f}', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout.decode().strip(), '1.03')


    def testDateTimeFormat(self):
        r = self.Auracle(['info', '-F', '{submitted}', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout.decode().strip(), 'Sun Jul  2 16:40:08 2017')

        r = self.Auracle(['info', '-F', '{submitted::%s}', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout.decode().strip(), ':1499013608')


    def testListFormat(self):
        r = self.Auracle(['info', '-F', '{depends}', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertListEqual(r.process.stdout.decode().strip().split(), [
            'pacman', 'libarchive.so', 'libcurl.so', 'libsystemd.so',
        ])

        r = self.Auracle(['info', '-F', '{depends::,,}', 'auracle-git'])
        self.assertEqual(r.process.returncode, 0)
        self.assertEqual(r.process.stdout.decode().strip(),
            'pacman:,,libarchive.so:,,libcurl.so:,,libsystemd.so')


    def testInvalidFormat(self):
        r = self.Auracle(['info', '-F', '{invalid}'])
        self.assertNotEqual(r.process.returncode, 0)
        self.assertIn('invalid arg', r.process.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
