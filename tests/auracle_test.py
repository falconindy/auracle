#!/usr/bin/env python

import fakeaur.server
import glob
import multiprocessing
import os.path
import subprocess
import tempfile
import time
import unittest


def FindMesonBuildDir():
    # When run through meson or ninja, we're already in the build dir
    if os.path.exists('.ninja_log'):
        return os.path.curdir

    # When run manually, we're probably in the repo root.
    paths = glob.glob('*/.ninja_log')
    if len(paths) > 1:
        raise ValueError('Multiple build directories found. Unable to proceed.')
    if len(paths) == 0:
        raise ValueError(
                'No build directory found. Have you run "meson build" yet?')

    return os.path.dirname(paths[0])


class HTTPRequest(object):

    def __init__(self, request):
        self.requestline = request.pop(0)
        self.command, self.path, self.request_version = self.requestline.split()

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
        self.stream.write('\n{} ({:.03}s)'.format(
            self.getDescription(test), elapsed))


class TestCase(unittest.TestCase):

    def setUp(self):
        self.build_dir = FindMesonBuildDir()
        self._tempdir = tempfile.TemporaryDirectory()
        self.tempdir = self._tempdir.name

        self.requests_file = tempfile.NamedTemporaryFile(
                dir=self.tempdir, prefix='requests-', delete=False).name

        q = multiprocessing.Queue()
        self.server = multiprocessing.Process(
                target=fakeaur.server.Serve, args=(q,))
        self.server.start()
        sockname = q.get()

        self.baseurl = 'http://{}:{}'.format(*sockname)

        self._WritePacmanConf()


    def tearDown(self):
        self.server.terminate()


    def _ProcessDebugOutput(self):
        self.requests_sent = []

        with open(self.requests_file) as f:
            header_text = []
            for line in f.read().splitlines():
                if line:
                    header_text.append(line)
                else:
                    self.requests_sent.append(HTTPRequest(header_text))
                    header_text = []

        self.request_uris = [r.path for r in self.requests_sent]


    def _WritePacmanConf(self):
        with open(os.path.join(self.tempdir, 'pacman.conf'), 'w') as f:
            f.write('[options]\nDBPath = {}/fakepacman'.format(
                os.path.dirname(os.path.realpath(__file__))))


    def Auracle(self, args):
        env = {
            'PATH': '{}/fakeaur:{}'.format(
                os.path.dirname(os.path.realpath(__file__)), os.getenv('PATH')),
            'AURACLE_TEST_TMPDIR': self.tempdir,
            'AURACLE_DEBUG': 'requests:{}'.format(self.requests_file),
        }

        cmdline = [
            os.path.join(self.build_dir, 'auracle'),
            '--baseurl', self.baseurl,
            '--color=never',
            '--pacmanconfig={}/pacman.conf'.format(self.tempdir),
            '--chdir', self.tempdir,
        ] + args

        p = subprocess.run(cmdline, env=env, capture_output=True)

        self._ProcessDebugOutput()

        return p


    def assertPkgbuildExists(self, pkgname, git=False):
        self.assertTrue(
                os.path.exists(os.path.join(self.tempdir, pkgname, 'PKGBUILD')))
        if git:
            self.assertTrue(
                    os.path.exists(os.path.join(self.tempdir, pkgname, '.git')))


def main():
    test_runner = unittest.TextTestRunner(resultclass=TimeLoggingTestResult)
    unittest.main(testRunner=test_runner)
