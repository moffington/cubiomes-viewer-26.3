# Saved markers and exploration progress (revision 3)

## Find your usual targets
Open **Map > Find exploration targets** and choose:

- **Villages and strongholds (Overworld)**.
- **Bastions and fortresses (Nether)**.

These presets switch dimension, enable the two relevant overlays, and zoom in
enough for their icons to appear. Other structure overlays are turned off; your
saved markers remain available. Pan and zoom as before. Structure predictions
retain the backend's documented accuracy limits.

## Save places and mark them completed
- Right-click a structure icon and choose **Save ... marker** to keep it on your
  to-do list, or **Mark ... as completed** to save it as already visited.
- Right-click an empty spot and choose **Add marker here...**. Give it a name,
  optionally add notes, and press **Save**.
- **Ctrl+Shift+M** adds a marker at the map center.
- In **Ctrl+G / Go to**, tick **Save this location as a marker** before Apply/OK
  to keep that destination after closing the app. Without the checkbox, Go to
  keeps the temporary coordinate indicator from revision 2.

Pending markers have a blue badge. Completed markers have a green check. Saved
structure markers retain the structure's icon. Click a saved marker to see its
name; **Show marker names** displays names for all saved markers.

Right-click a saved marker to edit it, change its completed status, or delete it.
Deleting a structure marker also removes its saved progress; the predicted
structure itself is still part of the seed map.

## Revisit and manage saved places
Open the **Markers** tab or press **Ctrl+M**. It shows the current seed/version/
world type. You can:

- Tick **Done** to save completed status immediately; untick it to reopen a site.
- Filter by progress, type, dimension, or text in the name/notes/coordinates.
- Choose **All dimensions** to include your Nether and End markers.
- Select a row and use **Go to**, double-click it, or press Enter to return there.
  The map switches to that marker's dimension automatically.
- Rename a marker, edit notes, copy its X/Z, or delete it with confirmation.
- Enable **Hide completed on map** to hide completed markers and their predicted
  structure icons. Completed rows remain available in the list so you can undo
  completion or revisit a hidden site.

Custom marker coordinates can be edited. Structure marker coordinates stay tied
to the predicted structure; add a custom marker for a different nearby spot.

## Persistence and backups
Every successful edit saves immediately, including completion and visibility
options. Markers belong to their seed, Minecraft version, Large Biomes setting,
and dimension. Switching worlds does not erase them; return to the same inputs
to see them again. The map Y value does not split your saved markers.

On Windows, data normally lives in **%APPDATA%\cubiomes-viewer\markers.json**.
The exact path appears at the bottom of the Markers tab. This user-profile file
survives app restarts and extracting a newer portable build into another folder.
Markers are independent of the viewer's seed-search session files.

Use **Export backup...** to back up markers for **all worlds** to a JSON file.
**Import backup...** adds missing markers while keeping existing markers and
progress. It does not replace your current map visibility preferences. Copy that
backup to another computer and import it there to move your markers.

Writes are atomic and locked. Independent edits from two viewer copies are
merged before saving; reopening the Markers tab reloads the latest file. A bad
file or failed write is reported rather than silently replacing your progress.
If a save fails, resolve the error and save again before closing the editor.
There is a 16 MB data limit and notes can contain up to 4000 characters.

Markers and completion are manual annotations. The app does not read your
Minecraft installation, saves, inventory, or game progress, and has no cloud sync.
