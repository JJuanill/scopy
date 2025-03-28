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

#ifndef SYNCCONTROLLER_H
#define SYNCCONTROLLER_H
#include "scopy-adc_export.h"
#include <QMap>
#include <QObject>

namespace scopy {
namespace adc {

class SyncController;
class SCOPY_ADC_EXPORT SyncInstrument
{
public:
	virtual void setSyncController(SyncController *s) = 0;
	virtual void setSyncMode(bool) = 0;
	virtual bool syncMode() = 0;

	virtual void onArm() = 0;
	virtual void onDisarm() = 0;

	virtual void setSyncSingleShot(bool) = 0;
	virtual void setSyncBufferSize(uint32_t) = 0;
};

class SCOPY_ADC_EXPORT SyncController : public QObject
{
	Q_OBJECT
public:
	SyncController(QObject *parent = nullptr);
	~SyncController();

	void addInstrument(SyncInstrument *s);
	void removeInstrument(SyncInstrument *s);

	void arm(SyncInstrument *si);
	void disarm(SyncInstrument *si);

	void setBufferSize(SyncInstrument *si, uint32_t newBufferSize);
	void setSingleShot(SyncInstrument *si, bool newSingleShot);

Q_SIGNALS:
	void resetAll();

private:
	QMap<SyncInstrument *, bool> m_syncState;
	QList<SyncInstrument *> m_syncInstruments;
	//	Q_PROPERTY(float sampleRate READ sampleRate WRITE sampleRate);
};
} // namespace adc
} // namespace scopy
#endif // SYNCCONTROLLER_H
