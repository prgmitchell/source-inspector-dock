// SPDX-License-Identifier: GPL-2.0-or-later
#include "scene-utils.hpp"
#include <unordered_set>

namespace {
struct Search {
	obs_sceneitem_t *item = nullptr;
	obs_source_t *source = nullptr;
	std::unordered_set<obs_scene_t *> visited;
	bool found = false;
};

void visit(obs_scene_t *scene, Search &search)
{
	if (!scene || !search.visited.insert(scene).second)
		return;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *data) {
			auto &search = *static_cast<Search *>(data);
			auto *source = obs_sceneitem_get_source(item);
			if (item == search.item || source == search.source) {
				search.found = true;
				return false;
			}
			if (obs_sceneitem_is_group(item))
				visit(obs_sceneitem_group_get_scene(item), search);
			else if (search.source)
				visit(obs_scene_from_source(source), search);
			return !search.found;
		},
		&search);
}
} // namespace

bool inspector::sceneContainsItem(obs_scene_t *scene, obs_sceneitem_t *item)
{
	if (!item)
		return false;
	Search search;
	search.item = item;
	visit(scene, search);
	return search.found;
}

bool inspector::sceneUsesSource(obs_scene_t *scene, obs_source_t *source)
{
	if (!source)
		return false;
	Search search;
	search.source = source;
	visit(scene, search);
	return search.found;
}
