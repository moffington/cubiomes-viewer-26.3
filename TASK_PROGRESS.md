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
- Install workspace-local Qt build files, integrate backend with qmake.
- Update version selection, biome labels and camp overlay.
- Compile Release with one job; run upstream and targeted headless tests.
- Package portable Windows runtime, source, licenses, and validation notes.

## Constraints / limitations
- No Minecraft, launcher, server, visible application or browser will be opened.
- No game files, processes, or system settings will be changed.
- Backend parameter tree is annotated 26.3-1 snapshot; confirm available coverage
  and describe the limits of fixture-based validation honestly.
- Initial Git Schannel failure resolved with per-command OpenSSL backend.
