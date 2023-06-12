#ifndef REGMAPPLUGIN_H
#define REGMAPPLUGIN_H

#define SCOPY_PLUGIN_NAME RegmapPlugin
#define SCOPY_PLUGIN_PRIO 100

#include <QObject>
#include <pluginbase/plugin.h>
#include "pluginbase/pluginbase.h"
#include "scopy-regmapplugin_export.h"
#include <pluginbase/pluginbase.h>
#include <iio.h>

namespace Ui {

}

namespace scopy {
namespace regmap {
class JsonFormatedElement;
}

class SCOPY_REGMAPPLUGIN_EXPORT RegmapPlugin : public QObject, public PluginBase
{
	Q_OBJECT
	SCOPY_PLUGIN;

public:
	bool loadPage() override;
	bool loadIcon() override;
	void loadToolList() override;
	void unload() override;
	bool compatible(QString uri, QString category) override;
    void initPreferences() override;
    bool loadPreferencesPage() override;


	void initMetadata() override;
	QString description() override;

    QWidget* getTool();
public Q_SLOTS:
	bool onConnect() override;
	bool onDisconnect() override;

private:
    QWidget *m_registerMapWidget = nullptr;
    QList<iio_device*> *m_deviceList = nullptr;
    struct iio_device* getIioDevice(iio_context* ctx, const char *dev_name);
    bool isBufferCapable(iio_device *dev);

private Q_SLOTS:
    void handlePreferenceChange(QString, QVariant);



};
}

#endif // REGMAPPLUGIN_H