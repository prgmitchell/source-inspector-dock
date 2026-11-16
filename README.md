# Source Inspector Dock

An OBS Studio dock for the selected source's properties, transform, and filters.

## Use

Enable **Docks > Source Inspector**, then select a source. Expand each section to
edit its settings.

- **Pin** keeps the inspector on that source and scene item while you select other
  sources or change scenes. **Unpin** follows selection again. Removing the item
  or changing scene collections clears the pin.
- **Search settings** finds property labels, transform controls, and filters by
  name or setting. Matching sections expand while searching; clearing the search
  restores their previous expanded or collapsed state.
- **Add filter** lists compatible filters. Each filter has up/down buttons and an
  **…** menu to rename or remove it. **Manage** still opens OBS's native dialog.
- **Transform presets** save position, scale, rotation, alignment, bounds, and crop.
  Use **Presets > Save current as preset…**, choose a name, then select it and click
  **Apply preset** to recall it on any source. Presets also support update, rename,
  and delete. They persist across OBS restarts and scene collections and use the
  saved numeric values, so check the result on sources of different dimensions.
- **Used in N scenes** lists scenes sharing the source, including groups and nested
  scenes. Property and filter edits affect every reference; transforms affect only
  the inspected scene item.

Source edits, filter actions, and applying a preset support OBS undo/redo. Locked items stay locked. Properties normally apply live;
devices with deferred settings show **Apply** and **Discard**. Leaving the source
discards unapplied changes. In Studio Mode, the dock follows the preview scene.

## Install

Close OBS and download your package from [Releases](https://github.com/prgmitchell/source-inspector-dock/releases/latest).

- **Windows (OBS 33+):** run the installer, or extract the ZIP into `C:\ProgramData\obs-studio\plugins`. Signed by MITCHELL SOFTWARE SOLUTIONS LLC.
- **Windows portable (OBS 33+):** extract the ZIP into the `plugins` folder at the root of your portable OBS installation. Create it if needed; portable OBS ignores plugins in `ProgramData`.
- **macOS:** run the universal `.pkg`. Currently unsigned and not notarized.
- **Ubuntu 24.04:** run `sudo apt install ./source-inspector-dock-*.deb`. Requires native OBS; Flatpak is not supported.

The Windows package uses [OBS 33's plugin layout](https://obsproject.com/kb/legacy-plugin-developer-info):

```text
plugins/
  source-inspector-dock/
    source-inspector-dock.dll
    data/
      locale/
        en-US.ini
```

When upgrading, the installer removes this plugin's old `bin\64bit` DLL and PDB.
For ZIP upgrades, remove those old files yourself after extracting the new package.
Windows users on OBS 32 or earlier should use a previous release with the legacy layout.

GPL-2.0-or-later. Uses [OBS Studio's property editor](https://github.com/obsproject/obs-studio/tree/32.2.1/shared/properties-view), downloaded during the build.
