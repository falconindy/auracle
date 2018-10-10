#!/usr/bin/env python

import auracle_test
import json


class TestE2EInfo(auracle_test.TestCase):

    def testSingleInfoQuery(self):
        p = self.Auracle(['info', 'auracle-git'])
        self.assertEqual(p.returncode, 0)


    def testExitSuccessIfAtLeastOneFound(self):
        p = self.Auracle(['info', 'auracle-git', 'packagenotfoundbro'])
        self.assertEqual(p.returncode, 0)


    def testExitFailureOnNotFound(self):
        p = self.Auracle(['info', 'packagenotfoundbro'])
        self.assertNotEqual(p.returncode, 0)


if __name__ == '__main__':
    auracle_test.main()
