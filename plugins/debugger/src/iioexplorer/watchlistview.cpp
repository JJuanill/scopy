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

#include "watchlistview.h"
#include "debuggerloggingcategories.h"
#include "style_properties.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QScrollBar>
#include <style.h>

#define NAME_POS 0
#define VALUE_POS 1
#define TYPE_POS 2
#define PATH_POS 3
#define CLOSE_BTN_POS 4
#define DIVISION_REGION 5

using namespace scopy::debugger;

WatchListView::WatchListView(QWidget *parent)
	: QTableWidget(parent)
	, m_offsets({0, 0, 0, 0, 0})
	, m_apiObject(new WatchListView_API(this))
	, m_entryObjects(new QMap<QString, WatchListEntry *>())
{
	setupUi();
	connectSignalsAndSlots();
	m_apiObject->setObjectName("WatchListView");
}

void WatchListView::setupUi()
{
	setContentsMargins(0, 0, 0, 0);

	QStringList headers = {"Name", "Value", "Type", "Path", ""};
	setColumnCount(headers.size());
	setHorizontalHeaderLabels(headers);
	verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	verticalHeader()->setDefaultSectionSize(12);
	verticalHeader()->hide();
	horizontalScrollBar()->setDisabled(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// the header setup has to happen after setting the QTableWidget
	auto *header = new QHeaderView(Qt::Horizontal, this);
	setHorizontalHeader(header);
	header->setCascadingSectionResizes(true);
	header->setSectionResizeMode(NAME_POS, QHeaderView::Interactive);
	header->setSectionResizeMode(VALUE_POS, QHeaderView::Interactive);
	header->setSectionResizeMode(TYPE_POS, QHeaderView::Interactive);
	header->setSectionResizeMode(PATH_POS, QHeaderView::Interactive);
	header->setSectionResizeMode(CLOSE_BTN_POS, QHeaderView::Interactive);
	header->setFrameShadow(QFrame::Shadow::Plain);
	header->setFrameShape(QFrame::Shape::NoFrame);
	header->setFrameStyle(0);
	header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	Style::setStyle(this, style::properties::debugger::watchListView);
	Style::setStyle(header, style::properties::debugger::headerView, true, true);
	verticalHeader()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

void WatchListView::connectSignalsAndSlots()
{
	QObject::connect(this, &QTableWidget::cellClicked, this, [this](int row, int column) {
		Q_UNUSED(column)
		selectRow(row);
		QString path = item(row, PATH_POS)->text();
		qInfo(CAT_WATCHLIST) << "Selected" << path;

		if(!m_entryObjects->contains(path)) {
			qWarning(CAT_WATCHLIST) << "No object with name" << path
						<< "was found in the entry objects map, skipping cell selection.";
			return;
		}
		WatchListEntry *watchListEntry = (*m_entryObjects)[path];
		IIOStandardItem *item = watchListEntry->item();

		Q_EMIT selectedItem(item);
	});

	// this lambda is responsible for modifying the column offsets
	QObject::connect(horizontalHeader(), &QHeaderView::sectionResized, this,
			 [this](int logicalIndex, int oldSize, int newSize) {
				 Q_UNUSED(oldSize)
				 int originalSize = width() / DIVISION_REGION;
				 int offset = newSize - originalSize;
				 m_offsets[logicalIndex] = offset;
			 });
}

void WatchListView::saveSettings(QSettings &s) { m_apiObject->save(s); }

void WatchListView::loadSettings(QSettings &s) { m_apiObject->load(s); }

QMap<QString, WatchListEntry *> *WatchListView::watchListEntries() const { return m_entryObjects; }

void WatchListView::addToWatchlist(IIOStandardItem *item)
{
	auto entry = new WatchListEntry(item, this);
	m_entryObjects->insert(entry->path()->text(), entry);
	int row = rowCount();
	insertRow(row);
	setItem(row, NAME_POS, entry->name());
	setCellWidget(row, VALUE_POS, entry->valueUi());
	setItem(row, TYPE_POS, entry->type());
	setItem(row, PATH_POS, entry->path());
	selectRow(row);

	auto deleteButton = new QPushButton("X");
	QString style = "background-color: transparent; border: 0px;";
	deleteButton->setStyleSheet(style);
	setCellWidget(row, CLOSE_BTN_POS, deleteButton);
	deleteButton->setContentsMargins(0, 0, 0, 0);
	QObject::connect(deleteButton, &QPushButton::clicked, this, [this, entry, item]() {
		int row = -1;
		for(int i = 0; i < rowCount(); ++i) {
			if(this->item(i, PATH_POS)->text() == entry->path()->text()) {
				row = i;
				break;
			}
		}

		if(row != -1) {
			removeRow(row);
			entry->deleteLater();
		}

		item->setWatched(false);
		entry->deleteLater();
		m_entryObjects->remove(item->path());

		Q_EMIT removeItem(item);
	});
}

void WatchListView::removeFromWatchlist(IIOStandardItem *item)
{
	int row = -1;
	for(int i = 0; i < rowCount(); ++i) {
		if(this->item(i, PATH_POS)->text() == item->path()) {
			row = i;
			break;
		}
	}

	if(row != -1) {
		removeRow(row);
		WatchListEntry *entry = m_entryObjects->value(item->path(), nullptr);
		if(entry) {
			entry->deleteLater();
			m_entryObjects->remove(item->path());
		} else {
			qWarning(CAT_WATCHLIST)
				<< "Entry" << item->path() << "not present in entries, cannot remove from watchlist.";
		}
	}
}

void WatchListView::currentTreeSelectionChanged(IIOStandardItem *item)
{
	QString path = item->path();
	if(m_entryObjects->contains(path)) {
		// highlight path
		for(int i = 0; i < rowCount(); ++i) {
			if(this->item(i, PATH_POS)->text() == path) {
				selectRow(i);
				return;
			}
		}
	} else {
		// clear any highlights to avoid confusion
		clearSelection();
	}
}

void WatchListView::refreshWatchlist()
{
	for(auto object : qAsConst(*m_entryObjects)) {
		IIOStandardItem::Type type = object->item()->type();
		if(type == IIOStandardItem::ContextAttribute || type == IIOStandardItem::DeviceAttribute ||
		   type == IIOStandardItem::ChannelAttribute) {
			// leaf node
			IIOWidget *iioWidget = object->item()->getIIOWidgets()[0];
			iioWidget->getDataStrategy()->readAsync();
		}
	}
}

void WatchListView::resizeEvent(QResizeEvent *event)
{
	int w = width();
	int sectionWidth = w / DIVISION_REGION;
	setColumnWidth(NAME_POS, sectionWidth + m_offsets[NAME_POS]);
	setColumnWidth(VALUE_POS, sectionWidth + m_offsets[VALUE_POS]);
	setColumnWidth(TYPE_POS, sectionWidth + m_offsets[TYPE_POS]);
	setColumnWidth(PATH_POS, sectionWidth + m_offsets[PATH_POS]);
	setColumnWidth(CLOSE_BTN_POS, sectionWidth + m_offsets[CLOSE_BTN_POS]);

	QTableWidget::resizeEvent(event);
}

WatchListView_API::WatchListView_API(WatchListView *p)
	: ApiObject(p)
	, p(p)
{
	setObjectName("WatchListView_API");
}

QList<int> WatchListView_API::tableHeader() const
{
	QHeaderView *header = p->horizontalHeader();
	int columns = p->columnCount();
	QList<int> sizes;
	for(int i = 0; i < columns; ++i) {
		int size = header->sectionSize(i);
		sizes.append(size);
	}
	return sizes;
}

void WatchListView_API::setTableHeader(const QList<int> &newTableHeader)
{
	QHeaderView *header = p->horizontalHeader();
	int columns = p->columnCount();
	for(int i = 0; i < qMin(newTableHeader.size(), columns); ++i) {
		header->resizeSection(i, newTableHeader.at(i));
	}
}

QList<int> WatchListView_API::offsets() const { return p->m_offsets; }

void WatchListView_API::setOffsets(const QList<int> &newOffsets)
{
	p->m_offsets = newOffsets;
	int size = newOffsets.size();
	int sum = 0;

	for(int i = 0; i < size; ++i) {
		sum += newOffsets[i];
	}

	// abs(sum) < 1 is an error that can be accepted when computing offsets
	if(sum < -1 || sum > 1) {
		// the table is not fully visible, this is a backup measure to allow good display
		qWarning(CAT_WATCHLIST) << "The table contents are not fully visible, the offset that is not seen is"
					<< sum << "and it will be subtracted from the widest column from the last 2.";

		if(newOffsets[size - 1] > newOffsets[size - 2]) {
			p->m_offsets[size - 1] -= sum;
		} else {
			p->m_offsets[size - 2] -= sum;
		}
	}
}

#include "moc_watchlistview.cpp"
