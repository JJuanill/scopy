#ifndef DEVICEICON_H
#define DEVICEICON_H
#include <QAbstractButton>
#include <device.h>
#include "scopy-core_export.h"

namespace scopy {
class SCOPY_CORE_EXPORT DeviceIcon : public QAbstractButton {
	Q_OBJECT
public:
	DeviceIcon(QWidget *parent = nullptr) : QAbstractButton(parent) {};
	virtual ~DeviceIcon() {};
public Q_SLOTS:
//	virtual Device* device() = 0;
	virtual void setConnected(bool) = 0;
Q_SIGNALS:
	void refresh();
	void forget();
};
}

#endif // DEVICEICON_H
