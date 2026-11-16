// SPDX-License-Identifier: GPL-2.0-or-later
#include "form-search.hpp"
#include <QAbstractButton>
#include <QGroupBox>
#include <QLabel>

namespace {
QString widgetText(QWidget *widget)
{
	if (!widget)
		return {};
	QString result = widget->accessibleName();
	if (auto *label = qobject_cast<QLabel *>(widget))
		result += " " + label->text();
	if (auto *button = qobject_cast<QAbstractButton *>(widget))
		result += " " + button->text();
	if (auto *group = qobject_cast<QGroupBox *>(widget))
		result += " " + group->title();
	for (auto *child : widget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
		result += " " + widgetText(child);
	return result.remove('&');
}

QString itemText(QLayoutItem *item)
{
	if (!item)
		return {};
	if (item->widget())
		return widgetText(item->widget());
	QString result;
	if (auto *layout = item->layout()) {
		for (int i = 0; i < layout->count(); ++i)
			result += " " + itemText(layout->itemAt(i));
	}
	return result;
}
} // namespace

bool FormSearch::apply(QFormLayout *form, const QString &query)
{
	for (const auto &entry : hidden) {
		if (entry.form && entry.row < entry.form->rowCount())
			entry.form->setRowVisible(entry.row, true);
	}
	hidden.clear();
	return !form ? query.isEmpty() : filter(form, query.trimmed());
}

bool FormSearch::filter(QFormLayout *form, const QString &query)
{
	bool any = query.isEmpty();
	for (int row = 0; row < form->rowCount(); ++row) {
		if (!form->isRowVisible(row))
			continue;
		auto *label = form->itemAt(row, QFormLayout::LabelRole);
		auto *field = form->itemAt(row, QFormLayout::FieldRole);
		if (!field)
			field = form->itemAt(row, QFormLayout::SpanningRole);
		bool matches = query.isEmpty() ||
			       (itemText(label) + " " + itemText(field)).contains(query, Qt::CaseInsensitive);
		if (auto *group = field ? qobject_cast<QGroupBox *>(field->widget()) : nullptr) {
			if (auto *nested = qobject_cast<QFormLayout *>(group->layout())) {
				const bool groupMatch = group->title().contains(query, Qt::CaseInsensitive);
				matches = filter(nested, groupMatch ? QString() : query) || groupMatch;
			}
		}
		if (!matches) {
			form->setRowVisible(row, false);
			hidden.push_back({form, row});
		}
		any = any || matches;
	}
	return any;
}
