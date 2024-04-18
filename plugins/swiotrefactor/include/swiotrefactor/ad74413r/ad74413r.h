/*
 * Copyright (c) 2023 Analog Devices Inc.
 *
 * This file is part of Scopy
 * (see https://www.github.com/analogdevicesinc/scopy).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef AD74413R_H
#define AD74413R_H

#include "bufferlogic.h"
#include "bufferacquisitionhandler.h"
#include "pluginbase/toolmenuentry.h"
#include "readerthread.h"

#include <iio.h>

#include <gui/mapstackedwidget.h>
#include <gui/widgets/toolbuttons.h>
#include <gui/tooltemplate.h>
#include <gui/widgets/menucontrolbutton.h>
#include <gui/plotaxis.h>

#include <QVector>
#include <QWidget>
#include <plotinfo.h>
#include <spinbox_a.hpp>

#include <iioutil/connection.h>
#define MAX_CURVES_NUMBER 8
#define AD_NAME "ad74413r"
#define SWIOT_DEVICE_NAME "swiot"
#define MAX_SAMPLE_RATE 4800

namespace scopy {

namespace swiotrefactor {
class Ad74413r : public QWidget
{
	Q_OBJECT
public:
	explicit Ad74413r(QString uri = "", ToolMenuEntry *tme = 0, QWidget *parent = nullptr);

	~Ad74413r();

public Q_SLOTS:
	void onRunBtnPressed(bool toggled);
	void onSingleBtnPressed(bool toggled);

	void onReaderThreadFinished();
	void onSingleCaptureFinished();

	void onDiagnosticFunctionUpdated();

	void onActivateRunBtns(bool activate);

	void handleConnectionDestroyed();

	void onSamplingFrequencyUpdated(int channelId, int sampFreq);

Q_SIGNALS:
	void broadcastReadThreshold();
	void updateDiagSamplingFreq(QString data);
	void exportBtnClicked(QMap<int, bool> exportConfig);

	void activateExportButton();
	void activateRunBtns(bool activate);

	void configBtnPressed();
private Q_SLOTS:
	void onConfigBtnPressed();
	void showPlotLabels(bool b);
	void setupChannel(int chnlIdx, QString function);
	void onSamplingFreqComputed(double freq);
	void onBufferRefilled(QMap<int, QVector<double>> chnlData);
	void onChannelBtnChecked(int chnWidgetId, bool en);
	void samplingFreqWritten(bool en);

private:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void updateXData(int dataSize);
	void plotData(QVector<double> curveData, int chnlIdx);
	void createDevicesMap(iio_context *ctx);
	void setupConnections();
	void verifyChnlsChanges();
	void initTutorialProperties();
	void initPlotData();
	void setupToolTemplate();
	void initPlot();
	void setupDeviceBtn();
	void setupChannelBtn(MenuControlButton *btn, PlotChannel *ch, QString chnlId, int chnlIdx);
	void setupChannelsMenuControlBtn(MenuControlButton *btn, QString name);
	QPushButton *createConfigBtn();
	QWidget *createSettingsMenu(QWidget *parent);
	PlotAxis *createXChnlAxis(QPen pen, int xMin = -1, int xMax = 0);
	PlotAxis *createYChnlAxis(QPen pen, QString unitType = "V", int yMin = -1, int yMax = 1);

private:
	ToolMenuEntry *m_tme;
	PositionSpinButton *m_timespanSpin;

	QVector<bool> m_enabledChannels;

	QString m_uri;

	BufferLogic *m_swiotAdLogic;
	ReaderThread *m_readerThread;
	BufferAcquisitionHandler *m_acqHandler;
	CommandQueue *m_cmdQueue;

	struct iio_context *m_ctx;
	Connection *m_conn;

	ToolTemplate *m_tool;
	RunBtn *m_runBtn;
	SingleShotBtn *m_singleBtn;
	QPushButton *m_configBtn;
	GearBtn *m_settingsBtn;
	InfoBtn *m_infoBtn;

	PlotWidget *m_plot;
	TimePlotInfo *m_info;
	PlotSamplingInfo m_currentSamplingInfo;
	QMap<int, PlotChannel *> m_plotChnls;

	QMap<QString, iio_device *> m_iioDevicesMap;
	CollapsableMenuControlButton *m_devBtn;
	QButtonGroup *m_rightMenuBtnGrp;
	QButtonGroup *m_chnlsBtnGroup;

	MapStackedWidget *m_channelStack;

	bool m_fullyFilled = false;
	int m_currentChannelSelected = 0;
	QVector<double> m_xTime;

	QTimer *m_rstAcqTimer;
	const QString channelsMenuId = "channels";
};
} // namespace swiotrefactor
} // namespace scopy
#endif // AD74413R_H
