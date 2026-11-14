# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify complete capture retention, nested stages and incomplete-transfer rejection."""
from pathlib import Path
import hashlib
import json
import tempfile
from capture_archive import archive_capture, CAPTURES


class Source:
    def __init__(self, files):
        self.files = files
        self.truncate = None
        self.change = False
        self.inventories = 0
    def inventory(self, root):
        self.inventories += 1
        result = {name: {'bytes': len(data), 'modified': 'fixed'} for name, data in self.files.items() if name.startswith(root + '/')}
        if self.change and self.inventories > 1:
            result[root + '/late-file.raw'] = {'bytes': 3, 'modified': 'new'}
        return result
    def bytes(self, name):
        return self.files[name]
    def retrieve(self, name, receive):
        data = self.files[name]
        if name == self.truncate:
            data = data[:-1]
        for offset in range(0, len(data), 17):
            receive(data[offset:offset + 17])


def main():
    remote = CAPTURES + '/capture-00-frame-123'
    raw = bytes(range(256)) * 8
    files = {
        remote + '/index.txt': f'# header\n0 0x10 0 32 16 1 1 {len(raw)} 0 32 16 1 0x0 target.raw\n'.encode(),
        remote + '/target.raw': raw,
        remote + '/readers.txt': b'readers\n', remote + '/writers.txt': b'writers\n',
        remote + '/sequence.txt': b'sequence\n', remote + '/unexpected.extension': b'keep this too',
        remote + '/stages/index.txt': b'# stage\n0 0x11 0 1 1 1 1 4 0 1 1 2 0x0 before.raw\n',
        remote + '/stages/before.raw': b'\x00\x01\xfe\xff',
        remote + '/stages/nested/metadata.json': b'{"all":"retained"}\n',
    }
    with tempfile.TemporaryDirectory(prefix='oot-archive-test-') as temp:
        temp = Path(temp)
        result = archive_capture(Source(files), remote, temp / 'complete')
        assert result['file_count'] == len(files) and result['complete']
        for name, data in files.items():
            relative = name[len(remote) + 1:]
            assert (temp / 'complete' / relative).read_bytes() == data
            assert result['files'][relative]['sha256'] == hashlib.sha256(data).hexdigest()
        assert json.loads((temp / 'complete/ARCHIVE-MANIFEST.json').read_text())['complete']
        for label, truncated, changed in [('truncated', True, False), ('generation-changed', False, True)]:
            source = Source(files)
            source.truncate = remote + '/target.raw' if truncated else None
            source.change = changed
            try:
                archive_capture(source, remote, temp / label)
                raise AssertionError('Incomplete capture was accepted')
            except RuntimeError:
                pass
            assert not (temp / label / 'ARCHIVE-MANIFEST.json').exists()
        try:
            archive_capture(Source(files), CAPTURES + '/../other', temp / 'invalid')
            raise AssertionError('Invalid remote root accepted')
        except AssertionError as exc:
            assert str(exc) != 'Invalid remote root accepted'
    print('PASS all files/extensions/nested stages, raw byte/hash fidelity, metadata, truncated-transfer rejection, changing-generation rejection and path scope')


if __name__ == '__main__':
    main()
