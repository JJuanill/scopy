#ifndef GUIDETAILSVIEW_H
#define GUIDETAILSVIEW_H

#include "iiostandarditem.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <gui/mousewheelwidgetguard.h>

namespace scopy::debugger {
class GuiDetailsView : public QWidget
{
	Q_OBJECT
public:
	explicit GuiDetailsView(QWidget *parent = nullptr);

	void setupUi();
	void setIIOStandardItem(IIOStandardItem *item);

private:
	IIOStandardItem *m_currentItem;
	MenuCollapseSection *m_detailsSeparator;
	QScrollArea *m_scrollArea;
	QWidget *m_scrollAreaContents;
	QList<IIOWidget *> m_currentWidgets;
	QList<QLabel *> m_detailsList;
	QSpacerItem *m_spacer;
	MouseWheelWidgetGuard *m_wheelGuard;

	void clearWidgets();
};
} // namespace scopy::debugger

#endif // GUIDETAILSVIEW_H