# OBS property editor

These files are from OBS Studio **32.2.1**, under GPL-2.0-or-later (see COPYING).
Source: https://github.com/obsproject/obs-studio/tree/32.2.1/shared

The editor and its Qt helper widgets are compiled privately because OBS does not
export OBSPropertiesView as a supported frontend API. This retains native handling
of dynamic properties, paths, fonts, colors, editable lists and frame rates.

Local adaptations initialize the deferred-update flag before the first queued
reload and guard scroll-position division when the viewport has zero size.
An optional callback context lets the wrapper manage edits while the native
editor retains the real OBS object for source-specific property buttons.
Embedded sizing and edit cancellation are handled by the plugin's wrapper.
The properties owner uses the older SDK's equivalent RAII wrapper, and a fallback
deprecation macro keeps the editor compatible with the template's OBS 31 SDK.
See the git diff when updating these files. Keep upstream copyright notices.
