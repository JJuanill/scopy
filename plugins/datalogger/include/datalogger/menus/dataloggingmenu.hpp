#ifndef DATALOGGINGMENU_HPP
#define DATALOGGINGMENU_HPP

#include <QWidget>
#include <menuonoffswitch.h>
#include <progresslineedit.h>
#include "scopy-datalogger_export.h"

namespace scopy {
namespace datamonitor {

enum ProgressBarState
{
	SUCCESS,
	ERROR,
	BUSY
};

class SCOPY_DATALOGGER_EXPORT DataLoggingMenu : public QWidget
{
	friend class DataLogger_API;
	friend class DataMonitorStyleHelper;
	Q_OBJECT
public:
	explicit DataLoggingMenu(QWidget *parent = nullptr);

	bool liveDataLogging() const;

public Q_SLOTS:
	void updateDataLoggingStatus(ProgressBarState status);

Q_SIGNALS:
	void pathChanged(QString path);
	void requestLiveDataLogging(QString path);
	void requestDataLogging(QString path);
	void requestDataLoading(QString path);

private:
	QString filename;
	ProgressLineEdit *dataLoggingFilePath;
	QPushButton *dataLoggingBrowseBtn;
	QPushButton *dataLoggingBtn;
	QPushButton *dataLoadingBtn;
	MenuOnOffSwitch *liveDataLoggingButton;
	bool m_liveDataLogging = false;
	void chooseFile();
	void toggleButtonsEnabled(bool en);
};
} // namespace datamonitor
} // namespace scopy
#endif // DATALOGGINGMENU_HPP