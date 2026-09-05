// SPDX-License-Identifier: GPL-2.0-or-later
// Real libobs objects and native Qt controls; only OBS's application shell is stubbed.
#include "inspector-dock.hpp"
#include "source-properties.hpp"
#include "transform-editor.hpp"
#include "section.hpp"
#include <properties-view.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/text-lookup.h>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QLibrary>
#include <QPushButton>
#include <QSpinBox>
#include <QStyleFactory>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

static lookup_t *locale;
static obs_source_t *currentScene;
static obs_source_t *buttonSource;
struct UndoAction {
	undo_redo_cb undo;
	undo_redo_cb redo;
	std::string before;
	std::string after;
};
static std::vector<UndoAction> actions;

extern "C" {
const char *obs_module_text(const char *key)
{
	const char *value = key;
	if (locale)
		text_lookup_getstr(locale, key, &value);
	return value;
}
obs_module_t *obs_current_module(void)
{
	return nullptr;
}
void obs_frontend_save(void) {}
bool obs_frontend_is_theme_dark(void)
{
	return true;
}
bool obs_frontend_preview_program_mode_active(void)
{
	return false;
}
obs_source_t *obs_frontend_get_current_scene(void)
{
	return obs_source_get_ref(currentScene);
}
obs_source_t *obs_frontend_get_current_preview_scene(void)
{
	return obs_source_get_ref(currentScene);
}
void obs_frontend_open_source_filters(obs_source_t *) {}
void obs_frontend_add_undo_redo_action(const char *, undo_redo_cb undo, undo_redo_cb redo, const char *before,
				       const char *after, bool)
{
	actions.push_back({undo, redo, before, after});
}
}

static void check(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
	std::printf("PASS: %s\n", message);
}

static void events(int milliseconds = 20)
{
	QEventLoop loop;
	QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
	loop.exec();
}

static obs_properties_t *testProperties(void *data)
{
	auto *props = obs_properties_create();
	if (obs_source_get_unversioned_id(static_cast<obs_source_t *>(data)) == std::string("inspector_deferred"))
		obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
	obs_properties_add_text(props, "caption", "Caption", OBS_TEXT_DEFAULT);
	obs_properties_add_int_slider(props, "amount", "Amount", 0, 100, 1);
	auto *advanced = obs_properties_add_bool(props, "advanced", "Advanced");
	obs_property_set_modified_callback(advanced,
					   [](obs_properties_t *properties, obs_property_t *, obs_data_t *settings) {
						   obs_property_set_visible(obs_properties_get(properties, "gain"),
									    obs_data_get_bool(settings, "advanced"));
						   return true;
					   });
	obs_properties_add_float_slider(props, "gain", "Gain", -10, 10, 0.1);
	obs_properties_add_color_alpha(props, "color", "Tint");
	obs_properties_add_button2(
		props, "test_button", "Source action",
		[](obs_properties_t *, obs_property_t *, void *obj) {
			buttonSource = static_cast<obs_source_t *>(obj);
			return false;
		},
		nullptr);
	return props;
}

static void registerTestSource(const char *id, obs_source_type type)
{
	obs_source_info info = {};
	info.id = id;
	info.type = type;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = [](void *) {
		return "Inspector test source";
	};
	info.create = [](obs_data_t *, obs_source_t *source) -> void * {
		return source;
	};
	info.destroy = [](void *) {
	};
	info.get_width = [](void *) -> uint32_t {
		return 1920;
	};
	info.get_height = [](void *) -> uint32_t {
		return 1080;
	};
	info.get_defaults = [](obs_data_t *settings) {
		obs_data_set_default_string(settings, "caption", "Studio camera");
		obs_data_set_default_int(settings, "amount", 50);
		obs_data_set_default_bool(settings, "advanced", true);
		obs_data_set_default_int(settings, "color", 0xffb8926a);
	};
	info.get_properties = testProperties;
	obs_register_source(&info);
}

static QSpinBox *amountControl(SourceProperties *editor)
{
	return editor->findChild<QSpinBox *>();
}

static void runTests(const char *screenshot)
{
	// No video renderer is needed for these controls. Avoid normalized coordinates
	// dividing by an uninitialized canvas resolution in this headless harness.
	OBSDataAutoRelease privateSettings = obs_get_private_data();
	obs_data_set_bool(privateSettings, "AbsoluteCoordinates", true);
	registerTestSource("inspector_input", OBS_SOURCE_TYPE_INPUT);
	registerTestSource("inspector_filter", OBS_SOURCE_TYPE_FILTER);
	registerTestSource("inspector_deferred", OBS_SOURCE_TYPE_INPUT);
	OBSSceneAutoRelease scene = obs_scene_create("Inspector integration scene");
	currentScene = obs_scene_get_source(scene);
	OBSSourceAutoRelease source = obs_source_create("inspector_input", "Studio camera", nullptr, nullptr);
	OBSSourceAutoRelease other = obs_source_create("inspector_input", "Second camera", nullptr, nullptr);
	OBSSourceAutoRelease filter = obs_source_create("inspector_filter", "Color correction", nullptr, nullptr);
	obs_source_filter_add(source, filter);
	auto *item = obs_scene_add(scene, source);
	auto *otherItem = obs_scene_add(scene, other);
	InspectorDock dock;
	dock.setAttribute(Qt::WA_DontShowOnScreen);
	dock.resize(390, 1100);
	dock.show();
	dock.inspectScene(currentScene);
	check(!dock.inspectedItem(), "empty selection stays empty");
	obs_sceneitem_select(item, true);
	dock.inspectScene(currentScene);
	events();
	check(dock.inspectedItem() == item, "source selection populates dock");
	check(dock.findChildren<OBSPropertiesView *>().size() == 2, "source and filter use native property editors");
	auto editors = dock.findChildren<SourceProperties *>();
	check(amountControl(editors[0]) != nullptr, "integer property rendered");
	for (auto *button : editors[0]->findChildren<QPushButton *>()) {
		if (button->text() == "Source action") {
			button->click();
			break;
		}
	}
	check(buttonSource == source.Get(), "property button receives the real source context");
	auto *amount = amountControl(editors[0]);
	amount->setValue(72);
	events(650);
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	check(obs_data_get_int(settings, "amount") == 72, "property edit updates real source");
	check(!actions.empty(), "property edit creates undo action");
	auto propertyUndo = actions.back();
	propertyUndo.undo(propertyUndo.before.c_str());
	check(obs_data_get_int(settings, "amount") == 50, "property undo restores previous value");
	propertyUndo.redo(propertyUndo.after.c_str());
	check(obs_data_get_int(settings, "amount") == 72, "property redo restores edited value");
	amountControl(editors[1])->setValue(34);
	events(650);
	OBSDataAutoRelease filterSettings = obs_source_get_settings(filter);
	check(obs_data_get_int(filterSettings, "amount") == 34, "filter property updates");
	auto filterUndo = actions.back();
	filterUndo.undo(filterUndo.before.c_str());
	check(obs_data_get_int(filterSettings, "amount") == 50, "filter property undo resolves filter UUID");

	auto *position = dock.findChild<QDoubleSpinBox *>("PositionX");
	position->setValue(321.5);
	obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);
	check(info.pos.x == 321.5f, "transform edit updates selected scene item");
	auto transformUndo = actions.back();
	transformUndo.undo(transformUndo.before.c_str());
	obs_sceneitem_get_info2(item, &info);
	check(info.pos.x == 0.0f, "transform undo restores scene item");
	transformUndo.redo(transformUndo.after.c_str());
	obs_sceneitem_get_info2(item, &info);
	check(info.pos.x == 321.5f, "transform redo restores scene item");
	obs_sceneitem_set_locked(item, true);
	dock.inspectScene(currentScene);
	check(!position->isEnabled(), "locked transform controls are disabled");
	obs_sceneitem_set_locked(item, false);

	obs_sceneitem_select(otherItem, true);
	dock.inspectScene(currentScene);
	check(dock.inspectedItem() == otherItem, "newest multi-selection becomes inspected item");
	obs_sceneitem_select(otherItem, false);
	dock.inspectScene(currentScene);
	check(dock.inspectedItem() == item, "remaining selected item is restored");
	events();
	obs_source_filter_remove(source, filter);
	dock.inspectScene(currentScene);
	events();
	check(dock.findChildren<OBSPropertiesView *>().size() == 1, "removed filter editor is destroyed safely");
	obs_source_filter_add(source, filter);
	dock.inspectScene(currentScene);
	events();
	check(dock.findChildren<OBSPropertiesView *>().size() == 2, "new filter appears automatically");
	for (auto *checkbox : dock.findChildren<QCheckBox *>()) {
		if (checkbox->text() == text("Enabled")) {
			checkbox->click();
			check(!obs_source_enabled(filter), "filter toggle updates enabled state");
			auto enabledUndo = actions.back();
			enabledUndo.undo(enabledUndo.before.c_str());
			check(obs_source_enabled(filter), "filter enabled state supports undo");
			break;
		}
	}

	// Exercise dynamically hidden controls and the editor's queued rebuild.
	for (auto *checkbox : dock.findChildren<QCheckBox *>()) {
		if (checkbox->text() == "Advanced") {
			checkbox->click();
			break;
		}
	}
	events(650);
	check(!obs_data_get_bool(settings, "advanced"), "dynamic property callback applies setting");
	OBSDataAutoRelease external = obs_data_create();
	obs_data_set_int(external, "amount", 23);
	obs_source_update(source, external);
	obs_source_update_properties(source);
	dock.setFocus();
	events();
	dock.inspectScene(currentScene);
	events();
	editors = dock.findChildren<SourceProperties *>();
	check(amountControl(editors[0])->value() == 23, "external source settings refresh in dock");

	// Snapshot the real dock widgets for visual review.
	if (screenshot)
		check(dock.grab().save(QString::fromUtf8(screenshot)), "dock preview screenshot saved");

	obs_sceneitem_t *group = obs_scene_add_group(scene, "Camera group");
	obs_sceneitem_select(item, false);
	auto *groupScene = obs_sceneitem_group_get_scene(group);
	auto *nested = obs_scene_add(groupScene, source);
	obs_sceneitem_select(nested, true);
	dock.inspectScene(currentScene);
	events();
	check(dock.inspectedItem() == nested, "selection inside group is inspected");
	position = dock.findChild<QDoubleSpinBox *>("PositionX");
	position->setValue(77);
	auto groupUndo = actions.back();
	groupUndo.undo(groupUndo.before.c_str());
	obs_sceneitem_get_info2(nested, &info);
	check(info.pos.x == 0.0f, "group child transform undo works");
	obs_sceneitem_remove(nested);
	dock.inspectScene(currentScene);
	events();
	check(!dock.inspectedItem(), "deleting selected group child clears dock safely");

	OBSSourceAutoRelease deferred = obs_source_create("inspector_deferred", "Deferred input", nullptr, nullptr);
	auto *deferredItem = obs_scene_add(scene, deferred);
	obs_sceneitem_select(deferredItem, true);
	dock.inspectScene(currentScene);
	events();
	editors = dock.findChildren<SourceProperties *>();
	amountControl(editors[0])->setValue(90);
	events(650);
	OBSDataAutoRelease deferredSettings = obs_source_get_settings(deferred);
	check(obs_data_get_int(deferredSettings, "amount") == 50, "deferred input waits for Apply");
	editors[0]->flush();
	check(obs_data_get_int(deferredSettings, "amount") == 90, "Apply commits deferred input");
	amountControl(editors[0])->setValue(12);
	obs_sceneitem_select(deferredItem, false);
	dock.inspectScene(currentScene);
	events(650);
	check(obs_data_get_int(deferredSettings, "amount") == 90, "leaving deferred editor discards unapplied changes");

	obs_sceneitem_select(item, true);
	dock.inspectScene(currentScene);
	events();
	editors = dock.findChildren<SourceProperties *>();
	amountControl(editors[0])->setValue(66);
	obs_sceneitem_remove(item);
	dock.inspectScene(currentScene);
	events(650);
	check(!dock.inspectedItem(), "pending edit and source removal do not crash");
	dock.suspend();
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *sceneItem, void *) {
			obs_sceneitem_remove(sceneItem);
			return true;
		},
		nullptr);
	obs_source_remove(currentScene);
	currentScene = nullptr;
	actions.clear();
}

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
		std::fprintf(stderr, "Qt: %s\n", message.toUtf8().constData());
	});
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	QPalette palette;
	palette.setColor(QPalette::Window, QColor("#24252a"));
	palette.setColor(QPalette::WindowText, QColor("#e8e8ea"));
	palette.setColor(QPalette::Base, QColor("#17181c"));
	palette.setColor(QPalette::Text, QColor("#e8e8ea"));
	palette.setColor(QPalette::Button, QColor("#35363d"));
	palette.setColor(QPalette::ButtonText, QColor("#e8e8ea"));
	palette.setColor(QPalette::Highlight, QColor("#5b8ee8"));
	app.setPalette(palette);
	locale = text_lookup_create(argc > 1 ? argv[1] : "");
	if (!obs_startup("en-US", nullptr, nullptr))
		return 2;
	int result = 0;
	QLibrary frontend;
	try {
		if (argc > 4) {
			frontend.setFileName(QString::fromUtf8(argv[5]));
			check(frontend.load(), "OBS frontend runtime loads");
			obs_module_t *module = nullptr;
			check(obs_open_module(&module, argv[3], argv[4]) == MODULE_SUCCESS,
			      "built plugin DLL loads in libobs");
			check(module && obs_init_module(module), "built plugin module initializes");
		}
		runTests(argc > 2 ? argv[2] : nullptr);
	} catch (const std::exception &error) {
		std::fprintf(stderr, "FAIL: %s\n", error.what());
		result = 1;
	}
	currentScene = nullptr;
	obs_wait_for_destroy_queue();
	obs_shutdown();
	text_lookup_destroy(locale);
	return result;
}
