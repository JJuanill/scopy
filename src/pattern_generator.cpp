#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <vector>
#include <string.h>

#include <iio.h>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QFile>
#include <QtQml/QJSEngine>
#include <QtQml/QQmlEngine>
#include <QDirIterator>
#include <QPushButton>
#include <QFileDialog>

///* pulseview and sigrok */
#include <boost/math/common_factor.hpp>
#include "pulseview/pv/mainwindow.hpp"
#include "pulseview/pv/devices/binarybuffer.hpp"
#include "pulseview/pv/devicemanager.hpp"
#include "pulseview/pv/toolbars/mainbar.hpp"
#include "libsigrokcxx/libsigrokcxx.hpp"
#include "libsigrokdecode/libsigrokdecode.h"

#include "pattern_generator.hpp"


using namespace std;
using namespace adiscope;

namespace pv {
class MainWindow;
class DeviceManager;
class Session;

namespace toolbars {
class MainBar;
}

namespace widgets {
class DeviceToolButton;
}
}

namespace sigrok {
class Context;
}

namespace adiscope {

/*QStringList PatternGenerator::digital_trigger_conditions = QStringList()
        << "edge-rising"
        << "edge-falling"
        << "edge-any"
        << "level-low"
        << "level-high";

*/

QStringList PatternGenerator::possibleSampleRates = QStringList()
                << "80000000"
                << "40000000"   << "20000000"  << "10000000"
                << "5000000"    << "2000000"   << "1000000"
                << "500000"     << "200000"    << "100000"
                << "50000"      << "20000"     << "10000"
                << "5000"       << "2000"      << "1000"
                << "500"        << "200"       << "100"
                << "50"         << "20"        << "10"
                << "5"          << "2"         << "1";

PatternGenerator::PatternGenerator(struct iio_context *ctx, Filter *filt,
                                   QPushButton *runBtn, QWidget *parent, bool offline_mode) :
	QWidget(parent),
	ctx(ctx),
	settings_group(new QButtonGroup(this)), menuRunButton(runBtn),
	ui(new Ui::PatternGenerator),
	pgSettings(new Ui::PGSettings),
	txbuf(0), sample_rate(100000), channel_enable_mask(0xffff),buffer(nullptr),
	buffer_created(0), currentUI(nullptr), offline_mode(offline_mode)
{

	// IIO
	if (!offline_mode) {
		dev = iio_context_find_device(ctx, "m2k-logic-analyzer-tx");
		channel_manager_dev = iio_context_find_device(ctx, "m2k-logic-analyzer");
		this->no_channels = iio_device_get_channels_count(channel_manager_dev);
	}

	// UI
	ui->setupUi(this);
	this->setAttribute(Qt::WA_DeleteOnClose, true);
	this->settings_group->setExclusive(true);
	settings_group->addButton(ui->btnChSettings);
	settings_group->addButton(ui->btnPGSettings);

	PatternFactory::init();

	pgSettings->setupUi(ui->pgSettings);
	connect(ui->btnChSettings, SIGNAL(pressed()), this, SLOT(toggleRightMenu()));
	connect(ui->btnPGSettings, SIGNAL(pressed()), this, SLOT(toggleRightMenu()));
	bufman = new PatternGeneratorBufferManager(&chm);

	bufui = new PatternGeneratorBufferManagerUi(ui->centralWidget,bufman,
	                ui->pgSettings,this);
	main_win = bufui->getPVWindow();
	chmui = new PatternGeneratorChannelManagerUI(ui->channelManagerWidget, main_win,
	                &chm,ui->cgSettings,this);

	ui->channelManagerWidgetLayout->addWidget(chmui);
	chmui->updateUi();
	chmui->setVisible(true);
	ui->rightWidget->setCurrentIndex(1);





	connect(chmui,SIGNAL(channelsChanged()),bufui,SLOT(updateUi()));
	/* connect(ui->btnGroupWithSelected,SIGNAL(clicked()),this,SLOT(on_btnGroupWithSelected_clicked()));
	 connect(ui->btnHideInactive,SIGNAL(clicked()),this,SLOT(on_btnHideInactive_clicked()));
	 connect(ui->btnExtendChannelManager,SIGNAL(clicked()),this,SLOT(on_extendChannelManager_PB_clicked()));*/
	/*
	    connect(ui->btnRunStop, SIGNAL(toggled(bool)), this, SLOT(startStop(bool)));
	    connect(runBtn, SIGNAL(toggled(bool)), ui->btnRunStop, SLOT(setChecked(bool)));
	    connect(ui->btnRunStop, SIGNAL(toggled(bool)), runBtn, SLOT(setChecked(bool)));
	    connect(ui->btnSingleRun, SIGNAL(pressed()), this, SLOT(singleRun()));
	*/
	bufui->updateUi();
}



PatternGenerator::~PatternGenerator()
{
	if (!offline_mode) {
		stopPatternGeneration();
	}

	for (auto var : patterns) {
		delete var;
	}

	delete ui;
	delete bufman;
	srd_exit();
}

void PatternGenerator::generatePattern()
{
	bufman->update();
}

void PatternGenerator::toggleRightMenu(QPushButton *btn)
{
	static rightMenuState rightMenuStatus=CLOSED;

	static QPushButton *prevButton;
	static bool prevMenuOpened = true;
	static bool menuOpened = true;
	static bool pgSettingsCheck = false;
	static bool chSettingsCheck = true;


	if (btn==ui->btnPGSettings) {
		ui->rightWidget->setCurrentIndex(0);
	} else {
		ui->rightWidget->setCurrentIndex(1);

	}

	if (prevButton==btn) {
		menuOpened = !menuOpened;
	} else {
		menuOpened = true;
	}

	if (menuOpened==false) {
		settings_group->setExclusive(false);
		ui->btnChSettings->setChecked(false);
		ui->btnPGSettings->setChecked(false);
		settings_group->setExclusive(true);
	}

	if (prevMenuOpened!=menuOpened) {
		ui->rightMenu->toggleMenu(menuOpened);
	}

	prevMenuOpened = menuOpened;
	prevButton=btn;

	if (menuOpened == true) {
		chmui->showHighlight(true);
	} else {
		chmui->showHighlight(false);
	}



}































bool PatternGenerator::startPatternGeneration(bool cyclic)
{

	/* Enable Tx channels*/
	char temp_buffer[12];

	if (!channel_manager_dev || !dev) {
		qDebug("Devices not found");
		return false;
	}

	qDebug("Setting channel direction");

	for (int j = 0; j < no_channels; j++) {
		if (channel_enable_mask & (1<<j)) {
			auto ch = iio_device_get_channel(channel_manager_dev, j);
			iio_channel_attr_write(ch, "direction", "out");
		}
	}

	qDebug("Setting sample rate");
	/* Set sample rate   */

	iio_device_attr_write(dev, "sampling_frequency",
	                      std::to_string(sample_rate).c_str());
	qDebug("Enabling channels");

	for (int j = 0; j < no_channels; j++) {
		auto ch = iio_device_get_channel(dev, j);
		iio_channel_enable(ch);
	}

	/* Create buffer     */
	qDebug("Creating buffer");
	txbuf = iio_device_create_buffer(dev, number_of_samples, cyclic);

	if (!txbuf) {
		qDebug("Could not create buffer - errno: %d - %s", errno, strerror(errno));
		return false;
	}

	buffer_created = true;
	short *p_dat;
	ptrdiff_t p_inc;

	int i = 0;

	for (p_dat = (short *)iio_buffer_start(txbuf); (p_dat < iio_buffer_end(txbuf));
	     (uint16_t *)p_dat++,i++) {
		*p_dat = buffer[i];
	}

	/* Push buffer       */
	auto number_of_bytes = iio_buffer_push(txbuf);
	qDebug("\nPushed %ld bytes to devices\r\n",number_of_bytes);
	return true;
}

void PatternGenerator::stopPatternGeneration()
{
	/* Destroy buffer */
	if (buffer_created == true) {
		iio_buffer_destroy(txbuf);
		buffer_created = false;
	}

	/* Reset Tx Channls*/
	auto nb_channels = iio_device_get_channels_count(channel_manager_dev);

	for (int j = 0; j < nb_channels; j++) {
		auto ch = iio_device_get_channel(channel_manager_dev, j);
		iio_channel_attr_write(ch, "direction", "in");
	}
}

void PatternGenerator::startStop(bool start)
{
	main_win->action_view_zoom_fit()->trigger();

	if (start) {
		if (startPatternGeneration(true)) {
			ui->btnRunStop->setText("Stop");
		} else {
			qDebug("Pattern generation failed");
		}
	} else {
		stopPatternGeneration();
		ui->btnRunStop->setText("Run");
	}
}

void PatternGenerator::singleRun()
{
	main_win->action_view_zoom_fit()->trigger();
	stopPatternGeneration();

	if (startPatternGeneration(false)) {
		uint32_t time_until_buffer_destroy = 1000 + (uint32_t)((number_of_samples/((
		                float)sample_rate))*1000.0);
		qDebug("Time until buffer destroy %d", time_until_buffer_destroy);
		QTimer::singleShot(time_until_buffer_destroy, this, SLOT(singleRunStop()));
		qDebug("Pattern generation single started");
		ui->btnSingleRun->setChecked(false);
	} else {
		qDebug("Pattern generation failed");
		ui->btnSingleRun->setChecked(true);
	}
}

void PatternGenerator::singleRunStop()
{
	qDebug("Pattern Generation stopped ");
	stopPatternGeneration();
	ui->btnSingleRun->setChecked(false);
}


void PatternGenerator::toggleRightMenu()
{
	toggleRightMenu(static_cast<QPushButton *>(QObject::sender()));
}

void PatternGenerator::on_btnHideInactive_clicked()
{
	if (chmui->isDisabledShown()) {
		chmui->hideDisabled();
		ui->btnHideInactive->setText("Show All");
	} else {
		chmui->showDisabled();
		ui->btnHideInactive->setText("Hide Inactive");

	}

	chmui->updateUi();
}

void PatternGenerator::on_btnGroupWithSelected_clicked()
{
	chmui->groupSplitSelected();
	chmui->updateUi();
}

void PatternGenerator::on_btnExtendChannelManager_clicked()
{
	if (chmui->areDetailsShown()) {
		chmui->hideDetails();
		ui->btnGroupWithSelected->setVisible(false);
		chmui->updateUi();
		//  ui->channelManagerWidget->toggleMenu(true);
	} else {
		chmui->showDetails();
		ui->btnGroupWithSelected->setVisible(true);
		chmui->updateUi();
		//  ui->channelManagerWidget->toggleMenu(true);
	}
}
}
