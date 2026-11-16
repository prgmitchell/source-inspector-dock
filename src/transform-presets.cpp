// SPDX-License-Identifier: GPL-2.0-or-later
#include "transform-presets.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>
#include <cmath>

namespace {
bool number(const QJsonObject &object, const char *key, double min, double max)
{
	const auto value = object.value(key);
	return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= min &&
	       value.toDouble() <= max;
}

bool integer(const QJsonObject &object, const char *key, int min, int max)
{
	return number(object, key, min, max) &&
	       std::floor(object.value(key).toDouble()) == object.value(key).toDouble();
}

bool alignment(const QJsonObject &object, const char *key)
{
	if (!integer(object, key, 0, 15))
		return false;
	const auto value = object.value(key).toInt();
	return (value & (OBS_ALIGN_LEFT | OBS_ALIGN_RIGHT)) != (OBS_ALIGN_LEFT | OBS_ALIGN_RIGHT) &&
	       (value & (OBS_ALIGN_TOP | OBS_ALIGN_BOTTOM)) != (OBS_ALIGN_TOP | OBS_ALIGN_BOTTOM);
}
} // namespace

std::vector<inspector::TransformPreset> inspector::loadTransformPresets(QSettings &settings)
{
	std::vector<TransformPreset> result;
	const auto document = QJsonDocument::fromJson(settings.value("transformPresets").toByteArray()).object();
	if (document.value("version").toInt() != 1)
		return result;
	for (const auto &value : document.value("presets").toArray()) {
		const auto object = value.toObject();
		const QString name = object.value("name").toString().trimmed();
		if (name.isEmpty() || std::any_of(result.begin(), result.end(), [&](const TransformPreset &preset) {
			    return preset.name.compare(name, Qt::CaseInsensitive) == 0;
		    }))
			continue;
		if (!number(object, "x", -1000000, 1000000) || !number(object, "y", -1000000, 1000000) ||
		    !number(object, "scaleX", -1000, 1000) || !number(object, "scaleY", -1000, 1000) ||
		    !number(object, "rotation", -36000, 36000) || !alignment(object, "alignment") ||
		    !integer(object, "boundsType", OBS_BOUNDS_NONE, OBS_BOUNDS_MAX_ONLY) ||
		    !alignment(object, "boundsAlignment") || !number(object, "width", 0, 1000000) ||
		    !number(object, "height", 0, 1000000) || !object.value("cropToBounds").isBool() ||
		    !integer(object, "left", 0, 1000000) || !integer(object, "top", 0, 1000000) ||
		    !integer(object, "right", 0, 1000000) || !integer(object, "bottom", 0, 1000000))
			continue;
		TransformPreset preset;
		preset.name = name;
		preset.info.pos = {static_cast<float>(object.value("x").toDouble()),
				   static_cast<float>(object.value("y").toDouble())};
		preset.info.scale = {static_cast<float>(object.value("scaleX").toDouble()),
				     static_cast<float>(object.value("scaleY").toDouble())};
		preset.info.rot = static_cast<float>(object.value("rotation").toDouble());
		preset.info.alignment = static_cast<uint32_t>(object.value("alignment").toInt());
		preset.info.bounds_type = static_cast<obs_bounds_type>(object.value("boundsType").toInt());
		preset.info.bounds_alignment = static_cast<uint32_t>(object.value("boundsAlignment").toInt());
		preset.info.bounds = {static_cast<float>(object.value("width").toDouble()),
				      static_cast<float>(object.value("height").toDouble())};
		preset.info.crop_to_bounds = object.value("cropToBounds").toBool();
		preset.crop = {object.value("left").toInt(), object.value("top").toInt(), object.value("right").toInt(),
			       object.value("bottom").toInt()};
		result.push_back(preset);
	}
	return result;
}

bool inspector::saveTransformPresets(QSettings &settings, const std::vector<TransformPreset> &presets)
{
	QJsonArray entries;
	for (const auto &preset : presets) {
		const auto &info = preset.info;
		const auto &crop = preset.crop;
		entries.append(QJsonObject{{"name", preset.name},
					   {"x", info.pos.x},
					   {"y", info.pos.y},
					   {"scaleX", info.scale.x},
					   {"scaleY", info.scale.y},
					   {"rotation", info.rot},
					   {"alignment", static_cast<int>(info.alignment)},
					   {"boundsType", static_cast<int>(info.bounds_type)},
					   {"boundsAlignment", static_cast<int>(info.bounds_alignment)},
					   {"width", info.bounds.x},
					   {"height", info.bounds.y},
					   {"cropToBounds", info.crop_to_bounds},
					   {"left", crop.left},
					   {"top", crop.top},
					   {"right", crop.right},
					   {"bottom", crop.bottom}});
	}
	settings.setValue(
		"transformPresets",
		QJsonDocument(QJsonObject{{"version", 1}, {"presets", entries}}).toJson(QJsonDocument::Compact));
	settings.sync();
	return settings.status() == QSettings::NoError;
}
