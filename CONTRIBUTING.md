# Contributing to DirkuLator

Fork either public repository and create a topic branch. Development candidates
are published from `DirkuLator-dev`; include the base revision in a pull request.

Focus changes on the goals in [README.md](README.md). Describe the failure,
the shared emulator behavior that changes, and the supporting evidence. Link
the exact upstream PR and commit when importing work, and preserve its authorship,
copyright notices, and license terms.

Keep patches and dependency pins reproducible. New source must have a compatible
license; do not relabel third-party files as exclusively owned by this project.
Contributors retain copyright in their contributions. No copyright assignment
is required by this project.

Run the checks relevant to the change and report their actual results. Distinguish
build/package checks, mocked or host component tests, shader acceptance tests,
and physical PS5 results. State the revision, configuration, reproduction steps,
and limits. Shader acceptance is not proof of numerical correctness; a desktop
benchmark is not a PS5 benchmark.

For the controller/capture components, useful checks from the repository root are:

```sh
python3 eden/tools/check-input-bridge.py
python3 tools/bridge/test_capture_archive.py
python3 eden/tools/check-repository.py
```

Use the normal native build/package checks for release changes. Keep unsupported
behavior visible and provide a regression for a correctness fix. Avoid disabling
rendering work merely to hide an error.

Do not commit games, keys, proprietary firmware or console modules, saves,
credentials, raw captures, personal machine configuration, or generated output.
Use `.gitignore` and inspect `git diff --cached` before committing. Public bug
reports should contain only evidence the reporter is allowed to share.

Preserve published candidate tags. Put follow-up changes in a new commit and
identify a new candidate when emulator code changes. Keep matching source access
and license notices available with binary distributions; see [SOURCES.md](SOURCES.md).
