#ifndef NAVIGATIONWIDGET_H
#define NAVIGATIONWIDGET_H

#include <QPushButton>
#include <QWidget>
#include "scopy-gui_export.h"

namespace scopy {
class SCOPY_GUI_EXPORT PageNavigationWidget : public QWidget {
	Q_OBJECT
public:
	PageNavigationWidget(bool hasHome = false, bool hasOpen = false, QWidget *parent = nullptr);
	~PageNavigationWidget();

	QPushButton *getHomeBtn();
	QPushButton *getBackwardBtn();
	QPushButton *getForwardBtn();
	QPushButton *getOpenBtn();
	void setHomeBtnVisible(bool en);
	void setOpenBtnVisible(bool en);

private:
	QPushButton *homeButton;
	QPushButton *backwardButton;
	QPushButton *forwardButton;
	QPushButton *openButton;

	void initUI(QWidget *parent);
};
}
#endif // NAVIGATIONWIDGET_H