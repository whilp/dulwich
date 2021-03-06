# test_pack.py -- Compatibilty tests for git packs.
# Copyright (C) 2010 Google, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License or (at your option) any later version of
# the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.

"""Compatibilty tests for git packs."""


import binascii
import os
import shutil
import tempfile

from dulwich.pack import (
    write_pack,
    )
from dulwich.tests.test_pack import (
    pack1_sha,
    PackTests,
    )
from utils import (
    require_git_version,
    run_git,
    )


class TestPack(PackTests):
    """Compatibility tests for reading and writing pack files."""

    def setUp(self):
        require_git_version((1, 5, 0))
        PackTests.setUp(self)
        self._tempdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self._tempdir)
        PackTests.tearDown(self)

    def test_copy(self):
        origpack = self.get_pack(pack1_sha)
        self.assertEquals(True, origpack.index.check())
        pack_path = os.path.join(self._tempdir, "Elch")
        write_pack(pack_path, [(x, "") for x in origpack.iterobjects()],
                   len(origpack))

        returncode, output = run_git(['verify-pack', '-v', pack_path],
                                     capture_stdout=True)
        self.assertEquals(0, returncode)

        pack_shas = set()
        for line in output.splitlines():
            sha = line[:40]
            try:
                binascii.unhexlify(sha)
            except TypeError:
                continue  # non-sha line
            pack_shas.add(sha)
        orig_shas = set(o.id for o in origpack.iterobjects())
        self.assertEquals(orig_shas, pack_shas)
