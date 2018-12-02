#!/usr/bin/env python

import auracle_test


class TestInfo(auracle_test.TestCase):

    def testStringFormat(self):
        p = self.Auracle(['info', '-F', '{name} {version}', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode(), 'auracle-git r74.82e863f-1\n')


    def testFloatingPointFormat(self):
        p = self.Auracle(['info', '-F', '{popularity}', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode().strip(), '1.02916')

        p = self.Auracle(['info', '-F', '{popularity:.2f}', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode().strip(), '1.03')


    def testDateTimeFormat(self):
        p = self.Auracle(['info', '-F', '{submitted}', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode().strip(), 'Sun Jul  2 16:40:08 2017')

        p = self.Auracle(['info', '-F', '{submitted::%s}', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode().strip(), ':1499013608')


    def testListFormat(self):
        p = self.Auracle(['info', '-F', '{depends}', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertListEqual(p.stdout.decode().strip().split(), [
            'pacman', 'libarchive.so', 'libcurl.so', 'libsystemd.so',
        ])

        p = self.Auracle(['info', '-F', '{depends::,,}', 'auracle-git'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout.decode().strip(),
            'pacman:,,libarchive.so:,,libcurl.so:,,libsystemd.so')


    def testInvalidFormat(self):
        p = self.Auracle(['info', '-F', '{invalid}'])
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('invalid arg', p.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
