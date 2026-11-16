// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <obs.hpp>
#include <vector>

class QCheckBox;
class QLabel;
class QSettings;
class QToolButton;
class Section;
class SourceProperties;

class FilterStack : public QWidget {
	Q_OBJECT
public:
	explicit FilterStack(obs_source_t *source, QSettings *preferences, QWidget *parent = nullptr);
	void synchronize();
	void cancel();
	bool setSearchQuery(const QString &query);
	void addFilter();
	size_t count() const { return filters.size(); }

signals:
	void searchResultsChanged();

private:
	struct Filter {
		OBSSource source;
		Section *section;
		QCheckBox *enabled;
		SourceProperties *editor;
		QToolButton *up;
		QToolButton *down;
	};
	OBSSource source;
	QSettings *preferences;
	QLabel *empty;
	std::vector<Filter> filters;
	QString searchQuery;
	bool cancelled = false;
	void finishEdits();
	void rename(obs_source_t *filter);
	void remove(obs_source_t *filter);
	void move(obs_source_t *filter, obs_order_movement direction);
	void createFilter(const QString &id);
	QString requestName(const QString &title, const QString &initial, obs_source_t *except = nullptr);
};

namespace inspector {
bool filterCompatible(uint32_t sourceFlags, uint32_t filterFlags);
} // namespace inspector
