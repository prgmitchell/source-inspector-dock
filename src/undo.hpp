// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <obs.hpp>
#include <QString>
#include <functional>

namespace inspector {
void updateSource(obs_source_t *source, obs_data_t *before, obs_data_t *after);
void setFilterEnabled(obs_source_t *filter, bool enabled);
void editTransform(obs_scene_t *root, obs_sceneitem_t *item, const QString &label, const std::function<void()> &edit);
} // namespace inspector
