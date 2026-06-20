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

    def testFormatsFields(self):
        r = self.Auracle(['info', 'auracle-git'])
        self.assertEqual(0, r.process.returncode)

        out = r.process.stdout.decode()

        # Vector-valued fields are joined with the default delimiter.
        self.assertIn(
            'Depends On     : pacman  libarchive.so  libcurl.so  libsystemd.so',
            out,
        )
        # absl::Time fields render using the default RFC3339 format. The test
        # harness pins LC_TIME=C and TZ=UTC.
        self.assertIn('Submitted      : 2017-07-02T16:40:08+00:00', out)
        self.assertIn('Last Modified  : 2018-10-10T18:21:49+00:00', out)

    def testExitFailureOnNotFound(self):
        r = self.Auracle(['info', 'packagenotfoundbro'])
        self.assertNotEqual(0, r.process.returncode)

    def testBadResponsesFromAur(self):
        r = self.Auracle(['info', '503'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertEqual(r.process.stderr.decode(), 'error: INTERNAL: HTTP 503\n')

        r = self.Auracle(['info', '429'])
        self.assertNotEqual(0, r.process.returncode)
        self.assertEqual(
            (
                'error: RESOURCE_EXHAUSTED: Too many requests: '
                'the AUR has throttled your IP.\n'
            ),
            r.process.stderr.decode(),
        )


if __name__ == '__main__':
    auracle_test.main()
