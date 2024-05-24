#ifndef SETTINGSCUSTOMWIDGET_H
#define SETTINGSCUSTOMWIDGET_H

#include "customPushButton.h"

#include <QButtonGroup>
#include <QWidget>

using namespace scopy;

namespace Ui {
class SettingsPairWidget;
}

namespace scopy {
namespace m2k {
class SettingsPairWidget : public QWidget
{
	Q_OBJECT

public:
	explicit SettingsPairWidget(QWidget *parent = nullptr);
	~SettingsPairWidget();

	CustomPushButton *getGeneralSettingsBtn();
	QPushButton *getSettingsBtn();

private:
	Ui::SettingsPairWidget *m_ui;
};
} // namespace m2k
} // namespace scopy

#endif // SETTINGSPAIRWIDGET_H
