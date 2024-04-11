#ifndef DATAMONITOR_API_HPP
#define DATAMONITOR_API_HPP

#include "scopy-datamonitorplugin_export.h"

#include <datamonitorplugin.h>

namespace scopy::datamonitor {

class SCOPY_DATAMONITORPLUGIN_EXPORT DataMonitor_API : public ApiObject
{
	Q_OBJECT
public:
	explicit DataMonitor_API(DataMonitorPlugin *dataMonitorPlugin);
	~DataMonitor_API();

	// PLUGIN RELATED
	Q_INVOKABLE QString showAvailableMonitors();
	Q_INVOKABLE QString showAvailableDevices();
	Q_INVOKABLE QString showMonitorsOfDevice(QString device);
	Q_INVOKABLE QString enableMonitor(QString monitor);
	Q_INVOKABLE QString disableMonitor(QString monitor);
	Q_INVOKABLE void setRunning(bool running);
	Q_INVOKABLE void clearData();

	// TOOL RELATED
	Q_INVOKABLE QString createTool();
	Q_INVOKABLE QString getToolList();
	Q_INVOKABLE QString enableMonitorOfTool(QString toolName, QString monitor);
	Q_INVOKABLE QString disableMonitorOfTool(QString toolName, QString monitor);
	Q_INVOKABLE void setLogPathOfTool(QString toolName, QString path);
	Q_INVOKABLE void logAtPathForTool(QString toolName, QString path);
	Q_INVOKABLE void continuousLogAtPathForTool(QString toolName, QString path);
	Q_INVOKABLE void stopContinuousLogForTool(QString toolName);
	Q_INVOKABLE void importDataFromPathForTool(QString toolName, QString path);

private:
	DataMonitorPlugin *m_dataMonitorPlugin;
	QString m_activeTool;
};
} // namespace scopy::datamonitor
#endif // DATAMONITOR_API_HPP
