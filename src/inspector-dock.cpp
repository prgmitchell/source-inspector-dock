// SPDX-License-Identifier: GPL-2.0-or-later
#include "inspector-dock.hpp"
#include "section.hpp"
#include "source-properties.hpp"
#include "transform-editor.hpp"
#include "undo.hpp"
#include <obs-frontend-api.h>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <algorithm>

namespace {
bool collectSelection(obs_scene_t *, obs_sceneitem_t *item, void *data)
{
	auto &selection = *static_cast<std::vector<OBSSceneItem> *>(data);
	if (obs_sceneitem_selected(item))
		selection.emplace_back(item);
	if (obs_sceneitem_is_group(item))
		obs_scene_enum_items(obs_sceneitem_group_get_scene(item), collectSelection, data);
	return true;
}
} // namespace

InspectorDock::InspectorDock(QWidget *parent) : QWidget(parent)
{
	setObjectName("SourceInspectorDock");
	setMinimumWidth(260);
	resize(360, 700);
	char *configPath = obs_current_module() ? obs_module_config_path("inspector.ini") : nullptr;
	if (configPath) {
		QDir().mkpath(QFileInfo(QString::fromUtf8(configPath)).absolutePath());
		preferences = std::make_unique<QSettings>(QString::fromUtf8(configPath), QSettings::IniFormat);
		bfree(configPath);
	}
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	title = new QLabel(text("DockTitle"));
	title->setTextFormat(Qt::PlainText);
	title->setWordWrap(true);
	auto font = title->font();
	font.setBold(true);
	font.setPointSizeF(font.pointSizeF() + 2);
	title->setFont(font);
	layout->addWidget(title);
	subtitle = new QLabel;
	subtitle->setTextFormat(Qt::PlainText);
	subtitle->setWordWrap(true);
	layout->addWidget(subtitle);
	empty = new QLabel(text("NoSelection"));
	empty->setWordWrap(true);
	empty->setAlignment(Qt::AlignCenter);
	layout->addWidget(empty, 1);
	scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	layout->addWidget(scroll, 1);
	scroll->hide();
	timer.setInterval(100);
	connect(&timer, &QTimer::timeout, this, &InspectorDock::refresh);
	timer.start();
}

InspectorDock::~InspectorDock()
{
	shutdown();
}

void InspectorDock::shutdown()
{
	stopped = true;
	suspend();
}

void InspectorDock::suspend()
{
	timer.stop();
	clear(true);
	scene = nullptr;
	previousSelection.clear();
}

void InspectorDock::resume()
{
	if (!stopped) {
		timer.start();
		refresh();
	}
}

void InspectorDock::clear(bool removed)
{
	if (removed) {
		if (properties)
			properties->cancel();
		for (auto &filter : filters)
			filter.editor->cancel();
	}
	// Destroy editors before releasing their source / scene-item references.
	delete scroll->takeWidget();
	filterSection = nullptr;
	properties = nullptr;
	transform = nullptr;
	filters.clear();
	selectedItem = nullptr;
	scroll->hide();
	empty->show();
	title->setText(text("DockTitle"));
	subtitle->clear();
}

void InspectorDock::refresh()
{
	if (stopped || !isVisible() || QApplication::activeModalWidget())
		return;
	OBSSourceAutoRelease current = obs_frontend_preview_program_mode_active()
					       ? obs_frontend_get_current_preview_scene()
					       : obs_frontend_get_current_scene();
	inspectScene(current);
}

void InspectorDock::inspectScene(obs_source_t *current)
{
	if (scene != current) {
		clear();
		previousSelection.clear();
		scene = current;
	}
	std::vector<OBSSceneItem> selection;
	if (auto *currentScene = obs_scene_from_source(current))
		obs_scene_enum_items(currentScene, collectSelection, &selection);
	OBSSceneItem target;
	// Prefer the newest selection; retain the current item when a multi-selection is unchanged.
	for (const auto &item : selection) {
		if (std::find(previousSelection.begin(), previousSelection.end(), item) == previousSelection.end())
			target = item;
	}
	if (!target && std::find(selection.begin(), selection.end(), selectedItem) != selection.end())
		target = selectedItem;
	if (!target && !selection.empty())
		target = selection.front();
	previousSelection = selection;
	if (target != selectedItem) {
		const bool removed = selectedItem && (!obs_sceneitem_get_scene(selectedItem) ||
						      obs_source_removed(obs_sceneitem_get_source(selectedItem)));
		clear(removed);
		selectedItem = target;
		if (selectedItem)
			populate();
	}
	if (!selectedItem)
		return;
	auto *source = obs_sceneitem_get_source(selectedItem);
	title->setText(QString::fromUtf8(obs_source_get_name(source)));
	const char *display = obs_source_get_display_name(obs_source_get_id(source));
	QString description = QString::fromUtf8(display ? display : obs_source_get_id(source));
	description +=
		QStringLiteral(" · %1 × %2").arg(obs_source_get_width(source)).arg(obs_source_get_height(source));
	if (selection.size() > 1)
		description += "\n" + text("MultipleSelected").arg(static_cast<qulonglong>(selection.size()));
	subtitle->setText(description);
	transform->synchronize();
	properties->synchronize();
	synchronizeFilters();
}

void InspectorDock::rememberSection(Section *section, const QString &key)
{
	if (!preferences)
		return;
	section->toggle->setChecked(preferences->value(key, true).toBool());
	connect(section->toggle, &QToolButton::toggled, this,
		[this, key](bool expanded) { preferences->setValue(key, expanded); });
}

void InspectorDock::populate()
{
	auto *source = obs_sceneitem_get_source(selectedItem);
	auto *contents = new QWidget;
	auto *sections = new QVBoxLayout(contents);
	sections->setContentsMargins(0, 0, 0, 0);
	sections->setSpacing(10);
	sections->setSizeConstraint(QLayout::SetMinAndMaxSize);
	sections->setAlignment(Qt::AlignTop);
	auto *propertySection = new Section(text("SourceProperties"));
	properties = new SourceProperties(source);
	propertySection->content->addWidget(properties);
	rememberSection(propertySection, "sections/properties");
	sections->addWidget(propertySection);
	auto *transformSection = new Section(text("Transform"));
	transform = new TransformEditor(obs_scene_from_source(scene), selectedItem);
	transformSection->content->addWidget(transform);
	rememberSection(transformSection, "sections/transform");
	sections->addWidget(transformSection);
	filterSection = new Section(text("Filters"));
	auto *manage = new QPushButton(text("ManageFilters"));
	manage->setToolTip(text("ManageFiltersTooltip"));
	filterSection->header->addWidget(manage);
	connect(manage, &QPushButton::clicked, this, [this] {
		if (selectedItem)
			obs_frontend_open_source_filters(obs_sceneitem_get_source(selectedItem));
	});
	rememberSection(filterSection, "sections/filters");
	sections->addWidget(filterSection);
	scroll->setWidget(contents);
	empty->hide();
	scroll->show();
}

void InspectorDock::synchronizeFilters()
{
	std::vector<OBSSource> current;
	obs_source_enum_filters(
		obs_sceneitem_get_source(selectedItem),
		[](obs_source_t *, obs_source_t *filter, void *data) {
			static_cast<std::vector<OBSSource> *>(data)->emplace_back(filter);
		},
		&current);
	bool changed = current.size() != filters.size();
	for (size_t i = 0; !changed && i < current.size(); ++i)
		changed = current[i] != filters[i].source;
	if (changed) {
		for (auto &filter : filters) {
			if (std::find(current.begin(), current.end(), filter.source) == current.end())
				filter.editor->cancel();
		}
		while (auto *entry = filterSection->content->takeAt(0)) {
			delete entry->widget();
			delete entry;
		}
		filters.clear();
		for (const auto &source : current) {
			auto *section = new Section(QString::fromUtf8(obs_source_get_name(source)));
			auto *enabled = new QCheckBox(text("Enabled"));
			enabled->setChecked(obs_source_enabled(source));
			section->header->addWidget(enabled);
			auto *editor = new SourceProperties(source);
			section->content->addWidget(editor);
			rememberSection(section, "filters/" + QString::fromUtf8(obs_source_get_uuid(source)));
			connect(enabled, &QCheckBox::clicked, section,
				[source](bool checked) { inspector::setFilterEnabled(source, checked); });
			filterSection->content->addWidget(section);
			filters.push_back({source, section, enabled, editor});
		}
	}
	// Initial empty stack also needs a useful empty state.
	if (current.empty() && filterSection->content->count() == 0) {
		auto *label = new QLabel(text("NoFilters"));
		label->setWordWrap(true);
		filterSection->content->addWidget(label);
	}
	filterSection->toggle->setText(text("Filters") + QStringLiteral(" (%1)").arg(filters.size()));
	for (auto &filter : filters) {
		filter.section->toggle->setText(QString::fromUtf8(obs_source_get_name(filter.source)));
		const QSignalBlocker blocker(filter.enabled);
		filter.enabled->setChecked(obs_source_enabled(filter.source));
		filter.editor->synchronize();
	}
}
