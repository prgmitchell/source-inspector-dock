// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <obs.hpp>
#include <map>
#include <string>

class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QFormLayout;

class TransformEditor : public QWidget {
public:
	explicit TransformEditor(obs_scene_t *root, obs_sceneitem_t *item, QWidget *parent = nullptr);
	void synchronize();

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
	void addNumber(QFormLayout *form, const char *key, double minimum, double maximum, int decimals = 2);
	void changeNumber(const std::string &key, double value);
	void action(const QString &name);
};
