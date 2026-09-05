// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QTimer>
#include <obs.hpp>
#include <memory>
#include <vector>

class QLabel;
class QScrollArea;
class QSettings;
class Section;
class TransformEditor;
class SourceProperties;
class QCheckBox;

class InspectorDock : public QWidget {
public:
	explicit InspectorDock(QWidget *parent = nullptr);
	~InspectorDock() override;
	void refresh();
	void clear(bool removed = false);
	void shutdown();
	void suspend();
	void resume();

private:
	struct Filter {
		OBSSource source;
		Section *section;
		QCheckBox *enabled;
		SourceProperties *editor;
	};
	QTimer timer;
	OBSSource scene;
	OBSSceneItem selectedItem;
	std::vector<OBSSceneItem> previousSelection;
	QLabel *title;
	QLabel *subtitle;
	QLabel *empty;
	QScrollArea *scroll;
	Section *filterSection = nullptr;
	TransformEditor *transform = nullptr;
	SourceProperties *properties = nullptr;
	std::vector<Filter> filters;
	std::unique_ptr<QSettings> preferences;
	bool stopped = false;
	void inspectScene(obs_source_t *scene);
	void populate();
	void synchronizeFilters();
	void rememberSection(Section *section, const QString &key);
};
