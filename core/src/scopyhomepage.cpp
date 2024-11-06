#include "scopyhomepage.h"

#include "scopyhomeinfopage.h"

#include "ui_scopyhomepage.h"

#include <QPushButton>

#include <deviceicon.h>

using namespace scopy;
ScopyHomePage::ScopyHomePage(QWidget *parent, PluginManager *pm)
	: QWidget(parent)
	, ui(new Ui::ScopyHomePage)
{
	ui->setupUi(this);
	auto &&is = ui->wInfoPageStack;
	auto &&hc = is->getHomepageControls();
	auto &&db = ui->wDeviceBrowser;
	auto &&deviceListWidget = ui->DeviceListWidget;
	auto &&deviceListHeader = ui->DeviceListHeader;
	auto &&devicesLabel = ui->devicesLabel;
	auto &&scanLabel = ui->scanLabel;
	add = new ScopyHomeAddPage(this, pm);

	is->add("home", new ScopyHomeInfoPage());
	is->add("add", add);

	QString headerStyle = QString(R"css(
										QWidget#DeviceListHeader {
											border-bottom: 1px solid &&HMC-Color-Layout-Divider&&;
										}
										)css"
	);
	headerStyle.replace("&&HMC-Color-Layout-Divider&&", StyleHelper::getColor("Dark-HMC-Color-Layout-Divider-Silent"));
	deviceListHeader->setStyleSheet(headerStyle);

	StyleHelper::SubtitleLarge(devicesLabel, "devicesLabel"); 
	StyleHelper::BodySmall(scanLabel, "scanLabel");

	QString cardStyle = QString(R"css(
										QWidget#DeviceListWidget {
											background-color: &&HMC-Color-Layout-Container&&;
											border: 1px solid &&HMC-Color-Layout-Divider&&;
											border-radius: 4px;
										}
										)css"
	);
	cardStyle.replace("&&HMC-Color-Layout-Container&&", StyleHelper::getColor("Dark-HMC-Color-Layout-Container"));
	cardStyle.replace("&&HMC-Color-Layout-Divider&&", StyleHelper::getColor("Dark-HMC-Color-Content-Silent"));
	deviceListWidget->setStyleSheet(cardStyle);

	QString infoStackStyle = QString(R"css(
										scopy--InfoPageStack {
											background-color: &&HMC-Color-Layout-Container&&;
											border: 1px solid &&HMC-Color-Layout-Divider&&;
											border-radius: 4px;
										}
										)css"
	);
	infoStackStyle.replace("&&HMC-Color-Layout-Container&&", StyleHelper::getColor("Dark-HMC-Color-Layout-Container"));
	infoStackStyle.replace("&&HMC-Color-Layout-Divider&&", StyleHelper::getColor("Dark-HMC-Color-Layout-Divider-Silent"));
	is->setStyleSheet(infoStackStyle);

	//	addDevice("dev1","dev1","descr1",new QPushButton("abc"),new QLabel("page1"));
	connect(hc, SIGNAL(goLeft()), db, SLOT(prevDevice()));
	connect(hc, SIGNAL(goRight()), db, SLOT(nextDevice()));
	connect(db, SIGNAL(requestDevice(QString, int)), is, SLOT(slideInKey(QString, int)));
	connect(db, SIGNAL(requestDevice(QString, int)), this, SIGNAL(requestDevice(QString)));
	connect(this, SIGNAL(deviceAddedToUi(QString)), add, SLOT(deviceAddedToUi(QString)));

	connect(add, &ScopyHomeAddPage::requestDevice, this, [=](QString id) { Q_EMIT db->requestDevice(id, -1); });
	connect(add, &ScopyHomeAddPage::newDeviceAvailable, this, [=](DeviceImpl *d) { Q_EMIT newDeviceAvailable(d); });

	connect(db, &DeviceBrowser::displayNameChanged, this, &ScopyHomePage::displayNameChanged);
}

ScopyHomePage::~ScopyHomePage() { delete ui; }

void ScopyHomePage::addDevice(QString id, Device *d)
{
	auto &&is = ui->wInfoPageStack;
	auto &&db = ui->wDeviceBrowser;
	db->addDevice(id, d);
	is->add(id, d);
	Q_EMIT deviceAddedToUi(id);
}

void ScopyHomePage::removeDevice(QString id)
{
	auto &&is = ui->wInfoPageStack;
	auto &&db = ui->wDeviceBrowser;
	db->removeDevice(id);
	is->remove(id);
}

void ScopyHomePage::viewDevice(QString id)
{
	auto &&db = ui->wDeviceBrowser;
	Q_EMIT db->requestDevice(id, -1);
}

void ScopyHomePage::connectDevice(QString id)
{
	auto &&db = ui->wDeviceBrowser;
	db->connectDevice(id);
}
void ScopyHomePage::disconnectDevice(QString id)
{
	auto &&db = ui->wDeviceBrowser;
	db->disconnectDevice(id);
}

QPushButton *ScopyHomePage::scanControlBtn() { return ui->btnScan; }

#include "moc_scopyhomepage.cpp"
