# Source Inspector Dock

An OBS Studio dock for the selected source's properties, transform, and filters.

## Use

Enable **Docks > Source Inspector**, then select a source. Expand each section to
edit its settings. **Manage** opens OBS's filter dialog to add or reorder filters.

Edits support undo. Locked items stay locked. Properties normally apply live;
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
