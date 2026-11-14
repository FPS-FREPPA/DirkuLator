# SPDX-License-Identifier: GPL-3.0-or-later
"""Send acknowledged, bounded controls to DirkuLator's native PS5 bridge."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP, error_perm
import hashlib
import io
import json
from pathlib import Path
import capture_archive
from capture_archive import request_capture
import re
import time

ROOT = Path.cwd() / 'dirkulator-evidence'
DATA = '/data/eden-nx2'
BUTTONS = dict(create=1, l3=2, r3=4, options=8, up=16, right=32, down=64, left=128,
               l2=256, r2=512, l1=1024, r1=2048, triangle=4096, circle=8192, cross=16384, square=32768)


def main():
    global ROOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['status', 'arm', 'pulse', 'stop', 'capture'])
    parser.add_argument('--host', required=True)
    parser.add_argument('--buttons', nargs='*', choices=list(BUTTONS), default=[])
    parser.add_argument('--milliseconds', type=int, default=300)
    for axis in ['lx', 'ly', 'rx', 'ry', 'l2', 'r2']:
        parser.add_argument('--' + axis, type=int, default=128 if axis[1] in 'xy' else 0)
    parser.add_argument('--evidence-dir', default=str(ROOT))
    args = parser.parse_args()
    ROOT = Path(args.evidence_dir).resolve()
    capture_archive.ROOT = ROOT
    assert 1 <= args.milliseconds <= 3000
    assert all(0 <= getattr(args, name) <= 255 for name in ['lx', 'ly', 'rx', 'ry', 'l2', 'r2'])
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    out = ROOT / 'bridge-client' / (stamp + '-' + args.operation)
    out.mkdir(parents=True)
    events = []

    def record(operation, **values):
        events.append({'utc': datetime.now(timezone.utc).isoformat(), 'operation': operation, **values})
        (out / 'events.json').write_text(json.dumps(events, indent=2) + '\n')

    try:
        with FTP() as ftp:
            ftp.connect(args.host, 2121, timeout=10)
            ftp.login()

            def read(path, maximum=8192):
                body = bytearray()
                def append(chunk):
                    body.extend(chunk)
                    if len(body) > maximum:
                        raise RuntimeError('Diagnostic exceeds byte limit: ' + path)
                ftp.retrbinary('RETR ' + path, append, blocksize=262144)
                return bytes(body)

            def write(path, body):
                assert path in [DATA + '/input-bridge-command.txt', DATA + '/capture-now.txt']
                temporary = DATA + '/.codex-' + path.rsplit('/', 1)[-1] + '.upload'
                ftp.storbinary('STOR ' + temporary, io.BytesIO(body))
                assert read(temporary) == body
                ftp.rename(temporary, path)
                record('atomic request', path=path, body=body.decode(), sha256=hashlib.sha256(body).hexdigest())

            if args.operation == 'capture':
                request_capture(ftp, read, write, record, out)
                return

            def status():
                value = json.loads(read(DATA + '/logs/input-bridge-status.json'))
                assert value.get('version') == 1 and isinstance(value.get('session'), str)
                assert re.fullmatch(r'[a-zA-Z0-9_-]{1,64}', value['session'])
                return value

            current = before = status()
            record('initial status', response=before)
            if args.operation == 'status':
                print(json.dumps(before, indent=2), flush=True)
                return
            assert before.get('ready'), 'The bridge is not running'
            session = before['session']
            limit = time.monotonic() + 3
            while time.monotonic() < limit and current.get('heartbeat') == before.get('heartbeat'):
                time.sleep(0.1)
                current = status()
            assert current['session'] == session and current.get('ready') and current.get('heartbeat') != before.get('heartbeat'), 'Bridge heartbeat is stale'
            sequence = max(time.time_ns(), current['sequence'], current['rejected_sequence']) + 1
            if args.operation == 'pulse':
                assert current.get('enabled'), 'Explicitly arm the bridge before sending input'
                operation = 'input'
                duration = args.milliseconds
                buttons = sum(BUTTONS[name] for name in set(args.buttons))
                values = [args.lx, args.ly, args.rx, args.ry, args.l2, args.r2]
            else:
                operation = args.operation; duration = buttons = 0
                values = [128, 128, 128, 128, 0, 0]
            body = f'v1 {session} {sequence} {operation} {duration} {buttons} ' + ' '.join(map(str, values)) + '\n'
            write(DATA + '/input-bridge-command.txt', body.encode())
            deadline = time.monotonic() + 4
            acknowledged = None
            while time.monotonic() < deadline:
                time.sleep(0.05)
                value = status()
                assert value['session'] == session and value.get('ready'), 'Bridge stopped or session changed'
                if value.get('rejected_sequence') == sequence:
                    raise RuntimeError('Command rejected: ' + value.get('error', 'unknown'))
                if value.get('sequence') != sequence:
                    continue
                if args.operation == 'arm' and not value.get('enabled'):
                    raise RuntimeError('Arming canceled: ' + value.get('state', 'unknown'))
                if args.operation == 'pulse' and value.get('applied_sequence') != sequence:
                    continue
                if args.operation == 'pulse' and value.get('state') not in ['pending', 'active', 'expired']:
                    raise RuntimeError('Input canceled: ' + value.get('state', 'unknown'))
                if args.operation == 'stop' and value.get('remote_active'):
                    continue
                acknowledged = value
                break
            assert acknowledged is not None, 'No applied acknowledgment within four seconds; pulse expiry remains bounded'
            record('native acknowledgment', response=acknowledged)
            print(json.dumps(acknowledged, indent=2), flush=True)
    except Exception as exc:
        record('error', error=type(exc).__name__ + ': ' + str(exc))
        raise
    finally:
        print('BRIDGE_EVIDENCE', out, flush=True)




if __name__ == '__main__':
    main()
