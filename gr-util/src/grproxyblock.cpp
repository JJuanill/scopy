/*
 * Copyright (c) 2024 Analog Devices Inc.
 *
 * This file is part of Scopy
 * (see
 * https://www.github.com/analogdevicesinc/scopy).
 *
 * This program is free software: you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation,
 * either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the
 * hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have
 * received a copy of the GNU General Public License
 * along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 *
 */

#include "grproxyblock.h"

#include "grlog.h"
#include "grtopblock.h"

using namespace scopy::grutil;
GRProxyBlock::GRProxyBlock(QObject *parent)
	: QObject(parent)
	, m_enabled(true)
{}

GRProxyBlock::~GRProxyBlock() {}

void GRProxyBlock::build_blks(GRTopBlock *top) {}

void GRProxyBlock::destroy_blks(GRTopBlock *top) { start_blk.clear(); }

void GRProxyBlock::connect_blk(GRTopBlock *top, GRProxyBlock *src)
{
	qInfo() << "created grfftfloatproc";
	if(src == nullptr) // block is a source
		return;
	int nrOfOutputs = src->getGrEndPoint()->output_signature()->min_streams();
	for(int i = 0; i < nrOfOutputs; i++) {

		int j = 0;
		int input = i;

		for(j = 0; j < start_blk.count(); j++) {
			int nrinputs = start_blk[j]->input_signature()->max_streams();
			if(input < nrinputs)
				break;
			input -= nrinputs;
		}
		top->connect(src->getGrEndPoint(), i, start_blk[j], input);
	}
}

void GRProxyBlock::disconnect_blk(GRTopBlock *top)
{
	for(int i = 0; i < start_blk.count(); i++)
		top->getGrBlock()->disconnect(start_blk[i]);
}

void GRProxyBlock::setEnabled(bool v)
{
	if(m_enabled == v)
		return;

	m_enabled = v;
}

bool GRProxyBlock::enabled() { return m_enabled; }

bool GRProxyBlock::built() { return (!start_blk.isEmpty()) || (end_blk != nullptr); }

QList<gr::basic_block_sptr> GRProxyBlock::getGrStartPoint() { return start_blk; }
gr::basic_block_sptr GRProxyBlock::getGrEndPoint() { return end_blk; }

#include "moc_grproxyblock.cpp"
