# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

"""
This script runs Python unit tests checking whether the license
is mentioned in source and other files. This does not check the
copyright (see copyright.py).

Files and folders can be ignored as well as file extension. See
the class TestLicense members _ingore_files, _ignore_folders
and _ignore_file_ext.

The location of MySQL Router source can be provided using the command
line argument --cmake-source-dir. By default, the current working
directory is used. If the environment variable CMAKE_SOURCE_DIR is
found, it will be use as default.
"""

from datetime import date
from hashlib import sha1
import os
import re
from subprocess import check_output
import sys
from time import strptime
import unittest

from tests import get_arguments, get_path_root, seek_needle, git_tracked

EXP_SHORT_LICENSE = """
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
"""


class TestLicense(unittest.TestCase):

    root_path = ''
    _ignore_file_ext = ['.o', '.pyc', '.pyo', '.txt', '.md']

    # Folders not checked, relative to root_path
    _ignore_folders = [
        os.path.join('.git'),
        os.path.join('.idea'),
        os.path.join('build'),
        os.path.join('gtest'),
        os.path.join('boost'),
    ]

    # Files not checked, relative to root_path
    _ignore_files = [
        os.path.join('.gitignore'),
        os.path.join('License.txt'),
    ]

    def setUp(self):
        self.root_path = os.path.abspath(self.root_path)

    def _check_license_presence(self, path):
        """Check if short license text is present

        Returns True if all is OK; False otherwise.
        """
        ext = os.path.splitext(path)[1]
        errmsg = "License problem in %s (license line: %%d)" % path

        with open(path, 'rb') as fp:
            # Go to the line containing the copyright statement
            line = seek_needle(fp, 'Copyright (c)')
            self.assertTrue(
                line is not None,
                "Could not find start of license in %s" % path)

            # Always blank line after copyright
            line = next(fp)
            if line[0] == '#':
                line = line[1:]
            self.assertEqual("", line.strip(),
                             errmsg % 0)

            # Now check license part
            curr_line = 1
            exp_lines = EXP_SHORT_LICENSE.split("\n")
            for line in fp:
                # Remove hash sign if present
                if line[0] == '#':
                    line = line[1:]
                if curr_line == len(exp_lines) - 1:
                    # We are at the end; skip blank
                    break
                self.assertEqual(exp_lines[curr_line].strip(),
                                 line.strip(),
                                 errmsg % curr_line)
                curr_line += 1
            self.assertEqual(13, curr_line)


    def test_short_license_notice(self):
        """WL8400: Check short license notice"""

        for base, dirs, files in os.walk(self.root_path):
            if base != self.root_path:
                relative_base = base.replace(self.root_path + os.sep, '')
            else:
                relative_base = ''
            if get_path_root(relative_base) in self._ignore_folders:
                continue

            for filename in files:
                fullpath = os.path.join(base, filename)
                if not git_tracked(fullpath):
                    continue
                relative = os.path.join(relative_base, filename)
                if relative in self._ignore_files:
                    continue

                _, ext = os.path.splitext(filename)
                if ext not in self._ignore_file_ext:
                    if os.path.getsize(fullpath):
                        self._check_license_presence(fullpath)

    def test_license_txt(self):
        """WL8400: Check content of License.txt"""
        with open(os.path.join(self.root_path, 'License.txt')) as fp:
            hash = sha1(fp.read()).hexdigest()

        self.assertEqual('06877624ea5c77efe3b7e39b0f909eda6e25a4ec',
                         hash)

    def test_readme_license_foss(self):
        """WL8400: Check FOSS exception in README.txt"""
        with open(os.path.join(self.root_path, 'README.txt')) as fp:
            line = seek_needle(fp, 'MySQL FOSS License Exception')
            self.assertTrue(
                line is not None,
                "Could not find start of FOSS exception")

            nr_lines = 16
            lines = []
            while nr_lines > 0:
                lines.append(next(fp))
                nr_lines -= 1
            hash = sha1(''.join(lines)).hexdigest()

            self.assertEqual(
                'd319794f726e1d8dae88227114e30761bc98b11f',
                hash,
                "FOSS exception in README.txt changed?")

    def test_readme_gpl_disclamer(self):
        """WL8400: Check GPL Disclaimer in README.txt"""
        with open(os.path.join(self.root_path, 'README.txt')) as fp:
            line = seek_needle(fp, 'GPLv2 Disclaimer')
            self.assertTrue(
                line is not None,
                "Could not find start of GPL Disclaimer exception")

            nr_lines = 7
            lines = []
            while nr_lines > 0:
                lines.append(next(fp))
                nr_lines -= 1
            hash = sha1(''.join(lines)).hexdigest()

            self.assertEqual(
                '7ea8fbbe1fcdf8965a3ee310f14e6eb7cb1543d0',
                hash,
                "GPL Disclaimer in README.txt changed?")

if __name__ == '__main__':
    args = get_arguments(description=__doc__)

    TestLicense.root_path = args.cmake_source_dir
    unittest.main(argv=[sys.argv[0]], verbosity=3)
