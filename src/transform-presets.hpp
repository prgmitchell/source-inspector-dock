// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <obs.hpp>
#include <QString>
#include <vector>

class QSettings;

namespace inspector {
struct TransformPreset {
	QString name;
	obs_transform_info info = {};
	obs_sceneitem_crop crop = {};
};
std::vector<TransformPreset> loadTransformPresets(QSettings &settings);
bool saveTransformPresets(QSettings &settings, const std::vector<TransformPreset> &presets);
} // namespace inspector
