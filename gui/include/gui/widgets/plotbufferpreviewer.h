#ifndef PLOTBUFFERPREVIEWER_H
#define PLOTBUFFERPREVIEWER_H

#include "scopy-gui_export.h"
#include <QWidget>
#include "plotwidget.h"
#include "buffer_previewer.hpp"

namespace scopy {

class SCOPY_GUI_EXPORT PlotBufferPreviewer : public QWidget
{
	Q_OBJECT
public:
	explicit PlotBufferPreviewer(PlotWidget *p, BufferPreviewer *b, QWidget *parent = nullptr);
	~PlotBufferPreviewer();

	void setManualDataLimits(bool enabled);
	void updateDataLimits(double min, double max);

public Q_SLOTS:
	void updateBufferPreviewer();

private:
	double m_bufferDataLimitMin;
	double m_bufferDataLimitMax;
	double m_bufferPrevInitMin;
	double m_bufferPrevInitMax;
	bool m_manualDataLimits;

	void setupBufferPreviewer();
	PlotWidget *m_plot;
	BufferPreviewer *m_bufferPreviewer;
};

} // namespace scopy

#endif // PLOTBUFFERPREVIEWER_H
