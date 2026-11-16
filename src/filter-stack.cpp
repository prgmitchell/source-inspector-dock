// SPDX-License-Identifier: GPL-2.0-or-later
#include "filter-stack.hpp"
#include "section.hpp"
#include "source-properties.hpp"
#include "undo.hpp"
#include <QCheckBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <algorithm>

namespace {
size_t filterGroup(uint32_t flags)
{
	return (flags & (OBS_SOURCE_AUDIO | OBS_SOURCE_ASYNC)) ? 0 : 1;
}
} // namespace

bool inspector::filterCompatible(uint32_t sourceFlags, uint32_t filterFlags)
{
	if (filterFlags & (OBS_SOURCE_DEPRECATED | OBS_SOURCE_CAP_DISABLED))
		return false;
	const bool video = (sourceFlags & OBS_SOURCE_VIDEO) != 0;
	const bool audio = (sourceFlags & OBS_SOURCE_AUDIO) != 0;
	const bool async = (sourceFlags & OBS_SOURCE_ASYNC) != 0;
	if ((filterFlags & OBS_SOURCE_VIDEO) && !video)
		return false;
	if ((filterFlags & OBS_SOURCE_AUDIO) && !audio)
		return false;
	if ((filterFlags & OBS_SOURCE_ASYNC) && !(filterFlags & OBS_SOURCE_AUDIO) && !async)
		return false;
	return (filterFlags & (OBS_SOURCE_AUDIO | OBS_SOURCE_ASYNC)) ? (audio || async) : video;
}

FilterStack::FilterStack(obs_source_t *source_, QSettings *preferences_, QWidget *parent)
	: QWidget(parent),
	  source(source_),
	  preferences(preferences_)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	empty = new QLabel(text("NoFilters"));
	empty->setWordWrap(true);
	layout->addWidget(empty);
	const char *titles[] = {"AudioVideoFilters", "EffectFilters"};
	for (size_t i = 0; i < groups.size(); ++i) {
		auto &group = groups[i];
		group.widget = new QWidget;
		group.layout = new QVBoxLayout(group.widget);
		group.layout->setContentsMargins(0, 0, 0, 0);
		auto *heading = new QLabel(text(titles[i]));
		auto font = heading->font();
		font.setBold(true);
		heading->setFont(font);
		group.layout->addWidget(heading);
		layout->addWidget(group.widget);
	}
	synchronize();
}

void FilterStack::cancel()
{
	cancelled = true;
	for (auto &filter : filters)
		filter.editor->cancel();
	setEnabled(false);
}

void FilterStack::finishEdits()
{
	for (auto &filter : filters)
		filter.editor->finishPendingEdit();
}

void FilterStack::synchronize()
{
	if (cancelled)
		return;
	std::vector<OBSSource> current;
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *data) {
			static_cast<std::vector<OBSSource> *>(data)->emplace_back(filter);
		},
		&current);
	// Retain existing editors when adding or reordering so deferred edits are not lost.
	for (auto it = filters.begin(); it != filters.end();) {
		if (std::find(current.begin(), current.end(), it->source) == current.end()) {
			it->editor->cancel();
			delete it->section;
			it = filters.erase(it);
		} else {
			++it;
		}
	}
	for (const auto &filter : current) {
		if (std::any_of(filters.begin(), filters.end(),
				[&](const Filter &entry) { return entry.source == filter; }))
			continue;
		auto *section = new Section(QString::fromUtf8(obs_source_get_name(filter)));
		auto *enabled = new QCheckBox(text("Enabled"));
		section->header->addWidget(enabled);
		auto *up = new QToolButton;
		up->setArrowType(Qt::UpArrow);
		up->setToolTip(text("MoveFilterUp"));
		up->setAccessibleName(text("MoveFilterUp"));
		auto *down = new QToolButton;
		down->setArrowType(Qt::DownArrow);
		down->setToolTip(text("MoveFilterDown"));
		down->setAccessibleName(text("MoveFilterDown"));
		section->header->addWidget(up);
		section->header->addWidget(down);
		auto *actions = new QToolButton;
		actions->setText(QString::fromUtf8("⋯"));
		actions->setAccessibleName(text("FilterActions"));
		actions->setToolTip(text("FilterActions"));
		actions->setPopupMode(QToolButton::InstantPopup);
		// The ellipsis already identifies the menu; a second indicator overlaps it.
		actions->setStyleSheet("QToolButton::menu-indicator { image: none; }");
		auto *menu = new QMenu(actions);
		menu->addAction(text("RenameFilter"), this, [this, filter] { rename(filter); });
		menu->addAction(text("RemoveFilter"), this, [this, filter] { remove(filter); });
		actions->setMenu(menu);
		section->header->addWidget(actions);
		auto *editor = new SourceProperties(filter);
		section->content->addWidget(editor);
		connect(editor, &SourceProperties::searchResultsChanged, this, &FilterStack::searchResultsChanged);
		if (preferences) {
			const QString key = "filters/" + QString::fromUtf8(obs_source_get_uuid(filter));
			section->toggle->setChecked(preferences->value(key, true).toBool());
			connect(section->toggle, &QToolButton::toggled, this,
				[this, key](bool expanded) { preferences->setValue(key, expanded); });
		}
		connect(enabled, &QCheckBox::clicked, section, [this, filter](bool checked) {
			if (obs_source_filter_get_index(source, filter) >= 0) {
				finishEdits();
				inspector::setFilterEnabled(filter, checked);
			}
		});
		connect(up, &QToolButton::clicked, this, [this, filter] { move(filter, OBS_ORDER_MOVE_UP); });
		connect(down, &QToolButton::clicked, this, [this, filter] { move(filter, OBS_ORDER_MOVE_DOWN); });
		groups[filterGroup(obs_source_get_output_flags(filter))].layout->addWidget(section);
		filters.push_back({filter, section, enabled, editor, up, down});
	}
	std::sort(filters.begin(), filters.end(), [&](const Filter &a, const Filter &b) {
		return std::find(current.begin(), current.end(), a.source) <
		       std::find(current.begin(), current.end(), b.source);
	});
	std::array<int, 2> nextRow = {1, 1};
	for (size_t i = 0; i < filters.size(); ++i) {
		auto &filter = filters[i];
		const auto group = filterGroup(obs_source_get_output_flags(filter.source));
		auto *layout = groups[group].layout;
		const int row = nextRow[group]++;
		if (layout->indexOf(filter.section) != row) {
			layout->removeWidget(filter.section);
			layout->insertWidget(row, filter.section);
		}
		const auto name = QString::fromUtf8(obs_source_get_name(filter.source));
		filter.section->toggle->setText(name);
		filter.section->toggle->setToolTip(name);
		const QSignalBlocker blocker(filter.enabled);
		filter.enabled->setChecked(obs_source_enabled(filter.source));
		auto sameKind = [group](const Filter &entry) {
			return filterGroup(obs_source_get_output_flags(entry.source)) == group;
		};
		filter.up->setEnabled(std::any_of(filters.begin(), filters.begin() + i, sameKind));
		filter.down->setEnabled(std::any_of(filters.begin() + i + 1, filters.end(), sameKind));
		filter.editor->synchronize();
	}
	empty->setVisible(filters.empty() && searchQuery.isEmpty());
	setSearchQuery(searchQuery);
}

bool FilterStack::setSearchQuery(const QString &query)
{
	searchQuery = query;
	bool any = query.isEmpty();
	std::array<bool, 2> groupMatches = {false, false};
	for (auto &filter : filters) {
		const bool nameMatches = filter.section->toggle->text().contains(query, Qt::CaseInsensitive);
		const bool matches = filter.editor->setSearchQuery(nameMatches ? QString() : query) || nameMatches;
		filter.section->setSearchState(!query.isEmpty(), matches);
		groupMatches[filterGroup(obs_source_get_output_flags(filter.source))] |= matches;
		any = any || matches;
	}
	for (size_t i = 0; i < groups.size(); ++i)
		groups[i].widget->setVisible(groupMatches[i]);
	empty->setVisible(filters.empty() && query.isEmpty());
	return any;
}

QString FilterStack::requestName(const QString &title, const QString &initial, obs_source_t *except)
{
	QString name = initial;
	for (;;) {
		bool ok = false;
		name = QInputDialog::getText(this, title, text("FilterName"), QLineEdit::Normal, name, &ok).trimmed();
		if (!ok || obs_source_removed(source))
			return {};
		OBSSourceAutoRelease existing = obs_source_get_filter_by_name(source, name.toUtf8().constData());
		if (!name.isEmpty() && (!existing || existing.Get() == except))
			return name;
		QMessageBox::information(this, title, text(name.isEmpty() ? "NameRequired" : "FilterNameExists"));
	}
}

void FilterStack::addFilter()
{
	if (cancelled || obs_source_removed(source))
		return;
	QMenu menu(this);
	std::vector<std::pair<QString, QString>> types;
	const char *id;
	for (size_t i = 0; obs_enum_filter_types(i, &id); ++i) {
		if (inspector::filterCompatible(obs_source_get_output_flags(source), obs_get_source_output_flags(id))) {
			const char *name = obs_source_get_display_name(id);
			types.emplace_back(QString::fromUtf8(name ? name : id), QString::fromUtf8(id));
		}
	}
	std::sort(types.begin(), types.end());
	std::array<QMenu *, 2> menus = {menu.addMenu(text("AudioVideoFilters")), menu.addMenu(text("EffectFilters"))};
	for (const auto &[name, id] : types)
		menus[filterGroup(obs_get_source_output_flags(id.toUtf8().constData()))]->addAction(name)->setData(id);
	for (auto *group : menus)
		group->menuAction()->setVisible(!group->isEmpty());
	if (types.empty())
		menu.addAction(text("NoCompatibleFilters"))->setEnabled(false);
	if (auto *choice = menu.exec(QCursor::pos()))
		createFilter(choice->data().toString());
}

void FilterStack::createFilter(const QString &id)
{
	const auto type = id.toUtf8();
	const char *display = obs_source_get_display_name(type.constData());
	QString base = QString::fromUtf8(display ? display : type.constData());
	QString initial = base;
	for (int suffix = 2;; ++suffix) {
		OBSSourceAutoRelease existing = obs_source_get_filter_by_name(source, initial.toUtf8().constData());
		if (!existing)
			break;
		initial = base + QStringLiteral(" %1").arg(suffix);
	}
	const QString name = requestName(text("AddFilter"), initial);
	if (name.isEmpty() || cancelled)
		return;
	OBSSourceAutoRelease filter = obs_source_create(type.constData(), name.toUtf8().constData(), nullptr, nullptr);
	if (!filter) {
		QMessageBox::warning(this, text("AddFilter"), text("FilterCreateFailed"));
		return;
	}
	finishEdits();
	inspector::editFilters(source, text("AddFilter"), [&] { obs_source_filter_add(source, filter); });
	emit searchResultsChanged();
}

void FilterStack::rename(obs_source_t *filter)
{
	const QString name = requestName(text("RenameFilter"), QString::fromUtf8(obs_source_get_name(filter)), filter);
	if (!name.isEmpty() && !cancelled && obs_source_filter_get_index(source, filter) >= 0) {
		finishEdits();
		inspector::renameFilter(filter, name);
	}
}

void FilterStack::remove(obs_source_t *filter)
{
	if (cancelled || obs_source_filter_get_index(source, filter) < 0)
		return;
	finishEdits();
	inspector::editFilters(source, text("RemoveFilter"), [&] { obs_source_filter_remove(source, filter); });
}

void FilterStack::move(obs_source_t *filter, obs_order_movement direction)
{
	if (cancelled || obs_source_filter_get_index(source, filter) < 0)
		return;
	finishEdits();
	inspector::editFilters(source, text("ReorderFilters"),
			       [&] { obs_source_filter_set_order(source, filter, direction); });
}
