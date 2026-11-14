#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Three PS5 memory fixes from mihawk-99/PS5_Mesa, newer than the pinned driver.

tools/radv-patches holds them unchanged, in the order they were committed:
- 53cc83f: a large buffer is backed by several direct-memory ranges when no single one is free;
- 249fe49: a piece of such a buffer is mapped only where nothing else is;
- f5610fe: a freed buffer keeps its memory until the GPU has run what was submitted before, and an
  allocation that fails waits for the GPU and tries again before reporting out of memory.
Applied to the driver checkout after patch-radv-transfer-split.py; running it again changes nothing.
"""
from pathlib import Path
import hashlib
import subprocess
import sys

here = Path(__file__).resolve()
patches = sorted((here.parent / 'radv-patches').glob('*.diff'))
digest = hashlib.sha256(here.read_bytes())
for patch in patches:
    digest.update(patch.read_bytes())
fingerprint = digest.hexdigest()
if sys.argv[1:2] == ['--digest']:
    print(fingerprint)
    raise SystemExit(0)
if sys.argv[1:2] == ['--check']:
    stamp = Path(sys.argv[2])
    raise SystemExit(0 if stamp.is_file() and stamp.read_text().strip() == fingerprint else 1)

root = here.parents[1]
src = Path(sys.argv[1]) if len(sys.argv) > 1 else root.parent / 'mihawk-vulkan-review/.deps/work/radv-src'
assert len(patches) == 3, 'tools/radv-patches changed'
applied = src / '.eden-ps5-memory-applied'
if not applied.is_file():
    for patch in patches:
        subprocess.run(['patch', '-d', str(src), '-p1', '-N', '-s', '-f', '--no-backup-if-mismatch',
                        '-i', str(patch)], check=True)
    applied.write_text(fingerprint + '\n')
    # As patch-radv-disk-cache.py: set the receipt aside so the next build compiles what changed.
    provenance = root.parent / 'mihawk-vulkan-review/.deps/native/radv-release/PROVENANCE.txt'
    if provenance.exists():
        provenance.replace(provenance.with_suffix('.base.txt'))
print(f'RADV PS5 memory fixes: {len(patches)} applied')
