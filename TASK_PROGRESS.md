# Minecraft Java 26.3 update

## Completed
- Cloned Cubitect/cubiomes-viewer at 3acc863245b30c655498d60323c50da0865e0199.
- Inspected qmake/Qt build, map pipeline and version/overlay definitions.
- Selected xpple/cubiomes b99f6e472018cab903a096697117aab9d9170a19.
- Verified explicit MC_26_3, dappled forest, sulfur caves, abandoned camps, and
  version-dependent End overflow fix in the backend source.
- Reviewed upstream viewer PR 394 and library PR 166 (26.2 changes); the xpple
  fork is more complete for this task. SeedMapper also uses this backend.

## Remaining
- Revision 3: final portable-archive verification and publication. Implementation,
  Release build, 60 offscreen checks, seven UI regressions and offscreen visual
  inspection are complete. No generation changes.

## Implementation and validation completed
- Workspace-local Qt 6.8.3 / matching MinGW 13.1.0; native x64 Release built.
- Dedicated 26.3 backend, version selector/default, biome labels/colors, camp
  overlay/analysis/filter and new salt-provider API integrated.
- Portable runtime staged; 22/22 isolated offscreen viewer checks passed.
- 5/5 complete upstream suites passed; four relevant upstream suites also passed
  against the exact production backend objects linked into the executable.
- 300 independent candidate-position comparisons across 15 structure types passed.
- All 256 biome IDs matched final 26.3 camp eligibility (18 allowed biomes).
- Added user/build/validation documentation with explicit accuracy limits.
- Packaged native executable with Qt/compiler DLLs, plugins and licenses.
- Portable ZIP CRC and relocation with spaces passed all 22 offscreen checks.
- Corresponding source archive and release notes included alongside the ZIP.
- Screenshot follow-up: different input seeds explained the ocean/island mismatch.
  Production-object UI regression passed seed editing and version-dropdown signals:
  matching inputs give matching island results. A public upstream fixture switches
  Plains to Dappled Forest when selecting 26.3. User confirmed the island matches.
  No executable correction was needed.

## GitHub publication
- User authorized publication under the connected account moffington.
- GitHub CLI authentication completed and account identity verified as moffington.
- Created public fork: https://github.com/moffington/cubiomes-viewer-26.3
- Published community prerelease v4.1.2-mc26.3 with the revision 2 Windows ZIP,
  matching complete source ZIP, licenses, validation records and SHA256 checksums:
  https://github.com/moffington/cubiomes-viewer-26.3/releases/tag/v4.1.2-mc26.3
- Release tag points to tested source 3c9bb729323bdda9fefb7ae60df69a81caed5d89.
- Downloaded all three public assets without authentication; every byte and hash
  matched the prepared files, and both ZIP CRC checks passed. No publication
  blockers remain. This publication record follows the fixed release tag.

## Go to marker request (revision 2)
- Added an exact, fixed-size red/white crosshair and X/Z label on Go to Apply/OK.
- Marker follows map pan/zoom/resize and stays in its originating dimension.
- New Go to replaces it; seed changes and the context-menu clear action remove it.
- Immediate travel now cancels an earlier animation so it cannot undo the jump.
- Release rebuilt with one low-priority job; all 30 offscreen checks passed,
  including eight dialog/rendering regressions. Seven UI seed/version checks
  also passed; preview visually checked offscreen.
- Portable packaging uses a separate r2 folder without changing the running
  viewer. Matching source, notices and test logs accompany the release.
- User confirmed the coordinate marker works in the updated viewer.

## Revision 3: saved markers and usability
- Persistent named markers and notes, manual completion/reopening, right-click
  structure actions, green completion badges and hidden completed predictions.
- Markers tab with progress/type/text/dimension filters, Go to, Enter activation,
  edit, copy coordinates, confirmed deletion and backup export/import.
- Go to Save marker checkbox, Ctrl+M / Ctrl+Shift+M shortcuts, and two target
  presets for Overworld villages/strongholds and Nether bastions/fortresses.
- Atomic/locked JSON storage in the user profile; exact 64-bit seeds; separate
  seed/version/world-type/dimension data; failed writes and corrupt data reported.
- 60 offscreen checks and seven seed/version UI regressions passed. All four
  requested structure types tested using real generated icons. UI inspected
  offscreen; generation library unchanged. Guide: MARKERS_GUIDE.md.
- Revision 3 packaging/publication in progress in a separate output directory.

## Constraints / limitations
- No Minecraft, launcher, server, visible application or browser will be opened.
- No game files, processes, or system settings will be changed.
- Backend parameter tree is annotated with its originating 26.3 snapshot;
  fixture-based validation and full-equivalence limits are documented.
- Initial Git Schannel failure resolved with per-command OpenSSL backend.
