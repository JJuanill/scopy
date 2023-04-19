#ifndef M2KPLUGIN_H
#define M2KPLUGIN_H

#define SCOPY_PLUGIN_NAME M2kPlugin

#include <QObject>
//#include <pluginbase/plugin.h>
#include <pluginbase/pluginbase.h>
#include "scopym2kplugin_export.h"
#include <QLineEdit>
#include <QPushButton>
#include "m2kcontroller.h"
#include <gui/infopage.h>
#include <calibration.hpp>


namespace adiscope {

namespace m2k {
class SCOPYM2KPLUGIN_EXPORT M2kPlugin : public QObject, public PluginBase
{
	Q_OBJECT
	SCOPY_PLUGIN;
public:

	void initMetadata() override;
	bool compatible(QString m_param) override;


	void preload() override;
	void loadToolList() override;
	bool loadPage() override;
	bool loadExtraButtons() override;

	bool onConnect() override;
	bool onDisconnect() override;
	bool loadIcon() override;


public Q_SLOTS:
	void showPageCallback() override;
	void hidePageCallback() override;

	void calibrationStarted();
	void calibrationSuccess();
	void calibrationFinished();
	void updateTemperature(double);

private:
	QTimer *m_infoPageTimer;

	QPushButton *m_btnIdentify;
	QPushButton *m_btnCalibrate;
	QPushButton *m_btnRegister;

	Calibration *calib;

	M2kController *m_m2kController;
	InfoPage *m_m2kInfoPage;

	const int infoPageTimerTimeout = 1000;
	const QStringList calibrationToolNames = {"Oscilloscope", "Spectrum Analyzer", "Network Analyzer", "Signal Generator", "Voltmeter", "Calibration"};


	QMap<QString,ToolMenuEntry*> toolMenuEntryCalibrationCache;
	void storeToolState(QStringList tools);
	void restoreToolState(QStringList tools);
	bool loadDecoders(QString path);


	// Plugin interface
public:
	void initPreferences() override;
	bool loadPreferencesPage() override;

	// Plugin interface
public:
	void init() override;
};
}
}


#endif // M2KPLUGIN_H