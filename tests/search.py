#!/usr/bin/env python

import auracle_test
import json


class TestE2ESearch(auracle_test.HermeticTestCase):

    def testExitSuccessOnNoResults(self):
        p = self.Auracle(['search', 'wontfindanypackages'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout, b"")


    def testExitFailureOnAurError(self):
        p = self.Auracle(['search', 'git'])
        self.assertNotEqual(p.returncode, 0)


if __name__ == '__main__':
    auracle_test.main()

