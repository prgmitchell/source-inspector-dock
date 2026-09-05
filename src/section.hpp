// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <obs-module.h>

inline QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

class Section : public QFrame {
public:
	QToolButton *toggle;
	QWidget *body;
	QHBoxLayout *header;
	QVBoxLayout *content;

	explicit Section(const QString &title, QWidget *parent = nullptr) : QFrame(parent)
	{
		setObjectName("InspectorSection");
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(4);
		header = new QHBoxLayout;
		toggle = new QToolButton;
		toggle->setText(title);
		toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		toggle->setArrowType(Qt::DownArrow);
		toggle->setCheckable(true);
		toggle->setChecked(true);
		toggle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		toggle->setToolTip(title);
		header->addWidget(toggle, 1);
		layout->addLayout(header);
		body = new QWidget;
		content = new QVBoxLayout(body);
		content->setContentsMargins(8, 0, 4, 8);
		layout->addWidget(body);
		connect(toggle, &QToolButton::toggled, this, [this](bool expanded) {
			toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
			body->setVisible(expanded);
		});
	}
};
