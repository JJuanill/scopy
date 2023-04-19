#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <QPair>
#include <QTimer>
#include <QWidget>
#include <QFileDialog>
#include <QRadioButton>
//#include <QJSEngine>
#include <spinbox_a.hpp>
#include <QElapsedTimer>

//#include <filter.hpp>
//#include <tool_launcher.hpp>
//#include <tool.hpp>
//#include "gui/tool_view_builder.hpp"
//#include "dataloggerreaderthread.hpp"

/* libm2k includes */
//#include <libm2k/context.hpp>
#include <libm2k/analog/m2kanalogin.hpp>
#include <libm2k/m2k.hpp>
#include <libm2k/analog/dmm.hpp>
//#include <scopyExceptionHandler.h>
#include <libm2k/m2kexceptions.hpp>
//#include <core/toolmenuitem.h>
#include <src/refactoring/tool/tool_view.hpp>
#include <src/dataloggerreaderthread.hpp>

namespace libm2k::context {
class ContextBuilder;
}

namespace adiscope {
namespace gui {
class GenericMenu;
class ChannelManager;
class DataLoggerGenericMenu;
}

class ChannelWidget;
class CustomColQGridLayout;
class ChannelMonitorComponent;
class DataLogger_API;
class DataLoggerController;
class CustomSwitch;

class DataLogger : public QWidget
{
	friend class DataLogger_API;

	Q_OBJECT

public:
	explicit DataLogger(struct iio_context *ctx, QWidget *parent = nullptr);
	~DataLogger();

	adiscope::gui::ToolView* getToolView();

	CustomSwitch* showAllSWitch;
	int getPrecision();
	void setPrecision(int precision);
	int getValueReadingTimeInterval();
	void setNativeDialogs(bool nativeDialogs);

private:
	int VALUE_READING_TIME_INTERVAL = 1000;

	QWidget* parent;
	QTimer *m_timer;
	QElapsedTimer *m_elapsed;

	QLineEdit *precisionValue;
	PositionSpinButton *recording_timer;

	QList<QColor> m_colors;
	QMap<int,QColor> m_color;
	DataLoggerController *dataLoggerController;
	adiscope::gui::ToolView* m_toolView;
	CustomColQGridLayout* m_customColGrid;
	libm2k::context::Context* m_context;
	std::vector<libm2k::analog::DMM*> m_dmmList;
	adiscope::gui::GenericMenu* m_generalSettingsMenu;
	adiscope::gui::ChannelManager* m_monitorChannelManager;
	QMap<int,ChannelMonitorComponent*> m_activeChannels;
	DataLoggerReaderThread *readerThread;
	QColor generateColor();
	void initMonitorToolView();
	QColor getChannelColor(int chId);
	adiscope::gui::GenericMenu* generateMenu(QString title, QColor* color);
	std::vector<libm2k::analog::DMM*> getDmmList(libm2k::context::Context* m2k_context);
	void createConnections(adiscope::gui::DataLoggerGenericMenu* mainMenu,adiscope::gui::DataLoggerGenericMenu* menu,adiscope::ChannelMonitorComponent* monitor);

Q_SIGNALS:
	void precisionChanged(int precision);
	void channelValueUpdated(int chId, double value,QString nameOfUnitOfMeasure,QString symbolOfUnitOfMeasure);
	void recordingIntervalChanged(double recordingInterval);
	void toggleAll(bool showAll);
	void disableActivateChannel(bool disable);
};

}
#endif // DATALOGGER_H
