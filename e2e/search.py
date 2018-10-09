#!/usr/bin/env python

import auracle_e2e_test
import json


class TestE2ESearch(auracle_e2e_test.TestCase):

    def testExitSuccessOnNoResults(self):
        p = self.Auracle(['search', 'wontfindanypackages'])
        self.assertEqual(p.returncode, 0)
        self.assertEqual(p.stdout, b"")


    def testExitFailureOnAurError(self):
        p = self.Auracle(['search', 'git'])
        self.assertNotEqual(p.returncode, 0)


if __name__ == '__main__':
    auracle_e2e_test.main()

