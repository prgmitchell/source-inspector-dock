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

- **Windows:** run the installer, or extract the ZIP into `C:\ProgramData\obs-studio\plugins`. Signed by MITCHELL SOFTWARE SOLUTIONS LLC.
- **macOS:** run the universal `.pkg`. Currently unsigned and not notarized.
- **Ubuntu 24.04:** run `sudo apt install ./source-inspector-dock-*.deb`. Requires native OBS; Flatpak is not supported.

GPL-2.0-or-later. Uses [OBS Studio's property editor](https://github.com/obsproject/obs-studio/tree/32.2.1/shared/properties-view), downloaded during the build.
