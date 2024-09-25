#ifndef SCOPY_DATABUFFERBUILDER_H
#define SCOPY_DATABUFFERBUILDER_H

#include "scopy-dac_export.h"
#include <QWidget>

namespace scopy {
namespace dac {
class DataBufferStrategyInterface;
class DataGuiStrategyInterface;
class DataBuffer;
class SCOPY_DAC_EXPORT DataBufferBuilder : public QObject
{
	Q_OBJECT
public:
	enum DS
	{ // Data Strategy
		NoDataStrategy,
		FileStrategy,
		CsvFileStrategy,
		MatlabFileStrategy,
		BinaryFileStrategy,
		SinewaveData,
	};

	enum GuiDS
	{ // GUI Strategy
		NoGuiStrategy,
		AutoGuiStrategy,
		FileGuiStrategy,
		SinewaveGuiStrategy,
	};

	explicit DataBufferBuilder(QObject *parent = nullptr);
	~DataBufferBuilder();

	/**
	 * @brief Build a DataBuffer based on the given parameters
	 * @return DataBuffer*
	 */
	DataBuffer *build();

	/**
	 * @brief Sets the data strategy (DS) that will be used.
	 * @param dataStrategy
	 */
	DataBufferBuilder &dataStrategy(DataBufferBuilder::DS dataStrategy);

	/**
	 * @brief Sets the data strategy (GuiDS) that will be used.
	 * @param dataStrategy
	 */
	DataBufferBuilder &guiStrategy(DataBufferBuilder::GuiDS guiStrategy);

	/**
	 * @brief Set the filename to be used by the DS.
	 * @param fullFilename
	 * @return
	 */
	DataBufferBuilder &file(QString fullFilename);

	/**
	 * @brief Sets the parent of the DataBuffer that will be built.
	 * @param parent
	 */
	DataBufferBuilder &parent(QWidget *parent);

private:
	DataBufferStrategyInterface *createDS();
	DataGuiStrategyInterface *createGuiGS();

	DataBufferBuilder::DS m_dataStrategy;
	DataBufferBuilder::GuiDS m_guiStrategy;
	QWidget *m_widgetParent;
	QString m_filename;

	bool checkFileValidity(QString filepath, DataBufferBuilder::DS ds);
};
} // namespace dac
} // namespace scopy
#endif // SCOPY_DATABUFFERBUILDER_H