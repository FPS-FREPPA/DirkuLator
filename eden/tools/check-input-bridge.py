#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile and run the actual input bridge with address/undefined sanitizers."""
from pathlib import Path
import shutil, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
compiler = shutil.which('clang++-21') or shutil.which('clang++-18') or shutil.which('clang++')
assert compiler, 'Install Clang with address/undefined sanitizer support'
with tempfile.TemporaryDirectory(prefix='dirkulator-bridge-') as name:
    temp = Path(name)
    source = temp / 'test.cpp'
    source.write_bytes((root / 'tests-nx2/input_bridge_test.cpp').read_bytes())
    subprocess.run([compiler, '-std=c++20', '-O1', '-g', '-pthread',
        '-DEDEN_PS5_INPUT_BRIDGE', '-fsanitize=address,undefined',
        '-fno-omit-frame-pointer', '-I', str(root / 'headless'),
        '-I', str(root / 'third_party'), str(source), '-o', str(temp / 'test')], check=True)
    subprocess.run([str(temp / 'test'), str(temp / 'transport')], check=True, timeout=30)
