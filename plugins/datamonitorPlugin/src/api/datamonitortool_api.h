#ifndef DATAMONITORTOOL_API_H
#define DATAMONITORTOOL_API_H

#include "scopy-datamonitorplugin_export.h"
#include "pluginbase/apiobject.h"
#include <datamonitortool.h>

namespace scopy::datamonitor {

class SCOPY_DATAMONITORPLUGIN_EXPORT DataMonitorTool_API : public ApiObject
{
	Q_OBJECT

public:
	explicit DataMonitorTool_API(DatamonitorTool *tool);
	~DataMonitorTool_API();

	Q_INVOKABLE QString showAvailableDevices();
	Q_INVOKABLE QString showAvailableMonitors();
	Q_INVOKABLE void setRunning(bool running);
	Q_INVOKABLE void clearData();
	Q_INVOKABLE QString showMonitorsOfDevice(QString device);
	Q_INVOKABLE QString enableMonitor(QString monitor);
	Q_INVOKABLE QString disableMonitor(QString monitor);
	Q_INVOKABLE void setLogPath(QString path);
	Q_INVOKABLE void logAtPath(QString path);
	Q_INVOKABLE void continuousLogAtPath(QString path);
	Q_INVOKABLE void stopContinuousLog();
	Q_INVOKABLE void importDataFromPath(QString path);

private:
	DatamonitorTool *m_tool;
};
} // namespace scopy::datamonitor
#endif // DATAMONITORTOOL_API_H
