# Source and rebuilding

The source for this development candidate is the `v0.5.54rc` tag at
https://github.com/FPS-FREPPA/DirkuLator-dev and the accompanying source archive.
The Git history, port modifications, patch series, and build/packaging scripts
are included. From the source root, run `cd eden` and `make package JOBS=6` on
the Linux/WSL environment described in `eden/docs/BUILDING.md`.

Exact upstream revisions, archive hashes and download locations are retained in
`eden/UPSTREAM.json` and `eden/tools/deps.json`. `make deps` fetches those inputs;
`eden/tools/prepare-build.sh` applies the supplied patch series in filename order.
The source dependencies and original notices remain under their own licenses.
The original public source URLs are also recorded in THIRD_PARTY_NOTICES.md.
The native runtime source is pinned by the boilerplate entry in the manifests;
no proprietary console system libraries are distributed.

The release includes collected original license/copyright texts under
`licenses/dependencies/`. The bridge regression can be run with
`python3 eden/tools/check-input-bridge.py`; ordinary core and package checks are
part of the provided build scripts. Private captured shaders used for extra local
diagnostics are excluded from the public source and are not required to build it.

Keep the matching source, dependency pins, notices, and build instructions
available whenever redistributing a binary, and follow each included license.
