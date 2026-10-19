# Validation record — 2026-10-18

## Passed
- Windows x86-64 Release build with Qt 6.8.3 / MinGW GCC 13.1.0.
- Portable executable with restricted PATH, packaged Qt DLLs/plugins and no
  developer Qt paths: `--self-test` exited 0, all 22 checks passed.
- Offscreen checks exercise version/biome gates, new labels/colors, the upstream
  fixed Dappled Forest coordinate, underground Sulfur Caves generation, tiled vs
  scalar biome queries in every dimension, legacy and corrected End ring results,
  an older Overworld fixture, camp configuration/location/variant/overlay wiring,
  salt overrides, real UI version selection, tile rendering, dimension changes,
  panning, zooming, seed replacement and switching to an older Minecraft version.
- Existing `test_biomes`, `test_structures`, `test_pieces`, `test_carvers` compiled
  and passed using the exact production backend object files from the viewer.
- Existing complete backend CMake/CTest build: 5/5 suites passed, including loot
  (the viewer itself does not include a loot UI or loot parser).
- Independent Python implementation of Java's 48-bit RNG matched all 300
  candidate positions for 15 structure types, five seeds and four regions each.
  Includes negative seeds/regions, triangular placement and End's center exclusion.
- All 256 biome IDs checked against the 18 final-release abandoned camp biome
  tags; every eligibility result matched.

## Reference provenance
Backend fixtures are in the pinned fork's `tests/test_biomes.c`,
`tests/test_structures.c`, `tests/test_loot.c` and README:
https://github.com/xpple/cubiomes/tree/b99f6e472018cab903a096697117aab9d9170a19

Final vanilla data extracted by mcmeta, pinned to version tag `26.3-summary`:
https://github.com/misode/mcmeta/tree/26.3-summary

- `data/worldgen/structure_set/data.json` (included test fixture)
- `data/tag/worldgen/biome/data.json` (camp tags included as a reduced fixture)
- `data/worldgen/structure/data.json` (camp heightmap/type inspected)

These independent data checks verify configurations, candidate positions and
camp biome eligibility. They do not independently validate biome noise or terrain.
SeedMapper's 26.3 branch was inspected for the fork's API usage; it was not run.
The supplied mcseedmap page was inspected, but no live coordinate comparison was
obtained from it. No game or Java process was launched.

## Unverified / inherited limits
- Full equivalence of every part of the fork's biome/structure algorithm to final
  vanilla 26.3; the 26.3 biome tree originates in a snapshot. No older-version tree
  is substituted for 26.3.
- Full terrain/block layout viability, including abandoned camps; several terrain
  tests and spawn use upstream approximations.
- Independent fixed-coordinate Sulfur Caves reference (generation smoke checked).
- Exhaustive advanced search, all structure variants, image export, clipboard and
  native visible-window interactions. Those retain upstream implementations.
- No performance benchmark or in-game comparison was run while the user played.

Compilation was limited to at most two jobs, normally one at low priority.
Tests were short, offline/headless where possible, and offscreen.
No game files/processes, launcher profiles or system settings were modified.
