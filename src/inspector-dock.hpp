// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QTimer>
#include <obs.hpp>
#include <memory>
#include <vector>

class QLabel;
class QScrollArea;
class QVBoxLayout;
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
	obs_sceneitem_t *inspectedItem() const { return selectedItem; }
	// Also used by the standalone integration harness with real libobs scenes.
	void inspectScene(obs_source_t *scene);

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
	QWidget *contents = nullptr;
	QVBoxLayout *sections = nullptr;
	Section *filterSection = nullptr;
	TransformEditor *transform = nullptr;
	SourceProperties *properties = nullptr;
	std::vector<Filter> filters;
	std::unique_ptr<QSettings> preferences;
	bool stopped = false;
	void populate();
	void synchronizeFilters();
	void rememberSection(Section *section, const QString &key);
};
