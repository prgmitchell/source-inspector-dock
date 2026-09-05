// SPDX-License-Identifier: GPL-2.0-or-later
#include "source-properties.hpp"
#include "section.hpp"
#include "undo.hpp"
#include <properties-view.hpp>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QFormLayout>
#include <QPushButton>

SourceProperties::SourceProperties(obs_source_t *source_, QWidget *parent) : QWidget(parent), source(source_)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	lastSettings = obs_data_get_json(settings);
	OBSDataAutoRelease editable = obs_get_source_defaults(obs_source_get_id(source));
	if (!editable)
		editable = obs_data_create();
	obs_data_apply(editable, settings);
	view = new OBSPropertiesView(OBSData(editable.Get()), source.Get(), reload, commit, liveUpdate);
	view->SetCallbackContext(this);
	view->setObjectName("InspectorProperties");
	view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	layout->addWidget(view);
	deferredBar = new QWidget;
	auto *buttons = new QHBoxLayout(deferredBar);
	buttons->setContentsMargins(0, 0, 0, 0);
	auto *apply = new QPushButton(text("Apply"));
	auto *discard = new QPushButton(text("Discard"));
	buttons->addStretch();
	buttons->addWidget(discard);
	buttons->addWidget(apply);
	deferredBar->hide();
	layout->addWidget(deferredBar);
	connect(apply, &QPushButton::clicked, this, [this] { flush(); });
	connect(discard, &QPushButton::clicked, this, [this] {
		dirty = false;
		before = nullptr;
		reloadSettings();
	});
	connect(view, &OBSPropertiesView::Changed, this, [this] { dirty = true; });
	connect(view, &OBSPropertiesView::PropertiesRefreshed, this, [this] {
		deferredBar->setVisible(view->DeferUpdate());
		if (auto *container = view->QScrollArea::widget()) {
			if (auto *form = qobject_cast<QFormLayout *>(container->layout())) {
				form->setRowWrapPolicy(QFormLayout::WrapAllRows);
				form->setLabelAlignment(Qt::AlignLeft);
				form->setContentsMargins(0, 0, 0, 0);
			}
			container->layout()->activate();
			view->setFixedHeight(container->sizeHint().height() + 4);
		}
	});
	signal_handler_connect(obs_source_get_signal_handler(source), "update_properties", propertiesChanged, this);
}

SourceProperties::~SourceProperties()
{
	signal_handler_disconnect(obs_source_get_signal_handler(source), "update_properties", propertiesChanged, this);
	// Flush normal edits while the source and callback context still exist.
	if (!cancelled && !view->DeferUpdate())
		flush();
	cancelled = true;
	delete view;
}

void SourceProperties::cancel()
{
	cancelled = true;
	dirty = false;
	setEnabled(false);
}

obs_properties_t *SourceProperties::reload(void *data)
{
	auto *self = static_cast<SourceProperties *>(data);
	return obs_source_properties(self->source);
}

void SourceProperties::liveUpdate(void *data, obs_data_t *settings)
{
	auto *self = static_cast<SourceProperties *>(data);
	if (self->cancelled || obs_source_removed(self->source))
		return;
	if (!self->before) {
		OBSDataAutoRelease current = obs_source_get_settings(self->source);
		self->before = obs_data_create();
		obs_data_release(self->before);
		obs_data_apply(self->before, current);
	}
	obs_source_update(self->source, settings);
}

void SourceProperties::commit(void *data, obs_data_t *oldSettings, obs_data_t *settings)
{
	auto *self = static_cast<SourceProperties *>(data);
	if (self->cancelled || obs_source_removed(self->source))
		return;
	// A pending native timer can fire after an explicit flush; do not add duplicate undo entries.
	if (!self->dirty && !self->before)
		return;
	OBSDataAutoRelease current = obs_source_get_settings(self->source);
	obs_data_t *previous = self->before ? self->before.Get() : (oldSettings ? oldSettings : current.Get());
	inspector::updateSource(self->source, previous, settings);
	self->before = nullptr;
	self->dirty = false;
	self->lastSettings = obs_data_get_json(settings);
}

void SourceProperties::flush()
{
	if (dirty || before)
		commit(this, nullptr, view->GetSettings());
}

void SourceProperties::propertiesChanged(void *data, calldata_t *)
{
	auto *self = static_cast<SourceProperties *>(data);
	QMetaObject::invokeMethod(self, [self] { self->reloadRequested = true; }, Qt::QueuedConnection);
}

void SourceProperties::reloadSettings()
{
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	obs_data_clear(view->GetSettings());
	obs_data_apply(view->GetSettings(), settings);
	lastSettings = obs_data_get_json(settings);
	reloadRequested = false;
	view->ReloadProperties();
}

void SourceProperties::synchronize()
{
	if (cancelled || dirty || QApplication::activeModalWidget())
		return;
	auto *focus = QApplication::focusWidget();
	if (focus && isAncestorOf(focus))
		return;
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	if (reloadRequested || lastSettings != QByteArray(obs_data_get_json(settings)))
		reloadSettings();
}
