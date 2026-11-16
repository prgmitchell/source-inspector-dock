// SPDX-License-Identifier: GPL-2.0-or-later
#include "undo.hpp"
#include "section.hpp"
#include <obs-frontend-api.h>
#include <algorithm>
#include <vector>

namespace {
OBSDataAutoRelease sourceState(obs_source_t *source, obs_data_t *settings)
{
	OBSDataAutoRelease state = obs_data_create();
	obs_data_set_string(state, "uuid", obs_source_get_uuid(source));
	// Toggle-only undo records omit settings; older OBS cannot serialize a null object.
	if (settings)
		obs_data_set_obj(state, "settings", settings);
	obs_data_set_bool(state, "enabled", obs_source_enabled(source));
	return state;
}

void restoreSource(const char *json)
{
	OBSDataAutoRelease state = obs_data_create_from_json(json);
	OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(state, "uuid"));
	if (!source || obs_source_removed(source))
		return;
	OBSDataAutoRelease settings = obs_data_get_obj(state, "settings");
	if (settings) {
		obs_source_reset_settings(source, settings);
		obs_source_update_properties(source);
	} else if (obs_data_has_user_value(state, "name")) {
		obs_source_set_name(source, obs_data_get_string(state, "name"));
	} else {
		obs_source_set_enabled(source, obs_data_get_bool(state, "enabled"));
	}
	obs_frontend_save();
}

void addUndo(obs_source_t *source, obs_data_t *before, obs_data_t *after, const QString &label, undo_redo_cb restore)
{
	const QByteArray oldJson(obs_data_get_json(before));
	const QByteArray newJson(obs_data_get_json(after));
	if (oldJson == newJson)
		return;
	const auto name = (label + " — " + QString::fromUtf8(obs_source_get_name(source))).toUtf8();
	obs_frontend_add_undo_redo_action(name.constData(), restore, restore, oldJson.constData(), newJson.constData(),
					  false);
	obs_frontend_save();
}

void restoreTransform(const char *json)
{
	obs_scene_load_transform_states(json);
	obs_frontend_save();
}

OBSDataAutoRelease filterState(obs_source_t *source)
{
	OBSDataAutoRelease state = obs_data_create();
	obs_data_set_string(state, "uuid", obs_source_get_uuid(source));
	OBSDataArrayAutoRelease filters = obs_source_backup_filters(source);
	obs_data_set_array(state, "filters", filters);
	return state;
}

void restoreFilters(const char *json)
{
	OBSDataAutoRelease state = obs_data_create_from_json(json);
	OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(state, "uuid"));
	if (!source || obs_source_removed(source))
		return;
	OBSDataArrayAutoRelease filters = obs_data_get_array(state, "filters");
	if (!filters)
		return;
	std::vector<OBSSource> current;
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *data) {
			static_cast<std::vector<OBSSource> *>(data)->emplace_back(filter);
		},
		&current);
	std::vector<OBSSource> restored;
	for (size_t i = 0; i < obs_data_array_count(filters); ++i) {
		OBSDataAutoRelease saved = obs_data_array_item(filters, i);
		// Editors or another plugin may still hold a removed filter. Reuse its UUID instead
		// of creating a duplicate, so earlier property/rename undo actions keep working.
		OBSSourceAutoRelease filter = obs_get_source_by_uuid(obs_data_get_string(saved, "uuid"));
		if (filter) {
			if (obs_source_get_type(filter) != OBS_SOURCE_TYPE_FILTER ||
			    (obs_filter_get_parent(filter) && obs_filter_get_parent(filter) != source.Get()))
				return;
		} else {
			filter = obs_load_source(saved);
		}
		if (!filter)
			return;
		if (obs_source_filter_get_index(source, filter) < 0) {
			OBSDataAutoRelease settings = obs_data_get_obj(saved, "settings");
			if (settings)
				obs_source_reset_settings(filter, settings);
			obs_source_set_enabled(filter, obs_data_get_bool(saved, "enabled"));
		}
		obs_source_set_name(filter, obs_data_get_string(saved, "name"));
		restored.emplace_back(filter.Get());
	}
	for (const auto &filter : current) {
		if (std::find(restored.begin(), restored.end(), filter) == restored.end())
			obs_source_filter_remove(source, filter);
	}
	for (const auto &filter : restored) {
		if (obs_source_filter_get_index(source, filter) < 0)
			obs_source_filter_add(source, filter);
	}
	for (size_t i = 0; i < restored.size(); ++i)
		obs_source_filter_set_index(source, restored[i], i);
	obs_frontend_save();
}
} // namespace

void inspector::updateSource(obs_source_t *source, obs_data_t *before, obs_data_t *after)
{
	if (!source || obs_source_removed(source))
		return;
	auto oldState = sourceState(source, before);
	auto newState = sourceState(source, after);
	obs_source_update(source, after);
	addUndo(source, oldState, newState, text("Undo.Properties"), restoreSource);
}

void inspector::setFilterEnabled(obs_source_t *filter, bool enabled)
{
	auto oldState = sourceState(filter, nullptr);
	obs_source_set_enabled(filter, enabled);
	auto newState = sourceState(filter, nullptr);
	addUndo(filter, oldState, newState, text("Undo.Filter"), restoreSource);
}

void inspector::renameFilter(obs_source_t *filter, const QString &name)
{
	if (!filter || obs_source_removed(filter))
		return;
	auto before = sourceState(filter, nullptr);
	obs_data_set_string(before, "name", obs_source_get_name(filter));
	obs_source_set_name(filter, name.toUtf8().constData());
	auto after = sourceState(filter, nullptr);
	obs_data_set_string(after, "name", obs_source_get_name(filter));
	addUndo(filter, before, after, text("RenameFilter"), restoreSource);
}

void inspector::editFilters(obs_source_t *source, const QString &label, const std::function<void()> &edit)
{
	if (!source || obs_source_removed(source))
		return;
	auto before = filterState(source);
	edit();
	auto after = filterState(source);
	addUndo(source, before, after, label, restoreFilters);
}

void inspector::editTransform(obs_scene_t *root, obs_sceneitem_t *item, const QString &label,
			      const std::function<void()> &edit)
{
	auto *scene = obs_sceneitem_get_scene(item);
	if (!scene || obs_sceneitem_locked(item))
		return;
	OBSDataAutoRelease before = obs_scene_save_transform_states(root, true);
	obs_sceneitem_defer_update_begin(item);
	edit();
	obs_sceneitem_defer_update_end(item);
	OBSDataAutoRelease after = obs_scene_save_transform_states(root, true);
	addUndo(obs_sceneitem_get_source(item), before, after, label, restoreTransform);
}
