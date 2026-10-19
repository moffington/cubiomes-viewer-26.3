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
- No implementation/build blockers. Final visible UI and in-game checks are
  explicitly left to the user; see README-26.3.md.

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
- Preparing community README, complete source, release notes and checksums.
- No binary rebuild is required; executable contents remain unchanged.

## Constraints / limitations
- No Minecraft, launcher, server, visible application or browser will be opened.
- No game files, processes, or system settings will be changed.
- Backend parameter tree is annotated with its originating 26.3 snapshot;
  fixture-based validation and full-equivalence limits are documented.
- Initial Git Schannel failure resolved with per-command OpenSSL backend.
