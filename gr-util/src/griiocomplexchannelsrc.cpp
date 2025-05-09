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

#include "griiocomplexchannelsrc.h"

#include "griiodevicesource.h"
#include "grlog.h"
#include "grtopblock.h"

#include <QDebug>

using namespace scopy::grutil;
GRIIOComplexChannelSrc::GRIIOComplexChannelSrc(QString channelName, GRIIODeviceSource *dev, QString channelNameI,
					       QString channelNameQ, QObject *parent)
	: GRIIOChannel(channelName, dev, parent)
	, channelNameI(channelNameI)
	, channelNameQ(channelNameQ)
{
	auto m_iioCh = iio_device_find_channel(dev->iioDev(), channelNameI.toStdString().c_str(), false);
	fmt = iio_channel_get_data_format(m_iioCh);
}

void GRIIOComplexChannelSrc::build_blks(GRTopBlock *top)
{
	qDebug(SCOPY_GR_UTIL) << "Building GRIIOComplexChannelSrc";
	m_dev->addChannel(this);
	s2f[0] = gr::blocks::short_to_float::make();
	s2f[1] = gr::blocks::short_to_float::make();
	f2c = gr::blocks::float_to_complex::make();

	s2v = gr::blocks::stream_to_vector::make(sizeof(gr_complex), top->vlen());

	top->connect(s2f[0], 0, f2c, 0);
	top->connect(s2f[1], 0, f2c, 1);
	top->connect(f2c, 0, s2v, 0);
	start_blk.append(s2f[0]);
	start_blk.append(s2f[1]);
	end_blk = s2v;
}

void GRIIOComplexChannelSrc::destroy_blks(GRTopBlock *top)
{
	m_dev->removeChannel(this);
	end_blk = nullptr;
	s2f[0] = s2f[1] = nullptr;
	f2c = nullptr;
	s2v = nullptr;
	start_blk.clear();
}

const QString &GRIIOComplexChannelSrc::getChannelNameI() const { return channelNameI; }

const QString &GRIIOComplexChannelSrc::getChannelNameQ() const { return channelNameQ; }

const iio_data_format *GRIIOComplexChannelSrc::getFmt() const { return fmt; }
