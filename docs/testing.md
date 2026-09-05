# Testing Source Inspector

The `inspector-integration` CTest executable runs real libobs scenes and the same
Qt widgets used by the dock. It stubs the frontend application's scene selection,
saving, and undo stack, so it never changes your OBS scene collections or starts
recording/streaming. On Windows its widgets are rendered without displaying a window.
The built plugin DLL is also loaded and initialized through libobs.

Covered behaviors include source and filter property editing, property defaults,
dynamic controls, external setting updates, undo/redo, locked transforms, multiple
selection, group children, filter insertion/removal, filter enable undo, deferred
Apply, abandoned deferred changes, deletion during a pending edit, and shutdown.

```powershell
cmake --build build_x64 --config RelWithDebInfo --parallel
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
```

The test saves `build_x64/inspector-preview.png` for visual review. The harness
does not initialize a video renderer, so libobs can log null-graphics diagnostics
while destroying synthetic sources. It does not test actual capture devices.

## Manual OBS checks

1. Open Docks > Source Inspector, dock it on the right, and resize it to a narrow width.
2. Select image, browser, text, media, audio, and device-capture sources. Check all
   controls and source-specific buttons against OBS's Properties dialog.
3. Change properties and filters, then use Edit > Undo / Redo. Change a setting in
   the original Properties dialog and check that the dock refreshes after focus leaves it.
4. Adjust position, scale, rotation, crop, bounds, and alignment. Confirm only the
   inspected scene item changes, including when the same source is used twice.
5. Select items inside groups; add/remove/reorder filters using Manage; toggle
   their enabled state. Delete a selected source while its dock is open.
6. Enable Studio Mode and confirm the dock follows the preview scene rather than
   the program scene. Switch collections with the dock both open and hidden.
7. Collapse sections, restart OBS, and verify dock placement and section states.
8. Test devices with deferred properties using Apply and Discard. Unapplied deferred
   edits are discarded when selection changes.

Windows x64 is built and integration-tested locally. Actual-device checks and
macOS/Linux builds still require testing on those systems.
