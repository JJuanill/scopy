#ifndef REGMAPSTYLEHELPER_HPP
#define REGMAPSTYLEHELPER_HPP

#include "qtitlespinbox.hpp"
#include "searchbarwidget.hpp"

#include <QCheckBox>
#include <QLabel>
#include <QMap>
#include <QObject>
#include <QPushButton>
#include <QSpinBox>

#include <register/bitfield/bitfielddetailedwidget.hpp>

#include <register/registersimplewidget.hpp>





namespace scopy::regmap {

class RegisterMapSettingsMenu;

class RegmapStyleHelper
{
protected:
    RegmapStyleHelper(QObject *parent = nullptr);
    ~RegmapStyleHelper();

public:
        // singleton
    RegmapStyleHelper(RegmapStyleHelper &other) = delete;
    void operator=(const RegmapStyleHelper &) = delete;
    static RegmapStyleHelper *GetInstance();

    static void initColorMap();
    static QString getColor(QString id);

    static void RegisterMapStyle(QWidget *widget);
    static void PartialFrameWidget(QWidget *widget);
    static void FrameWidget(QWidget *widget);
    static void bigTextLabelStyle(QLabel *label, QString objectName);
    static void labelStyle(QLabel *label, QString objectName);


    static QString regmapSettingsMenu(RegisterMapSettingsMenu *settings, QString objectName = "");
    static QString grayBackgroundHoverWidget(QWidget *widget, QString objectName = "");
    static QString BlueButton(QPushButton *btn, QString objectName = "");
    static QString checkboxStyle(QCheckBox *checkbox, QString objectName = "");
    static QString detailedBitFieldStyle(BitFieldDetailedWidget *widget, QString objectName);
    static QString simpleRegisterStyle(RegisterSimpleWidget *widget, QString objectName);
    static QString valueLabel(QLabel *label,QString objectName = "");
    static QString grayLabel(QLabel *label, QString objectName = "");
    static QString whiteSmallTextLable(QLabel *label, QString objectName = "");
    static QString frameBorderHover(QFrame *frame, QString objectName = "");
    static QString simpleWidgetWithButtonStyle(QWidget *widget, QString objectName = "");
    static QString simpleWidgetStyle(QWidget *widget, QString objectName = "");
    static QString comboboxStyle(QComboBox *combobox, QString objectName = "");
	static QString lineEditStyle(QLineEdit *lineEdit, QString objectName = "");
	static QString spinboxStyle(QSpinBox *spinbox, QString objectName = "");
	static QString titleSpinBoxStyle(QTitleSpinBox *spinbox, QString objectName = "");
	static QString searchBarStyle(SearchBarWidget *searchBar, QString objectName = "");

private:
    QMap<QString,QString> colorMap;
    static RegmapStyleHelper * pinstance_;
};
}
#endif // REGMAPSTYLEHELPER_HPP
