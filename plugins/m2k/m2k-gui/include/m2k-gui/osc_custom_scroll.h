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
#ifndef OSCCUSTOMSCROLL_H
#define OSCCUSTOMSCROLL_H

#include "scopy-m2k-gui_export.h"
#include <QEvent>
#include <QScrollArea>
#include <QScroller>
#include <QTimer>

namespace scopy {
class SCOPY_M2K_GUI_EXPORT OscCustomScrollArea : public QScrollArea
{
	Q_OBJECT
public:
	OscCustomScrollArea(QWidget *parent = 0);
	~OscCustomScrollArea();

public Q_SLOTS:
	void enterEvent(QEvent *);
	void leaveEvent(QEvent *);

private:
	QScroller *scroll;
	bool inside;
	// QScrollBar *bar;
	bool disableCursor;
};
} // namespace scopy

#endif // OSCCUSTOMSCROLL_H
