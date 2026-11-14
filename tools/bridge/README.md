# DirkuLator bridge tools

Python 3.11+; standard library only. The controller bridge must be compiled into
DirkuLator and a physical controller must be connected, powered on, and neutral.
Use the console's FTP host explicitly; no personal host or path is embedded.

```sh
python3 client.py status --host PS5_HOST
python3 client.py arm --host PS5_HOST
python3 client.py pulse --host PS5_HOST --ly 0 --milliseconds 500
python3 client.py stop --host PS5_HOST
python3 capture_archive.py --host PS5_HOST --stop-when-title-exits
python3 test_capture_archive.py
```

Commands and applied acknowledgments are retained under `dirkulator-evidence`.
Both tools accept `--evidence-dir` to share another output directory. Native
commands expire within three seconds and physical input cancels remote control.
The archiver preserves every produced capture file, nested stages, hashes, and
associated logs. It does not enable a timed capture cadence. `client.py capture`
requests a capture explicitly. Private evidence is not part of the public repo.
