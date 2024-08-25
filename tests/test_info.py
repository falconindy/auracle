#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import auracle_test
import json


class TestInfo(auracle_test.TestCase):

    def testSingleInfoQuery(self):
        r = self.Auracle(['info', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)

    def testExitSuccessIfAtLeastOneFound(self):
        r = self.Auracle(['info', 'auracle-git', 'packagenotfoundbro'])
        self.assertEqual(0, r.process.returncode)

    def testExitFailureOnNotFound(self):
        r = self.Auracle(['info', 'packagenotfoundbro'])
        self.assertNotEqual(0, r.process.returncode)

    def testBadResponsesFromAur(self):
        r = self.Auracle(['info', '503'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertEqual(r.process.stderr.decode(),
                         'error: INTERNAL: HTTP 503\n')

        r = self.Auracle(['info', '429'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertEqual(('error: RESOURCE_EXHAUSTED: Too many requests: '
                          'the AUR has throttled your IP.\n'),
                         r.process.stderr.decode())


if __name__ == '__main__':
    auracle_test.main()
