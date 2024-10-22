/*
 * Copyright (c) 2024 Analog Devices Inc.
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
 *
 */

#include "rmsinstrument.h"
#include "measurementlabel.h"

#include <stylehelper.h>
#include <qwt_legend.h>
#include <gui/widgets/menuonoffswitch.h>
#include <gui/widgets/measurementpanel.h>
#include <gui/widgets/menusectionwidget.h>
#include <gui/widgets/verticalchannelmanager.h>
#include <gui/tooltemplate.h>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(CAT_PQM_RMS, "PqmRms")

using namespace scopy::pqm;

RmsInstrument::RmsInstrument(ToolMenuEntry *tme, QString uri, QWidget *parent)
	: QWidget(parent)
	, m_tme(tme)
	, m_uri(uri)
	, m_running(false)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QHBoxLayout *instrumentLayout = new QHBoxLayout(this);
	setLayout(instrumentLayout);
	StyleHelper::GetInstance()->initColorMap();

	ToolTemplate *tool = new ToolTemplate(this);
	tool->topContainer()->setVisible(true);
	tool->centralContainer()->setVisible(true);
	tool->topContainerMenuControl()->setVisible(false);
	instrumentLayout->addWidget(tool);

	QWidget *central = new QWidget(this);
	QHBoxLayout *centralLayout = new QHBoxLayout();
	central->setLayout(centralLayout);
	centralLayout->setSpacing(8);
	centralLayout->setContentsMargins(0, 0, 0, 0);

	QWidget *voltageWidget = new QWidget(central);
	voltageWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QVBoxLayout *voltageLayout = new QVBoxLayout();
	voltageWidget->setLayout(voltageLayout);
	voltageWidget->setStyleSheet("background-color:" + StyleHelper::getColor("UIElementBackground"));

	MeasurementsPanel *voltagePanel = new MeasurementsPanel(voltageWidget);
	createLabels(voltagePanel, m_chnls["voltage"].values(),
		     {"RMS", "Angle", "Deviation under", "Deviation over", "Pinst", "Pst", "Plt"});
	createLabels(voltagePanel, {DEVICE_NAME}, {"U2", "U0", "Sneg V", "Spos V", "Szro V"});
	voltagePanel->refreshUi();
	voltageLayout->addWidget(voltagePanel);

	m_voltagePlot = new PolarPlotWidget(voltageWidget);
	initPlot(m_voltagePlot);
	setupPlotChannels(m_voltagePlot, m_chnls["voltage"]);
	voltageLayout->addWidget(m_voltagePlot);

	QWidget *currentWidget = new QWidget(central);
	currentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QVBoxLayout *currentLayout = new QVBoxLayout();
	currentWidget->setLayout(currentLayout);
	currentWidget->setStyleSheet("background-color:" + StyleHelper::getColor("UIElementBackground"));

	MeasurementsPanel *currentPanel = new MeasurementsPanel(currentWidget);
	createLabels(currentPanel, m_chnls["current"].values(), {"RMS", "Angle"});
	createLabels(currentPanel, {DEVICE_NAME}, {"I2", "I0", "Sneg I", "Spos I", "Szro I"});
	currentPanel->refreshUi();
	currentLayout->addWidget(currentPanel);

	m_currentPlot = new PolarPlotWidget(currentWidget);
	initPlot(m_currentPlot);
	setupPlotChannels(m_currentPlot, m_chnls["current"]);
	currentLayout->addWidget(m_currentPlot);

	centralLayout->addWidget(voltageWidget);
	centralLayout->setStretchFactor(voltageWidget, 1);
	centralLayout->addWidget(currentWidget);
	centralLayout->setStretchFactor(currentWidget, 1);

	tool->addWidgetToCentralContainerHelper(central);

	m_runBtn = new RunBtn(this);
	m_singleBtn = new SingleShotBtn(this);

	tool->addWidgetToTopContainerHelper(m_runBtn, TTA_RIGHT);
	tool->addWidgetToTopContainerHelper(m_singleBtn, TTA_RIGHT);

	connect(m_tme, &ToolMenuEntry::runClicked, m_runBtn, &QAbstractButton::setChecked);
	connect(this, &RmsInstrument::enableTool, m_tme, &ToolMenuEntry::setRunning);
	connect(m_runBtn, &QAbstractButton::toggled, m_singleBtn, &QAbstractButton::setDisabled);
	connect(m_runBtn, SIGNAL(toggled(bool)), this, SLOT(toggleRms(bool)));
	connect(m_singleBtn, &QAbstractButton::toggled, m_runBtn, &QAbstractButton::setDisabled);
	connect(m_singleBtn, SIGNAL(toggled(bool)), this, SLOT(toggleRms(bool)));
}

RmsInstrument::~RmsInstrument()
{
	m_labels.clear();
	m_attributes.clear();
}

void RmsInstrument::createLabels(MeasurementsPanel *mPanel, QStringList chnls, QStringList labels, QString color)
{
	int chIdx = 0;
	QString c = color;
	for(const QString &ch : chnls) {
		if(chnls.size() > 1) {
			c = StyleHelper::getColor("CH" + QString::number(chIdx));
		}
		for(const QString &l : labels) {
			MeasurementLabel *ml = new MeasurementLabel(mPanel);
			if(!c.isEmpty()) {
				ml->setColor(QColor(c));
			}
			ml->setPrecision(6);
			ml->setName(l);
			m_labels[ch].append(ml);
			mPanel->addMeasurement(ml);
		}
		chIdx++;
	}
}

void RmsInstrument::updateLabels()
{
	QStringList chnls = m_attributes.keys();
	for(const QString &ch : chnls) {
		if(!m_labels.contains(ch)) {
			continue;
		}
		const QList<MeasurementLabel *> mlList = m_labels[ch];
		for(MeasurementLabel *l : mlList) {
			QString attrName = m_attrDictionary[l->name()];
			if(m_attributes[ch].contains(attrName)) {
				l->setValue(m_attributes[ch][attrName].toDouble());
			}
		}
	}
}

void RmsInstrument::initPlot(PolarPlotWidget *plot)
{
	plot->setBgColor(QColor(StyleHelper::getColor("ScopyBackground")));
	plot->setAzimuthInterval(0.0, 360.0, 30.0);
	plot->plot()->insertLegend(new QwtLegend(), QwtPolarPlot::LeftLegend);
}

void RmsInstrument::setupPlotChannels(PolarPlotWidget *plot, QMap<QString, QString> channels, int thickness)
{
	int chIdx = 0;
	for(const QString &ch : channels) {
		QPen chPen = QPen(QColor(StyleHelper::getColor("CH" + QString::number(chIdx))), 1);
		PolarPlotChannel *plotCh = new PolarPlotChannel(channels.key(ch), chPen, plot, this);
		plotCh->setThickness(thickness);
		plotCh->setEnabled(true);
		plot->addPlotChannel(plotCh);
		chIdx++;
	}
}

QVector<QwtPointPolar> RmsInstrument::getPolarPlotPoints(QString chnlType)
{
	double maxRms = -1.0;
	bool okAngle = false, okRms = false;
	QVector<QwtPointPolar> plotPoints;
	// convert the attributes to double
	for(const QString &ch : m_chnls[chnlType]) {
		double angle = m_attributes[ch]["angle"].toDouble(&okAngle);
		double rms = m_attributes[ch]["rms"].toDouble(&okRms);
		if(!okRms || !okAngle) {
			plotPoints.clear();
			qWarning(CAT_PQM_RMS) << "Something went wrong with the rms/angle conversion!";
			qWarning(CAT_PQM_RMS)
				<< "Angle = " + m_attributes[ch]["angle"] + " RMS = " + m_attributes[ch]["rms"];
			return plotPoints;
		}
		maxRms = (rms > maxRms) ? rms : maxRms;
		plotPoints.push_back({angle, rms});
	}
	// normalization of rms values in the [0,1] range
	for(QwtPointPolar &attr : plotPoints) {
		double normalizedRadius = (maxRms > 0) ? (attr.radius() / maxRms) : 0.0;
		attr.setRadius(normalizedRadius);
	}

	return plotPoints;
}

void RmsInstrument::updatePlot(PolarPlotWidget *plot, QString type)
{
	QwtPointPolar originPoint(0.0, 0.0);
	QVector<QVector<QwtPointPolar>> plotData;
	QVector<QwtPointPolar> plotPoints = getPolarPlotPoints(type);
	for(const QwtPointPolar &point : plotPoints) {
		plotData.push_back({originPoint, point});
	}
	plot->setData(plotData);
}

void RmsInstrument::stop() { m_runBtn->setChecked(false); }

void RmsInstrument::toggleRms(bool en)
{
	m_running = en;
	if(en) {
		ResourceManager::open("pqm" + m_uri, this);
	} else {
		ResourceManager::close("pqm" + m_uri);
	}
	Q_EMIT enableTool(en);
}

void RmsInstrument::onAttrAvailable(QMap<QString, QMap<QString, QString>> data)
{
	if(!m_running) {
		return;
	}
	m_attributes = data;
	updateLabels();
	updatePlot(m_voltagePlot, "voltage");
	updatePlot(m_currentPlot, "current");
	if(m_singleBtn->isChecked()) {
		m_singleBtn->setChecked(false);
	}
}

#include "moc_rmsinstrument.cpp"
