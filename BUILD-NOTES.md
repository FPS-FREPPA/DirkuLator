# DirkuLator 0.5.54rc

First development release candidate. Native PS5 title PPSA99862, content version
00.005.054. The application is branded DirkuLator.

## Included

The complete current port source and patch series, the GPU idle-fence correction,
the bounded controller-input bridge, and existing shader/task/mesh diagnostics.
The older application name in the file-handle error and build banner is corrected.
No unreviewed upstream PR is presented as an incorporated fix.

## Verification

- Full native compilation from a fresh object tree with compiler-cache reuse
  disabled, followed by the final candidate-version rebuild.
- Normal release checks, native package inventory, loader/import/alignment checks,
  and strict package-isolation checks passed.
- The actual native bridge code is present. Its parser, expiry, applied input,
  physical cancellation, and background file transport passed ASan/UBSan tests.
- Twelve corrected idle-fence ordering cases passed; the preserved faulty draft
  is rejected by the regression controls.
- Both selected captured shader inputs passed lowering and sanitizer checks.
  This is acceptance evidence, not numerical-equivalence proof.
- Package files and archive members are checked against SHA-256 manifests.

## Limits

This candidate has not been deployed or tested on a physical PS5. Local build,
package, and component tests do not establish a complete game compatibility pass.
White/cyan fog, sharp fog boundaries, and mesh/vertex shattering remain unresolved.
Earlier 0.5.21 console runs are separate evidence and are not candidate validation.
Gameplay and phone snapshots were stopped at the user's request and not resumed.

## Source and licenses

The matching source archive, pinned dependency manifests, build instructions,
GPL license, third-party notices, and collected dependency license/copyright texts
accompany this candidate. Original contributor notices and Git history remain.
See SOURCES.md and VALIDATION.json. No game files, keys, firmware, saves, private
console modules, credentials, or captures are included.
