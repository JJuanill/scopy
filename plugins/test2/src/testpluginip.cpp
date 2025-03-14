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

#include "testpluginip.h"

#include "qlabel.h"
#include "qpushbutton.h"

#include <QLoggingCategory>
#include <QVBoxLayout>

#include <pluginbase/messagebroker.h>

Q_LOGGING_CATEGORY(CAT_TESTPLUGINIP, "TestPluginIp");
using namespace scopy;

bool TestPluginIp::compatible(QString m_param, QString category)
{
	qDebug(CAT_TESTPLUGINIP) << "compatible";
	return m_param.startsWith("ip:");
}

void TestPluginIp::unload()
{
	for(auto &tool : m_toolList) {
		delete tool;
	}
}

bool TestPluginIp::onConnect()
{
	qDebug(CAT_TESTPLUGINIP) << "connect";
	qDebug(CAT_TESTPLUGINIP) << m_toolList[0]->id() << m_toolList[0]->name();
	m_toolList[0]->setEnabled(true);
	m_toolList[0]->setName("IP tool1");
	m_toolList[0]->setRunBtnVisible(true);
	m_toolList[0]->setTool(new QLabel("TestPage IP Renamed"));

	m_toolList.append(SCOPY_NEW_TOOLMENUENTRY("IP tool2", "IP tool2", ""));
	m_toolList[1]->setEnabled(true);
	m_tool = new QWidget();
	QVBoxLayout *lay = new QVBoxLayout(m_tool);
	QPushButton *disc = new QPushButton("Disconnect");
	lay->addWidget(disc);
	QPushButton *btn = new QPushButton("LASTTOOOL testpage");
	lay->addWidget(btn);

	QPushButton *sendMessage = new QPushButton("SendMessage");
	lay->addWidget(sendMessage);

	connect(btn, &QPushButton::clicked, this, [=]() { requestTool(m_toolList[0]->id()); });
	connect(sendMessage, &QPushButton::clicked, this, [=]() {
		MessageBroker::GetInstance()->publish("TestPlugin", "testMessage");
		MessageBroker::GetInstance()->publish("broadcast", "testMessage");
		MessageBroker::GetInstance()->publish("TestPlugin2", "testMessage");
	});
	connect(disc, &QPushButton::clicked, this, [=]() { Q_EMIT disconnectDevice(); });

	Q_EMIT toolListChanged();
	m_toolList[1]->setTool(m_tool);

	return true;
}

bool TestPluginIp::onDisconnect()
{
	for(auto &tool : m_toolList) {
		tool->setEnabled(false);
		tool->setRunBtnVisible(false);
		QWidget *w = tool->tool();
		if(w) {
			tool->setTool(nullptr);
			delete w;
		}
	}
	m_toolList.removeLast();
	m_toolList[0]->setName("IP");

	qDebug(CAT_TESTPLUGINIP) << "disconnect";
	return true;
}

void TestPluginIp::postload() {}

bool TestPluginIp::loadIcon()
{
	static int count = 0;
	m_icon = new QLabel(QString::number(count));
	count++;
	m_icon->setStyleSheet("border-image: url(:/gui/icons/adalm.svg);");
	return true;
}

bool TestPluginIp::loadPage()
{
	m_page = new QLabel("TestPageIP");
	return true;
}

void TestPluginIp::loadToolList() { m_toolList.append(SCOPY_NEW_TOOLMENUENTRY("test2", "SecondPlugin", "")); }

void TestPluginIp::initMetadata()
{
	loadMetadata(
		R"plugin(
	{
	   "priority":2,
	   "category":[
	      "test"
	   ]
	}
		)plugin");
}

void TestPluginIp::saveSettings(QSettings &s) { s.setValue("ip", m_param); }

void TestPluginIp::loadSettings(QSettings &s) { qInfo(CAT_TESTPLUGINIP) << s.value("ip"); }

#include "moc_testpluginip.cpp"
