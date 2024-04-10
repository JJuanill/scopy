#include "datamonitortool_api.h"
#include "datamonitorsettings.hpp"

using namespace scopy::datamonitor;

DataMonitorTool_API::DataMonitorTool_API(DatamonitorTool *tool)
	: ApiObject()
	, m_tool(tool)
{}

DataMonitorTool_API::~DataMonitorTool_API() {}

QString DataMonitorTool_API::showAvailableDevices()
{
	Q_ASSERT(m_tool->m_dataAcquisitionManager != nullptr);
	QString availableDevices = "";
	foreach(QString monitor, m_tool->m_dataAcquisitionManager->getDataMonitorMap()->keys()) {
		if(!availableDevices.contains(monitor)) {
			availableDevices += monitor + "\n";
		}
	}
	return availableDevices;
}

QString DataMonitorTool_API::showAvailableMonitors()
{
	Q_ASSERT(m_tool->m_dataAcquisitionManager != nullptr);
	QString availableDevices = "";

	foreach(QString monitor, m_tool->m_dataAcquisitionManager->getDataMonitorMap()->keys()) {
		availableDevices += monitor + "\n";
	}

	return availableDevices;
}

void DataMonitorTool_API::setRunning(bool running)
{
	Q_ASSERT(m_tool->getRunButton() != nullptr);

	m_tool->getRunButton()->setChecked(running);
}

void DataMonitorTool_API::clearData()
{
	Q_ASSERT(m_tool != nullptr);
	Q_EMIT m_tool->clearBtn->click();
}

QString DataMonitorTool_API::showMonitorsOfDevice(QString device)
{
	Q_ASSERT(m_tool->m_dataAcquisitionManager != nullptr);
	QString availableDevices = "";

	foreach(QString monitor, m_tool->m_dataAcquisitionManager->getDataMonitorMap()->keys()) {
		if(monitor.contains(device)) {
			availableDevices +=
				m_tool->m_dataAcquisitionManager->getDataMonitorMap()->value(monitor)->getName() + "\n";
		}
	}

	return availableDevices;
}

QString DataMonitorTool_API::enableMonitor(QString monitor)
{
	Q_ASSERT(m_tool->m_dataAcquisitionManager != nullptr);

	if(!m_tool->m_dataAcquisitionManager->getDataMonitorMap()->contains(monitor)) {
		return "Selected monitor dosen't exists";
	} else {
		Q_EMIT m_tool->m_monitorSelectionMenu->requestMonitorToggled(true, monitor);
	}

	return "Success";
}

QString DataMonitorTool_API::disableMonitor(QString monitor)
{
	Q_ASSERT(m_tool->m_dataAcquisitionManager != nullptr);

	if(!m_tool->m_dataAcquisitionManager->getActiveMonitors().contains(monitor)) {
		return "Selected monitor dosen't exists or is already disabled";
	} else {
		Q_EMIT m_tool->m_monitorSelectionMenu->requestMonitorToggled(false, monitor);
	}

	return "Success";
}

void DataMonitorTool_API::setLogPath(QString path)
{
	Q_ASSERT(m_tool->m_dataMonitorSettings != nullptr);
	m_tool->m_dataMonitorSettings->dataLoggingMenu->filename = path;
	m_tool->m_dataMonitorSettings->dataLoggingMenu->dataLoggingFilePath->getLineEdit()->setText(path);
}

void DataMonitorTool_API::logAtPath(QString path)
{
	Q_ASSERT(m_tool->m_dataMonitorSettings != nullptr);
	Q_EMIT m_tool->m_dataMonitorSettings->dataLoggingMenu->requestDataLogging(path);
}

void DataMonitorTool_API::continuousLogAtPath(QString path)
{
	Q_ASSERT(m_tool->m_dataMonitorSettings != nullptr);
	setLogPath(path);
	m_tool->m_dataMonitorSettings->dataLoggingMenu->liveDataLoggingButton->onOffswitch()->setChecked(true);
}

void DataMonitorTool_API::stopContinuousLog()
{
	Q_ASSERT(m_tool->m_dataMonitorSettings != nullptr);
	m_tool->m_dataMonitorSettings->dataLoggingMenu->liveDataLoggingButton->onOffswitch()->setChecked(false);
}

void DataMonitorTool_API::importDataFromPath(QString path)
{
	Q_ASSERT(m_tool->m_dataMonitorSettings != nullptr);
	setLogPath(path);
	Q_EMIT m_tool->m_dataMonitorSettings->dataLoggingMenu->requestDataLoading(path);
}
