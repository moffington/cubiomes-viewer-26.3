# Cubiomes Viewer for Minecraft Java 26.3

Extract the complete portable ZIP and run **cubiomes-viewer.exe**. Keep its DLLs,
`platforms`, `styles`, and `qt.conf` alongside it. Requires Windows 10/11 x64.
No installer, Minecraft, Java, browser, or network connection is required.

This is Cubitect's original native Qt viewer with a focused backend update.
Select **26.3** in the Minecraft version list. New sessions default to 26.3;
an existing saved session keeps its selected version. Map generation defaults
to two worker threads; this can be adjusted in preferences.

## Go to coordinate marker (revision 2)
Press **Ctrl+G** (or **Map > Go to...**), enter X/Z and press **Apply** or **OK**.
A red-and-white crosshair marks the exact coordinates, with an X/Z label beside
it. The marker follows that location while you pan and zoom. Another Go to
replaces it; right-click the map and choose **Clear coordinate marker** to remove
it. It belongs to the dimension where it was placed, is cleared on a seed change,
and lasts until you close the viewer. Markers are not saved in session files.

## Added support
- Overworld, Nether and End biome maps using xpple/cubiomes 26.3 generation.
- Dappled Forest and Sulfur Caves names, colors, queries and biome selection.
  Lower the map Y value (for example to 0) to inspect underground biomes.
- Abandoned Camp toggle, biome viability, structure analysis and search filter.
  The camp toggle reuses the existing hut icon and has its own tooltip.
- The backend's 26.3 End ring correction and updated structure rules.
- Existing seed input, panning, zoom, dimension switching, image export and
  older version selection remain available. Custom structure salts use the
  fork's configuration-provider API.

## Versions and source
- Viewer base: Cubitect/cubiomes-viewer `3acc863245b30c655498d60323c50da0865e0199`.
- Backend: xpple/cubiomes `b99f6e472018cab903a096697117aab9d9170a19` (unmodified).
  https://github.com/xpple/cubiomes/commit/b99f6e472018cab903a096697117aab9d9170a19
- Qt 6.8.3, MinGW GCC 13.1.0, Release x86-64. Generation uses `-fwrapv` and
  avoids fast-math and machine-specific CPU tuning.
- The accompanying source ZIP includes the viewer, pinned backend, tests,
  licenses and build instructions. Third-party attribution is in `licenses/`.

## Validation and limits
The executable passed 30 isolated Qt offscreen checks, including actual tile
rendering in all dimensions. Upstream biome/structure/piece/carver tests passed
against the exact backend objects linked into this executable. All five upstream
suites also passed in a separate complete library build. An independent Java RNG
implementation matched 300 candidate positions across 15 structure types using
extracted final 26.3 structure-set data; all 256 biome IDs matched the final camp
eligibility tags. See `VALIDATION.md` and the included test logs for details.

Structure markers are predictions. The inherited backend approximates some
terrain checks; pyramids, jungle temples and mansions can have false positives,
and spawn is approximate. Camp markers use the backend's biome viability check;
their complete block layout and actual terrain placement have not been checked
in-game. No exhaustive validation of every structure type, every seed, or advanced
seed-search combinations was performed. The biome tree is the fork's dedicated
26.3 tree (annotated with its originating snapshot); the whole tree has not been
independently compared against final vanilla code. Sulfur Caves has a generation
smoke check, rather than a separately sourced fixed-coordinate fixture.

No Minecraft instance, launcher or server was started for development or tests.
Development checks used no visible viewer or other GUI. Basic interactive map use
was subsequently confirmed, including a known Dark Forest island. Clipboard,
dialogs and image export have not been systematically verified.

## Brief manual smoke test
1. Run the executable and select 26.3. Enter seed `-3829811542736183482`.
2. Use Go to position at X `68148`, Z `80990`, with map Y `77`; the biome query
   should report Dappled Forest. Apply/OK should leave a red-and-white coordinate
   marker. Pan and zoom to check it stays at that location; right-click to clear.
3. Return to that seed and enable Abandoned Camp. At X `224`, Z `-384` there
   should be a camp marker. Check Overworld, Nether and End switching.
4. For the End ring fixture, use seed `856461443495910397`, End, X `-371136`,
   Z `-64`; at coarse biome scale the 26.3 map gives End Highlands, while 26.2
   gives End Barrens. These are backend fixtures, not an in-game test performed
   during this task.

The optional `--self-test` argument runs the checks offscreen and exits without
showing a window or loading your settings. It needs temporary write access beside
the executable. Ordinary operation retains upstream's user-profile settings and
session storage behavior.

NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR
MICROSOFT.
