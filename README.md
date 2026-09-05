# Source Inspector Dock

An OBS Studio plugin that brings the selected source's properties, transform, and
filters into one dock, with collapsible sections like an editor's inspector.

## Use

Enable **Docks > Source Inspector**, then select a source in the preview or Sources
list. The dock follows selection, including sources inside groups. In Studio Mode
it follows the preview scene. With multiple selected items, it inspects the newest
selection and edits only that item; changing a source property affects every scene
item using that source, just as OBS's Properties dialog does.

- **Source properties:** OBS's native controls, including dynamic options, paths,
  fonts, colors, lists, and source-specific buttons.
- **Transform:** position, scale, rotation, crop, alignment, and bounding-box controls;
  reset, fit to canvas, and horizontal/vertical flips. Locked items remain locked.
- **Filters:** an expandable editor and enable toggle for every attached filter.
  **Manage** opens OBS's filter dialog to add, remove, rename, and reorder filters.
- **Undo:** property edits, filter enable changes, and transforms join OBS's undo stack.
- **Persistence:** section expansion states are remembered. OBS manages dock placement.

Normal property edits apply live. Devices declaring deferred properties show
**Apply** and **Discard**; leaving that source discards unapplied deferred changes.
External changes refresh when the editor is idle. Selection is checked every 100 ms
while the dock is visible, using public OBS APIs.

## Windows installation

Close OBS, then run the installer from [Releases](https://github.com/prgmitchell/source-inspector-dock/releases/latest),
or extract the ZIP into `C:\ProgramData\obs-studio\plugins`. Restart OBS and enable
**Docks > Source Inspector**. Windows binaries are signed by **MITCHELL SOFTWARE SOLUTIONS LLC**.

## macOS and Linux installation

- **macOS (Apple Silicon and Intel):** run the universal `.pkg` installer. It is
  currently unsigned and not notarized; see [Apple's installation guidance](https://support.apple.com/102445) if blocked.
- **Ubuntu 24.04 (x86-64):** with native OBS from the OBS PPA installed, run
  `sudo apt install ./source-inspector-dock-1.1.0-x86_64-linux-gnu.deb`.
  This package does not support Flatpak OBS.

Close OBS before installing, then restart it and enable **Docks > Source Inspector**.

## Build

Based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate).
For Windows, install Visual Studio 2022 C++ tools, a Windows SDK, Git, and CMake 3.28+:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
```

Dependencies download automatically. Visual Studio 2026 uses CMake 4.2+ and the
`windows-x64-2026` preset. macOS and Linux presets are also included.

## License

GPL-2.0-or-later. Includes the [OBS property editor](vendor/obs/README.md) with upstream copyright notices.
