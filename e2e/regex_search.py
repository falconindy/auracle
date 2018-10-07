#!/usr/bin/env python

import auracle_e2e_test


def ExtractHttpRequests(debug_output):
    requests = []

    for line in debug_output.decode().split('\n'):
        if line.startswith('> GET'):
            _, _, uri, _ = line.split()
            requests.append(uri)

    print('requests={}'.format(list(requests)))
    return requests


class TestE2ERegexSearch(auracle_e2e_test.TestCase):

    def testFragmentTooShort(self):
        p = self.Auracle(
                ['search', 'f'])
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('insufficient for searching by regular expression',
                p.stderr.decode())


    def testMultipleSearchesWithFiltering(self):
        p = self.Auracle(
                ['search', '--quiet', '^aurac.+', '.le-git$', 'auracle'],
                debug=True)
        self.assertEqual(p.returncode, 0)
        self.assertEqual('auracle-git', p.stdout.decode().strip())

        requests = ExtractHttpRequests(p.stderr)

        # TODO: post-process this further so that we don't have to match query
        # param ordering.
        self.assertCountEqual(requests, [
            '/rpc?by=name-desc&type=search&v=5&arg=aurac',
            '/rpc?by=name-desc&type=search&v=5&arg=le-git',
            '/rpc?by=name-desc&type=search&v=5&arg=auracle',
        ])


if __name__ == '__main__':
    auracle_e2e_test.main()
