#!/usr/bin/env python

import glob
import os.path
import shutil
import subprocess
import tempfile
import unittest

def FindMesonBuildDir():
    if os.path.exists('.ninja_log'):
        return os.path.curdir

    paths = glob.glob('*/.ninja_log')
    if len(paths) > 1:
        raise ValueError('Multiple build directories found. Unable to proceed.')
    if len(paths) == 0:
        raise ValueError(
                'No build directory found. Have you run "meson build" yet?')

    return os.path.dirname(paths[0])


class TestCase(unittest.TestCase):

    def setUp(self):
        self.build_dir = FindMesonBuildDir()

        self.tempdir = tempfile.mkdtemp()

        tmpfile = tempfile.mkstemp(dir=self.tempdir)
        os.close(tmpfile[0])
        self.requests_file = tmpfile[1]

    def tearDown(self):
        shutil.rmtree(self.tempdir)


    def Auracle(self, args):
        env = {
            'AURACLE_DEBUG': 'requests:{}'.format(self.requests_file)
        }

        cmdline = [
            os.path.join(self.build_dir, 'auracle'),
            '--color=never',
            '--chdir', self.tempdir,
        ] + list(args)

        p = subprocess.run(cmdline, env=env, capture_output=True)

        with open(self.requests_file) as f:
            self.requests_made = f.read().splitlines()

        return p


    def assertPkgbuildExists(self, pkgname, git=False):
        self.assertTrue(
                os.path.exists(os.path.join(self.tempdir, pkgname, 'PKGBUILD')))
        if git:
            self.assertTrue(
                    os.path.exists(os.path.join(self.tempdir, pkgname, '.git')))


def main():
    unittest.main()
