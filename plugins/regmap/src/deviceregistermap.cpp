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

#include "deviceregistermap.hpp"

#include "dynamicWidget.h"
#include "logging_categories.h"

#include <QLineEdit>
#include <QPushButton>
#include <iio.h>
#include <qboxlayout.h>
#include <qcheckbox.h>
#include <stylehelper.h>
#include <toolbuttons.h>
#include <utils.h>
#include "readwrite/iioregisterreadstrategy.hpp"
#include "readwrite/iioregisterwritestrategy.hpp"
#include "register/registerdetailedwidget.hpp"
#include "register/registermodel.hpp"
#include "registercontroller.hpp"
#include "registermaptemplate.hpp"
#include "registermapvalues.hpp"
#include "regmapstylehelper.hpp"
#include "search.hpp"
#include "utils.hpp"

#include <iio.h>

#include <QLineEdit>
#include <QPushButton>
#include <qboxlayout.h>
#include <qcheckbox.h>
#include <style.h>

#include <src/readwrite/fileregisterwritestrategy.hpp>
#include <src/recyclerview/registermaptable.hpp>
#include <utils.h>

#include "style_properties.h"

using namespace scopy;
using namespace regmap;

DeviceRegisterMap::DeviceRegisterMap(RegisterMapTemplate *registerMapTemplate, RegisterMapValues *registerMapValues,
				     QWidget *parent)
	: QWidget(parent)
	, registerMapValues(registerMapValues)
	, registerMapTemplate(registerMapTemplate)
{
	layout = new QVBoxLayout(this);
	Utils::removeLayoutMargins(layout);
	setLayout(layout);

	tool = new ToolTemplate(this);
	Utils::removeLayoutMargins(tool->layout());
	tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tool->topContainerMenuControl()->hide();
	tool->bottomCentral()->setVisible(true);
	layout->addWidget(tool);

	Style::setBackgroundColor(this, Style::getAttribute(json::theme::interactive_secondary_disabled));

	initSettings();

	registerController = new RegisterController(this);
	Style::setStyle(registerController, style::properties::regmap::registercontroller, true, true);

	QWidget *controllerWidget = new QWidget(this);
	QHBoxLayout *controllerLayout = new QHBoxLayout(controllerWidget);
	Utils::removeLayoutMargins(controllerLayout);
	controllerLayout->setSpacing(0);

	if(registerMapTemplate) {
		registerController->setHasMap(true);
		QObject::connect(registerController, &RegisterController::toggleDetailedMenu, this,
				 [=](bool toggled) { tool->openBottomContainerHelper(toggled); });

		QWidget *registerMapTable = new QWidget();
		QVBoxLayout *registerMapTableLayout = new QVBoxLayout(registerMapTable);
		registerMapTableLayout->setMargin(0);
		registerMapTableLayout->setSpacing(0);
		Utils::removeLayoutMargins(registerMapTableLayout);
		registerMapTable->setLayout(registerMapTableLayout);
		tool->addWidgetToCentralContainerHelper(registerMapTable);

		QWidget *tableHeadWidget = new QWidget(this);
		QHBoxLayout *tableHeadWidgetLayout = new QHBoxLayout(tableHeadWidget);
		tableHeadWidgetLayout->setSpacing(4);
		tableHeadWidgetLayout->setMargin(2);
		tableHeadWidget->setLayout(tableHeadWidgetLayout);

		QWidget *registerTableHead = new QWidget(tableHeadWidget);
		Style::setBackgroundColor(registerTableHead,
					  Style::getAttribute(json::theme::interactive_secondary_disabled));

		QHBoxLayout *registerTableHeadLayout = new QHBoxLayout(registerTableHead);
		registerTableHeadLayout->setSpacing(0);
		registerTableHead->setLayout(registerTableHeadLayout);

		QLabel *registerTableHeadName = new QLabel("Register", registerTableHead);
		registerTableHeadLayout->addWidget(registerTableHeadName);
		registerTableHead->setFixedWidth(180);

		QWidget *colBitCount = new QWidget(tableHeadWidget);
		Style::setBackgroundColor(colBitCount,
					  Style::getAttribute(json::theme::interactive_secondary_disabled));

		QHBoxLayout *tableHead = new QHBoxLayout(colBitCount);
		colBitCount->setLayout(tableHead);

		for(int i = registerMapTemplate->bitsPerRow() - 1; i >= 0; i--) {
			tableHead->addWidget(new QLabel("Bit" + QString::number(i)), 1);
		}

		tableHeadWidgetLayout->addWidget(registerTableHead, 1);
		tableHeadWidgetLayout->addWidget(colBitCount, 8);
		registerMapTableLayout->addWidget(tableHeadWidget);

		registerMapTableWidget = new RegisterMapTable(registerMapTemplate->getRegisterList(), this);
		registerMapTable->setProperty("tutorial_name", "REGISTER_MAP");

		QWidget *aux = registerMapTableWidget->getWidget();
		if(aux) {
			registerMapTableLayout->addWidget(aux);
		}

		QObject::connect(registerMapTableWidget, &RegisterMapTable::registerSelected, this,
				 [=](uint32_t address) {
					 registerController->blockSignals(true);
					 registerMapTableWidget->setRegisterSelected(address);
					 registerChanged(registerMapTemplate->getRegisterTemplate(address));
					 registerController->blockSignals(false);
					 if(autoread) {
						 Q_EMIT registerMapValues->requestRead(address);
					 }
				 });

		QObject::connect(registerController, &RegisterController::registerAddressChanged, this,
				 [=](uint32_t address) {
					 registerChanged(registerMapTemplate->getRegisterTemplate(address));
					 registerMapTableWidget->scrollTo(address);
					 if(autoread) {
						 Q_EMIT registerMapValues->requestRead(address);
					 }
				 });
	}

	QObject::connect(registerController, &RegisterController::requestRead, registerMapValues,
			 [=](uint32_t address) { Q_EMIT registerMapValues->requestRead(address); });
	QObject::connect(
		registerController, &RegisterController::requestWrite, registerMapValues,
		[=](uint32_t address, uint32_t value) { Q_EMIT registerMapValues->requestWrite(address, value); });
	QObject::connect(registerMapValues, &RegisterMapValues::registerValueChanged, this,
			 [=](uint32_t address, uint32_t value) {
				 int regSize = 8;
				 if(registerMapTemplate) {
					 regSize = registerMapTemplate->getRegisterTemplate(0)->getWidth();
				 }
				 registerController->registerValueChanged(Utils::convertToHexa(value, regSize));
				 if(registerMapTemplate) {
					 registerMapTableWidget->valueUpdated(address, value);
					 registerDetailedWidget->updateBitFieldsValue(value);
				 }
			 });

	tool->addWidgetToCentralContainerHelper(controllerWidget);
	controllerLayout->addWidget(registerController);

	initSimpleTutorial();
	if(registerMapTemplate) {
		registerChanged(registerMapTemplate->getRegisterList()->first());
		initTutorial();

	} else {
		tool->centralContainer()->layout()->addItem(
			new QSpacerItem(10, 10, QSizePolicy::Preferred, QSizePolicy::Expanding));
	}

	connect(this, &DeviceRegisterMap::tutorialAborted, this, &DeviceRegisterMap::abortTutorial);
}

DeviceRegisterMap::~DeviceRegisterMap()
{
	delete layout;
	if(registerController)
		delete registerController;
	if(registerMapTableWidget)
		delete registerMapTableWidget;
	if(docRegisterMapTable)
		delete docRegisterMapTable;
	if(registerDetailedWidget)
		delete registerDetailedWidget;
	delete tool;
}

void DeviceRegisterMap::registerChanged(RegisterModel *regModel)
{
	registerController->registerChanged(regModel->getAddress());
	registerController->registerValueChanged("N/R");

	if(registerDetailedWidget) {
		delete registerDetailedWidget;
	}

	registerDetailedWidget = new RegisterDetailedWidget(regModel, tool->bottomContainer());
	registerDetailedWidget->setProperty("tutorial_name", "DETAILED_BITFIELDS");
	registerDetailedWidget->setMaximumHeight(140);
	tool->bottomCentral()->layout()->addWidget(registerDetailedWidget);

	QObject::connect(registerDetailedWidget, &RegisterDetailedWidget::bitFieldValueChanged, registerController,
			 &RegisterController::registerValueChanged);
	QObject::connect(registerController, &RegisterController::valueChanged, this, [=](QString val) {
		registerDetailedWidget->updateBitFieldsValue(Utils::convertQStringToUint32(val));
	});

	if(registerMapValues) {
		uint32_t address = regModel->getAddress();
		if(registerMapValues->hasValue(address)) {
			uint32_t value = registerMapValues->getValueOfRegister(address);
			registerDetailedWidget->updateBitFieldsValue(value);
			registerController->registerValueChanged(Utils::convertToHexa(value, regModel->getWidth()));
		}
	}
}

void DeviceRegisterMap::toggleAutoread(bool toggled) { autoread = toggled; }

void DeviceRegisterMap::applyFilters(QString filter)
{
	if(registerMapTemplate) {
		registerMapTableWidget->setFilters(
			Search::searchForRegisters(registerMapTemplate->getRegisterList(), filter));
	}
}

bool DeviceRegisterMap::hasTemplate()
{
	if(registerMapTemplate) {
		return true;
	}

	return false;
}

bool DeviceRegisterMap::getAutoread() { return autoread; }
void DeviceRegisterMap::startTutorial() { registerController->startTutorial(); }
void DeviceRegisterMap::startSimpleTutorial() { registerController->startSimpleTutorial(); }

void DeviceRegisterMap::initSettings()
{
	QObject::connect(this, &DeviceRegisterMap::requestRead, registerMapValues, &RegisterMapValues::requestRead);
	QObject::connect(this, &DeviceRegisterMap::requestRegisterDump, registerMapValues,
			 &RegisterMapValues::registerDump);
	QObject::connect(this, &DeviceRegisterMap::requestWrite, registerMapValues, &RegisterMapValues::requestWrite);
}

void DeviceRegisterMap::initTutorial()
{

	controllerTutorialFinish = connect(registerController, &RegisterController::tutorialFinished, this, [=]() {
		QWidget *parent = Util::findContainingWindow(this);
		tutorial = new gui::TutorialBuilder(this, ":/registermap/tutorial_chapters.json", "device_register_map",
						    parent);

		connect(tutorial, &gui::TutorialBuilder::finished, this, [=]() { Q_EMIT tutorialFinished(); });
		connect(tutorial, &gui::TutorialBuilder::aborted, this, &DeviceRegisterMap::tutorialAborted);
		tutorial->setTitle("Tutorial");
		tutorial->start();
	});
}

void DeviceRegisterMap::initSimpleTutorial()
{
	connect(registerController, &RegisterController::tutorialAborted, this, &DeviceRegisterMap::tutorialAborted);
	connect(registerController, &RegisterController::simpleTutorialFinished, this,
		&DeviceRegisterMap::simpleTutorialFinished);
}

void DeviceRegisterMap::abortTutorial() { disconnect(controllerTutorialFinish); }
