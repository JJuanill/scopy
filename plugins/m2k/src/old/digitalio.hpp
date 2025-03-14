/*
 * Copyright (c) 2019 Analog Devices Inc.
 *
 * This file is part of Scopy
 * (see http://www.github.com/analogdevicesinc/scopy).
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DIGITAL_IO_H
#define DIGITAL_IO_H

#include "digitalchannel_manager.hpp"
#include "filter.hpp"
#include "m2ktool.hpp"
#include "pluginbase/apiobject.h"

#include <QList>
#include <QPair>
#include <QPushButton>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <string>
#include <vector>

extern "C"
{
	struct iio_context;
	struct iio_device;
	struct iio_channel;
	struct iio_buffer;
}

namespace Ui {
class DigitalIO;
class DigitalIoMenu;
class dioElement;
class dioGroup;
class dioChannel;
} // namespace Ui

namespace scopy::m2k {
class DigitalIO;
class DigitalIO_API;

class DigitalIoGroup : public QWidget
{
	friend class DigitalIO_API;

	Q_OBJECT
	int nr_of_channels;
	int ch_mask;
	int io_mask;
	DigitalIO *dio;

public:
	DigitalIoGroup(QString label, int ch_mask, int io_mask, DigitalIO *dio, QWidget *parent = 0);
	~DigitalIoGroup();
	Ui::dioElement *ui;
	QList<QPair<QWidget *, Ui::dioChannel *> *> chui;

Q_SIGNALS:
	void slider(int val);

private Q_SLOTS:
	void on_lineEdit_editingFinished();
	void on_horizontalSlider_valueChanged(int value);
	void on_comboBox_activated(int index);
	void changeDirection();
};

class DigitalIO : public M2kTool
{
	friend class DigitalIO_API;
	friend class ToolLauncher_API;

	Q_OBJECT

private:
	Ui::DigitalIO *ui;
	Filter *filt;
	bool offline_mode;
	QList<DigitalIoGroup *> groups;
	QTimer *poll;
	DIOManager *diom;
	int polling_rate = 500; // ms

	QPair<QWidget *, Ui::dioChannel *> *findIndividualUi(int ch);

private Q_SLOTS:
	void readPreferences() override;

public:
	explicit DigitalIO(Filter *filt, ToolMenuEntry *toolMenuItem, DIOManager *diom, QJSEngine *engine,
			   QWidget *parent, bool offline_mode = 0);
	~DigitalIO();
	void setDirection(int ch, int direction);
	void setOutput(int ch, int out);
	void setVisible(bool visible) override;

public Q_SLOTS:
	void run() override;
	void stop() override;
	void updateUi();
	void setDirection();
	void setOutput();
	void setSlider(int val);
	void lockUi();
	void startStop(bool);
Q_SIGNALS:
	void showTool();
};
} // namespace scopy::m2k

#endif // DIGITAL_IO_H
