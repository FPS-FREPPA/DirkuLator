# Repository and license-preservation review

Reviewed 15 November 2026, after the first 0.5.54rc development publication.

## Added or improved

- README now leads with the goal of accurate, useful Nintendo Switch 2 emulation
  on native PS5 homebrew, and explains CPU/GPU correctness, rendering, stability,
  performance measurement, and reproducible open development priorities.
- Dated modification/provenance notice, contributor expectations, and links to
  retained attribution, source access, licenses, and validation limits.
- Dependency-license inventory and byte hashes. All159 original collected
  license/copyright/notice texts are preserved in both working files and Git blobs.
- Third-party decoder notice corrected to include AV1 through dav1d.
- Ignore coverage expanded for builds, dependency caches, Python/test/profiler
  output, release binaries, console/phone evidence, games/firmware/keys,
  credentials, local environments, VM disks, and editor/OS caches.
- Portable LF rules for scripts/source, while original license bytes and
  authored binary assets remain byte-preserved.
- Reusable check: `python3 eden/tools/check-repository.py`.

## Checked

57 generated/private-path ignore cases and25 retained-source/asset cases pass.
No tracked file is hidden by ignore rules and no forbidden private artifact is
tracked.159 dependency license hashes match the actual Git blobs. The original
GPL text matches the inherited port copy. Git object integrity passes.

No emulator, shader, graphics patch, controller implementation, or app metadata
changed in this review. The published candidate tag remains at
75d0cbc780904b3f3916e288ff97cbbb5e03fa8b. Original candidate binary/source assets
and their checksums are preserved. The review is a later documentation/tooling
commit, accompanied by a release supplement.

Both GitHub repositories are public and forkable, retain the inherited history,
and have separate project/development default branches. Their updated heads are
verified after publication. This review records concrete repository and notice
preservation checks; each component's license continues to govern its use.

The candidate's physical PS5 validation remains pending, and its known fog/mesh
defects remain open. No additional hardware or compatibility result is claimed.
