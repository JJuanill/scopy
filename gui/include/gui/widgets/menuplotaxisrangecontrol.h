#ifndef MENUPLOTAXISRANGECONTROL_H
#define MENUPLOTAXISRANGECONTROL_H

#include "menuspinbox.h"
#include "plotaxis.h"
#include "scopy-gui_export.h"
#include "spinbox_a.hpp"

#include <QWidget>

namespace scopy::gui {

class SCOPY_GUI_EXPORT MenuPlotAxisRangeControl : public QWidget
{
	Q_OBJECT
	QWIDGET_PAINT_EVENT_HELPER
public:
	MenuPlotAxisRangeControl(PlotAxis *, QWidget *parent = nullptr);
	~MenuPlotAxisRangeControl();
	double min();
	double max();
Q_SIGNALS:
	void intervalChanged(double, double);
public Q_SLOTS:
	void setMin(double);
	void setMax(double);

	MenuSpinbox *minSpinbox();
	MenuSpinbox *maxSpinbox();

	void addAxis(PlotAxis *ax);
	void removeAxis(PlotAxis *ax);

private:
	MenuSpinbox *m_min;
	MenuSpinbox *m_max;

	QMap<PlotAxis *, QList<QMetaObject::Connection>> connections;
};
} // namespace scopy::gui

#endif // MENUPLOTAXISRANGECONTROL_H