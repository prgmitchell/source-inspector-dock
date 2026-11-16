// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <obs.hpp>

namespace inspector {
bool sceneContainsItem(obs_scene_t *scene, obs_sceneitem_t *item);
bool sceneUsesSource(obs_scene_t *scene, obs_source_t *source);
} // namespace inspector
