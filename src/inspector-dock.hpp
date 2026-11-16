// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
#include <obs.hpp>
#include <memory>
#include <vector>

class QLabel;
class QScrollArea;
class QSettings;
class Section;
class TransformEditor;
class SourceProperties;
class QToolButton;
class QLineEdit;
class FilterStack;

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
	QTimer timer;
	QElapsedTimer usageRefresh;
	OBSSource scene;
	OBSSceneItem selectedItem;
	std::vector<OBSSceneItem> previousSelection;
	QLabel *title;
	QLabel *subtitle;
	QLabel *empty;
	QScrollArea *scroll;
	QToolButton *pin;
	QToolButton *usage;
	QLineEdit *searchInput;
	QLabel *noMatches;
	QStringList usageScenes;
	Section *propertySection = nullptr;
	Section *transformSection = nullptr;
	Section *filterSection = nullptr;
	FilterStack *filterStack = nullptr;
	TransformEditor *transform = nullptr;
	SourceProperties *properties = nullptr;
	std::unique_ptr<QSettings> preferences;
	bool stopped = false;
	void inspectScene(obs_source_t *scene);
	void populate();
	void applySearch();
	void updateUsage();
	void showUsage();
	void rememberSection(Section *section, const QString &key);
};
