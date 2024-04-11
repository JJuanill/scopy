#include "datamonitor_api.hpp"
#include "datamonitorsettings.hpp"

using namespace scopy::datamonitor;

Q_LOGGING_CATEGORY(CAT_DATAMONITOR_API, "DataMonitor_API")

DataMonitor_API::DataMonitor_API(DataMonitorPlugin *dataMonitorPlugin)
	: ApiObject()
	, m_dataMonitorPlugin(dataMonitorPlugin)
{}

DataMonitor_API::~DataMonitor_API() {}

QString DataMonitor_API::showAvailableDevices()
{
	Q_ASSERT(m_dataMonitorPlugin->m_dataAcquisitionManager != nullptr);
	QString availableDevices = "";
	foreach(QString monitor, m_dataMonitorPlugin->m_dataAcquisitionManager->getDataMonitorMap()->keys()) {
		if(!availableDevices.contains(monitor)) {
			availableDevices += monitor + "\n";
		}
	}
	return availableDevices;
}

QString DataMonitor_API::showMonitorsOfDevice(QString device)
{
	Q_ASSERT(m_dataMonitorPlugin->m_dataAcquisitionManager != nullptr);
	QString availableDevices = "";

	foreach(QString monitor, m_dataMonitorPlugin->m_dataAcquisitionManager->getDataMonitorMap()->keys()) {
		if(monitor.contains(device)) {
			availableDevices += m_dataMonitorPlugin->m_dataAcquisitionManager->getDataMonitorMap()
						    ->value(monitor)
						    ->getName() +
				"\n";
		}
	}

	return availableDevices;
}

QString DataMonitor_API::showAvailableMonitors()
{
	Q_ASSERT(m_dataMonitorPlugin->m_dataAcquisitionManager != nullptr);
	QString availableDevices = "";

	foreach(QString monitor, m_dataMonitorPlugin->m_dataAcquisitionManager->getDataMonitorMap()->keys()) {
		availableDevices += monitor + "\n";
	}

	return availableDevices;
}

QString DataMonitor_API::enableMonitor(QString monitor)
{
	Q_ASSERT(m_dataMonitorPlugin->m_dataAcquisitionManager != nullptr);

	if(!m_dataMonitorPlugin->m_dataAcquisitionManager->getDataMonitorMap()->contains(monitor)) {
		return "Selected monitor dosen't exists";
	} else {
		m_dataMonitorPlugin->m_dataAcquisitionManager->updateActiveMonitors(true, monitor);
	}

	return "Success";
}

QString DataMonitor_API::disableMonitor(QString monitor)
{
	Q_ASSERT(m_dataMonitorPlugin->m_dataAcquisitionManager != nullptr);

	if(!m_dataMonitorPlugin->m_dataAcquisitionManager->getActiveMonitors().contains(monitor)) {
		return "Selected monitor dosen't exists or is already disabled";
	} else {
		m_dataMonitorPlugin->m_dataAcquisitionManager->updateActiveMonitors(false, monitor);
	}

	return "Success";
}

void DataMonitor_API::setRunning(bool running)
{
	Q_ASSERT(m_dataMonitorPlugin != nullptr);
	m_dataMonitorPlugin->toggleRunState(running);
}

void DataMonitor_API::clearData()
{
	Q_ASSERT(m_dataMonitorPlugin->m_dataAcquisitionManager != nullptr);
	m_dataMonitorPlugin->m_dataAcquisitionManager->clearMonitorsData();
}

QString DataMonitor_API::createTool()
{
	Q_ASSERT(m_dataMonitorPlugin != nullptr);
	m_dataMonitorPlugin->addNewTool();

	return "Tool created";
}

QString DataMonitor_API::getToolList()
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());
	QString tools = "";
	for(ToolMenuEntry *tool : m_dataMonitorPlugin->m_toolList) {
		if(tool->pluginName() == "DataMonitorPlugin") {
			tools += tool->name() + "\n";
		}
	}
	return tools;
}

QString DataMonitor_API::enableMonitorOfTool(QString toolName, QString monitor)
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());

	ToolMenuEntry *tool = ToolMenuEntry::findToolMenuEntryByName(m_dataMonitorPlugin->m_toolList, toolName);
	if(tool) {
		DatamonitorTool *monitorTool = dynamic_cast<DatamonitorTool *>(tool->tool());
		if(!monitorTool->m_dataAcquisitionManager->getDataMonitorMap()->contains(monitor)) {
			return "Selected monitor dosen't exists";
		} else {
			Q_EMIT monitorTool->m_monitorSelectionMenu->requestMonitorToggled(true, monitor);
		}
	}
	return "OK";
}

QString DataMonitor_API::disableMonitorOfTool(QString toolName, QString monitor)
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());

	ToolMenuEntry *tool = ToolMenuEntry::findToolMenuEntryByName(m_dataMonitorPlugin->m_toolList, toolName);
	if(tool) {
		DatamonitorTool *monitorTool = dynamic_cast<DatamonitorTool *>(tool->tool());
		if(!monitorTool->m_dataAcquisitionManager->getDataMonitorMap()->contains(monitor)) {
			return "Selected monitor dosen't exists";
		} else {
			Q_EMIT monitorTool->m_monitorSelectionMenu->requestMonitorToggled(false, monitor);
		}
	}
	return "OK";
}

void DataMonitor_API::setLogPathOfTool(QString toolName, QString path)
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());

	ToolMenuEntry *tool = ToolMenuEntry::findToolMenuEntryByName(m_dataMonitorPlugin->m_toolList, toolName);
	if(tool) {
		DatamonitorTool *monitorTool = dynamic_cast<DatamonitorTool *>(tool->tool());
		monitorTool->m_dataMonitorSettings->dataLoggingMenu->filename = path;
		monitorTool->m_dataMonitorSettings->dataLoggingMenu->dataLoggingFilePath->getLineEdit()->setText(path);
	}
}

void DataMonitor_API::logAtPathForTool(QString toolName, QString path)
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());

	ToolMenuEntry *tool = ToolMenuEntry::findToolMenuEntryByName(m_dataMonitorPlugin->m_toolList, toolName);
	if(tool) {
		DatamonitorTool *monitorTool = dynamic_cast<DatamonitorTool *>(tool->tool());
		Q_EMIT monitorTool->m_dataMonitorSettings->dataLoggingMenu->requestDataLogging(path);
	}
}

void DataMonitor_API::continuousLogAtPathForTool(QString toolName, QString path)
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());

	ToolMenuEntry *tool = ToolMenuEntry::findToolMenuEntryByName(m_dataMonitorPlugin->m_toolList, toolName);
	if(tool) {
		DatamonitorTool *monitorTool = dynamic_cast<DatamonitorTool *>(tool->tool());
		setLogPathOfTool(toolName, path);
		monitorTool->m_dataMonitorSettings->dataLoggingMenu->liveDataLoggingButton->onOffswitch()->setChecked(
			true);
	}
}

void DataMonitor_API::stopContinuousLogForTool(QString toolName)
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());

	ToolMenuEntry *tool = ToolMenuEntry::findToolMenuEntryByName(m_dataMonitorPlugin->m_toolList, toolName);
	if(tool) {
		DatamonitorTool *monitorTool = dynamic_cast<DatamonitorTool *>(tool->tool());
		monitorTool->m_dataMonitorSettings->dataLoggingMenu->liveDataLoggingButton->onOffswitch()->setChecked(
			false);
	}
}

void DataMonitor_API::importDataFromPathForTool(QString toolName, QString path)
{
	Q_ASSERT(!m_dataMonitorPlugin->m_toolList.isEmpty());

	ToolMenuEntry *tool = ToolMenuEntry::findToolMenuEntryByName(m_dataMonitorPlugin->m_toolList, toolName);
	if(tool) {
		DatamonitorTool *monitorTool = dynamic_cast<DatamonitorTool *>(tool->tool());
		setLogPathOfTool(toolName, path);
		Q_EMIT monitorTool->m_dataMonitorSettings->dataLoggingMenu->requestDataLoading(path);
	}
}
