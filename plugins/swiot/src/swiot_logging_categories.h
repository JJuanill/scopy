#ifndef SCOPY_SWIOT_LOGGING_CATEGORIES_H
#define SCOPY_SWIOT_LOGGING_CATEGORIES_H

#include <QLoggingCategory>

#ifndef QT_NO_DEBUG_OUTPUT
Q_DECLARE_LOGGING_CATEGORY(CAT_SWIOT)
Q_DECLARE_LOGGING_CATEGORY(CAT_SWIOT_CONFIG)
Q_DECLARE_LOGGING_CATEGORY(CAT_SWIOT_AD74413R)
Q_DECLARE_LOGGING_CATEGORY(CAT_SWIOT_FAULTS)
Q_DECLARE_LOGGING_CATEGORY(CAT_SWIOT_MAX14906)
#else
#define CAT_SWIOT
#define CAT_SWIOT_CONFIG
#define CAT_SWIOT_AD74413R
#define CAT_SWIOT_FAULTS
#define CAT_SWIOT_MAX14906
#endif

#endif //SCOPY_SWIOT_LOGGING_CATEGORIES_H