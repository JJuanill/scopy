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

#include "adcacquisitionmanager.h"
#include <QSet>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(CAT_ACQTREENODE, "AcqTreeNode")

using namespace scopy;
using namespace scopy::adc;

GRIIOFloatChannelNode::GRIIOFloatChannelNode(GRTopBlockNode *top, GRIIOFloatChannelSrc *c, QObject *parent)
	: AcqTreeNode(c->getChannelName(), parent)
	, m_top(top)
{
	m_src = c;
}

GRIIOFloatChannelNode::~GRIIOFloatChannelNode() {}

GRIIOFloatChannelSrc *GRIIOFloatChannelNode::src() const { return m_src; }

GRTopBlockNode *GRIIOFloatChannelNode::top() const { return m_top; }

GRIIODeviceSourceNode::GRIIODeviceSourceNode(GRTopBlockNode *top, GRIIODeviceSource *d, QObject *parent)
	: AcqTreeNode(d->deviceName(), parent)
	, m_top(top)
{
	m_src = d;
}

GRIIODeviceSourceNode::~GRIIODeviceSourceNode() {}

GRIIODeviceSource *GRIIODeviceSourceNode::src() const { return m_src; }

GRTopBlockNode *GRIIODeviceSourceNode::top() const { return m_top; }

GRTopBlockNode::GRTopBlockNode(GRTopBlock *g, QObject *parent)
	: AcqTreeNode(g->name(), parent)
	, m_src(g)
{
	m_sync = new SyncController(this);
	m_data = g;
}

GRTopBlockNode::~GRTopBlockNode() {}

GRTopBlock *GRTopBlockNode::src() const { return m_src; }

SyncController *GRTopBlockNode::sync() const { return m_sync; }

iio_context *GRTopBlockNode::ctx() const { return m_ctx; }

void GRTopBlockNode::setCtx(iio_context *ctx) { m_ctx = ctx; }

AcqTreeNode::AcqTreeNode(QString name, QObject *parent)
	: QObject(parent)
{
	m_name = name;
	m_treeParent = nullptr;
}

AcqTreeNode::~AcqTreeNode() {}

void AcqTreeNode::addTreeChild(AcqTreeNode *t)
{
	m_treeChildren.append(t);
	connect(t, &AcqTreeNode::newChild, this, &AcqTreeNode::newChild);
	t->setTreeParent(this);
	for(AcqTreeNode *c : t->bfs()) {
		Q_EMIT newChild(c);
	}
}

bool AcqTreeNode::removeTreeChild(AcqTreeNode *t)
{
	bool b = m_treeChildren.removeAll(t);
	if(b) {
		// bfs delete all (?)
		Q_EMIT deletedChild(t);
		t->deleteLater(); // ???
	}
	return b;
}

void *AcqTreeNode::data() { return m_data; }

QList<AcqTreeNode *> AcqTreeNode::bfs()
{
	QList<AcqTreeNode *> list;
	QList<AcqTreeNode *> visited;
	visited.push_back(this);
	list.append(this);

	while(!list.empty()) {
		AcqTreeNode *current = list.front();
		list.pop_front();
		for(AcqTreeNode *child : qAsConst(current->m_treeChildren)) {
			if(!visited.contains(child)) {
				visited.push_back(child);
				list.push_back(child);
			}
		}
	}
	return visited;
}

QString AcqTreeNode::name() const { return m_name; }

AcqTreeNode *AcqTreeNode::treeParent() const { return m_treeParent; }

void AcqTreeNode::setTreeParent(AcqTreeNode *newTreeParent) { m_treeParent = newTreeParent; }

QList<AcqTreeNode *> AcqTreeNode::treeChildren() const { return m_treeChildren; }

AcqTreeNode *AcqTreeNode::treeRoot()
{
	AcqTreeNode *root = this;
	while(root->m_treeParent) {
		qInfo() << root->m_name;
		root = root->m_treeParent;
	}
	return root;
}

ImportFloatChannelNode::ImportFloatChannelNode(SnapshotRecipe rec, QObject *parent)
	: AcqTreeNode(rec.name, parent)
	, m_recipe(rec)
{}

ImportFloatChannelNode::~ImportFloatChannelNode() {}

SnapshotRecipe ImportFloatChannelNode::recipe() const { return m_recipe; }
