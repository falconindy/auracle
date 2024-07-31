#!/usr/bin/env python
# SPDX-License-Identifier: MIT

import fakeaur.server
import glob
import multiprocessing
import os.path
import subprocess
import tempfile
import textwrap
import time
import unittest

__scriptdir__ = os.path.dirname(os.path.abspath(__file__))


def FindMesonBuildDir():
    # When run through meson or ninja, we're already in the build dir
    if os.path.exists('.ninja_log'):
        return os.path.curdir

    # When run manually, we're probably in the repo root.
    paths = glob.glob('*/.ninja_log')
    if len(paths) > 1:
        raise ValueError(
            'Multiple build directories found. Unable to proceed.')
    if len(paths) == 0:
        raise ValueError(
            'No build directory found. Have you run "meson build" yet?')

    return os.path.dirname(paths[0])


class HTTPRequest:

    def __init__(self, request):
        requestline = request.pop(0)
        self.command, self.path, self.request_version = requestline.split()

        self.headers = {}
        for line in request:
            k, v = line.split(':', 1)
            self.headers[k.lower()] = v.strip()


class TimeLoggingTestResult(unittest.runner.TextTestResult):

    def startTest(self, test):
        self._started_at = time.time()
        super().startTest(test)

    def addSuccess(self, test):
        elapsed = time.time() - self._started_at
        super(unittest.runner.TextTestResult, self).addSuccess(test)
        if self.showAll:
            self.stream.writeln(f'ok ({elapsed:.03}s)')
        elif self.dots:
            self.stream.write('.')
            self.stream.flush()


unittest.runner.TextTestRunner.resultclass = TimeLoggingTestResult


class AuracleRunResult:

    def _ProcessDebugOutput(self, requests_file):
        requests_sent = []

        with open(requests_file) as f:
            header_text = []
            for line in f.read().splitlines():
                if line:
                    header_text.append(line)
                else:
                    requests_sent.append(HTTPRequest(header_text))
                    header_text = []

        return requests_sent

    def __init__(self, process, request_log):
        self.process = process
        self.requests_sent = self._ProcessDebugOutput(request_log)
        self.request_uris = list(r.path for r in self.requests_sent)


class TestCase(unittest.TestCase):

    def setUp(self):
        self.build_dir = FindMesonBuildDir()
        self._tempdir = tempfile.TemporaryDirectory()
        self.tempdir = self._tempdir.name

        q = multiprocessing.Queue()
        self.server = multiprocessing.Process(target=fakeaur.server.Serve,
                                              args=(q, ))
        self.server.start()
        self.baseurl = q.get()

        self._WritePacmanConf()

    def tearDown(self):
        self.server.terminate()
        self.server.join()
        self._tempdir.cleanup()
        self.assertEqual(0, self.server.exitcode,
                         'Server did not exit cleanly')

    def _WritePacmanConf(self):
        with open(os.path.join(self.tempdir, 'pacman.conf'), 'w') as f:
            f.write(
                textwrap.dedent(f'''\
                [options]
                DBPath = {__scriptdir__}/fakepacman

                [extra]
                Server = file:///dev/null

                [community]
                Server = file:///dev/null
            '''))

    def Auracle(self, args):
        requests_file = tempfile.NamedTemporaryFile(dir=self.tempdir,
                                                    prefix='requests-',
                                                    delete=False).name

        env = {
            'PATH': f'{__scriptdir__}/fakeaur:{os.getenv("PATH")}',
            'AURACLE_TEST_TMPDIR': self.tempdir,
            'AURACLE_DEBUG': f'requests:{requests_file}',
            'LC_TIME': 'C',
            'TZ': 'UTC',
        }

        cmdline = [
            os.path.join(self.build_dir, 'auracle'),
            '--color=never',
            f'--baseurl={self.baseurl}',
            f'--pacmanconfig={self.tempdir}/pacman.conf',
            f'--chdir={self.tempdir}',
        ] + args

        return AuracleRunResult(
            subprocess.run(cmdline, env=env, capture_output=True),
            requests_file)

    def assertPkgbuildExists(self, pkgname):
        self.assertTrue(
            os.path.exists(os.path.join(self.tempdir, pkgname, 'PKGBUILD')))
        self.assertTrue(
            os.path.exists(os.path.join(self.tempdir, pkgname, '.git')))

    def assertPkgbuildNotExists(self, pkgname):
        self.assertFalse(
            os.path.exists(os.path.join(self.tempdir, pkgname, 'PKGBUILD')))
        self.assertFalse(
            os.path.exists(os.path.join(self.tempdir, pkgname, '.git')))


def main():
    unittest.main()
