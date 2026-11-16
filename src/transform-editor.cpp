// SPDX-License-Identifier: GPL-2.0-or-later
#include "transform-editor.hpp"
#include "section.hpp"
#include "undo.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <algorithm>

namespace {
QComboBox *alignmentCombo()
{
	auto *combo = new QComboBox;
	const std::pair<const char *, uint32_t> choices[] = {{"TopLeft", OBS_ALIGN_TOP | OBS_ALIGN_LEFT},
							     {"Top", OBS_ALIGN_TOP},
							     {"TopRight", OBS_ALIGN_TOP | OBS_ALIGN_RIGHT},
							     {"Left", OBS_ALIGN_LEFT},
							     {"Center", OBS_ALIGN_CENTER},
							     {"Right", OBS_ALIGN_RIGHT},
							     {"BottomLeft", OBS_ALIGN_BOTTOM | OBS_ALIGN_LEFT},
							     {"Bottom", OBS_ALIGN_BOTTOM},
							     {"BottomRight", OBS_ALIGN_BOTTOM | OBS_ALIGN_RIGHT}};
	for (const auto &[name, value] : choices)
		combo->addItem(text(name), value);
	return combo;
}

void selectValue(QComboBox *combo, int value)
{
	const QSignalBlocker blocker(combo);
	combo->setCurrentIndex(combo->findData(value));
}
} // namespace

TransformEditor::TransformEditor(obs_scene_t *root, obs_sceneitem_t *item_, QSettings *preferences_, QWidget *parent)
	: QWidget(parent),
	  item(item_),
	  rootScene(root),
	  preferences(preferences_)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	locked = new QLabel(text("TransformLocked"));
	locked->setWordWrap(true);
	layout->addWidget(locked);
	controls = new QWidget;
	layout->addWidget(controls);
	auto *form = new QFormLayout(controls);
	form->setContentsMargins(0, 0, 0, 0);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	form->setRowWrapPolicy(QFormLayout::WrapLongRows);
	if (preferences) {
		auto *presetsRow = new QVBoxLayout;
		presetChoice = new QComboBox;
		presetChoice->setObjectName("TransformPresetChoice");
		presetChoice->setAccessibleName(text("TransformPresets"));
		presetChoice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		presetsRow->addWidget(presetChoice);
		auto *buttons = new QHBoxLayout;
		auto *apply = new QPushButton(text("ApplyPreset"));
		apply->setObjectName("ApplyPreset");
		auto *manage = new QToolButton;
		manage->setText(text("PresetActions"));
		manage->setObjectName("PresetActions");
		manage->setPopupMode(QToolButton::InstantPopup);
		auto *menu = new QMenu(manage);
		menu->addAction(text("SavePreset"), this, [this] { savePreset(false); });
		auto *update = menu->addAction(text("UpdatePreset"), this, [this] { savePreset(true); });
		auto *rename = menu->addAction(text("RenamePreset"), this, [this] { renamePreset(); });
		auto *remove = menu->addAction(text("DeletePreset"), this, [this] { deletePreset(); });
		connect(menu, &QMenu::aboutToShow, this, [this, update, rename, remove] {
			const bool selected = presetChoice->currentIndex() > 0;
			for (auto *action : {update, rename, remove})
				action->setEnabled(selected);
		});
		manage->setMenu(menu);
		buttons->addWidget(apply);
		buttons->addWidget(manage);
		presetsRow->addLayout(buttons);
		form->addRow(text("TransformPresets"), presetsRow);
		connect(apply, &QPushButton::clicked, this, [this] { applyPreset(); });
		connect(presetChoice, &QComboBox::currentIndexChanged, apply,
			[apply](int index) { apply->setEnabled(index > 0); });
		reloadPresets();
		apply->setEnabled(false);
	}
	addNumber(form, "PositionX", -1000000, 1000000);
	addNumber(form, "PositionY", -1000000, 1000000);
	addNumber(form, "ScaleX", -100000, 100000);
	addNumber(form, "ScaleY", -100000, 100000);
	fields.at("ScaleX")->setSuffix(" %");
	fields.at("ScaleY")->setSuffix(" %");
	addNumber(form, "Rotation", -36000, 36000);
	fields.at("Rotation")->setSuffix(QString::fromUtf8("°"));
	alignment = alignmentCombo();
	form->addRow(text("Alignment"), alignment);
	boundsType = new QComboBox;
	const char *types[] = {"BoundsNone",  "BoundsStretch", "BoundsInner", "BoundsOuter",
			       "BoundsWidth", "BoundsHeight",  "BoundsMax"};
	for (int i = 0; i < 7; ++i)
		boundsType->addItem(text(types[i]), i);
	form->addRow(text("BoundsType"), boundsType);
	addNumber(form, "BoundsWidthValue", 0, 1000000);
	addNumber(form, "BoundsHeightValue", 0, 1000000);
	boundsAlignment = alignmentCombo();
	form->addRow(text("BoundsAlignment"), boundsAlignment);
	cropToBounds = new QCheckBox(text("CropToBounds"));
	form->addRow(cropToBounds);
	for (const char *key : {"CropLeft", "CropTop", "CropRight", "CropBottom"})
		addNumber(form, key, 0, 1000000, 0);
	for (auto *combo : {alignment, boundsType, boundsAlignment}) {
		connect(combo, &QComboBox::activated, this, [this, combo](int) {
			inspector::editTransform(rootScene, item, text("Undo.Transform"), [this, combo] {
				obs_transform_info info;
				obs_sceneitem_get_info2(item, &info);
				const int value = combo->currentData().toInt();
				if (combo == alignment)
					info.alignment = static_cast<uint32_t>(value);
				else if (combo == boundsType)
					info.bounds_type = static_cast<obs_bounds_type>(value);
				else
					info.bounds_alignment = static_cast<uint32_t>(value);
				obs_sceneitem_set_info2(item, &info);
			});
			synchronize();
		});
	}
	connect(cropToBounds, &QCheckBox::clicked, this, [this](bool checked) {
		inspector::editTransform(rootScene, item, text("Undo.Transform"), [this, checked] {
			obs_transform_info info;
			obs_sceneitem_get_info2(item, &info);
			info.crop_to_bounds = checked;
			obs_sceneitem_set_info2(item, &info);
		});
	});
	auto *actions = new QGridLayout;
	int index = 0;
	for (const char *name : {"ResetTransform", "FitToCanvas", "FlipHorizontal", "FlipVertical"}) {
		auto *button = new QPushButton(text(name));
		button->setObjectName(name);
		actions->addWidget(button, index / 2, index % 2);
		connect(button, &QPushButton::clicked, this, [this, name] { action(name); });
		++index;
	}
	form->addRow(actions);
	synchronize();
}

void TransformEditor::addNumber(QFormLayout *form, const char *key, double minimum, double maximum, int decimals)
{
	auto *spin = new QDoubleSpinBox;
	spin->setObjectName(key);
	spin->setAccessibleName(text(key));
	spin->setDecimals(decimals);
	spin->setRange(minimum, maximum);
	spin->setKeyboardTracking(false);
	spin->setAccelerated(true);
	fields.emplace(key, spin);
	form->addRow(text(key), spin);
	connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key](double value) { changeNumber(key, value); });
}

void TransformEditor::changeNumber(const std::string &key, double value)
{
	inspector::editTransform(rootScene, item, text("Undo.Transform"), [this, &key, value] {
		obs_transform_info info;
		obs_sceneitem_crop crop;
		obs_sceneitem_get_info2(item, &info);
		obs_sceneitem_get_crop(item, &crop);
		const float number = static_cast<float>(value);
		if (key == "PositionX")
			info.pos.x = number;
		else if (key == "PositionY")
			info.pos.y = number;
		else if (key == "ScaleX")
			info.scale.x = number / 100.0f;
		else if (key == "ScaleY")
			info.scale.y = number / 100.0f;
		else if (key == "Rotation")
			info.rot = number;
		else if (key == "BoundsWidthValue")
			info.bounds.x = number;
		else if (key == "BoundsHeightValue")
			info.bounds.y = number;
		else if (key == "CropLeft")
			crop.left = static_cast<int>(value);
		else if (key == "CropTop")
			crop.top = static_cast<int>(value);
		else if (key == "CropRight")
			crop.right = static_cast<int>(value);
		else if (key == "CropBottom")
			crop.bottom = static_cast<int>(value);
		obs_sceneitem_set_info2(item, &info);
		obs_sceneitem_set_crop(item, &crop);
	});
}

void TransformEditor::synchronize()
{
	const bool isLocked = obs_sceneitem_locked(item);
	locked->setVisible(isLocked);
	controls->setEnabled(!isLocked);
	obs_transform_info info;
	obs_sceneitem_crop crop;
	obs_sceneitem_get_info2(item, &info);
	obs_sceneitem_get_crop(item, &crop);
	const std::pair<const char *, double> values[] = {{"PositionX", info.pos.x},
							  {"PositionY", info.pos.y},
							  {"ScaleX", info.scale.x * 100.0},
							  {"ScaleY", info.scale.y * 100.0},
							  {"Rotation", info.rot},
							  {"BoundsWidthValue", info.bounds.x},
							  {"BoundsHeightValue", info.bounds.y},
							  {"CropLeft", static_cast<double>(crop.left)},
							  {"CropTop", static_cast<double>(crop.top)},
							  {"CropRight", static_cast<double>(crop.right)},
							  {"CropBottom", static_cast<double>(crop.bottom)}};
	for (const auto &[key, value] : values) {
		auto *spin = fields.at(key);
		if (spin->hasFocus() || spin->isAncestorOf(QApplication::focusWidget()))
			continue;
		const QSignalBlocker blocker(spin);
		spin->setValue(value);
	}
	selectValue(alignment, static_cast<int>(info.alignment));
	selectValue(boundsType, static_cast<int>(info.bounds_type));
	selectValue(boundsAlignment, static_cast<int>(info.bounds_alignment));
	const QSignalBlocker blocker(cropToBounds);
	cropToBounds->setChecked(info.crop_to_bounds);
	const bool bounded = info.bounds_type != OBS_BOUNDS_NONE;
	fields.at("BoundsWidthValue")->setEnabled(bounded);
	fields.at("BoundsHeightValue")->setEnabled(bounded);
	boundsAlignment->setEnabled(bounded);
	cropToBounds->setEnabled(bounded);
}

bool TransformEditor::setSearchQuery(const QString &query)
{
	return search.apply(qobject_cast<QFormLayout *>(controls->layout()), query);
}

void TransformEditor::reloadPresets(const QString &selected)
{
	presets = inspector::loadTransformPresets(*preferences);
	presetChoice->clear();
	presetChoice->addItem(text("ChoosePreset"));
	for (const auto &preset : presets)
		presetChoice->addItem(preset.name);
	presetChoice->setCurrentIndex(std::max(0, presetChoice->findText(selected)));
}

QString TransformEditor::requestPresetName(const QString &initial, int except)
{
	QString name = initial;
	for (;;) {
		bool ok = false;
		name = QInputDialog::getText(this, text("TransformPresets"), text("PresetName"), QLineEdit::Normal,
					     name, &ok)
			       .trimmed();
		if (!ok)
			return {};
		bool exists = false;
		for (size_t i = 0; i < presets.size(); ++i) {
			if (static_cast<int>(i) != except && presets[i].name.compare(name, Qt::CaseInsensitive) == 0)
				exists = true;
		}
		if (!name.isEmpty() && !exists)
			return name;
		QMessageBox::information(this, text("TransformPresets"),
					 text(name.isEmpty() ? "NameRequired" : "PresetNameExists"));
	}
}

void TransformEditor::writePresets(const QString &selected)
{
	if (!inspector::saveTransformPresets(*preferences, presets))
		QMessageBox::warning(this, text("TransformPresets"), text("PresetSaveFailed"));
	reloadPresets(selected);
}

void TransformEditor::savePreset(bool replace)
{
	const int index = presetChoice->currentIndex() - 1;
	if (replace && (index < 0 || static_cast<size_t>(index) >= presets.size()))
		return;
	const QString name =
		replace ? presets[index].name
			: requestPresetName(QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))));
	if (name.isEmpty() || !obs_sceneitem_get_scene(item))
		return;
	inspector::TransformPreset preset;
	preset.name = name;
	obs_sceneitem_get_info2(item, &preset.info);
	obs_sceneitem_get_crop(item, &preset.crop);
	if (replace)
		presets[index] = preset;
	else
		presets.push_back(preset);
	writePresets(name);
}

void TransformEditor::renamePreset()
{
	const int index = presetChoice->currentIndex() - 1;
	if (index < 0 || static_cast<size_t>(index) >= presets.size())
		return;
	const QString name = requestPresetName(presets[index].name, index);
	if (!name.isEmpty()) {
		presets[index].name = name;
		writePresets(name);
	}
}

void TransformEditor::deletePreset()
{
	const int index = presetChoice->currentIndex() - 1;
	if (index < 0 || static_cast<size_t>(index) >= presets.size())
		return;
	if (QMessageBox::question(this, text("DeletePreset"), text("DeletePresetConfirm").arg(presets[index].name),
				  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;
	presets.erase(presets.begin() + index);
	writePresets({});
}

void TransformEditor::applyPreset()
{
	const int index = presetChoice->currentIndex() - 1;
	if (index < 0 || static_cast<size_t>(index) >= presets.size())
		return;
	const auto preset = presets[index];
	inspector::editTransform(rootScene, item, text("ApplyPreset") + " — " + preset.name, [this, preset] {
		obs_sceneitem_set_info2(item, &preset.info);
		obs_sceneitem_set_crop(item, &preset.crop);
	});
	synchronize();
}

void TransformEditor::action(const QString &name)
{
	inspector::editTransform(rootScene, item, text(name.toUtf8().constData()), [this, name] {
		obs_transform_info info;
		obs_sceneitem_get_info2(item, &info);
		if (name == "ResetTransform") {
			info = {};
			info.scale = {1.0f, 1.0f};
			info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
			obs_sceneitem_crop crop = {};
			obs_sceneitem_set_crop(item, &crop);
		} else if (name == "FitToCanvas") {
			auto *sceneSource = obs_scene_get_source(obs_sceneitem_get_scene(item));
			info.pos = {};
			info.rot = 0.0f;
			info.scale = {1.0f, 1.0f};
			info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
			info.bounds_type = OBS_BOUNDS_SCALE_INNER;
			info.bounds_alignment = OBS_ALIGN_CENTER;
			info.bounds = {static_cast<float>(obs_source_get_width(sceneSource)),
				       static_cast<float>(obs_source_get_height(sceneSource))};
		} else if (name == "FlipHorizontal") {
			info.scale.x = -info.scale.x;
		} else if (name == "FlipVertical") {
			info.scale.y = -info.scale.y;
		}
		obs_sceneitem_set_info2(item, &info);
	});
	synchronize();
}
