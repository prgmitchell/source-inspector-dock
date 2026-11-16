// SPDX-License-Identifier: GPL-2.0-or-later
#include "inspector-dock.hpp"
#include "section.hpp"
#include "source-properties.hpp"
#include "transform-editor.hpp"
#include "undo.hpp"
#include "filter-stack.hpp"
#include "scene-utils.hpp"
#include <obs-frontend-api.h>
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QMessageBox>
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
	title->setObjectName("InspectedSourceName");
	title->setTextFormat(Qt::PlainText);
	title->setWordWrap(true);
	auto font = title->font();
	font.setBold(true);
	font.setPointSizeF(font.pointSizeF() + 2);
	title->setFont(font);
	auto *heading = new QHBoxLayout;
	heading->addWidget(title, 1);
	pin = new QToolButton;
	pin->setObjectName("PinSource");
	pin->setText(text("PinSource"));
	pin->setToolTip(text("PinSourceTooltip"));
	pin->setCheckable(true);
	pin->setEnabled(false);
	heading->addWidget(pin);
	layout->addLayout(heading);
	connect(pin, &QToolButton::toggled, this, [this](bool pinned) {
		pin->setText(text(pinned ? "UnpinSource" : "PinSource"));
		refresh();
	});
	subtitle = new QLabel;
	subtitle->setTextFormat(Qt::PlainText);
	subtitle->setWordWrap(true);
	layout->addWidget(subtitle);
	usage = new QToolButton;
	usage->setObjectName("SourceUsage");
	usage->setToolTip(text("SourceUsageTooltip"));
	usage->hide();
	layout->addWidget(usage, 0, Qt::AlignLeft);
	connect(usage, &QToolButton::clicked, this, [this] { showUsage(); });
	searchInput = new QLineEdit;
	searchInput->setObjectName("SearchSettings");
	searchInput->setPlaceholderText(text("SearchSettings"));
	searchInput->setAccessibleName(text("SearchSettings"));
	searchInput->setClearButtonEnabled(true);
	searchInput->setEnabled(false);
	layout->addWidget(searchInput);
	connect(searchInput, &QLineEdit::textChanged, this, [this] { applySearch(); });
	noMatches = new QLabel(text("NoSearchResults"));
	noMatches->setObjectName("NoSearchResults");
	noMatches->setWordWrap(true);
	noMatches->hide();
	layout->addWidget(noMatches);
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
		if (filterStack)
			filterStack->cancel();
	}
	// Destroy editors before releasing their source / scene-item references.
	delete scroll->takeWidget();
	filterSection = nullptr;
	filterStack = nullptr;
	propertySection = nullptr;
	transformSection = nullptr;
	properties = nullptr;
	transform = nullptr;
	selectedItem = nullptr;
	scroll->hide();
	empty->show();
	title->setText(text("DockTitle"));
	subtitle->clear();
	const QSignalBlocker blocker(pin);
	pin->setChecked(false);
	pin->setText(text("PinSource"));
	pin->setEnabled(false);
	searchInput->setEnabled(false);
	noMatches->hide();
	usage->hide();
	usageScenes.clear();
	usageRefresh.invalidate();
}

void InspectorDock::refresh()
{
	if (stopped || !isVisible() || QApplication::activeModalWidget() || QApplication::activePopupWidget())
		return;
	if (selectedItem &&
	    (!scene || obs_source_removed(scene) || obs_source_removed(obs_sceneitem_get_source(selectedItem)) ||
	     !inspector::sceneContainsItem(obs_scene_from_source(scene), selectedItem))) {
		clear(true);
		previousSelection.clear();
	}
	OBSSourceAutoRelease current = obs_frontend_preview_program_mode_active()
					       ? obs_frontend_get_current_preview_scene()
					       : obs_frontend_get_current_scene();
	if (!pin->isChecked())
		inspectScene(current);
	if (!selectedItem)
		return;
	auto *source = obs_sceneitem_get_source(selectedItem);
	title->setText(QString::fromUtf8(obs_source_get_name(source)));
	const char *display = obs_source_get_display_name(obs_source_get_id(source));
	QString description = QString::fromUtf8(display ? display : obs_source_get_id(source));
	description +=
		QStringLiteral(" · %1 × %2").arg(obs_source_get_width(source)).arg(obs_source_get_height(source));
	if (pin->isChecked())
		description += "\n" + text("PinnedScene").arg(QString::fromUtf8(obs_source_get_name(scene)));
	else if (previousSelection.size() > 1)
		description += "\n" + text("MultipleSelected").arg(static_cast<qulonglong>(previousSelection.size()));
	subtitle->setText(description);
	transform->synchronize();
	properties->synchronize();
	filterStack->synchronize();
	filterSection->toggle->setText(text("Filters") + QStringLiteral(" (%1)").arg(filterStack->count()));
	applySearch();
	if (!usageRefresh.isValid() || usageRefresh.elapsed() >= 1000)
		updateUsage();
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
	propertySection = new Section(text("SourceProperties"));
	properties = new SourceProperties(source);
	connect(
		properties, &SourceProperties::searchResultsChanged, this, [this] { applySearch(); },
		Qt::QueuedConnection);
	propertySection->content->addWidget(properties);
	rememberSection(propertySection, "sections/properties");
	sections->addWidget(propertySection);
	transformSection = new Section(text("Transform"));
	transform = new TransformEditor(obs_scene_from_source(scene), selectedItem, preferences.get());
	transformSection->content->addWidget(transform);
	rememberSection(transformSection, "sections/transform");
	sections->addWidget(transformSection);
	filterSection = new Section(text("Filters"));
	filterStack = new FilterStack(source, preferences.get());
	filterSection->content->addWidget(filterStack);
	connect(filterStack, &FilterStack::searchResultsChanged, this, [this] { applySearch(); }, Qt::QueuedConnection);
	auto *add = new QPushButton(text("AddFilter"));
	add->setObjectName("AddFilter");
	filterSection->header->addWidget(add);
	connect(add, &QPushButton::clicked, filterStack, &FilterStack::addFilter);
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
	pin->setEnabled(true);
	searchInput->setEnabled(true);
}

void InspectorDock::applySearch()
{
	if (!properties || !transform || !filterStack)
		return;
	const QString query = searchInput->text().trimmed();
	const bool active = !query.isEmpty();
	const bool propertyMatch = properties->setSearchQuery(
		text("SourceProperties").contains(query, Qt::CaseInsensitive) ? QString() : query);
	const bool transformMatch =
		transform->setSearchQuery(text("Transform").contains(query, Qt::CaseInsensitive) ? QString() : query);
	const bool filterMatch =
		filterStack->setSearchQuery(text("Filters").contains(query, Qt::CaseInsensitive) ? QString() : query);
	propertySection->setSearchState(active, propertyMatch);
	transformSection->setSearchState(active, transformMatch);
	filterSection->setSearchState(active, filterMatch);
	noMatches->setVisible(active && !propertyMatch && !transformMatch && !filterMatch);
}

void InspectorDock::updateUsage()
{
	usageScenes.clear();
	if (!selectedItem)
		return;
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; ++i) {
		auto *source = scenes.sources.array[i];
		if (inspector::sceneUsesSource(obs_scene_from_source(source), obs_sceneitem_get_source(selectedItem)))
			usageScenes.push_back(QString::fromUtf8(obs_source_get_name(source)));
	}
	obs_frontend_source_list_free(&scenes);
	usageScenes.sort(Qt::CaseInsensitive);
	usage->setText(usageScenes.size() == 1 ? text("UsedInOneScene") : text("UsedInScenes").arg(usageScenes.size()));
	usage->show();
	usageRefresh.restart();
}

void InspectorDock::showUsage()
{
	if (!selectedItem)
		return;
	updateUsage();
	QMessageBox message(QMessageBox::Information, text("SourceUsage"),
			    text("SourceUsageDetails").arg(usageScenes.join("\n")), QMessageBox::Ok, this);
	message.setTextFormat(Qt::PlainText);
	message.exec();
}
