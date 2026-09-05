// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <obs.hpp>

class OBSPropertiesView;

class SourceProperties : public QWidget {
	Q_OBJECT

public:
	explicit SourceProperties(obs_source_t *source, QWidget *parent = nullptr);
	~SourceProperties() override;
	void synchronize();
	void cancel();

private:
	OBSSource source;
	OBSDataAutoRelease before;
	OBSPropertiesView *view = nullptr;
	QWidget *deferredBar = nullptr;
	bool dirty = false;
	bool cancelled = false;
	bool reloadRequested = false;
	QByteArray lastSettings;
	static obs_properties_t *reload(void *data);
	static void commit(void *data, obs_data_t *oldSettings, obs_data_t *settings);
	static void liveUpdate(void *data, obs_data_t *settings);
	static void propertiesChanged(void *data, calldata_t *);
	void reloadSettings();
	void flush();
};
