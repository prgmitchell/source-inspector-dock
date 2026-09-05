// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Mitchell
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include "inspector-dock.hpp"
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {
constexpr const char *dockId = "source-inspector-dock";
QPointer<InspectorDock> dock;
bool exiting = false;
bool frontendRegistered = false;

void frontendEvent(obs_frontend_event event, void *)
{
	if (!dock)
		return;
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		exiting = true;
		dock->shutdown();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP ||
		   event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
		dock->suspend();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		dock->resume();
	} else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING || event == OBS_FRONTEND_EVENT_SCENE_CHANGED ||
		   event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
		   event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED ||
		   event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
		dock->refresh();
	}
}
} // namespace

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Source Inspector Dock";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Inspect and edit selected source properties, transforms, and filters in one dock.";
}

bool obs_module_load(void)
{
	return true;
}

void obs_module_post_load(void)
{
	if (!obs_frontend_get_main_window())
		return;
	dock = new InspectorDock;
	if (!obs_frontend_add_dock_by_id(dockId, obs_module_text("DockTitle"), dock)) {
		delete dock;
		obs_log(LOG_ERROR, "Could not register Source Inspector dock");
		return;
	}
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	frontendRegistered = true;
	obs_log(LOG_INFO, "Source Inspector Dock %s loaded", PLUGIN_VERSION);
}

void obs_module_unload(void)
{
	if (frontendRegistered) {
		obs_frontend_remove_event_callback(frontendEvent, nullptr);
		frontendRegistered = false;
	}
	if (dock) {
		dock->shutdown();
		if (!exiting)
			obs_frontend_remove_dock(dockId);
		else
			delete dock;
	}
}
