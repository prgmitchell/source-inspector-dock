// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <obs.hpp>
#include <map>
#include <string>
#include "form-search.hpp"
#include "transform-presets.hpp"

class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QFormLayout;
class QSettings;

class TransformEditor : public QWidget {
public:
	explicit TransformEditor(obs_scene_t *root, obs_sceneitem_t *item, QSettings *preferences,
				 QWidget *parent = nullptr);
	void synchronize();
	bool setSearchQuery(const QString &query);

private:
	OBSSceneItem item;
	OBSScene rootScene;
	QWidget *controls;
	QLabel *locked;
	QComboBox *alignment;
	QComboBox *boundsType;
	QComboBox *boundsAlignment;
	QCheckBox *cropToBounds;
	std::map<std::string, QDoubleSpinBox *> fields;
	FormSearch search;
	QSettings *preferences;
	QComboBox *presetChoice = nullptr;
	std::vector<inspector::TransformPreset> presets;
	void reloadPresets(const QString &selected = {});
	void savePreset(bool replace);
	void renamePreset();
	void deletePreset();
	void applyPreset();
	void writePresets(const QString &selected);
	QString requestPresetName(const QString &initial, int except = -1);
	void addNumber(QFormLayout *form, const char *key, double minimum, double maximum, int decimals = 2);
	void changeNumber(const std::string &key, double value);
	void action(const QString &name);
};
