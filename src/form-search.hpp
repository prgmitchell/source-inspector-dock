// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QFormLayout>
#include <QPointer>
#include <QString>
#include <vector>

// Only rows hidden by this search are restored. Native property visibility stays intact.
class FormSearch {
public:
	bool apply(QFormLayout *form, const QString &query);

private:
	struct HiddenRow {
		QPointer<QFormLayout> form;
		int row;
	};
	std::vector<HiddenRow> hidden;
	bool filter(QFormLayout *form, const QString &query);
};
