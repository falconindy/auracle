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
        self.tempdir = tempfile.mkdtemp()
        self.build_dir = FindMesonBuildDir()


    def tearDown(self):
        shutil.rmtree(self.tempdir)


    def Auracle(self, args, debug=False):
        env = {}
        if debug:
            env['AURACLE_DEBUG'] = '1'

        return subprocess.run([
            os.path.join(self.build_dir, 'auracle'),
            '--color=never',
            '--chdir', self.tempdir,
        ] + list(args), env=env, capture_output=True)


    def assertPkgbuildExists(self, pkgname, git=False):
        self.assertTrue(
                os.path.exists(os.path.join(self.tempdir, pkgname, 'PKGBUILD')))
        if git:
            self.assertTrue(
                    os.path.exists(os.path.join(self.tempdir, pkgname, '.git')))


def main():
    unittest.main()
