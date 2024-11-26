#include "harmoniccalibration.h"
#include "qtconcurrentrun.h"

#include <widgets/horizontalspinbox.h>
#include <stylehelper.h>
#include <admtstylehelper.h>

static int acquisitionUITimerRate = 50;
static int calibrationUITimerRate = 50;
static int utilityTimerRate = 1000;

static int bufferSize = 1;
static int dataGraphSamples = 100;
static int tempGraphSamples = 100;
static bool running = false;
static double *dataGraphValue;
static double *tempGraphValue;

static int cycleCount = 11;
static int samplesPerCycle = 256;
static int totalSamplesCount = cycleCount * samplesPerCycle;
static int samplesCollected = 0;
static bool isStartAcquisition = false;
static bool isStartMotor = false;
static bool isPostCalibration = false;
static bool isCalculatedCoeff = false;
static bool isAngleDisplayFormat = false;
static bool resetToZero = true;
static bool hasMTDiagnostics = false;

static double motorTimeUnit = 1.048576; // t = 2^24/16Mhz
static int motorMicrostepPerRevolution = 51200;
static int motorfCLK = 16000000; // 16Mhz

// static bool autoCalibrate = false;

static uint32_t h1MagDeviceRegister = 0x15;
static uint32_t h2MagDeviceRegister = 0x17;
static uint32_t h3MagDeviceRegister = 0x19;
static uint32_t h8MagDeviceRegister = 0x1B;
static uint32_t h1PhaseDeviceRegister = 0x16;
static uint32_t h2PhaseDeviceRegister = 0x18;
static uint32_t h3PhaseDeviceRegister = 0x1A;
static uint32_t h8PhaseDeviceRegister = 0x1C;

static int acquisitionDisplayLength = 200;
static QVector<double> acquisitionAngleList, graphDataList, graphPostDataList;

static const QColor scopyBlueColor = scopy::StyleHelper::getColor("ScopyBlue");
static const QColor channel0Color = scopy::StyleHelper::getColor("CH0");
static const QColor sineColor = QColor("#85e94c");
static const QColor cosineColor = QColor("#91e6cf");
static const QColor faultLEDColor = QColor("#c81a28");
static const QColor gpioLEDColor = scopyBlueColor;
static const QColor statusLEDColor = QColor("#2e9e6f");

static const QPen scopyBluePen(scopyBlueColor);
static const QPen channel0Pen(channel0Color);
static const QPen sinePen(sineColor);
static const QPen cosinePen(cosineColor);

static map<string, string> deviceRegisterMap;
static map<string, int> generalRegisterMap;
static QString deviceName = "";
static QString deviceType = "";
static bool is5V = false;

static double H1_MAG_ANGLE, H2_MAG_ANGLE, H3_MAG_ANGLE, H8_MAG_ANGLE, H1_PHASE_ANGLE, H2_PHASE_ANGLE, H3_PHASE_ANGLE, H8_PHASE_ANGLE;
static uint32_t H1_MAG_HEX, H2_MAG_HEX, H3_MAG_HEX, H8_MAG_HEX, H1_PHASE_HEX, H2_PHASE_HEX, H3_PHASE_HEX, H8_PHASE_HEX;

using namespace scopy;
using namespace scopy::admt;

HarmonicCalibration::HarmonicCalibration(ADMTController *m_admtController, bool isDebug, QWidget *parent)
	: QWidget(parent)
	, isDebug(isDebug)
	, m_admtController(m_admtController)
{
	readDeviceProperties();
	initializeMotor();

	rotationChannelName = m_admtController->getChannelId(ADMTController::Channel::ROTATION);
	angleChannelName = m_admtController->getChannelId(ADMTController::Channel::ANGLE);
	countChannelName = m_admtController->getChannelId(ADMTController::Channel::COUNT);
	temperatureChannelName = m_admtController->getChannelId(ADMTController::Channel::TEMPERATURE);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QHBoxLayout *lay = new QHBoxLayout(this);
    tool = new ToolTemplate(this);
	setLayout(lay);
    lay->setMargin(0);
	tabWidget = new QTabWidget(this);
	tabWidget->addTab(tool, "Acquisition");

    openLastMenuButton = new OpenLastMenuBtn(dynamic_cast<MenuHAnim *>(tool->rightContainer()), true, this);
	rightMenuButtonGroup = dynamic_cast<OpenLastMenuBtn *>(openLastMenuButton)->getButtonGroup();

    settingsButton = new GearBtn(this);
	lay->insertWidget(1, tabWidget);

    runButton = new RunBtn(this);

	QPushButton *resetGMRButton = new QPushButton(this);
	resetGMRButton->setText("GMR Reset");
	ADMTStyleHelper::TopContainerButtonStyle(resetGMRButton);
	connect(resetGMRButton, &QPushButton::clicked, this, &HarmonicCalibration::GMRReset);

	rightMenuButtonGroup->addButton(settingsButton);

	#pragma region Raw Data Widget
	QScrollArea *rawDataScroll = new QScrollArea(this);
	rawDataScroll->setWidgetResizable(true);
	QWidget *rawDataWidget = new QWidget(rawDataScroll);
	rawDataScroll->setWidget(rawDataWidget);
	QVBoxLayout *rawDataLayout = new QVBoxLayout(rawDataWidget);
	rawDataLayout->setMargin(0);
	rawDataWidget->setLayout(rawDataLayout);

	MenuSectionWidget *rotationWidget = new MenuSectionWidget(rawDataWidget);
	MenuSectionWidget *angleWidget = new MenuSectionWidget(rawDataWidget);
	MenuSectionWidget *countWidget = new MenuSectionWidget(rawDataWidget);
	MenuSectionWidget *tempWidget = new MenuSectionWidget(rawDataWidget);
	rotationWidget->contentLayout()->setSpacing(8);
	angleWidget->contentLayout()->setSpacing(8);
	countWidget->contentLayout()->setSpacing(8);
	tempWidget->contentLayout()->setSpacing(8);
	MenuCollapseSection *rotationSection = new MenuCollapseSection("AMR Angle", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, rotationWidget);
	MenuCollapseSection *angleSection = new MenuCollapseSection("GMR Angle", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, angleWidget);
	MenuCollapseSection *countSection = new MenuCollapseSection("Count", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, countWidget);
	MenuCollapseSection *tempSection = new MenuCollapseSection("Sensor Temperature", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, tempWidget);
	rotationSection->contentLayout()->setSpacing(8);
	angleSection->contentLayout()->setSpacing(8);
	countSection->contentLayout()->setSpacing(8);
	tempSection->contentLayout()->setSpacing(8);

	rotationWidget->contentLayout()->addWidget(rotationSection);
	angleWidget->contentLayout()->addWidget(angleSection);
	countWidget->contentLayout()->addWidget(countSection);
	tempWidget->contentLayout()->addWidget(tempSection);

	rotationValueLabel = new QLabel(rotationSection);
	StyleHelper::MenuControlLabel(rotationValueLabel, "rotationValueLabel");
	angleValueLabel = new QLabel(angleSection);
	StyleHelper::MenuControlLabel(angleValueLabel, "angleValueLabel");
	countValueLabel = new QLabel(countSection);
	StyleHelper::MenuControlLabel(countValueLabel, "countValueLabel");
	tempValueLabel = new QLabel(tempSection);
	StyleHelper::MenuControlLabel(tempValueLabel, "tempValueLabel");
	
	rotationValueLabel->setText("--.--°");
	angleValueLabel->setText("--.--°");
	countValueLabel->setText("--");
	tempValueLabel->setText("--.-- °C");

	rotationSection->contentLayout()->addWidget(rotationValueLabel);
	angleSection->contentLayout()->addWidget(angleValueLabel);
	countSection->contentLayout()->addWidget(countValueLabel);
	tempSection->contentLayout()->addWidget(tempValueLabel);

	#pragma region Acquisition Motor Configuration Section Widget
	MenuSectionWidget *motorConfigurationSectionWidget = new MenuSectionWidget(rawDataWidget);
	MenuCollapseSection *motorConfigurationCollapseSection = new MenuCollapseSection("Motor Configuration", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, motorConfigurationSectionWidget);
	motorConfigurationCollapseSection->header()->toggle();
	motorConfigurationSectionWidget->contentLayout()->addWidget(motorConfigurationCollapseSection);

	motorMaxVelocitySpinBox = new HorizontalSpinBox("Max Velocity", convertVMAXtoRPS(rotate_vmax), "rps", motorConfigurationSectionWidget);
	motorMaxVelocitySpinBox->setValue(1);
	motorAccelTimeSpinBox = new HorizontalSpinBox("Acceleration Time", convertAMAXtoAccelTime(amax), "sec", motorConfigurationSectionWidget);
	motorAccelTimeSpinBox->setValue(1);
	motorMaxDisplacementSpinBox = new HorizontalSpinBox("Max Displacement", dmax, "", motorConfigurationSectionWidget);

	m_calibrationMotorRampModeMenuCombo = new MenuCombo("Ramp Mode", motorConfigurationSectionWidget);
	auto calibrationMotorRampModeCombo = m_calibrationMotorRampModeMenuCombo->combo();
	calibrationMotorRampModeCombo->addItem("Position", QVariant(ADMTController::MotorRampMode::POSITION));
	calibrationMotorRampModeCombo->addItem("Ramp Mode 1", QVariant(ADMTController::MotorRampMode::RAMP_MODE_1));
	ADMTStyleHelper::ComboBoxStyle(calibrationMotorRampModeCombo);

	motorConfigurationCollapseSection->contentLayout()->setSpacing(8);
	motorConfigurationCollapseSection->contentLayout()->addWidget(motorMaxVelocitySpinBox);
	motorConfigurationCollapseSection->contentLayout()->addWidget(motorAccelTimeSpinBox);
	motorConfigurationCollapseSection->contentLayout()->addWidget(motorMaxDisplacementSpinBox);
	motorConfigurationCollapseSection->contentLayout()->addWidget(m_calibrationMotorRampModeMenuCombo);
	#pragma endregion

	#pragma region Acquisition Motor Control Section Widget
	MenuSectionWidget *motorControlSectionWidget = new MenuSectionWidget(rawDataWidget);
	MenuCollapseSection *motorControlCollapseSection = new MenuCollapseSection("Motor Control", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, motorControlSectionWidget);
	motorControlSectionWidget->contentLayout()->setSpacing(8);
	motorControlSectionWidget->contentLayout()->addWidget(motorControlCollapseSection);
	QLabel *currentPositionLabel = new QLabel("Current Position", motorControlSectionWidget);
	StyleHelper::MenuSmallLabel(currentPositionLabel, "currentPositionLabel");
	acquisitionMotorCurrentPositionLabel = new QLabel("--.--", motorControlSectionWidget);
	acquisitionMotorCurrentPositionLabel->setAlignment(Qt::AlignRight);
	applyLabelStyle(acquisitionMotorCurrentPositionLabel);

	motorTargetPositionSpinBox = new HorizontalSpinBox("Target Position", target_pos, "", motorControlSectionWidget);

	motorControlCollapseSection->contentLayout()->setSpacing(8);
	motorControlCollapseSection->contentLayout()->addWidget(currentPositionLabel);
	motorControlCollapseSection->contentLayout()->addWidget(acquisitionMotorCurrentPositionLabel);
	motorControlCollapseSection->contentLayout()->addWidget(motorTargetPositionSpinBox);
	#pragma endregion

	rawDataLayout->addWidget(rotationWidget);
	rawDataLayout->addWidget(angleWidget);
	rawDataLayout->addWidget(countWidget);
	rawDataLayout->addWidget(tempWidget);
	rawDataLayout->addWidget(motorConfigurationSectionWidget);
	rawDataLayout->addWidget(motorControlSectionWidget);
	rawDataLayout->addStretch();
	#pragma endregion

	#pragma region Acquisition Graph Section Widget
	MenuSectionWidget *acquisitionGraphSectionWidget = new MenuSectionWidget(this);
	MenuCollapseSection *acquisitionGraphCollapseSection = new MenuCollapseSection("Captured Data", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, acquisitionGraphSectionWidget);
	acquisitionGraphSectionWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	acquisitionGraphSectionWidget->contentLayout()->addWidget(acquisitionGraphCollapseSection);

	acquisitionGraphPlotWidget = new PlotWidget();
	ADMTStyleHelper::PlotWidgetStyle(acquisitionGraphPlotWidget);
	acquisitionGraphPlotWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	acquisitionGraphPlotWidget->setShowXAxisLabels(true);
	acquisitionGraphPlotWidget->setShowYAxisLabels(true);
	acquisitionGraphPlotWidget->showAxisLabels();

	acquisitionXPlotAxis = new PlotAxis(QwtAxis::XBottom, acquisitionGraphPlotWidget, channel0Pen);
	acquisitionXPlotAxis->setInterval(0, acquisitionDisplayLength);
	acquisitionYPlotAxis = new PlotAxis(QwtAxis::YLeft, acquisitionGraphPlotWidget, channel0Pen);
	acquisitionYPlotAxis->setInterval(0, 360);

	acquisitionAnglePlotChannel = new PlotChannel("Angle", channel0Pen, acquisitionXPlotAxis, acquisitionYPlotAxis);

	acquisitionGraphPlotWidget->addPlotChannel(acquisitionAnglePlotChannel);
	acquisitionAnglePlotChannel->setEnabled(true);
	acquisitionGraphPlotWidget->selectChannel(acquisitionAnglePlotChannel);

	acquisitionGraphPlotWidget->replot();

	acquisitionGraphCollapseSection->contentLayout()->addWidget(acquisitionGraphPlotWidget);
	#pragma endregion

	#pragma region General Setting
	QScrollArea *generalSettingScroll = new QScrollArea(this);
	generalSettingScroll->setWidgetResizable(true);
	QWidget *generalSettingWidget = new QWidget(generalSettingScroll);
	generalSettingScroll->setWidget(generalSettingWidget);
	QVBoxLayout *generalSettingLayout = new QVBoxLayout(generalSettingWidget);
	generalSettingLayout->setMargin(0);
	generalSettingWidget->setLayout(generalSettingLayout);

	header = new MenuHeaderWidget(deviceName + " " + deviceType, scopyBluePen, this);

	// General Setting Widget
	MenuSectionWidget *generalWidget = new MenuSectionWidget(generalSettingWidget);
	generalWidget->contentLayout()->setSpacing(8);
	MenuCollapseSection *generalSection = new MenuCollapseSection("Data Acquisition", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, generalWidget);
	generalSection->header()->toggle();
	generalSection->contentLayout()->setSpacing(8);
	generalWidget->contentLayout()->addWidget(generalSection);

	// Graph Update Interval
	QLabel *graphUpdateIntervalLabel = new QLabel(generalSection);
	graphUpdateIntervalLabel->setText("Graph Update Interval (ms)");
	StyleHelper::MenuSmallLabel(graphUpdateIntervalLabel, "graphUpdateIntervalLabel");
	graphUpdateIntervalLineEdit = new QLineEdit(generalSection);
	ADMTStyleHelper::LineEditStyle(graphUpdateIntervalLineEdit);
	graphUpdateIntervalLineEdit->setText(QString::number(acquisitionUITimerRate));

	connectLineEditToNumber(graphUpdateIntervalLineEdit, acquisitionUITimerRate, 1, 5000);

	generalSection->contentLayout()->addWidget(graphUpdateIntervalLabel);
	generalSection->contentLayout()->addWidget(graphUpdateIntervalLineEdit);

	// Data Sample Size
	QLabel *displayLengthLabel = new QLabel("Display Length", generalSection);
	StyleHelper::MenuSmallLabel(displayLengthLabel);
	displayLengthLineEdit = new QLineEdit(generalSection);
	ADMTStyleHelper::LineEditStyle(displayLengthLineEdit);
	displayLengthLineEdit->setText(QString::number(acquisitionDisplayLength));

	connectLineEditToNumber(displayLengthLineEdit, acquisitionDisplayLength, 1, 2048);

	generalSection->contentLayout()->addWidget(displayLengthLabel);
	generalSection->contentLayout()->addWidget(displayLengthLineEdit);

	MenuSectionWidget *sequenceWidget = new MenuSectionWidget(generalSettingWidget);
	MenuCollapseSection *sequenceSection = new MenuCollapseSection("Sequence", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, sequenceWidget);
	sequenceWidget->contentLayout()->addWidget(sequenceSection);
	sequenceSection->contentLayout()->setSpacing(8);

	sequenceTypeMenuCombo = new MenuCombo("Sequence Type", sequenceSection);
	QComboBox *sequenceTypeComboBox = sequenceTypeMenuCombo->combo();
	sequenceTypeComboBox->addItem("Mode 1", QVariant(0));
	if(deviceType.toStdString() == "Automotive")
	{
		sequenceTypeComboBox->addItem("Mode 2", QVariant(1));
	}
	ADMTStyleHelper::ComboBoxStyle(sequenceTypeComboBox);

	conversionTypeMenuCombo = new MenuCombo("Conversion Type", sequenceSection);
	QComboBox *conversionTypeComboBox = conversionTypeMenuCombo->combo();
	conversionTypeComboBox->addItem("Continuous conversions", QVariant(0));
	conversionTypeComboBox->addItem("One-shot conversion", QVariant(1));
	ADMTStyleHelper::ComboBoxStyle(conversionTypeComboBox);

	convertSynchronizationMenuCombo = new MenuCombo("Convert Synchronization", sequenceSection);
	QComboBox *convertSynchronizationComboBox = convertSynchronizationMenuCombo->combo();
	convertSynchronizationComboBox->addItem("Enabled", QVariant(1));
	convertSynchronizationComboBox->addItem("Disabled", QVariant(0));
	ADMTStyleHelper::ComboBoxStyle(convertSynchronizationComboBox);

	angleFilterMenuCombo = new MenuCombo("Angle Filter", sequenceSection);
	QComboBox *angleFilterComboBox = angleFilterMenuCombo->combo();
	angleFilterComboBox->addItem("Enabled", QVariant(1));
	angleFilterComboBox->addItem("Disabled", QVariant(0));
	ADMTStyleHelper::ComboBoxStyle(angleFilterComboBox);

	eighthHarmonicMenuCombo = new MenuCombo("8th Harmonic", sequenceSection);
	QComboBox *eighthHarmonicComboBox = eighthHarmonicMenuCombo->combo();
	eighthHarmonicComboBox->addItem("User-Supplied Values", QVariant(1));
	eighthHarmonicComboBox->addItem("ADI Factory Values", QVariant(0));
	ADMTStyleHelper::ComboBoxStyle(eighthHarmonicComboBox);

	readSequence();

	applySequenceButton = new QPushButton("Apply", sequenceSection);
	StyleHelper::BlueButton(applySequenceButton, "applySequenceButton");
	connect(applySequenceButton, &QPushButton::clicked, this, &HarmonicCalibration::applySequence);

	sequenceSection->contentLayout()->addWidget(sequenceTypeMenuCombo);
	sequenceSection->contentLayout()->addWidget(conversionTypeMenuCombo);
	sequenceSection->contentLayout()->addWidget(convertSynchronizationMenuCombo);
	sequenceSection->contentLayout()->addWidget(angleFilterMenuCombo);
	sequenceSection->contentLayout()->addWidget(eighthHarmonicMenuCombo);
	sequenceSection->contentLayout()->addWidget(applySequenceButton);

	generalSettingLayout->addWidget(header);
	generalSettingLayout->addSpacerItem(new QSpacerItem(0, 3, QSizePolicy::Fixed, QSizePolicy::Fixed));
	generalSettingLayout->addWidget(sequenceWidget);
	generalSettingLayout->addWidget(generalWidget);
	generalSettingLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
	#pragma endregion

	tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tool->topContainer()->setVisible(true);
	tool->leftContainer()->setVisible(true);
	tool->rightContainer()->setVisible(true);
	tool->bottomContainer()->setVisible(false);
    tool->setLeftContainerWidth(210);
	tool->setRightContainerWidth(300);
	tool->setTopContainerHeight(100);
	tool->openBottomContainerHelper(false);
	tool->openTopContainerHelper(false);
    tool->addWidgetToTopContainerMenuControlHelper(openLastMenuButton, TTA_RIGHT);
	tool->addWidgetToTopContainerMenuControlHelper(settingsButton, TTA_LEFT);
    tool->addWidgetToTopContainerHelper(resetGMRButton, TTA_RIGHT);
    tool->addWidgetToTopContainerHelper(runButton, TTA_RIGHT);
	tool->leftStack()->add("rawDataScroll", rawDataScroll);
	tool->rightStack()->add("generalSettingScroll", generalSettingScroll);
	tool->addWidgetToCentralContainerHelper(acquisitionGraphSectionWidget);

	connect(runButton, &QPushButton::toggled, this, &HarmonicCalibration::setRunning);
	connect(this, &HarmonicCalibration::runningChanged, this, &HarmonicCalibration::run);
	connect(this, &HarmonicCalibration::runningChanged, runButton, &QAbstractButton::setChecked);
	connectLineEditToRPSConversion(motorMaxVelocitySpinBox->lineEdit(), rotate_vmax);
	connectLineEditToAMAXConversion(motorAccelTimeSpinBox->lineEdit(), amax);
	connectLineEditToNumberWrite(motorMaxDisplacementSpinBox->lineEdit(), dmax, ADMTController::MotorAttribute::DMAX);
	connectLineEditToNumberWrite(motorTargetPositionSpinBox->lineEdit(), target_pos, ADMTController::MotorAttribute::TARGET_POS);
	connectMenuComboToNumber(m_calibrationMotorRampModeMenuCombo, ramp_mode);

	acquisitionUITimer = new QTimer(this);
	connect(acquisitionUITimer, &QTimer::timeout, this, &HarmonicCalibration::acquisitionUITask);

	// timer = new QTimer(this);
	// connect(timer, &QTimer::timeout, this, &HarmonicCalibration::timerTask);

	calibrationUITimer = new QTimer(this);
	connect(calibrationUITimer, &QTimer::timeout, this, &HarmonicCalibration::calibrationUITask);

	utilityTimer = new QTimer(this);
	connect(utilityTimer, &QTimer::timeout, this, &HarmonicCalibration::utilityTask);

	tabWidget->addTab(createCalibrationWidget(), "Calibration");
	tabWidget->addTab(createUtilityWidget(), "Utility");
	tabWidget->addTab(createRegistersWidget(), "Registers");

	connect(tabWidget, &QTabWidget::currentChanged, [=](int index){
		tabWidget->setCurrentIndex(index);

		if(index == 0) // Acquisition Tab
		{ 
			readSequence();
		}
		else
		{
			stop();
		}

		if(index == 1) // Calibration Tab
		{ 
			calibrationUITimer->start(calibrationUITimerRate); 
		}
		else 
		{ 
			calibrationUITimer->stop();
		}

		if(index == 2) // Utility Tab
		{ 
			utilityTimer->start(utilityTimerRate); 
			readSequence();
			toggleFaultRegisterMode(generalRegisterMap.at("Sequence Type"));
			toggleMTDiagnostics(generalRegisterMap.at("Sequence Type"));
			updateDIGIOToggle();
		}	
		else { utilityTimer->stop(); }

		if(index == 3) // Registers Tab
		{ 
			readSequence();
			toggleSequenceModeRegisters(generalRegisterMap.at("Sequence Type"));
		}
	});
}

HarmonicCalibration::~HarmonicCalibration() {}

void HarmonicCalibration::startAcquisition()
{
	isStartAcquisition = true;
	acquisitionXPlotAxis->setInterval(0, acquisitionDisplayLength);
    QFuture<void> future = QtConcurrent::run(this, &HarmonicCalibration::getAcquisitionSamples);
}

void HarmonicCalibration::getAcquisitionSamples()
{
	while(isStartAcquisition)
	{
		updateChannelValues();
		prependAcquisitionData(angle, acquisitionAngleList);
		readMotorAttributeValue(ADMTController::MotorAttribute::CURRENT_POS, current_pos);
	}
}

void HarmonicCalibration::prependAcquisitionData(double& data, QVector<double>& list)
{
	list.prepend(data);
}

void HarmonicCalibration::plotAcquisition(QVector<double>& list, PlotChannel* channel, PlotWidget* plot)
{
	channel->curve()->setSamples(list);
	plot->replot();
}

void HarmonicCalibration::initializeMotor()
{
	rotate_vmax = 53687.0912;
	writeMotorAttributeValue(ADMTController::MotorAttribute::ROTATE_VMAX, rotate_vmax);
	readMotorAttributeValue(ADMTController::MotorAttribute::ROTATE_VMAX, rotate_vmax);
	writeMotorAttributeValue(ADMTController::MotorAttribute::DISABLE, 1);
	
	amax = 439.8046511104;
	writeMotorAttributeValue(ADMTController::MotorAttribute::AMAX, amax);
	readMotorAttributeValue(ADMTController::MotorAttribute::AMAX, amax);

	dmax = 3000;
	writeMotorAttributeValue(ADMTController::MotorAttribute::DMAX, dmax);
	readMotorAttributeValue(ADMTController::MotorAttribute::DMAX, dmax);

	ramp_mode = 0;
	writeMotorAttributeValue(ADMTController::MotorAttribute::RAMP_MODE, ramp_mode);
	readMotorAttributeValue(ADMTController::MotorAttribute::RAMP_MODE, ramp_mode);

	target_pos = 0;
	writeMotorAttributeValue(ADMTController::MotorAttribute::TARGET_POS, target_pos);

	current_pos = 0;
	readMotorAttributeValue(ADMTController::MotorAttribute::CURRENT_POS, current_pos);
}

ToolTemplate* HarmonicCalibration::createCalibrationWidget()
{
	ToolTemplate *tool = new ToolTemplate(this);

	#pragma region Calibration Data Graph Widget
	QWidget *calibrationDataGraphWidget = new QWidget();
	QGridLayout *calibrationDataGraphLayout = new QGridLayout(calibrationDataGraphWidget);
	calibrationDataGraphWidget->setLayout(calibrationDataGraphLayout);
	calibrationDataGraphLayout->setMargin(0);
	calibrationDataGraphLayout->setSpacing(5);

	MenuSectionWidget *calibrationDataGraphSectionWidget = new MenuSectionWidget(calibrationDataGraphWidget);
	calibrationDataGraphTabWidget = new QTabWidget(calibrationDataGraphSectionWidget);
	applyTabWidgetStyle(calibrationDataGraphTabWidget);
	calibrationDataGraphSectionWidget->contentLayout()->setSpacing(8);
	calibrationDataGraphSectionWidget->contentLayout()->addWidget(calibrationDataGraphTabWidget);

	#pragma region Calibration Samples
	QWidget *calibrationSamplesWidget = new QWidget(calibrationDataGraphTabWidget);
	QVBoxLayout *calibrationSamplesLayout = new QVBoxLayout(calibrationSamplesWidget);
	calibrationSamplesWidget->setLayout(calibrationSamplesLayout);
	calibrationSamplesLayout->setMargin(0);
	calibrationSamplesLayout->setSpacing(0);

	calibrationRawDataPlotWidget = new PlotWidget();
	ADMTStyleHelper::PlotWidgetStyle(calibrationRawDataPlotWidget);

	calibrationRawDataXPlotAxis = new PlotAxis(QwtAxis::XBottom, calibrationRawDataPlotWidget, scopyBluePen);
	calibrationRawDataXPlotAxis->setMin(0);
	calibrationRawDataYPlotAxis = new PlotAxis(QwtAxis::YLeft, calibrationRawDataPlotWidget, scopyBluePen);
	calibrationRawDataYPlotAxis->setInterval(0, 400);	

	calibrationRawDataPlotChannel = new PlotChannel("Samples", scopyBluePen, calibrationRawDataXPlotAxis, calibrationRawDataYPlotAxis);
	calibrationSineDataPlotChannel = new PlotChannel("Sine", sinePen, calibrationRawDataXPlotAxis, calibrationRawDataYPlotAxis);
	calibrationCosineDataPlotChannel = new PlotChannel("Cosine", cosinePen, calibrationRawDataXPlotAxis, calibrationRawDataYPlotAxis);

	calibrationRawDataPlotWidget->addPlotChannel(calibrationRawDataPlotChannel);
	calibrationRawDataPlotWidget->addPlotChannel(calibrationSineDataPlotChannel);
	calibrationRawDataPlotWidget->addPlotChannel(calibrationCosineDataPlotChannel);
	calibrationRawDataPlotChannel->setEnabled(true);
	calibrationSineDataPlotChannel->setEnabled(true);
	calibrationCosineDataPlotChannel->setEnabled(true);
	calibrationRawDataPlotWidget->selectChannel(calibrationRawDataPlotChannel);

	calibrationRawDataPlotWidget->setShowXAxisLabels(true);
	calibrationRawDataPlotWidget->setShowYAxisLabels(true);
	calibrationRawDataPlotWidget->showAxisLabels();

	calibrationRawDataPlotWidget->replot();

	QWidget *calibrationDataGraphChannelsWidget = new QWidget(calibrationDataGraphTabWidget);
	QHBoxLayout *calibrationDataGraphChannelsLayout = new QHBoxLayout(calibrationDataGraphChannelsWidget);
	calibrationDataGraphChannelsWidget->setLayout(calibrationDataGraphChannelsLayout);
	QString calibrationDataGraphChannelsStyle = QString(R"css(
														background-color: &&colorname&&;
														)css");
	calibrationDataGraphChannelsStyle.replace(QString("&&colorname&&"), StyleHelper::getColor("UIElementBackground"));
	calibrationDataGraphChannelsWidget->setStyleSheet(calibrationDataGraphChannelsStyle);
	calibrationDataGraphChannelsLayout->setContentsMargins(20, 13, 20, 5);
	calibrationDataGraphChannelsLayout->setSpacing(20);
	
	MenuControlButton *toggleAngleButton = createChannelToggleWidget("Angle", scopyBlueColor, calibrationDataGraphChannelsWidget);
	MenuControlButton *toggleSineButton = createChannelToggleWidget("Sine", sineColor, calibrationDataGraphChannelsWidget);
	MenuControlButton *toggleCosineButton = createChannelToggleWidget("Cosine", cosineColor, calibrationDataGraphChannelsWidget);

	calibrationDataGraphChannelsLayout->addWidget(toggleAngleButton);
	calibrationDataGraphChannelsLayout->addWidget(toggleSineButton);
	calibrationDataGraphChannelsLayout->addWidget(toggleCosineButton);
	calibrationDataGraphChannelsLayout->addStretch();

	calibrationSamplesLayout->addWidget(calibrationRawDataPlotWidget);
	calibrationSamplesLayout->addWidget(calibrationDataGraphChannelsWidget);
	#pragma endregion

	#pragma region Post Calibration Samples
	QWidget *postCalibrationSamplesWidget = new QWidget(calibrationDataGraphTabWidget);
	QVBoxLayout *postCalibrationSamplesLayout = new QVBoxLayout(postCalibrationSamplesWidget);
	postCalibrationSamplesWidget->setLayout(postCalibrationSamplesLayout);
	postCalibrationSamplesLayout->setMargin(0);
	postCalibrationSamplesLayout->setSpacing(0);

	postCalibrationRawDataPlotWidget = new PlotWidget();
	ADMTStyleHelper::PlotWidgetStyle(postCalibrationRawDataPlotWidget);
	postCalibrationRawDataPlotWidget->xAxis()->setVisible(false);
	postCalibrationRawDataPlotWidget->yAxis()->setVisible(false);

	postCalibrationRawDataXPlotAxis = new PlotAxis(QwtAxis::XBottom, postCalibrationRawDataPlotWidget, scopyBluePen);
	postCalibrationRawDataXPlotAxis->setMin(0);
	postCalibrationRawDataYPlotAxis = new PlotAxis(QwtAxis::YLeft, postCalibrationRawDataPlotWidget, scopyBluePen);
	postCalibrationRawDataYPlotAxis->setInterval(0, 400);

	postCalibrationRawDataPlotChannel = new PlotChannel("Samples", scopyBluePen, postCalibrationRawDataXPlotAxis, postCalibrationRawDataYPlotAxis);
	postCalibrationSineDataPlotChannel = new PlotChannel("Sine", sinePen, postCalibrationRawDataXPlotAxis, postCalibrationRawDataYPlotAxis);
	postCalibrationCosineDataPlotChannel = new PlotChannel("Cosine", cosinePen, postCalibrationRawDataXPlotAxis, postCalibrationRawDataYPlotAxis);

	postCalibrationRawDataPlotWidget->addPlotChannel(postCalibrationRawDataPlotChannel);
	postCalibrationRawDataPlotWidget->addPlotChannel(postCalibrationSineDataPlotChannel);
	postCalibrationRawDataPlotWidget->addPlotChannel(postCalibrationCosineDataPlotChannel);

	postCalibrationRawDataPlotChannel->setEnabled(true);
	postCalibrationSineDataPlotChannel->setEnabled(true);
	postCalibrationCosineDataPlotChannel->setEnabled(true);
	postCalibrationRawDataPlotWidget->selectChannel(postCalibrationRawDataPlotChannel);
	postCalibrationRawDataPlotWidget->replot();

	postCalibrationRawDataPlotWidget->setShowXAxisLabels(true);
	postCalibrationRawDataPlotWidget->setShowYAxisLabels(true);
	postCalibrationRawDataPlotWidget->showAxisLabels();

	QWidget *postCalibrationDataGraphChannelsWidget = new QWidget(calibrationDataGraphTabWidget);
	QHBoxLayout *postCalibrationDataGraphChannelsLayout = new QHBoxLayout(postCalibrationDataGraphChannelsWidget);
	postCalibrationDataGraphChannelsWidget->setLayout(postCalibrationDataGraphChannelsLayout);
	postCalibrationDataGraphChannelsWidget->setStyleSheet(calibrationDataGraphChannelsStyle);
	postCalibrationDataGraphChannelsLayout->setContentsMargins(20, 13, 20, 5);
	postCalibrationDataGraphChannelsLayout->setSpacing(20);

	MenuControlButton *togglePostAngleButton = createChannelToggleWidget("Angle", scopyBlueColor, postCalibrationDataGraphChannelsWidget);
	MenuControlButton *togglePostSineButton = createChannelToggleWidget("Sine", sineColor, postCalibrationDataGraphChannelsWidget);
	MenuControlButton *togglePostCosineButton = createChannelToggleWidget("Cosine", cosineColor, postCalibrationDataGraphChannelsWidget);

	postCalibrationDataGraphChannelsLayout->addWidget(togglePostAngleButton);
	postCalibrationDataGraphChannelsLayout->addWidget(togglePostSineButton);
	postCalibrationDataGraphChannelsLayout->addWidget(togglePostCosineButton);
	postCalibrationDataGraphChannelsLayout->addStretch();

	postCalibrationSamplesLayout->addWidget(postCalibrationRawDataPlotWidget);
	postCalibrationSamplesLayout->addWidget(postCalibrationDataGraphChannelsWidget);
	#pragma endregion

	calibrationDataGraphTabWidget->addTab(calibrationSamplesWidget, "Calibration Samples");
	calibrationDataGraphTabWidget->addTab(postCalibrationSamplesWidget, "Post Calibration Samples");

	MenuSectionWidget *resultDataSectionWidget = new MenuSectionWidget(calibrationDataGraphWidget);
	resultDataTabWidget = new QTabWidget(resultDataSectionWidget);
	applyTabWidgetStyle(resultDataTabWidget);
	resultDataSectionWidget->contentLayout()->setSpacing(8);
	resultDataSectionWidget->contentLayout()->addWidget(resultDataTabWidget);

	QPen magnitudePen = QPen(StyleHelper::getColor("CH0"));
	QPen phasePen = QPen(StyleHelper::getColor("CH1"));

	#pragma region Angle Error Widget
	QWidget *angleErrorWidget = new QWidget();
	QVBoxLayout *angleErrorLayout = new QVBoxLayout(angleErrorWidget);
	angleErrorWidget->setLayout(angleErrorLayout);
	angleErrorLayout->setMargin(0);
	angleErrorLayout->setSpacing(0);

	angleErrorPlotWidget = new PlotWidget();
	ADMTStyleHelper::PlotWidgetStyle(angleErrorPlotWidget);
	angleErrorPlotWidget->xAxis()->setVisible(false);
	angleErrorPlotWidget->yAxis()->setVisible(false);
	
	angleErrorXPlotAxis = new PlotAxis(QwtAxis::XBottom, angleErrorPlotWidget, scopyBluePen);
	angleErrorXPlotAxis->setMin(0);
	angleErrorYPlotAxis = new PlotAxis(QwtAxis::YLeft, angleErrorPlotWidget, scopyBluePen);
	angleErrorYPlotAxis->setInterval(-4, 4);

	angleErrorPlotChannel = new PlotChannel("Angle Error", scopyBluePen, angleErrorXPlotAxis, angleErrorYPlotAxis);
	angleErrorPlotWidget->addPlotChannel(angleErrorPlotChannel);

	angleErrorPlotChannel->setEnabled(true);
	angleErrorPlotWidget->selectChannel(angleErrorPlotChannel);
	angleErrorPlotWidget->replot();

	angleErrorPlotWidget->setShowXAxisLabels(true);
	angleErrorPlotWidget->setShowYAxisLabels(true);
	angleErrorPlotWidget->showAxisLabels();

	angleErrorLayout->addWidget(angleErrorPlotWidget);
	#pragma endregion

	#pragma region FFT Angle Error Widget
	QWidget *FFTAngleErrorWidget = new QWidget();
	QVBoxLayout *FFTAngleErrorLayout = new QVBoxLayout(FFTAngleErrorWidget);
	FFTAngleErrorWidget->setLayout(FFTAngleErrorLayout);
	FFTAngleErrorLayout->setMargin(0);
	FFTAngleErrorLayout->setSpacing(0);

	FFTAngleErrorPlotWidget = new PlotWidget();
	ADMTStyleHelper::PlotWidgetStyle(FFTAngleErrorPlotWidget);
	FFTAngleErrorPlotWidget->xAxis()->setVisible(false);
	FFTAngleErrorPlotWidget->yAxis()->setVisible(false);

	FFTAngleErrorXPlotAxis = new PlotAxis(QwtAxis::XBottom, FFTAngleErrorPlotWidget, scopyBluePen);
	FFTAngleErrorXPlotAxis->setMin(0);
	FFTAngleErrorYPlotAxis = new PlotAxis(QwtAxis::YLeft, FFTAngleErrorPlotWidget, scopyBluePen);
	FFTAngleErrorYPlotAxis->setInterval(-4, 4);

	FFTAngleErrorMagnitudeChannel = new PlotChannel("FFT Angle Error Magnitude", magnitudePen, FFTAngleErrorXPlotAxis, FFTAngleErrorYPlotAxis);
	FFTAngleErrorPhaseChannel = new PlotChannel("FFT Angle Error Phase", phasePen, FFTAngleErrorXPlotAxis, FFTAngleErrorYPlotAxis);
	FFTAngleErrorPlotWidget->addPlotChannel(FFTAngleErrorMagnitudeChannel);
	FFTAngleErrorPlotWidget->addPlotChannel(FFTAngleErrorPhaseChannel);

	FFTAngleErrorMagnitudeChannel->setEnabled(true);
	FFTAngleErrorPhaseChannel->setEnabled(true);
	FFTAngleErrorPlotWidget->selectChannel(FFTAngleErrorMagnitudeChannel);
	FFTAngleErrorPlotWidget->replot();

	FFTAngleErrorPlotWidget->setShowXAxisLabels(true);
	FFTAngleErrorPlotWidget->setShowYAxisLabels(true);
	FFTAngleErrorPlotWidget->showAxisLabels();

	QWidget *FFTAngleErrorChannelsWidget = new QWidget();
	QHBoxLayout *FFTAngleErrorChannelsLayout = new QHBoxLayout(FFTAngleErrorChannelsWidget);
	FFTAngleErrorChannelsWidget->setStyleSheet(calibrationDataGraphChannelsStyle);
	FFTAngleErrorChannelsWidget->setLayout(FFTAngleErrorChannelsLayout);
	FFTAngleErrorChannelsLayout->setContentsMargins(20, 13, 20, 5);
	FFTAngleErrorChannelsLayout->setSpacing(20);

	MenuControlButton *toggleFFTAngleErrorMagnitudeButton = createChannelToggleWidget("Magnitude", QColor(StyleHelper::getColor("CH0")), FFTAngleErrorChannelsWidget);
	MenuControlButton *toggleFFTAngleErrorPhaseButton = createChannelToggleWidget("Phase", QColor(StyleHelper::getColor("CH1")), FFTAngleErrorChannelsWidget);

	FFTAngleErrorChannelsLayout->addWidget(toggleFFTAngleErrorMagnitudeButton);
	FFTAngleErrorChannelsLayout->addWidget(toggleFFTAngleErrorPhaseButton);
	FFTAngleErrorChannelsLayout->addStretch();

	FFTAngleErrorLayout->addWidget(FFTAngleErrorPlotWidget);
	FFTAngleErrorLayout->addWidget(FFTAngleErrorChannelsWidget);
	#pragma endregion

	#pragma region Corrected Error Widget
	QWidget *correctedErrorWidget = new QWidget();
	QVBoxLayout *correctedErrorLayout = new QVBoxLayout(correctedErrorWidget);
	correctedErrorWidget->setLayout(correctedErrorLayout);
	correctedErrorLayout->setMargin(0);
	correctedErrorLayout->setSpacing(0);

	correctedErrorPlotWidget = new PlotWidget();
	ADMTStyleHelper::PlotWidgetStyle(correctedErrorPlotWidget);
	correctedErrorPlotWidget->xAxis()->setVisible(false);
	correctedErrorPlotWidget->yAxis()->setVisible(false);

	correctedErrorXPlotAxis = new PlotAxis(QwtAxis::XBottom, correctedErrorPlotWidget, scopyBluePen);
	correctedErrorXPlotAxis->setMin(0);
	correctedErrorYPlotAxis = new PlotAxis(QwtAxis::YLeft, correctedErrorPlotWidget, scopyBluePen);
	correctedErrorYPlotAxis->setInterval(-4, 4);

	correctedErrorPlotChannel = new PlotChannel("Corrected Error", scopyBluePen, correctedErrorXPlotAxis, correctedErrorYPlotAxis);
	correctedErrorPlotWidget->addPlotChannel(correctedErrorPlotChannel);

	correctedErrorPlotChannel->setEnabled(true);
	correctedErrorPlotWidget->selectChannel(correctedErrorPlotChannel);
	correctedErrorPlotWidget->replot();

	correctedErrorPlotWidget->setShowXAxisLabels(true);
	correctedErrorPlotWidget->setShowYAxisLabels(true);
	correctedErrorPlotWidget->showAxisLabels();

	correctedErrorLayout->addWidget(correctedErrorPlotWidget);
	#pragma endregion

	#pragma region FFT Corrected Error Widget
	QWidget *FFTCorrectedErrorWidget = new QWidget();
	QVBoxLayout *FFTCorrectedErrorLayout = new QVBoxLayout(FFTCorrectedErrorWidget);
	FFTCorrectedErrorWidget->setLayout(FFTCorrectedErrorLayout);
	FFTCorrectedErrorLayout->setMargin(0);
	FFTCorrectedErrorLayout->setSpacing(0);

	FFTCorrectedErrorPlotWidget = new PlotWidget();
	ADMTStyleHelper::PlotWidgetStyle(FFTCorrectedErrorPlotWidget);
	FFTCorrectedErrorPlotWidget->xAxis()->setVisible(false);
	FFTCorrectedErrorPlotWidget->yAxis()->setVisible(false);

	FFTCorrectedErrorXPlotAxis = new PlotAxis(QwtAxis::XBottom, FFTCorrectedErrorPlotWidget, scopyBluePen);
	FFTCorrectedErrorXPlotAxis->setMin(0);
	FFTCorrectedErrorYPlotAxis = new PlotAxis(QwtAxis::YLeft, FFTCorrectedErrorPlotWidget, scopyBluePen);
	FFTCorrectedErrorYPlotAxis->setInterval(-4, 4);

	FFTCorrectedErrorMagnitudeChannel = new PlotChannel("FFT Corrected Error Magnitude", magnitudePen, FFTCorrectedErrorXPlotAxis, FFTCorrectedErrorYPlotAxis);
	FFTCorrectedErrorPhaseChannel = new PlotChannel("FFT Corrected Error Phase", phasePen, FFTCorrectedErrorXPlotAxis, FFTCorrectedErrorYPlotAxis);
	FFTCorrectedErrorPlotWidget->addPlotChannel(FFTCorrectedErrorMagnitudeChannel);
	FFTCorrectedErrorPlotWidget->addPlotChannel(FFTCorrectedErrorPhaseChannel);

	FFTCorrectedErrorMagnitudeChannel->setEnabled(true);
	FFTCorrectedErrorPhaseChannel->setEnabled(true);
	FFTCorrectedErrorPlotWidget->selectChannel(FFTCorrectedErrorMagnitudeChannel);
	FFTCorrectedErrorPlotWidget->replot();

	FFTCorrectedErrorPlotWidget->setShowXAxisLabels(true);
	FFTCorrectedErrorPlotWidget->setShowYAxisLabels(true);
	FFTCorrectedErrorPlotWidget->showAxisLabels();

	QWidget *FFTCorrectedErrorChannelsWidget = new QWidget();
	QHBoxLayout *FFTCorrectedErrorChannelsLayout = new QHBoxLayout(FFTCorrectedErrorChannelsWidget);
	FFTCorrectedErrorChannelsWidget->setStyleSheet(calibrationDataGraphChannelsStyle);
	FFTCorrectedErrorChannelsLayout->setContentsMargins(20, 13, 20, 5);
	FFTCorrectedErrorChannelsLayout->setSpacing(20);

	MenuControlButton *toggleFFTCorrectedErrorMagnitudeButton = createChannelToggleWidget("Magnitude", QColor(StyleHelper::getColor("CH0")), FFTCorrectedErrorChannelsWidget);
	MenuControlButton *toggleFFTCorrectedErrorPhaseButton = createChannelToggleWidget("Phase", QColor(StyleHelper::getColor("CH1")), FFTCorrectedErrorChannelsWidget);

	FFTCorrectedErrorChannelsLayout->addWidget(toggleFFTCorrectedErrorMagnitudeButton);
	FFTCorrectedErrorChannelsLayout->addWidget(toggleFFTCorrectedErrorPhaseButton);
	FFTCorrectedErrorChannelsLayout->addStretch();

	FFTCorrectedErrorLayout->addWidget(FFTCorrectedErrorPlotWidget);
	FFTCorrectedErrorLayout->addWidget(FFTCorrectedErrorChannelsWidget);
	#pragma endregion

	resultDataTabWidget->addTab(angleErrorWidget, "Angle Error");
	resultDataTabWidget->addTab(FFTAngleErrorWidget, "FFT Angle Error");
	resultDataTabWidget->addTab(correctedErrorWidget, "Corrected Error");
	resultDataTabWidget->addTab(FFTCorrectedErrorWidget, "FFT Corrected Error");

	calibrationDataGraphLayout->addWidget(calibrationDataGraphSectionWidget, 0, 0);
	calibrationDataGraphLayout->addWidget(resultDataSectionWidget, 1, 0);

	calibrationDataGraphLayout->setColumnStretch(0, 1);
	calibrationDataGraphLayout->setRowStretch(0, 1);
	calibrationDataGraphLayout->setRowStretch(1, 1);
	#pragma endregion
	
	#pragma region Calibration Settings Widget
	QScrollArea *calibrationSettingsScrollArea = new QScrollArea();
	QWidget *calibrationSettingsWidget = new QWidget(calibrationSettingsScrollArea);
	QVBoxLayout *calibrationSettingsLayout = new QVBoxLayout(calibrationSettingsWidget);
	calibrationSettingsScrollArea->setWidgetResizable(true);
	calibrationSettingsScrollArea->setWidget(calibrationSettingsWidget);
	calibrationSettingsWidget->setFixedWidth(260);
	calibrationSettingsWidget->setLayout(calibrationSettingsLayout);

	#pragma region Calibration Coefficient Section Widget
	MenuSectionWidget *calibrationCoeffSectionWidget = new MenuSectionWidget(calibrationSettingsWidget);
	QLabel *calibrationDisplayFormatLabel = new QLabel(calibrationCoeffSectionWidget);
	calibrationDisplayFormatLabel->setText("Display Format");
	StyleHelper::MenuCollapseHeaderLabel(calibrationDisplayFormatLabel, "calibrationDisplayFormatLabel");
	QLabel *calibrationCalculatedCoeffLabel = new QLabel(calibrationCoeffSectionWidget);
	calibrationCalculatedCoeffLabel->setText("Calculated Coefficients");
	StyleHelper::MenuCollapseHeaderLabel(calibrationCalculatedCoeffLabel, "calibrationCalculatedCoeffLabel");

	calibrationDisplayFormatSwitch = new CustomSwitch();
	calibrationDisplayFormatSwitch->setOffText("Hex");
	calibrationDisplayFormatSwitch->setOnText("Angle");
	calibrationDisplayFormatSwitch->setProperty("bigBtn", true);

	// Calculated Coefficients Widget
	QWidget *calibrationCalculatedCoeffWidget = new QWidget(calibrationCoeffSectionWidget);
	QGridLayout *calibrationCalculatedCoeffLayout = new QGridLayout(calibrationCalculatedCoeffWidget);
	calibrationCalculatedCoeffWidget->setLayout(calibrationCalculatedCoeffLayout);
	calibrationCalculatedCoeffLayout->setMargin(0);
	calibrationCalculatedCoeffLayout->setVerticalSpacing(4);
	QString calibrationCalculatedCoeffStyle = QString(R"css(
													background-color: &&colorname&&;
													)css");
	calibrationCalculatedCoeffStyle.replace(QString("&&colorname&&"), StyleHelper::getColor("UIElementBackground"));
	calibrationCalculatedCoeffWidget->setStyleSheet(calibrationCalculatedCoeffStyle);

	QString rowContainerStyle = QString(R"css(
										background-color: &&colorname&&;
										border-radius: 4px;
										)css");
	rowContainerStyle.replace(QString("&&colorname&&"), StyleHelper::getColor("ScopyBackground"));

	// H1
	QWidget *h1RowContainer = new QWidget(calibrationCalculatedCoeffWidget);
	QHBoxLayout *h1RowLayout = new QHBoxLayout(h1RowContainer);
	h1RowContainer->setLayout(h1RowLayout);
	h1RowContainer->setStyleSheet(rowContainerStyle);
	h1RowContainer->setFixedHeight(30);
	h1RowContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	h1RowLayout->setContentsMargins(12, 4, 12, 4);
	QLabel *calibrationH1Label = new QLabel("H1", calibrationCalculatedCoeffWidget);
	calibrationH1MagLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	calibrationH1PhaseLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	applyTextStyle(calibrationH1Label, "LabelText", true);
	applyTextStyle(calibrationH1MagLabel, "CH0");
	applyTextStyle(calibrationH1PhaseLabel, "CH1");
	calibrationH1Label->setFixedWidth(24);
	calibrationH1MagLabel->setContentsMargins(0, 0, 48, 0);
	calibrationH1PhaseLabel->setFixedWidth(56);

	h1RowLayout->addWidget(calibrationH1Label);
	h1RowLayout->addWidget(calibrationH1MagLabel, 0, Qt::AlignRight);
	h1RowLayout->addWidget(calibrationH1PhaseLabel);

	// H2
	QWidget *h2RowContainer = new QWidget(calibrationCalculatedCoeffWidget);
	QHBoxLayout *h2RowLayout = new QHBoxLayout(h2RowContainer);
	h2RowContainer->setLayout(h2RowLayout);
	h2RowContainer->setStyleSheet(rowContainerStyle);
	h2RowContainer->setFixedHeight(30);
	h2RowContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	h2RowLayout->setContentsMargins(12, 4, 12, 4);
	QLabel *calibrationH2Label = new QLabel("H2", calibrationCalculatedCoeffWidget);
	calibrationH2MagLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	calibrationH2PhaseLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	applyTextStyle(calibrationH2Label, "LabelText", true);
	applyTextStyle(calibrationH2MagLabel, "CH0");
	applyTextStyle(calibrationH2PhaseLabel, "CH1");
	calibrationH2Label->setFixedWidth(24);
	calibrationH2MagLabel->setContentsMargins(0, 0, 48, 0);
	calibrationH2PhaseLabel->setFixedWidth(56);

	h2RowLayout->addWidget(calibrationH2Label);
	h2RowLayout->addWidget(calibrationH2MagLabel, 0, Qt::AlignRight);
	h2RowLayout->addWidget(calibrationH2PhaseLabel);

	// H3
	QWidget *h3RowContainer = new QWidget(calibrationCalculatedCoeffWidget);
	QHBoxLayout *h3RowLayout = new QHBoxLayout(h3RowContainer);
	h3RowContainer->setLayout(h3RowLayout);
	h3RowContainer->setStyleSheet(rowContainerStyle);
	h3RowContainer->setFixedHeight(30);
	h3RowContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	h3RowLayout->setContentsMargins(12, 4, 12, 4);
	QLabel *calibrationH3Label = new QLabel("H3", calibrationCalculatedCoeffWidget);
	calibrationH3MagLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	calibrationH3PhaseLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	applyTextStyle(calibrationH3Label, "LabelText", true);
	applyTextStyle(calibrationH3MagLabel, "CH0");
	applyTextStyle(calibrationH3PhaseLabel, "CH1");
	calibrationH3Label->setFixedWidth(24);
	calibrationH3MagLabel->setContentsMargins(0, 0, 48, 0);
	calibrationH3PhaseLabel->setFixedWidth(56);

	h3RowLayout->addWidget(calibrationH3Label);
	h3RowLayout->addWidget(calibrationH3MagLabel, 0, Qt::AlignRight);
	h3RowLayout->addWidget(calibrationH3PhaseLabel);

	// H8
	QWidget *h8RowContainer = new QWidget(calibrationCalculatedCoeffWidget);
	QHBoxLayout *h8RowLayout = new QHBoxLayout(h8RowContainer);
	h8RowContainer->setLayout(h8RowLayout);
	h8RowContainer->setStyleSheet(rowContainerStyle);
	h8RowContainer->setFixedHeight(30);
	h8RowContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	h8RowLayout->setContentsMargins(12, 4, 12, 4);
	QLabel *calibrationH8Label = new QLabel("H8", calibrationCalculatedCoeffWidget);
	calibrationH8MagLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	calibrationH8PhaseLabel = new QLabel("0x----", calibrationCalculatedCoeffWidget);
	applyTextStyle(calibrationH8Label, "LabelText", true);
	applyTextStyle(calibrationH8MagLabel, "CH0");
	applyTextStyle(calibrationH8PhaseLabel, "CH1");
	calibrationH8Label->setFixedWidth(24);
	calibrationH8MagLabel->setContentsMargins(0, 0, 48, 0);
	calibrationH8PhaseLabel->setFixedWidth(56);

	h8RowLayout->addWidget(calibrationH8Label);
	h8RowLayout->addWidget(calibrationH8MagLabel, 0, Qt::AlignRight);
	h8RowLayout->addWidget(calibrationH8PhaseLabel);

	calibrationCalculatedCoeffLayout->addWidget(h1RowContainer, 0, 0);
	calibrationCalculatedCoeffLayout->addWidget(h2RowContainer, 1, 0);
	calibrationCalculatedCoeffLayout->addWidget(h3RowContainer, 2, 0);
	calibrationCalculatedCoeffLayout->addWidget(h8RowContainer, 3, 0);

	calibrationCoeffSectionWidget->contentLayout()->setSpacing(8);
	calibrationCoeffSectionWidget->contentLayout()->addWidget(calibrationDisplayFormatLabel);
	calibrationCoeffSectionWidget->contentLayout()->addWidget(calibrationDisplayFormatSwitch);
	calibrationCoeffSectionWidget->contentLayout()->addWidget(calibrationCalculatedCoeffLabel);
	calibrationCoeffSectionWidget->contentLayout()->addWidget(calibrationCalculatedCoeffWidget);
	#pragma endregion

	#pragma region Calibration Dataset Configuration
	MenuSectionWidget *calibrationDatasetConfigSectionWidget = new MenuSectionWidget(calibrationSettingsWidget);
	MenuCollapseSection *calibrationDatasetConfigCollapseSection = new MenuCollapseSection("Dataset Configuration", MenuCollapseSection::MenuHeaderCollapseStyle::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, calibrationDatasetConfigSectionWidget);
	calibrationDatasetConfigSectionWidget->contentLayout()->setSpacing(8);
	calibrationDatasetConfigSectionWidget->contentLayout()->addWidget(calibrationDatasetConfigCollapseSection);

	QLabel *calibrationCycleCountLabel = new QLabel("Cycle Count", calibrationDatasetConfigCollapseSection);
	StyleHelper::MenuSmallLabel(calibrationCycleCountLabel);
	QLineEdit *calibrationCycleCountLineEdit = new QLineEdit(calibrationDatasetConfigCollapseSection);
	ADMTStyleHelper::LineEditStyle(calibrationCycleCountLineEdit);
	calibrationCycleCountLineEdit->setText(QString::number(cycleCount));
	connectLineEditToNumber(calibrationCycleCountLineEdit, cycleCount, 1, 1000);

	QLabel *calibrationSamplesPerCycleLabel = new QLabel("Samples Per Cycle", calibrationDatasetConfigCollapseSection);
	StyleHelper::MenuSmallLabel(calibrationSamplesPerCycleLabel);
	QLineEdit *calibrationSamplesPerCycleLineEdit = new QLineEdit(calibrationDatasetConfigCollapseSection);
	ADMTStyleHelper::LineEditStyle(calibrationSamplesPerCycleLineEdit);
	calibrationSamplesPerCycleLineEdit->setText(QString::number(samplesPerCycle));
	calibrationSamplesPerCycleLineEdit->setReadOnly(true);
	connectLineEditToNumber(calibrationSamplesPerCycleLineEdit, samplesPerCycle, 1, 5000);

	calibrationDatasetConfigCollapseSection->contentLayout()->setSpacing(8);
	calibrationDatasetConfigCollapseSection->contentLayout()->addWidget(calibrationCycleCountLabel);
	calibrationDatasetConfigCollapseSection->contentLayout()->addWidget(calibrationCycleCountLineEdit);
	calibrationDatasetConfigCollapseSection->contentLayout()->addWidget(calibrationSamplesPerCycleLabel);
	calibrationDatasetConfigCollapseSection->contentLayout()->addWidget(calibrationSamplesPerCycleLineEdit);

	#pragma endregion

	#pragma region Calibration Data Section Widget
	MenuSectionWidget *calibrationDataSectionWidget = new MenuSectionWidget(calibrationSettingsWidget);
	MenuCollapseSection *calibrationDataCollapseSection = new MenuCollapseSection("Calibration Data", MenuCollapseSection::MenuHeaderCollapseStyle::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, calibrationDataSectionWidget);
	calibrationDataSectionWidget->contentLayout()->setSpacing(8);
	calibrationDataSectionWidget->contentLayout()->addWidget(calibrationDataCollapseSection);

	QPushButton *importSamplesButton = new QPushButton("Import Samples", calibrationDataCollapseSection);
	QPushButton *extractDataButton = new QPushButton("Save to CSV", calibrationDataCollapseSection);
	QPushButton *clearCalibrateDataButton = new QPushButton("Clear All Data", calibrationDataCollapseSection);
	StyleHelper::BlueButton(importSamplesButton);
	StyleHelper::BlueButton(extractDataButton);
	StyleHelper::BlueButton(clearCalibrateDataButton);

	calibrationDataCollapseSection->contentLayout()->setSpacing(8);
	calibrationDataCollapseSection->contentLayout()->addWidget(importSamplesButton);
	calibrationDataCollapseSection->contentLayout()->addWidget(extractDataButton);
	calibrationDataCollapseSection->contentLayout()->addWidget(clearCalibrateDataButton);
	#pragma endregion

	#pragma region Motor Configuration Section Widget
	MenuSectionWidget *motorConfigurationSectionWidget = new MenuSectionWidget(calibrationSettingsWidget);
	MenuCollapseSection *motorConfigurationCollapseSection = new MenuCollapseSection("Motor Configuration", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, motorConfigurationSectionWidget);
	motorConfigurationCollapseSection->header()->toggle();
	motorConfigurationSectionWidget->contentLayout()->addWidget(motorConfigurationCollapseSection);

	motorMaxVelocitySpinBox = new HorizontalSpinBox("Max Velocity", convertVMAXtoRPS(rotate_vmax), "rps", motorConfigurationSectionWidget);
	motorMaxVelocitySpinBox->setValue(1);
	motorAccelTimeSpinBox = new HorizontalSpinBox("Acceleration Time", convertAMAXtoAccelTime(amax), "sec", motorConfigurationSectionWidget);
	motorAccelTimeSpinBox->setValue(1);
	motorMaxDisplacementSpinBox = new HorizontalSpinBox("Max Displacement", dmax, "", motorConfigurationSectionWidget);

	m_calibrationMotorRampModeMenuCombo = new MenuCombo("Ramp Mode", motorConfigurationSectionWidget);
	auto calibrationMotorRampModeCombo = m_calibrationMotorRampModeMenuCombo->combo();
	calibrationMotorRampModeCombo->addItem("Position", QVariant(ADMTController::MotorRampMode::POSITION));
	calibrationMotorRampModeCombo->addItem("Ramp Mode 1", QVariant(ADMTController::MotorRampMode::RAMP_MODE_1));
	ADMTStyleHelper::ComboBoxStyle(calibrationMotorRampModeCombo);

	motorConfigurationCollapseSection->contentLayout()->setSpacing(8);
	motorConfigurationCollapseSection->contentLayout()->addWidget(motorMaxVelocitySpinBox);
	motorConfigurationCollapseSection->contentLayout()->addWidget(motorAccelTimeSpinBox);
	motorConfigurationCollapseSection->contentLayout()->addWidget(motorMaxDisplacementSpinBox);
	motorConfigurationCollapseSection->contentLayout()->addWidget(m_calibrationMotorRampModeMenuCombo);
	#pragma endregion

	#pragma region Motor Control Section Widget
	MenuSectionWidget *motorControlSectionWidget = new MenuSectionWidget(calibrationSettingsWidget);
	MenuCollapseSection *motorControlCollapseSection = new MenuCollapseSection("Motor Control", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, motorControlSectionWidget);
	motorControlSectionWidget->contentLayout()->setSpacing(8);
	motorControlSectionWidget->contentLayout()->addWidget(motorControlCollapseSection);
	QLabel *currentPositionLabel = new QLabel("Current Position", motorControlSectionWidget);
	StyleHelper::MenuSmallLabel(currentPositionLabel, "currentPositionLabel");
	calibrationMotorCurrentPositionLabel = new QLabel("--.--", motorControlSectionWidget);
	calibrationMotorCurrentPositionLabel->setAlignment(Qt::AlignRight);
	applyLabelStyle(calibrationMotorCurrentPositionLabel);

	motorTargetPositionSpinBox = new HorizontalSpinBox("Target Position", target_pos, "", motorControlSectionWidget);

	calibrationStartMotorButton = new QPushButton(motorControlSectionWidget);
	calibrationStartMotorButton->setCheckable(true);
	calibrationStartMotorButton->setChecked(false);
	calibrationStartMotorButton->setText("Start Motor");
	calibrationStartMotorButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	calibrationStartMotorButton->setFixedHeight(36);
	connect(calibrationStartMotorButton, &QPushButton::toggled, this, [=](bool b) { 
		calibrationStartMotorButton->setText(b ? " Stop Motor" : " Start Motor"); 
		totalSamplesCount = cycleCount * samplesPerCycle;
		isStartMotor = b;
		if(b){
			isPostCalibration = false;
			startMotor();
		}
	});
	QString calibrationStartMotorButtonStyle = QString(R"css(
			QPushButton {
				border-radius: 2px;
				padding-left: 20px;
				padding-right: 20px;
				color: white;
				font-weight: 700;
				font-size: 14px;
			}

			QPushButton:!checked {
				background-color: #27b34f;
			}

			  QPushButton:checked {
				background-color: #F45000;
			}

			QPushButton:disabled {
				background-color: grey;
			})css");
	calibrationStartMotorButton->setStyleSheet(calibrationStartMotorButtonStyle);
	QIcon playIcon;
	playIcon.addPixmap(Util::ChangeSVGColor(":/gui/icons/play.svg", "white", 1), QIcon::Normal, QIcon::Off);
	playIcon.addPixmap(Util::ChangeSVGColor(":/gui/icons/scopy-default/icons/play_stop.svg", "white", 1),
			QIcon::Normal, QIcon::On);
	calibrationStartMotorButton->setIcon(playIcon);
	calibrationStartMotorButton->setIconSize(QSize(64, 64));

	calibrateDataButton = new QPushButton(motorControlSectionWidget);
	calibrateDataButton->setText("Calibrate");
	calibrateDataButton->setEnabled(false);
	StyleHelper::BlueButton(calibrateDataButton, "calibrateDataButton");

	motorControlCollapseSection->contentLayout()->setSpacing(8);
	motorControlCollapseSection->contentLayout()->addWidget(currentPositionLabel);
	motorControlCollapseSection->contentLayout()->addWidget(calibrationMotorCurrentPositionLabel);
	motorControlCollapseSection->contentLayout()->addWidget(motorTargetPositionSpinBox);
	motorControlCollapseSection->contentLayout()->addWidget(calibrationStartMotorButton);
	motorControlCollapseSection->contentLayout()->addWidget(calibrateDataButton);
	#pragma endregion

	#pragma region Logs Section Widget
	MenuSectionWidget *logsSectionWidget = new MenuSectionWidget(calibrationSettingsWidget);
	MenuCollapseSection *logsCollapseSection = new MenuCollapseSection("Logs", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, logsSectionWidget);
	logsCollapseSection->header()->toggle();
	logsSectionWidget->contentLayout()->setSpacing(8);
	logsSectionWidget->contentLayout()->addWidget(logsCollapseSection);

	logsPlainTextEdit = new QPlainTextEdit(logsSectionWidget);
	logsPlainTextEdit->setReadOnly(true);
	logsPlainTextEdit->setFixedHeight(320);

	logsCollapseSection->contentLayout()->setSpacing(8);
	logsCollapseSection->contentLayout()->addWidget(logsPlainTextEdit);
	#pragma endregion

	calibrationSettingsLayout->setMargin(0);
	calibrationSettingsLayout->addWidget(calibrationDatasetConfigSectionWidget);
	calibrationSettingsLayout->addWidget(motorConfigurationSectionWidget);
	calibrationSettingsLayout->addWidget(motorControlSectionWidget);
	calibrationSettingsLayout->addWidget(calibrationDataSectionWidget);
	calibrationSettingsLayout->addWidget(calibrationCoeffSectionWidget);
	calibrationSettingsLayout->addWidget(logsSectionWidget);
	calibrationSettingsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
	#pragma endregion

	tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tool->topContainer()->setVisible(false);
	tool->topContainerMenuControl()->setVisible(false);
	tool->leftContainer()->setVisible(false);
	tool->rightContainer()->setVisible(true);
	tool->bottomContainer()->setVisible(false);
	tool->setLeftContainerWidth(270);
	tool->setRightContainerWidth(270);
	tool->openBottomContainerHelper(false);
	tool->openTopContainerHelper(false);

	tool->addWidgetToCentralContainerHelper(calibrationDataGraphWidget);
	tool->rightStack()->add("calibrationSettingsScrollArea", calibrationSettingsScrollArea);

	connect(calibrateDataButton, &QPushButton::clicked, this, &HarmonicCalibration::postCalibrateData);
	connect(extractDataButton, &QPushButton::clicked, this, &HarmonicCalibration::extractCalibrationData);
	connect(importSamplesButton, &QPushButton::clicked, this, &HarmonicCalibration::importCalibrationData);
	connect(clearCalibrateDataButton, &QPushButton::clicked, this, &HarmonicCalibration::clearRawDataList);
	connectLineEditToRPSConversion(motorMaxVelocitySpinBox->lineEdit(), rotate_vmax);
	connectLineEditToAMAXConversion(motorAccelTimeSpinBox->lineEdit(), amax);
	connectLineEditToNumberWrite(motorMaxDisplacementSpinBox->lineEdit(), dmax, ADMTController::MotorAttribute::DMAX);
	connectLineEditToNumberWrite(motorTargetPositionSpinBox->lineEdit(), target_pos, ADMTController::MotorAttribute::TARGET_POS);
	connectMenuComboToNumber(m_calibrationMotorRampModeMenuCombo, ramp_mode);
	connect(toggleAngleButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		calibrationRawDataPlotWidget->selectChannel(calibrationRawDataPlotChannel);
		calibrationRawDataPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(toggleSineButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		calibrationRawDataPlotWidget->selectChannel(calibrationSineDataPlotChannel);
		calibrationRawDataPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(toggleCosineButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		calibrationRawDataPlotWidget->selectChannel(calibrationCosineDataPlotChannel);
		calibrationRawDataPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(togglePostAngleButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		postCalibrationRawDataPlotWidget->selectChannel(postCalibrationRawDataPlotChannel);
		postCalibrationRawDataPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(togglePostSineButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		postCalibrationRawDataPlotWidget->selectChannel(postCalibrationSineDataPlotChannel);
		postCalibrationRawDataPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(togglePostCosineButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		postCalibrationRawDataPlotWidget->selectChannel(postCalibrationCosineDataPlotChannel);
		postCalibrationRawDataPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(toggleFFTAngleErrorMagnitudeButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		FFTAngleErrorPlotWidget->selectChannel(FFTAngleErrorMagnitudeChannel);
		FFTAngleErrorPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(toggleFFTAngleErrorPhaseButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		FFTAngleErrorPlotWidget->selectChannel(FFTAngleErrorPhaseChannel);
		FFTAngleErrorPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(toggleFFTCorrectedErrorMagnitudeButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		FFTCorrectedErrorPlotWidget->selectChannel(FFTCorrectedErrorMagnitudeChannel);
		FFTCorrectedErrorPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(toggleFFTCorrectedErrorPhaseButton->checkBox(), &QCheckBox::toggled, this, [=](bool b){
		FFTCorrectedErrorPlotWidget->selectChannel(FFTCorrectedErrorPhaseChannel);
		FFTCorrectedErrorPlotWidget->selectedChannel()->setEnabled(b);
	});
	connect(calibrationDisplayFormatSwitch, &CustomSwitch::toggled, this, [=](bool b){
		isAngleDisplayFormat = b;
		displayCalculatedCoeff();
	});

	return tool;
}

ToolTemplate* HarmonicCalibration::createRegistersWidget()
{
	ToolTemplate *tool = new ToolTemplate(this);

	QScrollArea *registerScrollArea = new QScrollArea();
	QWidget *registerWidget = new QWidget(registerScrollArea);
	QVBoxLayout *registerLayout = new QVBoxLayout(registerWidget);
	registerScrollArea->setWidgetResizable(true);
	registerScrollArea->setWidget(registerWidget);
	registerWidget->setLayout(registerLayout);
	registerLayout->setMargin(0);
	registerLayout->setSpacing(8);

	QLabel *registerConfigurationLabel = new QLabel("Configuration", registerWidget);
	StyleHelper::MenuControlLabel(registerConfigurationLabel, "registerConfigurationLabel");
	QWidget *registerConfigurationGridWidget = new QWidget(registerWidget);
	QGridLayout *registerConfigurationGridLayout = new QGridLayout(registerConfigurationGridWidget);
	registerConfigurationGridWidget->setLayout(registerConfigurationGridLayout);
	registerConfigurationGridLayout->setMargin(0);
	registerConfigurationGridLayout->setSpacing(8);

	QLabel *registerDeviceIDLabel = new QLabel("Device ID", registerWidget);
	StyleHelper::MenuControlLabel(registerDeviceIDLabel, "registerDeviceIDLabel");
	QWidget *registerDeviceIDGridWidget = new QWidget(registerWidget);
	QGridLayout *registerDeviceIDGridLayout = new QGridLayout(registerDeviceIDGridWidget);
	registerDeviceIDGridWidget->setLayout(registerDeviceIDGridLayout);
	registerDeviceIDGridLayout->setMargin(0);
	registerDeviceIDGridLayout->setSpacing(8);

	QLabel *registerHarmonicsLabel = new QLabel("Harmonics", registerWidget);
	StyleHelper::MenuControlLabel(registerHarmonicsLabel, "registerHarmonicsLabel");
	QWidget *registerHarmonicsGridWidget = new QWidget(registerWidget);
	QGridLayout *registerHarmonicsGridLayout = new QGridLayout(registerHarmonicsGridWidget);
	registerHarmonicsGridWidget->setLayout(registerHarmonicsGridLayout);
	registerHarmonicsGridLayout->setMargin(0);
	registerHarmonicsGridLayout->setSpacing(8);

	QLabel *registerSensorDataLabel = new QLabel("Sensor Data", registerWidget);
	StyleHelper::MenuControlLabel(registerSensorDataLabel, "registerSensorDataLabel");
	QWidget *registerSensorDataGridWidget = new QWidget(registerWidget);
	QGridLayout *registerSensorDataGridLayout = new QGridLayout(registerSensorDataGridWidget);
	registerSensorDataGridWidget->setLayout(registerSensorDataGridLayout);
	registerSensorDataGridLayout->setMargin(0);
	registerSensorDataGridLayout->setSpacing(8);

	cnvPageRegisterBlock = new RegisterBlockWidget("CNVPAGE", "Convert Start and Page Select", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::CNVPAGE), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	digIORegisterBlock = new RegisterBlockWidget("DIGIO", "Digital Input Output", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIO), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::DIGIO), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	faultRegisterBlock = new RegisterBlockWidget("FAULT", "Fault Register", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::FAULT), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::FAULT), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	generalRegisterBlock = new RegisterBlockWidget("GENERAL", "General Device Configuration", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::GENERAL), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::GENERAL), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	digIOEnRegisterBlock = new RegisterBlockWidget("DIGIOEN", "Enable Digital Input/Outputs", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::DIGIOEN), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	angleCkRegisterBlock = new RegisterBlockWidget("ANGLECK", "Primary vs Secondary Angle Check", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::ANGLECK), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::ANGLECK), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	eccDcdeRegisterBlock = new RegisterBlockWidget("ECCDCDE", "Error Correction Codes", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::ECCDCDE), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::ECCDCDE), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	eccDisRegisterBlock = new RegisterBlockWidget("ECCDIS", "Error Correction Code disable", m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::ECCDIS), m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::ECCDIS), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);

	absAngleRegisterBlock = new RegisterBlockWidget("ABSANGLE", "Absolute Angle", m_admtController->getSensorRegister(ADMTController::SensorRegister::ABSANGLE), m_admtController->getSensorPage(ADMTController::SensorRegister::ABSANGLE), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	angleRegisterBlock = new RegisterBlockWidget("ANGLE", "Angle Register", m_admtController->getSensorRegister(ADMTController::SensorRegister::ANGLEREG), m_admtController->getSensorPage(ADMTController::SensorRegister::ANGLEREG), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	angleSecRegisterBlock = new RegisterBlockWidget("ANGLESEC", "Secondary Angle", m_admtController->getSensorRegister(ADMTController::SensorRegister::ANGLESEC), m_admtController->getSensorPage(ADMTController::SensorRegister::ANGLESEC), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	sineRegisterBlock = new RegisterBlockWidget("SINE", "Sine Measurement", m_admtController->getSensorRegister(ADMTController::SensorRegister::SINE), m_admtController->getSensorPage(ADMTController::SensorRegister::SINE), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	cosineRegisterBlock = new RegisterBlockWidget("COSINE", "Cosine Measurement", m_admtController->getSensorRegister(ADMTController::SensorRegister::COSINE), m_admtController->getSensorPage(ADMTController::SensorRegister::COSINE), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	secAnglIRegisterBlock = new RegisterBlockWidget("SECANGLI", "In-phase secondary angle measurement", m_admtController->getSensorRegister(ADMTController::SensorRegister::SECANGLI), m_admtController->getSensorPage(ADMTController::SensorRegister::SECANGLI), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	secAnglQRegisterBlock = new RegisterBlockWidget("SECANGLQ", "Quadrature phase secondary angle measurement", m_admtController->getSensorRegister(ADMTController::SensorRegister::SECANGLQ), m_admtController->getSensorPage(ADMTController::SensorRegister::SECANGLQ), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	radiusRegisterBlock = new RegisterBlockWidget("RADIUS", "Angle measurement radius", m_admtController->getSensorRegister(ADMTController::SensorRegister::RADIUS), m_admtController->getSensorPage(ADMTController::SensorRegister::RADIUS), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	diag1RegisterBlock = new RegisterBlockWidget("DIAG1", "State of the MT reference resistors and AFE fixed input voltage", m_admtController->getSensorRegister(ADMTController::SensorRegister::DIAG1), m_admtController->getSensorPage(ADMTController::SensorRegister::DIAG1), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	diag2RegisterBlock = new RegisterBlockWidget("DIAG2", "Measurements of two diagnostics resistors of fixed value", m_admtController->getSensorRegister(ADMTController::SensorRegister::DIAG2), m_admtController->getSensorPage(ADMTController::SensorRegister::DIAG2), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	tmp0RegisterBlock = new RegisterBlockWidget("TMP0", "Primary Temperature Sensor", m_admtController->getSensorRegister(ADMTController::SensorRegister::TMP0), m_admtController->getSensorPage(ADMTController::SensorRegister::TMP0), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	tmp1RegisterBlock = new RegisterBlockWidget("TMP1", "Secondary Temperature Sensor", m_admtController->getSensorRegister(ADMTController::SensorRegister::TMP1), m_admtController->getSensorPage(ADMTController::SensorRegister::TMP1), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	cnvCntRegisterBlock = new RegisterBlockWidget("CNVCNT", "Conversion Count", m_admtController->getSensorRegister(ADMTController::SensorRegister::CNVCNT), m_admtController->getSensorPage(ADMTController::SensorRegister::CNVCNT), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);

	uniqID0RegisterBlock = new RegisterBlockWidget("UNIQID0", "Unique ID Register 0", m_admtController->getUniqueIdRegister(ADMTController::UniqueIDRegister::UNIQID0), m_admtController->getUniqueIdPage(ADMTController::UniqueIDRegister::UNIQID0), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	uniqID1RegisterBlock = new RegisterBlockWidget("UNIQID1", "Unique ID Register 1", m_admtController->getUniqueIdRegister(ADMTController::UniqueIDRegister::UNIQID1), m_admtController->getUniqueIdPage(ADMTController::UniqueIDRegister::UNIQID1), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	uniqID2RegisterBlock = new RegisterBlockWidget("UNIQID2", "Unique ID Register 2", m_admtController->getUniqueIdRegister(ADMTController::UniqueIDRegister::UNIQID2), m_admtController->getUniqueIdPage(ADMTController::UniqueIDRegister::UNIQID2), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);
	uniqID3RegisterBlock = new RegisterBlockWidget("UNIQID3", "Product, voltage supply. ASIL and ASIC revision identifiers", m_admtController->getUniqueIdRegister(ADMTController::UniqueIDRegister::UNIQID3), m_admtController->getUniqueIdPage(ADMTController::UniqueIDRegister::UNIQID3), RegisterBlockWidget::ACCESS_PERMISSION::READ, registerWidget);

	h1MagRegisterBlock = new RegisterBlockWidget("H1MAG", "1st Harmonic error magnitude", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1MAG), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H1MAG), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	h1PhRegisterBlock = new RegisterBlockWidget("H1PH", "1st Harmonic error phase", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1PH), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H1PH), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	h2MagRegisterBlock = new RegisterBlockWidget("H2MAG", "2nd Harmonic error magnitude", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2MAG), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H2MAG), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	h2PhRegisterBlock = new RegisterBlockWidget("H2PH", "2nd Harmonic error phase", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2PH), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H2PH), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	h3MagRegisterBlock = new RegisterBlockWidget("H3MAG", "3rd Harmonic error magnitude", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3MAG), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H3MAG), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	h3PhRegisterBlock = new RegisterBlockWidget("H3PH", "3rd Harmonic error phase", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3PH), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H3PH), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	h8MagRegisterBlock = new RegisterBlockWidget("H8MAG", "8th Harmonic error magnitude", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8MAG), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H8MAG), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	h8PhRegisterBlock = new RegisterBlockWidget("H8PH", "8th Harmonic error phase", m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8PH), m_admtController->getHarmonicPage(ADMTController::HarmonicRegister::H8PH), RegisterBlockWidget::ACCESS_PERMISSION::READWRITE, registerWidget);
	
	registerConfigurationGridLayout->addWidget(cnvPageRegisterBlock, 0, 0);
	registerConfigurationGridLayout->addWidget(digIORegisterBlock, 0, 1);
	registerConfigurationGridLayout->addWidget(faultRegisterBlock, 0, 2);
	registerConfigurationGridLayout->addWidget(generalRegisterBlock, 0, 3);
	registerConfigurationGridLayout->addWidget(digIOEnRegisterBlock, 0, 4);
	registerConfigurationGridLayout->addWidget(eccDcdeRegisterBlock, 1, 0);
	registerConfigurationGridLayout->addWidget(eccDisRegisterBlock, 1, 1);
	registerConfigurationGridLayout->addWidget(angleCkRegisterBlock, 1, 2);

	registerSensorDataGridLayout->addWidget(absAngleRegisterBlock, 0, 0);
	registerSensorDataGridLayout->addWidget(angleRegisterBlock, 0, 1);
	registerSensorDataGridLayout->addWidget(sineRegisterBlock, 0, 2);
	registerSensorDataGridLayout->addWidget(cosineRegisterBlock, 0, 3);
	registerSensorDataGridLayout->addWidget(cnvCntRegisterBlock, 0, 4);
	registerSensorDataGridLayout->addWidget(tmp0RegisterBlock, 1, 0);
	registerSensorDataGridLayout->addWidget(tmp1RegisterBlock, 1, 1);
	registerSensorDataGridLayout->addWidget(diag1RegisterBlock, 1, 2);
	registerSensorDataGridLayout->addWidget(diag2RegisterBlock, 1, 3);
	registerSensorDataGridLayout->addWidget(radiusRegisterBlock, 1, 4);
	registerSensorDataGridLayout->addWidget(angleSecRegisterBlock, 2, 0);
	registerSensorDataGridLayout->addWidget(secAnglIRegisterBlock, 2, 1);
	registerSensorDataGridLayout->addWidget(secAnglQRegisterBlock, 2, 2);

	registerDeviceIDGridLayout->addWidget(uniqID0RegisterBlock, 0, 0);
	registerDeviceIDGridLayout->addWidget(uniqID1RegisterBlock, 0, 1);
	registerDeviceIDGridLayout->addWidget(uniqID2RegisterBlock, 0, 2);
	registerDeviceIDGridLayout->addWidget(uniqID3RegisterBlock, 0, 3);
	QSpacerItem *registerDeviceSpacer = new QSpacerItem(0, 0, QSizePolicy::Preferred, QSizePolicy::Preferred);
	registerDeviceIDGridLayout->addItem(registerDeviceSpacer, 0, 4);

	registerHarmonicsGridLayout->addWidget(h1MagRegisterBlock, 0, 0);
	registerHarmonicsGridLayout->addWidget(h1PhRegisterBlock, 0, 1);
	registerHarmonicsGridLayout->addWidget(h2MagRegisterBlock, 0, 2);
	registerHarmonicsGridLayout->addWidget(h2PhRegisterBlock, 0, 3);
	registerHarmonicsGridLayout->addWidget(h3MagRegisterBlock, 0, 4);
	registerHarmonicsGridLayout->addWidget(h3PhRegisterBlock, 1, 0);
	registerHarmonicsGridLayout->addWidget(h8MagRegisterBlock, 1, 1);
	registerHarmonicsGridLayout->addWidget(h8PhRegisterBlock, 1, 2);
	
	for(int c=0; c < registerConfigurationGridLayout->columnCount(); ++c) registerConfigurationGridLayout->setColumnStretch(c,1);
	for(int c=0; c < registerDeviceIDGridLayout->columnCount(); ++c) registerDeviceIDGridLayout->setColumnStretch(c,1);
	for(int c=0; c < registerHarmonicsGridLayout->columnCount(); ++c) registerHarmonicsGridLayout->setColumnStretch(c,1);
	for(int c=0; c < registerSensorDataGridLayout->columnCount(); ++c) registerSensorDataGridLayout->setColumnStretch(c,1);

	readAllRegistersButton = new QPushButton("Read All Registers", registerWidget);
	StyleHelper::BlueButton(readAllRegistersButton, "readAllRegistersButton");
	readAllRegistersButton->setFixedWidth(270);
	connect(readAllRegistersButton, &QPushButton::clicked, this, &HarmonicCalibration::readAllRegisters);

	registerLayout->addWidget(readAllRegistersButton);
	registerLayout->addWidget(registerConfigurationLabel);
	registerLayout->addWidget(registerConfigurationGridWidget);
	registerLayout->addWidget(registerSensorDataLabel);
	registerLayout->addWidget(registerSensorDataGridWidget);
	registerLayout->addWidget(registerDeviceIDLabel);
	registerLayout->addWidget(registerDeviceIDGridWidget);
	registerLayout->addWidget(registerHarmonicsLabel);
	registerLayout->addWidget(registerHarmonicsGridWidget);
	registerLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

	tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tool->topContainer()->setVisible(false);
	tool->topContainerMenuControl()->setVisible(false);
	tool->leftContainer()->setVisible(false);
	tool->rightContainer()->setVisible(false);
	tool->bottomContainer()->setVisible(false);
	tool->setLeftContainerWidth(270);
	tool->setRightContainerWidth(270);
	tool->openBottomContainerHelper(false);
	tool->openTopContainerHelper(false);

	tool->addWidgetToCentralContainerHelper(registerScrollArea);

	connectRegisterBlockToRegistry(cnvPageRegisterBlock);
	connectRegisterBlockToRegistry(digIORegisterBlock);
	connectRegisterBlockToRegistry(faultRegisterBlock);
	connectRegisterBlockToRegistry(generalRegisterBlock);
	connectRegisterBlockToRegistry(digIOEnRegisterBlock);
	connectRegisterBlockToRegistry(angleCkRegisterBlock);
	connectRegisterBlockToRegistry(eccDcdeRegisterBlock);
	connectRegisterBlockToRegistry(eccDisRegisterBlock);

	connectRegisterBlockToRegistry(absAngleRegisterBlock);
	connectRegisterBlockToRegistry(angleRegisterBlock);
	connectRegisterBlockToRegistry(angleSecRegisterBlock);
	connectRegisterBlockToRegistry(sineRegisterBlock);
	connectRegisterBlockToRegistry(cosineRegisterBlock);
	connectRegisterBlockToRegistry(secAnglIRegisterBlock);
	connectRegisterBlockToRegistry(secAnglQRegisterBlock);
	connectRegisterBlockToRegistry(radiusRegisterBlock);
	connectRegisterBlockToRegistry(diag1RegisterBlock);
	connectRegisterBlockToRegistry(diag2RegisterBlock);
	connectRegisterBlockToRegistry(tmp0RegisterBlock);
	connectRegisterBlockToRegistry(tmp1RegisterBlock);
	connectRegisterBlockToRegistry(cnvCntRegisterBlock);

	connectRegisterBlockToRegistry(uniqID0RegisterBlock);
	connectRegisterBlockToRegistry(uniqID1RegisterBlock);
	connectRegisterBlockToRegistry(uniqID2RegisterBlock);
	connectRegisterBlockToRegistry(uniqID3RegisterBlock);

	connectRegisterBlockToRegistry(h1MagRegisterBlock);
	connectRegisterBlockToRegistry(h1PhRegisterBlock);
	connectRegisterBlockToRegistry(h2MagRegisterBlock);
	connectRegisterBlockToRegistry(h2PhRegisterBlock);
	connectRegisterBlockToRegistry(h3MagRegisterBlock);
	connectRegisterBlockToRegistry(h3PhRegisterBlock);
	connectRegisterBlockToRegistry(h8MagRegisterBlock);
	connectRegisterBlockToRegistry(h8PhRegisterBlock);

	return tool;
}

ToolTemplate* HarmonicCalibration::createUtilityWidget()
{
	ToolTemplate *tool = new ToolTemplate(this);

	#pragma region Left Utility Widget
	QWidget *leftUtilityWidget = new QWidget(this);
	QVBoxLayout *leftUtilityLayout = new QVBoxLayout(leftUtilityWidget);
	leftUtilityWidget->setLayout(leftUtilityLayout);
	leftUtilityLayout->setMargin(0);
	leftUtilityLayout->setSpacing(8);
	#pragma region Command Log Widget
	MenuSectionWidget *commandLogSectionWidget = new MenuSectionWidget(leftUtilityWidget);
	MenuCollapseSection *commandLogCollapseSection = new MenuCollapseSection("Command Log", MenuCollapseSection::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, commandLogSectionWidget);
	commandLogSectionWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	commandLogSectionWidget->contentLayout()->addWidget(commandLogCollapseSection);
	commandLogCollapseSection->contentLayout()->setSpacing(8);

	commandLogPlainTextEdit = new QPlainTextEdit(commandLogSectionWidget);
	commandLogPlainTextEdit->setReadOnly(true);
	commandLogPlainTextEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	clearCommandLogButton = new QPushButton("Clear Command Logs", commandLogSectionWidget);
	StyleHelper::BlueButton(clearCommandLogButton, "clearCommandLogButton");
	connect(clearCommandLogButton, &QPushButton::clicked, this, &HarmonicCalibration::clearCommandLog);

	commandLogCollapseSection->contentLayout()->addWidget(commandLogPlainTextEdit);
	commandLogCollapseSection->contentLayout()->addWidget(clearCommandLogButton);

	leftUtilityLayout->addWidget(commandLogSectionWidget, 1);
	leftUtilityLayout->addStretch();
	#pragma endregion
	#pragma endregion

	#pragma region Center Utility Widget
	QWidget *centerUtilityWidget = new QWidget(this);
	QHBoxLayout *centerUtilityLayout = new QHBoxLayout(centerUtilityWidget);
	centerUtilityWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	centerUtilityWidget->setContentsMargins(2, 0, 2, 0);
	centerUtilityWidget->setLayout(centerUtilityLayout);
	centerUtilityLayout->setMargin(0);
	centerUtilityLayout->setSpacing(8);

	QScrollArea *DIGIOScrollArea = new QScrollArea(centerUtilityWidget);
	QWidget *DIGIOWidget = new QWidget(DIGIOScrollArea);
	DIGIOScrollArea->setWidget(DIGIOWidget);
	DIGIOScrollArea->setWidgetResizable(true);
	QVBoxLayout *DIGIOLayout = new QVBoxLayout(DIGIOWidget);
	DIGIOWidget->setLayout(DIGIOLayout);
	DIGIOLayout->setMargin(0);
	DIGIOLayout->setSpacing(5);

	#pragma region DIGIO Monitor
	MenuSectionWidget *DIGIOMonitorSectionWidget = new MenuSectionWidget(DIGIOWidget);
	MenuCollapseSection *DIGIOMonitorCollapseSection = new MenuCollapseSection("DIGIO Monitor", MenuCollapseSection::MenuHeaderCollapseStyle::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, DIGIOMonitorSectionWidget);
	DIGIOMonitorSectionWidget->contentLayout()->addWidget(DIGIOMonitorCollapseSection);

	DIGIOBusyStatusLED = createStatusLEDWidget("BUSY", statusLEDColor, DIGIOMonitorCollapseSection);
	DIGIOCNVStatusLED = createStatusLEDWidget("CNV", statusLEDColor, DIGIOMonitorCollapseSection);
	DIGIOSENTStatusLED = createStatusLEDWidget("SENT", statusLEDColor, DIGIOMonitorCollapseSection);
	DIGIOACALCStatusLED = createStatusLEDWidget("ACALC", statusLEDColor, DIGIOMonitorCollapseSection);
	DIGIOFaultStatusLED = createStatusLEDWidget("FAULT", statusLEDColor, DIGIOMonitorCollapseSection);
	DIGIOBootloaderStatusLED = createStatusLEDWidget("BOOTLOADER", statusLEDColor, DIGIOMonitorCollapseSection);

	DIGIOMonitorCollapseSection->contentLayout()->addWidget(DIGIOBusyStatusLED);
	DIGIOMonitorCollapseSection->contentLayout()->addWidget(DIGIOCNVStatusLED);
	DIGIOMonitorCollapseSection->contentLayout()->addWidget(DIGIOSENTStatusLED);
	DIGIOMonitorCollapseSection->contentLayout()->addWidget(DIGIOACALCStatusLED);
	DIGIOMonitorCollapseSection->contentLayout()->addWidget(DIGIOFaultStatusLED);
	DIGIOMonitorCollapseSection->contentLayout()->addWidget(DIGIOBootloaderStatusLED);
	#pragma endregion

	#pragma region DIGIO Control
	MenuSectionWidget *DIGIOControlSectionWidget = new MenuSectionWidget(DIGIOWidget);
	MenuCollapseSection *DIGIOControlCollapseSection = new MenuCollapseSection("DIGIO Control", MenuCollapseSection::MenuHeaderCollapseStyle::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, DIGIOControlSectionWidget);
	DIGIOControlSectionWidget->contentLayout()->addWidget(DIGIOControlCollapseSection);

	QWidget *DIGIOControlGridWidget = new QWidget(DIGIOControlSectionWidget);
	QGridLayout *DIGIOControlGridLayout = new QGridLayout(DIGIOControlGridWidget);
	DIGIOControlGridWidget->setLayout(DIGIOControlGridLayout);
	DIGIOControlGridLayout->setMargin(0);
	DIGIOControlGridLayout->setSpacing(8);

	QString labelStyle = QString(R"css(
				QLabel {
					color: white;
					background-color: rgba(255,255,255,0);
					font-weight: 500;
					font-family: Open Sans;
					font-size: 12px;
					font-style: normal;
					}
				QLabel:disabled {
					color: grey;
				}
				)css");

	QLabel *DIGIO0Label = new QLabel("DIGIO0", DIGIOControlGridWidget);
	QLabel *DIGIO1Label = new QLabel("DIGIO1", DIGIOControlGridWidget);
	QLabel *DIGIO2Label = new QLabel("DIGIO2", DIGIOControlGridWidget);
	QLabel *DIGIO3Label = new QLabel("DIGIO3", DIGIOControlGridWidget);
	QLabel *DIGIO4Label = new QLabel("DIGIO4", DIGIOControlGridWidget);
	QLabel *DIGIO5Label = new QLabel("DIGIO5", DIGIOControlGridWidget);
	QLabel *DIGIOALLLabel = new QLabel("All DIGIO Output", DIGIOControlGridWidget);

	DIGIO0Label->setStyleSheet(labelStyle);
	DIGIO1Label->setStyleSheet(labelStyle);
	DIGIO2Label->setStyleSheet(labelStyle);
	DIGIO3Label->setStyleSheet(labelStyle);
	DIGIO4Label->setStyleSheet(labelStyle);
	DIGIO5Label->setStyleSheet(labelStyle);
	DIGIOALLLabel->setStyleSheet(labelStyle);

	DIGIO0ENToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO0ENToggleSwitch, "Enable", "Disable");
	connect(DIGIO0ENToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("DIGIO0EN", value);
	});

	DIGIO0FNCToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO0FNCToggleSwitch, "GPIO0", "BUSY");
	connect(DIGIO0FNCToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("BUSY", value);
	});

	DIGIO1ENToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO1ENToggleSwitch, "Enable", "Disable");
	connect(DIGIO1ENToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("DIGIO1EN", value);
	});

	DIGIO1FNCToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO1FNCToggleSwitch, "GPIO1", "CNV");
	connect(DIGIO1FNCToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("CNV", value);
	});

	DIGIO2ENToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO2ENToggleSwitch, "Enable", "Disable");
	connect(DIGIO2ENToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("DIGIO2EN", value);
	});

	DIGIO2FNCToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO2FNCToggleSwitch, "GPIO2", "SENT");
	connect(DIGIO2FNCToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("SENT", value);
	});

	DIGIO3ENToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO3ENToggleSwitch, "Enable", "Disable");
	connect(DIGIO3ENToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("DIGIO3EN", value);
	});

	DIGIO3FNCToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO3FNCToggleSwitch, "GPIO3", "ACALC");
	connect(DIGIO3FNCToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("ACALC", value);
	});

	DIGIO4ENToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO4ENToggleSwitch, "Enable", "Disable");
	connect(DIGIO4ENToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("DIGIO4EN", value);
	});

	DIGIO4FNCToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO4FNCToggleSwitch, "GPIO4", "FAULT");
	connect(DIGIO4FNCToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("FAULT", value);
	});

	DIGIO5ENToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO5ENToggleSwitch, "Enable", "Disable");
	connect(DIGIO5ENToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("DIGIO5EN", value);
	});

	DIGIO5FNCToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIO5FNCToggleSwitch, "GPIO5", "BOOT");
	connect(DIGIO5FNCToggleSwitch, &CustomSwitch::clicked, [=](bool value){
		toggleDIGIOEN("BOOTLOAD", value);
	});

	DIGIOALLToggleSwitch = new CustomSwitch();
	changeCustomSwitchLabel(DIGIOALLToggleSwitch, "Enable", "Disable");
	connect(DIGIOALLToggleSwitch, &CustomSwitch::toggled, [=](bool value){
		toggleAllDIGIOEN(value);
		// toggleAllDIGIO(value);
	});

	QPushButton *DIGIOResetButton = new QPushButton("Reset DIGIO", DIGIOControlGridWidget);
	DIGIOResetButton->setFixedWidth(100);
	StyleHelper::BlueButton(DIGIOResetButton);
	connect(DIGIOResetButton, &QPushButton::clicked, [=]{
		resetDIGIO();
		updateDIGIOToggle();
	});
	
	DIGIOControlGridLayout->addWidget(DIGIOALLLabel, 0, 0);
	DIGIOControlGridLayout->addWidget(DIGIOALLToggleSwitch, 0, 1);
	DIGIOControlGridLayout->addWidget(DIGIOResetButton, 0, 2);

	DIGIOControlGridLayout->addItem(new QSpacerItem(0, 4, QSizePolicy::Fixed, QSizePolicy::Expanding), 1, 0, 1, 2);
	QFrame *line = new QFrame();
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Plain);
	line->setFixedHeight(1);
	QString lineStyle = QString(R"css(
				QFrame {
					border: 1px solid #808085;
				}
				)css");
	line->setStyleSheet(lineStyle);
	DIGIOControlGridLayout->addWidget(line, 2, 0, 1, 2);
	DIGIOControlGridLayout->addItem(new QSpacerItem(0, 4, QSizePolicy::Fixed, QSizePolicy::Expanding), 3, 0, 1, 3);

	DIGIOControlGridLayout->addWidget(DIGIO0Label, 4, 0);
	DIGIOControlGridLayout->addWidget(DIGIO0ENToggleSwitch, 4, 1);
	DIGIOControlGridLayout->addWidget(DIGIO0FNCToggleSwitch, 4, 2);
	DIGIOControlGridLayout->addWidget(DIGIO1Label, 5, 0);
	DIGIOControlGridLayout->addWidget(DIGIO1ENToggleSwitch, 5, 1);
	DIGIOControlGridLayout->addWidget(DIGIO1FNCToggleSwitch, 5, 2);
	DIGIOControlGridLayout->addWidget(DIGIO2Label, 6, 0);
	DIGIOControlGridLayout->addWidget(DIGIO2ENToggleSwitch, 6, 1);
	DIGIOControlGridLayout->addWidget(DIGIO2FNCToggleSwitch, 6, 2);
	DIGIOControlGridLayout->addWidget(DIGIO3Label, 7, 0);
	DIGIOControlGridLayout->addWidget(DIGIO3ENToggleSwitch, 7, 1);
	DIGIOControlGridLayout->addWidget(DIGIO3FNCToggleSwitch, 7, 2);
	DIGIOControlGridLayout->addWidget(DIGIO4Label, 8, 0);
	DIGIOControlGridLayout->addWidget(DIGIO4ENToggleSwitch, 8, 1);
	DIGIOControlGridLayout->addWidget(DIGIO4FNCToggleSwitch, 8, 2);
	DIGIOControlGridLayout->addWidget(DIGIO5Label, 9, 0);
	DIGIOControlGridLayout->addWidget(DIGIO5ENToggleSwitch, 9, 1);
	DIGIOControlGridLayout->addWidget(DIGIO5FNCToggleSwitch, 9, 2);

	DIGIOControlCollapseSection->contentLayout()->addWidget(DIGIOControlGridWidget, 1);
	#pragma endregion

	DIGIOLayout->addWidget(DIGIOMonitorSectionWidget);
	DIGIOLayout->addWidget(DIGIOControlSectionWidget);
	DIGIOLayout->addStretch();

	#pragma region MTDIAG1 Widget
	MenuSectionWidget *MTDIAG1SectionWidget = new MenuSectionWidget(centerUtilityWidget);
	MenuCollapseSection *MTDIAG1CollapseSection = new MenuCollapseSection("MT Diagnostic Register", MenuCollapseSection::MenuHeaderCollapseStyle::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, MTDIAG1SectionWidget);
	MTDIAG1SectionWidget->contentLayout()->addWidget(MTDIAG1CollapseSection);

	R0StatusLED = createStatusLEDWidget("R0", statusLEDColor, MTDIAG1SectionWidget);
	R1StatusLED = createStatusLEDWidget("R1", statusLEDColor, MTDIAG1SectionWidget);
	R2StatusLED = createStatusLEDWidget("R2", statusLEDColor, MTDIAG1SectionWidget);
	R3StatusLED = createStatusLEDWidget("R3", statusLEDColor, MTDIAG1SectionWidget);
	R4StatusLED = createStatusLEDWidget("R4", statusLEDColor, MTDIAG1SectionWidget);
	R5StatusLED = createStatusLEDWidget("R5", statusLEDColor, MTDIAG1SectionWidget);
	R6StatusLED = createStatusLEDWidget("R6", statusLEDColor, MTDIAG1SectionWidget);
	R7StatusLED = createStatusLEDWidget("R7", statusLEDColor, MTDIAG1SectionWidget);

	MTDIAG1CollapseSection->contentLayout()->addWidget(R0StatusLED);
	MTDIAG1CollapseSection->contentLayout()->addWidget(R1StatusLED);
	MTDIAG1CollapseSection->contentLayout()->addWidget(R2StatusLED);
	MTDIAG1CollapseSection->contentLayout()->addWidget(R3StatusLED);
	MTDIAG1CollapseSection->contentLayout()->addWidget(R4StatusLED);
	MTDIAG1CollapseSection->contentLayout()->addWidget(R5StatusLED);
	MTDIAG1CollapseSection->contentLayout()->addWidget(R6StatusLED);
	MTDIAG1CollapseSection->contentLayout()->addWidget(R7StatusLED);
	#pragma endregion

	#pragma region MT Diagnostics
	MTDiagnosticsScrollArea = new QScrollArea(centerUtilityWidget);
	QWidget *MTDiagnosticsWidget = new QWidget(MTDiagnosticsScrollArea);
	MTDiagnosticsScrollArea->setWidget(MTDiagnosticsWidget);
	MTDiagnosticsScrollArea->setWidgetResizable(true);
	QVBoxLayout *MTDiagnosticsLayout = new QVBoxLayout(MTDiagnosticsWidget);
	MTDiagnosticsWidget->setLayout(MTDiagnosticsLayout);
	MTDiagnosticsLayout->setMargin(0);
	MTDiagnosticsLayout->setSpacing(5);

	MenuSectionWidget *MTDiagnosticsSectionWidget = new MenuSectionWidget(centerUtilityWidget);
	MenuCollapseSection *MTDiagnosticsCollapseSection = new MenuCollapseSection("MT Diagnostics", MenuCollapseSection::MenuHeaderCollapseStyle::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, MTDiagnosticsSectionWidget);
	MTDiagnosticsSectionWidget->contentLayout()->addWidget(MTDiagnosticsCollapseSection);
	MTDiagnosticsCollapseSection->contentLayout()->setSpacing(8);

	QLabel *AFEDIAG0Label = new QLabel("AFEDIAG0 (%)");
	StyleHelper::MenuSmallLabel(AFEDIAG0Label, "AFEDIAG0Label");
	QLabel *AFEDIAG1Label = new QLabel("AFEDIAG1 (%)");
	StyleHelper::MenuSmallLabel(AFEDIAG1Label, "AFEDIAG1Label");
	QLabel *AFEDIAG2Label = new QLabel("AFEDIAG2 (V)");
	StyleHelper::MenuSmallLabel(AFEDIAG2Label, "AFEDIAG2Label");

	AFEDIAG0LineEdit = new QLineEdit(MTDiagnosticsSectionWidget);
	AFEDIAG1LineEdit = new QLineEdit(MTDiagnosticsSectionWidget);
	AFEDIAG2LineEdit = new QLineEdit(MTDiagnosticsSectionWidget);
	ADMTStyleHelper::LineEditStyle(AFEDIAG0LineEdit);
	ADMTStyleHelper::LineEditStyle(AFEDIAG1LineEdit);
	ADMTStyleHelper::LineEditStyle(AFEDIAG2LineEdit);
	AFEDIAG0LineEdit->setReadOnly(true);
	AFEDIAG1LineEdit->setReadOnly(true);
	AFEDIAG2LineEdit->setReadOnly(true);
	connectLineEditToNumber(AFEDIAG0LineEdit, afeDiag0, "V");
	connectLineEditToNumber(AFEDIAG1LineEdit, afeDiag1, "V");
	connectLineEditToNumber(AFEDIAG2LineEdit, afeDiag2, "V");

	MTDiagnosticsCollapseSection->contentLayout()->addWidget(AFEDIAG0Label);
	MTDiagnosticsCollapseSection->contentLayout()->addWidget(AFEDIAG0LineEdit);
	MTDiagnosticsCollapseSection->contentLayout()->addWidget(AFEDIAG1Label);
	MTDiagnosticsCollapseSection->contentLayout()->addWidget(AFEDIAG1LineEdit);
	MTDiagnosticsCollapseSection->contentLayout()->addWidget(AFEDIAG2Label);
	MTDiagnosticsCollapseSection->contentLayout()->addWidget(AFEDIAG2LineEdit);

	MTDiagnosticsLayout->addWidget(MTDiagnosticsSectionWidget);
	MTDiagnosticsLayout->addWidget(MTDIAG1SectionWidget);
	MTDiagnosticsLayout->addStretch();
	#pragma endregion

	centerUtilityLayout->addWidget(DIGIOScrollArea);
	centerUtilityLayout->addWidget(MTDiagnosticsScrollArea);

	#pragma endregion

	#pragma region Right Utility Widget
	QScrollArea *rightUtilityScrollArea = new QScrollArea(this);
	QWidget *rightUtilityWidget = new QWidget(rightUtilityScrollArea);
	rightUtilityScrollArea->setWidget(rightUtilityWidget);
	rightUtilityScrollArea->setWidgetResizable(true);
	QVBoxLayout *rightUtilityLayout = new QVBoxLayout(rightUtilityWidget);
	rightUtilityWidget->setLayout(rightUtilityLayout);
	rightUtilityLayout->setMargin(0);
	rightUtilityLayout->setSpacing(8);

	MenuSectionWidget *faultRegisterSectionWidget = new MenuSectionWidget(rightUtilityWidget);
	MenuCollapseSection *faultRegisterCollapseSection = new MenuCollapseSection("Fault Register", MenuCollapseSection::MenuHeaderCollapseStyle::MHCW_NONE, MenuCollapseSection::MenuHeaderWidgetType::MHW_BASEWIDGET, faultRegisterSectionWidget);
	faultRegisterSectionWidget->contentLayout()->addWidget(faultRegisterCollapseSection);
	
	VDDUnderVoltageStatusLED = createStatusLEDWidget("VDD Under Voltage", faultLEDColor, faultRegisterCollapseSection);
	VDDOverVoltageStatusLED = createStatusLEDWidget("VDD Over Voltage", faultLEDColor, faultRegisterCollapseSection);
	VDRIVEUnderVoltageStatusLED = createStatusLEDWidget("VDRIVE Under Voltage", faultLEDColor, faultRegisterCollapseSection);
	VDRIVEOverVoltageStatusLED = createStatusLEDWidget("VDRIVE Over Voltage", faultLEDColor, faultRegisterCollapseSection);
	AFEDIAGStatusLED = createStatusLEDWidget("AFEDIAG", faultLEDColor, faultRegisterCollapseSection);
	NVMCRCFaultStatusLED = createStatusLEDWidget("NVM CRC Fault", faultLEDColor, faultRegisterCollapseSection);
	ECCDoubleBitErrorStatusLED = createStatusLEDWidget("ECC Double Bit Error", faultLEDColor, faultRegisterCollapseSection);
	OscillatorDriftStatusLED = createStatusLEDWidget("Oscillator Drift", faultLEDColor, faultRegisterCollapseSection);
	CountSensorFalseStateStatusLED = createStatusLEDWidget("Count Sensor False State", faultLEDColor, faultRegisterCollapseSection);
	AngleCrossCheckStatusLED = createStatusLEDWidget("Angle Cross Check", faultLEDColor, faultRegisterCollapseSection);
	TurnCountSensorLevelsStatusLED = createStatusLEDWidget("Turn Count Sensor Levels", faultLEDColor, faultRegisterCollapseSection);
	MTDIAGStatusLED = createStatusLEDWidget("MTDIAG", faultLEDColor, faultRegisterCollapseSection);
	TurnCounterCrossCheckStatusLED = createStatusLEDWidget("Turn Counter Cross Check", faultLEDColor, faultRegisterCollapseSection);
	RadiusCheckStatusLED = createStatusLEDWidget("Radius Check", faultLEDColor, faultRegisterCollapseSection);
	SequencerWatchdogStatusLED = createStatusLEDWidget("Sequencer Watchdog", faultLEDColor, faultRegisterCollapseSection);

	faultRegisterCollapseSection->contentLayout()->addWidget(VDDUnderVoltageStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(VDDOverVoltageStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(VDRIVEUnderVoltageStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(VDRIVEOverVoltageStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(AFEDIAGStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(NVMCRCFaultStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(ECCDoubleBitErrorStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(OscillatorDriftStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(CountSensorFalseStateStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(AngleCrossCheckStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(TurnCountSensorLevelsStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(MTDIAGStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(TurnCounterCrossCheckStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(RadiusCheckStatusLED);
	faultRegisterCollapseSection->contentLayout()->addWidget(SequencerWatchdogStatusLED);

	rightUtilityLayout->addWidget(faultRegisterSectionWidget);
	rightUtilityLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
	#pragma endregion
	
	tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tool->topContainer()->setVisible(false);
	tool->topContainerMenuControl()->setVisible(false);
	tool->leftContainer()->setVisible(true);
	tool->rightContainer()->setVisible(true);
	tool->bottomContainer()->setVisible(false);
	tool->setLeftContainerWidth(270);
	tool->setRightContainerWidth(270);
	tool->openBottomContainerHelper(false);
	tool->openTopContainerHelper(false);

	tool->leftStack()->addWidget(leftUtilityWidget);
	tool->addWidgetToCentralContainerHelper(centerUtilityWidget);
	tool->rightStack()->addWidget(rightUtilityScrollArea);

	return tool;
}

void HarmonicCalibration::resetDIGIO()
{
	m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
										m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN), 
										0x241b);
}

void HarmonicCalibration::readAllRegisters()
{
	readAllRegistersButton->setEnabled(false);
	readAllRegistersButton->setText(QString("Reading Registers..."));
	QTimer::singleShot(1000, this, [this](){
		readAllRegistersButton->setEnabled(true);
		readAllRegistersButton->setText(QString("Read All Registers"));
	});

	cnvPageRegisterBlock->readButton()->click();
	digIORegisterBlock->readButton()->click();
	faultRegisterBlock->readButton()->click();
	generalRegisterBlock->readButton()->click();
	digIOEnRegisterBlock->readButton()->click();
	eccDcdeRegisterBlock->readButton()->click();
	eccDisRegisterBlock->readButton()->click();
	absAngleRegisterBlock->readButton()->click();
	angleRegisterBlock->readButton()->click();
	sineRegisterBlock->readButton()->click();
	cosineRegisterBlock->readButton()->click();
	tmp0RegisterBlock->readButton()->click();
	cnvCntRegisterBlock->readButton()->click();
	uniqID0RegisterBlock->readButton()->click();
	uniqID1RegisterBlock->readButton()->click();
	uniqID2RegisterBlock->readButton()->click();
	uniqID3RegisterBlock->readButton()->click();
	h1MagRegisterBlock->readButton()->click();
	h1PhRegisterBlock->readButton()->click();
	h2MagRegisterBlock->readButton()->click();
	h2PhRegisterBlock->readButton()->click();
	h3MagRegisterBlock->readButton()->click();
	h3PhRegisterBlock->readButton()->click();
	h8MagRegisterBlock->readButton()->click();
	h8PhRegisterBlock->readButton()->click();

	if(generalRegisterMap.at("Sequence Type") == 1){
		angleSecRegisterBlock->readButton()->click();
		secAnglIRegisterBlock->readButton()->click();
		secAnglQRegisterBlock->readButton()->click();
		tmp1RegisterBlock->readButton()->click();
		angleCkRegisterBlock->readButton()->click();
		radiusRegisterBlock->readButton()->click();
		diag1RegisterBlock->readButton()->click();
		diag2RegisterBlock->readButton()->click();
	}
}

void HarmonicCalibration::toggleFaultRegisterMode(int mode)
{
	switch(mode){
		case 0:
			AFEDIAGStatusLED->hide();
			OscillatorDriftStatusLED->hide();
			AngleCrossCheckStatusLED->hide();
			TurnCountSensorLevelsStatusLED->hide();
			MTDIAGStatusLED->hide();
			SequencerWatchdogStatusLED->hide();
			break;
		case 1: 
			AFEDIAGStatusLED->show();
			OscillatorDriftStatusLED->show();
			AngleCrossCheckStatusLED->show();
			TurnCountSensorLevelsStatusLED->show();
			MTDIAGStatusLED->show();
			SequencerWatchdogStatusLED->show();
			break;
	}
}

void HarmonicCalibration::toggleMTDiagnostics(int mode)
{
	switch(mode){
		case 0:
			MTDiagnosticsScrollArea->hide();
			hasMTDiagnostics = false;
			break;
		case 1:
			MTDiagnosticsScrollArea->show();
			hasMTDiagnostics = true;
			break;
	}
}

void HarmonicCalibration::toggleSequenceModeRegisters(int mode)
{
	switch(mode){
		case 0:
			angleSecRegisterBlock->hide();
			secAnglIRegisterBlock->hide();
			secAnglQRegisterBlock->hide();
			tmp1RegisterBlock->hide();
			angleCkRegisterBlock->hide();
			radiusRegisterBlock->hide();
			diag1RegisterBlock->hide();
			diag2RegisterBlock->hide();
			break;
		case 1:
			angleSecRegisterBlock->show();
			secAnglIRegisterBlock->show();
			secAnglQRegisterBlock->show();
			tmp1RegisterBlock->show();
			angleCkRegisterBlock->show();
			radiusRegisterBlock->show();
			diag1RegisterBlock->show();
			diag2RegisterBlock->show();
			break;
	}
}

void HarmonicCalibration::toggleMotorControls(bool value)
{
	motorMaxVelocitySpinBox->setEnabled(value);
	motorAccelTimeSpinBox->setEnabled(value);
	motorMaxDisplacementSpinBox->setEnabled(value);
	m_calibrationMotorRampModeMenuCombo->setEnabled(value);
	motorTargetPositionSpinBox->setEnabled(value);
}

void HarmonicCalibration::toggleUtilityTask(bool run)
{
	if(run){
		utilityTimer->start(utilityTimerRate);
	}
	else{
		utilityTimer->stop();
	}
}

void HarmonicCalibration::toggleDIGIOEN(string DIGIOENName, bool& value)
{
	toggleUtilityTask(false);

	uint32_t *DIGIOENRegisterValue = new uint32_t;
	uint32_t DIGIOENPage = m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::DIGIOEN);

	bool success = false;

	if(changeCNVPage(DIGIOENPage))
	{
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
			m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN), 
			DIGIOENRegisterValue) != -1)
		{
			map<string, bool> DIGIOSettings = m_admtController->getDIGIOENRegisterBitMapping(static_cast<uint16_t>(*DIGIOENRegisterValue));

			DIGIOSettings[DIGIOENName] = value;

			uint16_t newRegisterValue = m_admtController->setDIGIOENRegisterBitMapping(static_cast<uint16_t>(*DIGIOENRegisterValue), DIGIOSettings);

			if(changeCNVPage(DIGIOENPage)){
				if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
														m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN), 
														static_cast<uint32_t>(newRegisterValue)) != -1)
				{
					success = updateDIGIOToggle();
				}
			}

		}
	}

	if(!success) { StatusBarManager::pushMessage("Failed to toggle" + QString::fromStdString(DIGIOENName) + " " + QString(value ? "on" : "off")); }

	toggleUtilityTask(true);
}

bool HarmonicCalibration::updateDIGIOToggle()
{
	uint32_t *DIGIOENRegisterValue = new uint32_t;
	uint32_t DIGIOENPage = m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::DIGIOEN);
	bool success = false;

	if(changeCNVPage(DIGIOENPage)){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
			m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN), 
			DIGIOENRegisterValue) != -1)
		{
			map<string, bool> DIGIOSettings = m_admtController->getDIGIOENRegisterBitMapping(static_cast<uint16_t>(*DIGIOENRegisterValue));
			DIGIO0ENToggleSwitch->setChecked(DIGIOSettings["DIGIO0EN"]);
			DIGIO1ENToggleSwitch->setChecked(DIGIOSettings["DIGIO1EN"]);
			DIGIO2ENToggleSwitch->setChecked(DIGIOSettings["DIGIO2EN"]);
			DIGIO3ENToggleSwitch->setChecked(DIGIOSettings["DIGIO3EN"]);
			DIGIO4ENToggleSwitch->setChecked(DIGIOSettings["DIGIO4EN"]);
			DIGIO5ENToggleSwitch->setChecked(DIGIOSettings["DIGIO5EN"]);
			DIGIO0FNCToggleSwitch->setChecked(DIGIOSettings["BUSY"]);
			DIGIO1FNCToggleSwitch->setChecked(DIGIOSettings["CNV"]);
			DIGIO2FNCToggleSwitch->setChecked(DIGIOSettings["SENT"]);
			DIGIO3FNCToggleSwitch->setChecked(DIGIOSettings["ACALC"]);
			DIGIO4FNCToggleSwitch->setChecked(DIGIOSettings["FAULT"]);
			DIGIO5FNCToggleSwitch->setChecked(DIGIOSettings["BOOTLOAD"]);
			success = true;
		}
	}
	return success;
}

void HarmonicCalibration::toggleAllDIGIOEN(bool value)
{
	toggleUtilityTask(false);
	uint32_t *DIGIOENRegisterValue = new uint32_t;
	uint32_t DIGIOENPage = m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::DIGIOEN);

	bool success = false;

	if(changeCNVPage(DIGIOENPage))
	{
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
			m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN), 
			DIGIOENRegisterValue) != -1)
		{
			map<string, bool> DIGIOSettings = m_admtController->getDIGIOENRegisterBitMapping(static_cast<uint16_t>(*DIGIOENRegisterValue));;
			DIGIOSettings["DIGIO5EN"] = value;
			DIGIOSettings["DIGIO4EN"] = value;
			DIGIOSettings["DIGIO3EN"] = value;
			DIGIOSettings["DIGIO2EN"] = value;
			DIGIOSettings["DIGIO1EN"] = value;
			DIGIOSettings["DIGIO0EN"] = value;
			uint16_t newRegisterValue = m_admtController->setDIGIOENRegisterBitMapping(static_cast<uint16_t>(*DIGIOENRegisterValue), DIGIOSettings);

			if(changeCNVPage(DIGIOENPage)){
				if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
					m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN), 
					static_cast<uint32_t>(newRegisterValue)) != -1)
				{
					success = updateDIGIOToggle();
				}
			}
		}
	}

	if(!success) { StatusBarManager::pushMessage("Failed to toggle all GPIO outputs " + QString(value ? "on" : "off")); }

	toggleUtilityTask(true);
}

void HarmonicCalibration::readDeviceProperties()
{
	uint32_t *uniqId3RegisterValue = new uint32_t;
    uint32_t *cnvPageRegValue = new uint32_t;
    uint32_t page = m_admtController->getUniqueIdPage(ADMTController::UniqueIDRegister::UNIQID3);
    uint32_t cnvPageAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE);

	bool success = false;

    if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, page) != -1){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, cnvPageRegValue) != -1){
			if(*cnvPageRegValue == page){
                if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getUniqueIdRegister(ADMTController::UniqueIDRegister::UNIQID3), uniqId3RegisterValue) != -1){
                    deviceRegisterMap = m_admtController->getUNIQID3RegisterMapping(static_cast<uint16_t>(*uniqId3RegisterValue));

                    if(deviceRegisterMap.at("Supply ID") == "5V") { is5V = true; }
                    else if(deviceRegisterMap.at("Supply ID") == "3.3V") { is5V = false; }
                    else { is5V = false; }

                    deviceName = QString::fromStdString(deviceRegisterMap.at("Product ID"));
                    
                    if(deviceRegisterMap.at("ASIL ID") == "ASIL QM") { deviceType = QString::fromStdString("Industrial"); }
                    else if(deviceRegisterMap.at("ASIL ID") == "ASIL B") { deviceType = QString::fromStdString("Automotive"); }
                    else { deviceType = QString::fromStdString(deviceRegisterMap.at("ASIL ID")); }

					success = true;
                }
			}
		}
	}

	if(!success) { StatusBarManager::pushMessage("Failed to read device properties"); }
}

void HarmonicCalibration::changeCustomSwitchLabel(CustomSwitch *customSwitch, QString onLabel, QString offLabel)
{
	customSwitch->setOnText(onLabel);
	customSwitch->setOffText(offLabel);
}

void HarmonicCalibration::GMRReset()
{
	// Set Motor Angle to 315 degrees
	target_pos = 0;
	writeMotorAttributeValue(ADMTController::MotorAttribute::TARGET_POS, target_pos); 

	// Write 1 to ADMT IIO Attribute coil_rs
	m_admtController->setDeviceAttributeValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000),
											  m_admtController->getDeviceAttribute(ADMTController::DeviceAttribute::SDP_COIL_RS), 1);

	// Write 0xc000 to CNVPAGE
	if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE), 0xc000) != -1)
	{
		// Write 0x0000 to CNVPAGE
		if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE), 0x0000) != -1)
		{
			// Read ABSANGLE

			StatusBarManager::pushMessage("GMR Reset Done");
		}
		else { StatusBarManager::pushMessage("Failed to write CNVPAGE Register"); }
	}
	else { StatusBarManager::pushMessage("Failed to write CNVPAGE Register"); }
}

void HarmonicCalibration::restart()
{
	if(m_running) {
		run(false);
		run(true);
	}
}

bool HarmonicCalibration::running() const { return m_running; }

void HarmonicCalibration::setRunning(bool newRunning)
{
	if(m_running == newRunning)
		return;
	m_running = newRunning;
	Q_EMIT runningChanged(newRunning);
}

void HarmonicCalibration::start() { run(true); }

void HarmonicCalibration::stop() { run(false); }

void HarmonicCalibration::run(bool b)
{
	qInfo() << b;
	QElapsedTimer tim;
	tim.start();

	if(!b) {
		isStartAcquisition = false;
		acquisitionUITimer->stop();
		runButton->setChecked(false);
	}
	else{
		acquisitionUITimer->start(acquisitionUITimerRate);
		startAcquisition();
	}

	updateGeneralSettingEnabled(!b);
}

void HarmonicCalibration::canCalibrate(bool value)
{
	calibrateDataButton->setEnabled(value);
}

void HarmonicCalibration::acquisitionUITask()
{
	plotAcquisition(acquisitionAngleList, acquisitionAnglePlotChannel, acquisitionGraphPlotWidget);
	updateLineEditValues();
	updateLabelValue(acquisitionMotorCurrentPositionLabel, ADMTController::MotorAttribute::CURRENT_POS);
}

void HarmonicCalibration::applySequence(){
	toggleWidget(applySequenceButton, false);
	applySequenceButton->setText("Writing...");
	QTimer::singleShot(1000, this, [this](){
		this->toggleWidget(applySequenceButton, true);
		applySequenceButton->setText("Apply");
	});
	uint32_t *generalRegValue = new uint32_t;
	uint32_t generalRegisterAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::GENERAL);
	std::map<string, int> settings;

	settings["Convert Synchronization"] = qvariant_cast<int>(convertSynchronizationMenuCombo->combo()->currentData()); // convertSync;
	settings["Angle Filter"] = qvariant_cast<int>(angleFilterMenuCombo->combo()->currentData()); // angleFilter;
	settings["8th Harmonic"] = qvariant_cast<int>(eighthHarmonicMenuCombo->combo()->currentData()); // eighthHarmonic;
	settings["Sequence Type"] = qvariant_cast<int>(sequenceTypeMenuCombo->combo()->currentData()); // sequenceType;
	settings["Conversion Type"] = qvariant_cast<int>(conversionTypeMenuCombo->combo()->currentData()); // conversionType;

	bool success = false;

	if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), generalRegisterAddress, generalRegValue) != -1){

		uint32_t newGeneralRegValue = m_admtController->setGeneralRegisterBitMapping(*generalRegValue, settings);
		uint32_t generalRegisterPage = m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::GENERAL);

		if(changeCNVPage(generalRegisterPage)){
			if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), generalRegisterAddress, newGeneralRegValue) != -1){
				if(readSequence()){
					if(settings.at("Convert Synchronization") == generalRegisterMap.at("Convert Synchronization") &&
						settings.at("Angle Filter") == generalRegisterMap.at("Angle Filter") &&
						settings.at("8th Harmonic") == generalRegisterMap.at("8th Harmonic") &&
						settings.at("Sequence Type") == generalRegisterMap.at("Sequence Type") &&
						settings.at("Conversion Type") == generalRegisterMap.at("Conversion Type"))
					{
						StatusBarManager::pushMessage("Sequence settings applied successfully");
						success = true;
					}
				}
			}
		}
	}


	if(!success){ StatusBarManager::pushMessage("Failed to apply sequence settings"); }
}

void HarmonicCalibration::toggleWidget(QPushButton *widget, bool value){
	widget->setEnabled(value);
}

bool HarmonicCalibration::readSequence(){
	uint32_t *generalRegValue = new uint32_t;
	uint32_t generalRegisterAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::GENERAL);
	uint32_t generalRegisterPage = m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::GENERAL);

	bool success = false;

	if(changeCNVPage(generalRegisterPage)){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), generalRegisterAddress, generalRegValue) != -1){
			if(*generalRegValue != UINT32_MAX){
				generalRegisterMap = m_admtController->getGeneralRegisterBitMapping(static_cast<uint16_t>(*generalRegValue));

				updateSequenceWidget();
				success = true;
			}
		}
	}

	return success;
}

void HarmonicCalibration::updateSequenceWidget(){
	if(generalRegisterMap.at("Sequence Type") == -1){ sequenceTypeMenuCombo->combo()->setCurrentText("Reserved"); }
	else{ sequenceTypeMenuCombo->combo()->setCurrentIndex(sequenceTypeMenuCombo->combo()->findData(generalRegisterMap.at("Sequence Type"))); }
	conversionTypeMenuCombo->combo()->setCurrentIndex(conversionTypeMenuCombo->combo()->findData(generalRegisterMap.at("Conversion Type")));
	// cnvSourceMenuCombo->combo()->setCurrentValue(generalRegisterMap.at("Sequence Type"));
	if(generalRegisterMap.at("Convert Synchronization") == -1){ convertSynchronizationMenuCombo->combo()->setCurrentText("Reserved"); }
	else{ convertSynchronizationMenuCombo->combo()->setCurrentIndex(convertSynchronizationMenuCombo->combo()->findData(generalRegisterMap.at("Convert Synchronization"))); }
	angleFilterMenuCombo->combo()->setCurrentIndex(angleFilterMenuCombo->combo()->findData(generalRegisterMap.at("Angle Filter")));
	eighthHarmonicMenuCombo->combo()->setCurrentIndex(eighthHarmonicMenuCombo->combo()->findData(generalRegisterMap.at("8th Harmonic")));
}

bool HarmonicCalibration::changeCNVPage(uint32_t page){
	uint32_t *cnvPageRegValue = new uint32_t;
	uint32_t cnvPageAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE);

	if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, page) != -1){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, cnvPageRegValue) != -1){
			if(*cnvPageRegValue == page){
				return true; 
			}
		}
	}

	return false;
}

void HarmonicCalibration::utilityTask(){
	updateDigioMonitor();
	updateFaultRegister();
	if(hasMTDiagnostics){
		updateMTDiagRegister();
		updateMTDiagnostics();
	}
	commandLogWrite("");
}

void HarmonicCalibration::updateDigioMonitor(){
	uint32_t *digioRegValue = new uint32_t;
	uint32_t digioEnPage = m_admtController->getConfigurationPage(ADMTController::ConfigurationRegister::DIGIOEN);
	if(changeCNVPage(digioEnPage))
	{
		uint32_t digioRegisterAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::DIGIOEN);

		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), digioRegisterAddress, digioRegValue) != -1){
			std::map<std::string, bool> digioBitMapping =  m_admtController->getDIGIOENRegisterBitMapping(static_cast<uint16_t>(*digioRegValue));
			if(digioBitMapping.at("DIGIO0EN")){ 
				if(!digioBitMapping.at("BUSY")){ 
					changeStatusLEDColor(DIGIOBusyStatusLED, statusLEDColor); 
					DIGIOBusyStatusLED->setName("BUSY");
				}
				else{
					changeStatusLEDColor(DIGIOBusyStatusLED, gpioLEDColor); 
					DIGIOBusyStatusLED->setName("GPIO0");
				}
			}
			else { 
				changeStatusLEDColor(DIGIOBusyStatusLED, faultLEDColor, false); 
				DIGIOBusyStatusLED->setName("DIGIO0");
			}

			if(digioBitMapping.at("DIGIO1EN")){ 
				if(!digioBitMapping.at("CNV")){ 
					changeStatusLEDColor(DIGIOCNVStatusLED, statusLEDColor); 
					DIGIOCNVStatusLED->setName("CNV");
				}
				else{
					changeStatusLEDColor(DIGIOCNVStatusLED, gpioLEDColor); 
					DIGIOCNVStatusLED->setName("GPIO1");
				}
			}
			else { 
				changeStatusLEDColor(DIGIOCNVStatusLED, faultLEDColor, false); 
				DIGIOCNVStatusLED->setName("DIGIO1");
			}

			if(digioBitMapping.at("DIGIO2EN")){ 
				if(!digioBitMapping.at("SENT")){ 
					changeStatusLEDColor(DIGIOSENTStatusLED, statusLEDColor); 
					DIGIOSENTStatusLED->setName("SENT");
				}
				else{
					changeStatusLEDColor(DIGIOSENTStatusLED, gpioLEDColor); 
					DIGIOSENTStatusLED->setName("GPIO2");
				}
			}
			else { 
				changeStatusLEDColor(DIGIOSENTStatusLED, faultLEDColor, false); 
				DIGIOSENTStatusLED->setName("DIGIO2");
			}

			if(digioBitMapping.at("DIGIO3EN")){ 
				if(!digioBitMapping.at("ACALC")){ 
					changeStatusLEDColor(DIGIOACALCStatusLED, statusLEDColor); 
					DIGIOACALCStatusLED->setName("ACALC");
				}
				else{
					changeStatusLEDColor(DIGIOACALCStatusLED, gpioLEDColor); 
					DIGIOACALCStatusLED->setName("GPIO3");
				}
			}
			else { 
				changeStatusLEDColor(DIGIOACALCStatusLED, faultLEDColor, false); 
				DIGIOACALCStatusLED->setName("DIGIO3");
			}

			if(digioBitMapping.at("DIGIO4EN")){ 
				if(!digioBitMapping.at("FAULT")){ 
					changeStatusLEDColor(DIGIOFaultStatusLED, statusLEDColor); 
					DIGIOFaultStatusLED->setName("FAULT");
				}
				else{
					changeStatusLEDColor(DIGIOFaultStatusLED, gpioLEDColor); 
					DIGIOFaultStatusLED->setName("GPIO4");
				}
			}
			else { 
				changeStatusLEDColor(DIGIOFaultStatusLED, faultLEDColor, false); 
				DIGIOFaultStatusLED->setName("DIGIO4");
			}

			if(digioBitMapping.at("DIGIO5EN")){ 
				if(!digioBitMapping.at("BOOTLOAD")){ 
					changeStatusLEDColor(DIGIOBootloaderStatusLED, statusLEDColor); 
					DIGIOBootloaderStatusLED->setName("BOOTLOAD");
				}
				else{
					changeStatusLEDColor(DIGIOBootloaderStatusLED, gpioLEDColor); 
					DIGIOBootloaderStatusLED->setName("GPIO5");
				}
			}
			else { 
				changeStatusLEDColor(DIGIOBootloaderStatusLED, faultLEDColor, false); 
				DIGIOBootloaderStatusLED->setName("DIGIO5");
			}

			commandLogWrite("DIGIOEN: 0b" + QString::number(static_cast<uint16_t>(*digioRegValue), 2).rightJustified(16, '0'));
		}
		else{ commandLogWrite("Failed to read DIGIOEN Register"); }
	}

}

void HarmonicCalibration::updateMTDiagRegister(){
	uint32_t *mtDiag1RegValue = new uint32_t;
	uint32_t *cnvPageRegValue = new	uint32_t;
	uint32_t mtDiag1RegisterAddress = m_admtController->getSensorRegister(ADMTController::SensorRegister::DIAG1);
	uint32_t mtDiag1PageValue = m_admtController->getSensorPage(ADMTController::SensorRegister::DIAG1);
	uint32_t cnvPageAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE);

	if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, mtDiag1PageValue) != -1){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, cnvPageRegValue) != -1){
			if(*cnvPageRegValue == mtDiag1PageValue){
				if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), mtDiag1RegisterAddress, mtDiag1RegValue) != -1){
					std::map<std::string, bool> mtDiag1BitMapping =  m_admtController->getDiag1RegisterBitMapping_Register(static_cast<uint16_t>(*mtDiag1RegValue));
					changeStatusLEDColor(R0StatusLED, statusLEDColor, mtDiag1BitMapping.at("R0"));
					changeStatusLEDColor(R1StatusLED, statusLEDColor, mtDiag1BitMapping.at("R1"));
					changeStatusLEDColor(R2StatusLED, statusLEDColor, mtDiag1BitMapping.at("R2"));
					changeStatusLEDColor(R3StatusLED, statusLEDColor, mtDiag1BitMapping.at("R3"));
					changeStatusLEDColor(R4StatusLED, statusLEDColor, mtDiag1BitMapping.at("R4"));
					changeStatusLEDColor(R5StatusLED, statusLEDColor, mtDiag1BitMapping.at("R5"));
					changeStatusLEDColor(R6StatusLED, statusLEDColor, mtDiag1BitMapping.at("R6"));
					changeStatusLEDColor(R7StatusLED, statusLEDColor, mtDiag1BitMapping.at("R7"));
					commandLogWrite("DIAG1: 0b" + QString::number(static_cast<uint16_t>(*mtDiag1RegValue), 2).rightJustified(16, '0'));
				}
				else{ commandLogWrite("Failed to read MT Diagnostic 1 Register"); }
			}
			else{ commandLogWrite("CNVPAGE for MT Diagnostic 1 is a different value, abort reading"); }
		}
		else{ commandLogWrite("Failed to read CNVPAGE for MT Diagnostic 1"); }
	}
	else{ commandLogWrite("Failed to write CNVPAGE for MT Diagnostic 1"); }
}

void HarmonicCalibration::updateFaultRegister(){
	uint32_t *faultRegValue = new uint32_t;
	uint32_t faultRegisterAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::FAULT);
	m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), faultRegisterAddress, 0); // Write all zeros to fault before read
	m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), faultRegisterAddress, faultRegValue);

	if(*faultRegValue != -1){
		std::map<std::string, bool> faultBitMapping =  m_admtController->getFaultRegisterBitMapping(static_cast<uint16_t>(*faultRegValue));
		changeStatusLEDColor(VDDUnderVoltageStatusLED, faultLEDColor, faultBitMapping.at("VDD Under Voltage"));
		changeStatusLEDColor(VDDOverVoltageStatusLED, faultLEDColor, faultBitMapping.at("VDD Over Voltage"));
		changeStatusLEDColor(VDRIVEUnderVoltageStatusLED, faultLEDColor, faultBitMapping.at("VDRIVE Under Voltage"));
		changeStatusLEDColor(VDRIVEOverVoltageStatusLED, faultLEDColor, faultBitMapping.at("VDRIVE Over Voltage"));
		changeStatusLEDColor(AFEDIAGStatusLED, faultLEDColor, faultBitMapping.at("AFE Diagnostic"));
		changeStatusLEDColor(NVMCRCFaultStatusLED, faultLEDColor, faultBitMapping.at("NVM CRC Fault"));
		changeStatusLEDColor(ECCDoubleBitErrorStatusLED, faultLEDColor, faultBitMapping.at("ECC Double Bit Error"));
		changeStatusLEDColor(OscillatorDriftStatusLED, faultLEDColor, faultBitMapping.at("Oscillator Drift"));
		changeStatusLEDColor(CountSensorFalseStateStatusLED, faultLEDColor, faultBitMapping.at("Count Sensor False State"));
		changeStatusLEDColor(AngleCrossCheckStatusLED, faultLEDColor, faultBitMapping.at("Angle Cross Check"));
		changeStatusLEDColor(TurnCountSensorLevelsStatusLED, faultLEDColor, faultBitMapping.at("Turn Count Sensor Levels"));
		changeStatusLEDColor(MTDIAGStatusLED, faultLEDColor, faultBitMapping.at("MT Diagnostic"));
		changeStatusLEDColor(TurnCounterCrossCheckStatusLED, faultLEDColor, faultBitMapping.at("Turn Counter Cross Check"));
		changeStatusLEDColor(RadiusCheckStatusLED, faultLEDColor, faultBitMapping.at("AMR Radius Check"));
		changeStatusLEDColor(SequencerWatchdogStatusLED, faultLEDColor, faultBitMapping.at("Sequencer Watchdog"));

		commandLogWrite("FAULT: 0b" + QString::number(static_cast<uint16_t>(*faultRegValue), 2).rightJustified(16, '0'));
	}
	else{ commandLogWrite("Failed to read FAULT Register"); }
}

void HarmonicCalibration::updateMTDiagnostics(){
	uint32_t *mtDiag1RegValue = new uint32_t;
	uint32_t *mtDiag2RegValue = new uint32_t;
	uint32_t *cnvPageRegValue = new uint32_t;

	uint32_t mtDiag1RegisterAddress = m_admtController->getSensorRegister(ADMTController::SensorRegister::DIAG1);
	uint32_t mtDiag2RegisterAddress = m_admtController->getSensorRegister(ADMTController::SensorRegister::DIAG2);

	uint32_t mtDiag1PageValue = m_admtController->getSensorPage(ADMTController::SensorRegister::DIAG1);
	uint32_t mtDiag2PageValue = m_admtController->getSensorPage(ADMTController::SensorRegister::DIAG2);

	uint32_t cnvPageAddress = m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE);

	if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, mtDiag1PageValue) != -1){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, cnvPageRegValue) != -1){
			if(*cnvPageRegValue == mtDiag1PageValue){
				if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), mtDiag1RegisterAddress, mtDiag1RegValue) != -1){
					std::map<std::string, double> mtDiag1BitMapping =  m_admtController->getDiag1RegisterBitMapping_Afe(static_cast<uint16_t>(*mtDiag1RegValue), is5V);

					afeDiag2 = mtDiag1BitMapping.at("AFE Diagnostic 2");
					AFEDIAG2LineEdit->setText(QString::number(afeDiag2) + " V");
				}
				else{ commandLogWrite("Failed to read MT Diagnostic 1 Register"); }
			}
			else{ commandLogWrite("CNVPAGE for MT Diagnostic 1 is a different value, abort reading"); }
		}
		else{ commandLogWrite("Failed to read CNVPAGE for MT Diagnostic 1"); }
	}
	else{ commandLogWrite("Failed to write CNVPAGE for MT Diagnostic 1"); }

	if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, mtDiag2PageValue) != -1){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), cnvPageAddress, cnvPageRegValue) != -1){
			if(*cnvPageRegValue == mtDiag2PageValue){
				if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), mtDiag2RegisterAddress, mtDiag2RegValue) != -1){
					std::map<std::string, double> mtDiag2BitMapping =  m_admtController->getDiag2RegisterBitMapping(static_cast<uint16_t>(*mtDiag2RegValue));
					
					afeDiag0 = mtDiag2BitMapping.at("AFE Diagnostic 0 (-57%)");
					afeDiag1 = mtDiag2BitMapping.at("AFE Diagnostic 1 (+57%)");
					AFEDIAG0LineEdit->setText(QString::number(afeDiag0) + " V");
					AFEDIAG1LineEdit->setText(QString::number(afeDiag1) + " V");

					commandLogWrite("DIAG2: 0b" + QString::number(static_cast<uint16_t>(*mtDiag2RegValue), 2).rightJustified(16, '0'));
				}
				else{ commandLogWrite("Failed to read MT Diagnostic 2 Register"); }
			}
			else{ commandLogWrite("CNVPAGE for MT Diagnostic 2 is a different value, abort reading"); }
		}
		else{ commandLogWrite("Failed to read CNVPAGE for MT Diagnostic 2"); }
	}
	else{ commandLogWrite("Failed to write CNVPAGE for MT Diagnostic 2"); }
}

void HarmonicCalibration::clearCommandLog(){
	commandLogPlainTextEdit->clear();
}

void HarmonicCalibration::updateChannelValues(){
	rotation = m_admtController->getChannelValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), rotationChannelName, bufferSize);
	angle = m_admtController->getChannelValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), angleChannelName, bufferSize);
	updateCountValue();
	temp = m_admtController->getChannelValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), temperatureChannelName, bufferSize);
}

void HarmonicCalibration::updateCountValue(){
	uint32_t *absAngleRegValue = new uint32_t;
	bool success = false;
	if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getConfigurationRegister(ADMTController::ConfigurationRegister::CNVPAGE), 0x0000) != -1){
		if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getSensorRegister(ADMTController::SensorRegister::ABSANGLE), absAngleRegValue) != -1){
			count = m_admtController->getAbsAngleTurnCount(static_cast<uint16_t>(*absAngleRegValue));
			success = true;
		}
	}
	if(!success){ count = static_cast<double>(UINT64_MAX); }
}

void HarmonicCalibration::updateLineEditValues(){
	if(rotation == static_cast<double>(UINT64_MAX)) { rotationValueLabel->setText("N/A"); }
	else { rotationValueLabel->setText(QString::number(rotation) + "°"); }
	if(angle == static_cast<double>(UINT64_MAX)) { angleValueLabel->setText("N/A"); }
	else { angleValueLabel->setText(QString::number(angle) + "°"); }
	if(count == static_cast<double>(UINT64_MAX)) { countValueLabel->setText("N/A"); }
	else { countValueLabel->setText(QString::number(count)); }
	if(temp == static_cast<double>(UINT64_MAX)) { tempValueLabel->setText("N/A"); }
	else { tempValueLabel->setText(QString::number(temp) + " °C"); }
}

void HarmonicCalibration::updateGeneralSettingEnabled(bool value)
{
	graphUpdateIntervalLineEdit->setEnabled(value);
	displayLengthLineEdit->setEnabled(value);
	// dataGraphSamplesLineEdit->setEnabled(value);
	// tempGraphSamplesLineEdit->setEnabled(value);
}

MenuControlButton *HarmonicCalibration::createStatusLEDWidget(const QString title, QColor color, QWidget *parent)
{
	MenuControlButton *menuControlButton = new MenuControlButton(parent);
	menuControlButton->setName(title);
	menuControlButton->setCheckBoxStyle(MenuControlButton::CheckboxStyle::CS_CIRCLE);
	menuControlButton->setOpenMenuChecksThis(true);
	menuControlButton->setDoubleClickToOpenMenu(true);
	menuControlButton->setColor(color);
	menuControlButton->button()->setVisible(false);
	menuControlButton->setCheckable(true);
	menuControlButton->checkBox()->setChecked(false);
	menuControlButton->setEnabled(false);
	menuControlButton->layout()->setMargin(8);
	return menuControlButton;
}

void HarmonicCalibration::changeStatusLEDColor(MenuControlButton *menuControlButton, QColor color, bool checked)
{
	menuControlButton->setColor(color);
	menuControlButton->checkBox()->setChecked(checked);
}

MenuControlButton *HarmonicCalibration::createChannelToggleWidget(const QString title, QColor color, QWidget *parent)
{
	MenuControlButton *menuControlButton = new MenuControlButton(parent);
	menuControlButton->setName(title);
	menuControlButton->setCheckBoxStyle(MenuControlButton::CheckboxStyle::CS_CIRCLE);
	menuControlButton->setOpenMenuChecksThis(true);
	menuControlButton->setDoubleClickToOpenMenu(true);
	menuControlButton->setColor(color);
	menuControlButton->button()->setVisible(false);
	menuControlButton->setCheckable(false);
	menuControlButton->checkBox()->setChecked(true);
	menuControlButton->layout()->setMargin(0);
	return menuControlButton;
}

void HarmonicCalibration::connectLineEditToNumber(QLineEdit* lineEdit, int& variable, int min, int max)
{
    connect(lineEdit, &QLineEdit::editingFinished, this, [&variable, lineEdit, min, max]() {
        bool ok;
        int value = lineEdit->text().toInt(&ok);
        if (ok && value >= min && value <= max) {
            variable = value;
        } else {
            lineEdit->setText(QString::number(variable));
        }
    });
}

void HarmonicCalibration::connectLineEditToNumber(QLineEdit* lineEdit, double& variable, QString unit)
{
    connect(lineEdit, &QLineEdit::editingFinished, this, [&variable, lineEdit, unit]() {
        bool ok;
        double value = lineEdit->text().replace(unit, "").trimmed().toDouble(&ok);
        if (ok) {
            variable = value;
			
        } else {
            lineEdit->setText(QString::number(variable) + " " + unit);
        }
    });
}

void HarmonicCalibration::connectLineEditToNumberWrite(QLineEdit* lineEdit, double& variable, ADMTController::MotorAttribute attribute)
{
    connect(lineEdit, &QLineEdit::editingFinished, [=, &variable]() {
        bool ok;
        double value = lineEdit->text().toDouble(&ok);
        if (ok) {
            variable = value;
			writeMotorAttributeValue(attribute, variable);
			
        } else {
            lineEdit->setText(QString::number(variable));
        }
    });
}

// void HarmonicCalibration::connectLineEditToGraphSamples(QLineEdit* lineEdit, int& variable, Sismograph* graph, int min, int max)
// {
//     connect(lineEdit, &QLineEdit::editingFinished, this, [&variable, lineEdit, graph, min, max]() {
//         bool ok;
//         int value = lineEdit->text().toInt(&ok);
//         if (ok && value >= min && value <= max) {
//             variable = value;
// 			graph->setNumSamples(variable);
//         } else {
//             lineEdit->setText(QString::number(variable));
//         }
//     });
// }

// void HarmonicCalibration::connectMenuComboToGraphDirection(MenuCombo* menuCombo, Sismograph* graph)
// {
// 	QComboBox *combo = menuCombo->combo();
// 	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [combo, graph]() {
// 		int value = qvariant_cast<int>(combo->currentData());
// 		switch(value)
// 		{
// 			case Sismograph::LEFT_TO_RIGHT:
// 				graph->setPlotDirection(Sismograph::LEFT_TO_RIGHT);
// 				graph->reset();
// 				break;
// 			case Sismograph::RIGHT_TO_LEFT:
// 				graph->setPlotDirection(Sismograph::RIGHT_TO_LEFT);
// 				graph->reset();
// 				break;
// 		}
// 	});
// }

void HarmonicCalibration::connectMenuComboToNumber(MenuCombo* menuCombo, double& variable)
{
	QComboBox *combo = menuCombo->combo();
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=, &variable]() {
		variable = qvariant_cast<int>(combo->currentData());
	});
}

void HarmonicCalibration::connectMenuComboToNumber(MenuCombo* menuCombo, int& variable)
{
	QComboBox *combo = menuCombo->combo();
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=, &variable]() {
		variable = qvariant_cast<int>(combo->currentData());
	});
}

// void HarmonicCalibration::changeGraphColorByChannelName(Sismograph* graph, const char* channelName)
// {
// 	int index = m_admtController->getChannelIndex(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), channelName);
// 	if(index > -1){
// 		graph->setColor(StyleHelper::getColor( QString::fromStdString("CH" + std::to_string(index) )));
// 	}
// }

// void HarmonicCalibration::connectMenuComboToGraphChannel(MenuCombo* menuCombo, Sismograph* graph)
// {
// 	QComboBox *combo = menuCombo->combo();
// 	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, combo, graph]() {
// 		int currentIndex = combo->currentIndex();
// 		QVariant currentData = combo->currentData();
// 		char *value = reinterpret_cast<char*>(currentData.value<void*>());
// 		switch(currentIndex)
// 		{
// 			case ADMTController::Channel::ROTATION:
// 				dataGraphValue = &rotation;
// 				graph->setUnitOfMeasure("Degree", "°");
// 				graph->setAxisScale(QwtAxis::YLeft, -30.0, 390.0);
// 				graph->setNumSamples(dataGraphSamples);
// 				graph->setAxisTitle(QwtAxis::YLeft, tr("Degree (°)"));
// 				break;
// 			case ADMTController::Channel::ANGLE:
// 				dataGraphValue = &angle;
// 				graph->setUnitOfMeasure("Degree", "°");
// 				graph->setAxisScale(QwtAxis::YLeft, -30.0, 390.0);
// 				graph->setNumSamples(dataGraphSamples);
// 				graph->setAxisTitle(QwtAxis::YLeft, tr("Degree (°)"));
// 				break;
// 			case ADMTController::Channel::COUNT:
// 				dataGraphValue = &count;
// 				graph->setUnitOfMeasure("Count", "");
// 				graph->setAxisScale(QwtAxis::YLeft, -1.0, 20.0);
// 				graph->setNumSamples(dataGraphSamples);
// 				graph->setAxisTitle(QwtAxis::YLeft, tr("Count"));
// 				break;
// 		}
// 		changeGraphColorByChannelName(graph, value);
// 		graph->reset();
// 	});
// }

void HarmonicCalibration::connectLineEditToRPSConversion(QLineEdit* lineEdit, double& vmax)
{
	connect(lineEdit, &QLineEdit::editingFinished, [=, &vmax]() {
        bool ok;
        double rps = lineEdit->text().toDouble(&ok);
        if (ok) {
            vmax = convertRPStoVMAX(rps);
			StatusBarManager::pushMessage("Converted VMAX: " + QString::number(vmax));
			writeMotorAttributeValue(ADMTController::MotorAttribute::ROTATE_VMAX, vmax);
			writeMotorAttributeValue(ADMTController::MotorAttribute::DISABLE, 1);
			amax = convertAccelTimetoAMAX(motorAccelTimeSpinBox->lineEdit()->text().toDouble());
			StatusBarManager::pushMessage("Converted AMAX: " + QString::number(amax));
			writeMotorAttributeValue(ADMTController::MotorAttribute::AMAX, amax);
        } else {
            lineEdit->setText(QString::number(convertVMAXtoRPS(vmax)));
        }
    });
}

void HarmonicCalibration::connectLineEditToAMAXConversion(QLineEdit* lineEdit, double& amax)
{
	connect(lineEdit, &QLineEdit::editingFinished, [=, &amax]() {
        bool ok;
        double accelTime = lineEdit->text().toDouble(&ok);
        if (ok) {
            amax = convertAccelTimetoAMAX(accelTime);
			StatusBarManager::pushMessage("Converted AMAX: " + QString::number(amax));
        } else {
            lineEdit->setText(QString::number(convertAMAXtoAccelTime(amax)));
        }
    });
}

void HarmonicCalibration::connectRegisterBlockToRegistry(RegisterBlockWidget* widget)
{
	uint32_t *readValue = new uint32_t;
	connect(widget->readButton(), &QPushButton::clicked, this, [=]{
		bool ok = false, success = false;

		if(widget->getCnvPage() != UINT32_MAX)
		{
			ok = this->changeCNVPage(widget->getCnvPage());
		}
		else { ok = true; }

		if(ok){
			if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), widget->getAddress(), readValue) == 0)
			{ widget->setValue(*readValue); }
		}
		else{ StatusBarManager::pushMessage("Failed to read registry"); }
	});
	if(widget->getAccessPermission() == RegisterBlockWidget::ACCESS_PERMISSION::READWRITE || 
	   widget->getAccessPermission() == RegisterBlockWidget::ACCESS_PERMISSION::WRITE){
		connect(widget->writeButton(), &QPushButton::clicked, this, [=]{
			bool ok = false, success = false;

			if(widget->getCnvPage() != UINT32_MAX)
			{
				ok = this->changeCNVPage(widget->getCnvPage());
			}
			else { ok = true; }

			if(ok){
				if(m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), widget->getAddress(), widget->getValue()) == 0)
				{ 
					if(m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), widget->getAddress(), readValue) == 0)
					{ 
						widget->setValue(*readValue); 
						success = true;
					}
				}
			}
			
			if(!success) { StatusBarManager::pushMessage("Failed to write to registry"); }
		});
	}
}

double HarmonicCalibration::convertRPStoVMAX(double rps) 
{ 
	return (rps * motorMicrostepPerRevolution * motorTimeUnit); 
}

double HarmonicCalibration::convertVMAXtoRPS(double vmax)
{
	return (vmax / motorMicrostepPerRevolution / motorTimeUnit);
}

double HarmonicCalibration::convertAccelTimetoAMAX(double accelTime)
{
	return (rotate_vmax * 131072 / accelTime / motorfCLK);
}

double HarmonicCalibration::convertAMAXtoAccelTime(double amax)
{
	return ((rotate_vmax * 131072) / (amax * motorfCLK));
}

void HarmonicCalibration::updateLabelValue(QLabel* label, int channelIndex)
{
	switch(channelIndex)
	{
		case ADMTController::Channel::ROTATION:
			label->setText(QString("%1").arg(rotation, 0, 'f', 2) + "°");
			break;
		case ADMTController::Channel::ANGLE:
			label->setText(QString("%1").arg(angle, 0, 'f', 2) + "°");
			break;
		case ADMTController::Channel::COUNT:
			label->setText(QString::number(count));
			break;
		case ADMTController::Channel::TEMPERATURE:
			label->setText(QString("%1").arg(temp, 0, 'f', 2) + "°C");
			break;
	}
}

void HarmonicCalibration::updateChannelValue(int channelIndex)
{
	switch(channelIndex)
	{
		case ADMTController::Channel::ROTATION:
			rotation = m_admtController->getChannelValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), rotationChannelName, 1);
			break;
		case ADMTController::Channel::ANGLE:
			angle = m_admtController->getChannelValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), angleChannelName, 1);
			break;
		case ADMTController::Channel::COUNT:
			count = m_admtController->getChannelValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), countChannelName, 1);
			break;
		case ADMTController::Channel::TEMPERATURE:
			temp = m_admtController->getChannelValue(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), temperatureChannelName, 1);
			break;
	}
}

int HarmonicCalibration::readMotorAttributeValue(ADMTController::MotorAttribute attribute, double& value)
{
	int result = -1;
	if(!isDebug){
		result = m_admtController->getDeviceAttributeValue(m_admtController->getDeviceId(ADMTController::Device::TMC5240),
															m_admtController->getMotorAttribute(attribute),
															&value);
	}
	return result;
}

int HarmonicCalibration::writeMotorAttributeValue(ADMTController::MotorAttribute attribute, double value)
{
	int result = -1;
	if(!isDebug){
		result = m_admtController->setDeviceAttributeValue(m_admtController->getDeviceId(ADMTController::Device::TMC5240), 
															m_admtController->getMotorAttribute(attribute), 
															value);
	}
	return result;
}

void HarmonicCalibration::updateLabelValue(QLabel *label, ADMTController::MotorAttribute attribute)
{
	switch(attribute)
	{
		case ADMTController::MotorAttribute::AMAX:
			label->setText(QString::number(amax));
			break;
		case ADMTController::MotorAttribute::ROTATE_VMAX:
			label->setText(QString::number(rotate_vmax));
			break;
		case ADMTController::MotorAttribute::DMAX:
			label->setText(QString::number(dmax));
			break;
		case ADMTController::MotorAttribute::DISABLE:
			label->setText(QString::number(disable));
			break;
		case ADMTController::MotorAttribute::TARGET_POS:
			label->setText(QString::number(target_pos));
			break;
		case ADMTController::MotorAttribute::CURRENT_POS:
			label->setText(QString::number(current_pos));
			break;
		case ADMTController::MotorAttribute::RAMP_MODE:
			label->setText(QString::number(ramp_mode));
			break;

	}
}

void HarmonicCalibration::calibrationUITask()
{
	if(!isDebug){
		readMotorAttributeValue(ADMTController::MotorAttribute::CURRENT_POS, current_pos);
		updateLabelValue(calibrationMotorCurrentPositionLabel, ADMTController::MotorAttribute::CURRENT_POS);

		if(isStartMotor)
		{
			if(isPostCalibration){
				postCalibrationRawDataPlotChannel->curve()->setSamples(graphPostDataList);
				postCalibrationRawDataPlotChannel->xAxis()->setMax(graphPostDataList.size());
				postCalibrationRawDataPlotWidget->replot();
			}
			else{
				calibrationRawDataPlotChannel->curve()->setSamples(graphDataList);
				calibrationRawDataPlotChannel->xAxis()->setMax(graphDataList.size());
				calibrationRawDataPlotWidget->replot();
			}
		}
	}
}

void HarmonicCalibration::getCalibrationSamples()
{
	if(resetToZero){
		resetCurrentPositionToZero();
	}
	if(isPostCalibration){
		while(isStartMotor && graphPostDataList.size() < totalSamplesCount){
			stepMotorAcquisition();
			updateChannelValue(ADMTController::Channel::ANGLE);
			graphPostDataList.append(angle);
		}
	}
	else{
		while(isStartMotor && graphDataList.size() < totalSamplesCount){
			stepMotorAcquisition();
			updateChannelValue(ADMTController::Channel::ANGLE);
			graphDataList.append(angle);
		}
	}
}

void HarmonicCalibration::resetCurrentPositionToZero()
{
	writeMotorAttributeValue(ADMTController::MotorAttribute::TARGET_POS, 0);
	readMotorAttributeValue(ADMTController::MotorAttribute::CURRENT_POS, current_pos);
	while(current_pos != 0){
		readMotorAttributeValue(ADMTController::MotorAttribute::CURRENT_POS, current_pos);
	}
	resetToZero = false;
}

void HarmonicCalibration::startMotor()
{
	toggleMotorControls(false);

	if(resetToZero && !isPostCalibration){
		clearCalibrationSamples();
	}
	QFuture<void> future = QtConcurrent::run(this, &HarmonicCalibration::getCalibrationSamples);
	QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);

	connect(watcher, &QFutureWatcher<void>::finished, this, [=]() {
		toggleMotorControls(true);

		calibrationRawDataPlotChannel->curve()->setSamples(graphDataList);
		calibrationRawDataPlotChannel->xAxis()->setMax(graphDataList.size());
		calibrationRawDataPlotWidget->replot();
		isStartMotor = false;
		calibrationStartMotorButton->setChecked(false);
		
		if(isPostCalibration)
		{
			if(samplesCollected == totalSamplesCount) 
			{
				computeSineCosineOfAngles(graphPostDataList);
				m_admtController->postcalibrate(vector<double>(graphPostDataList.begin(), graphPostDataList.end()), cycleCount, samplesPerCycle);
				isPostCalibration = false;
				isStartMotor = false;
				canStartMotor(true);
				QVector correctedError(m_admtController->correctedError.begin(), m_admtController->correctedError.end());
				correctedErrorPlotChannel->curve()->setSamples(correctedError);
				auto correctedErrorMagnitudeMinMax = std::minmax_element(correctedError.begin(), correctedError.end());
				correctedErrorYPlotAxis->setInterval(*correctedErrorMagnitudeMinMax.first, *correctedErrorMagnitudeMinMax.second);
				correctedErrorXPlotAxis->setMax(correctedError.size());
				correctedErrorPlotWidget->replot();

				QVector FFTCorrectedErrorMagnitude(m_admtController->FFTCorrectedErrorMagnitude.begin(), m_admtController->FFTCorrectedErrorMagnitude.end());
				QVector FFTCorrectedErrorPhase(m_admtController->FFTCorrectedErrorPhase.begin(), m_admtController->FFTCorrectedErrorPhase.end());
				FFTCorrectedErrorMagnitudeChannel->curve()->setSamples(FFTCorrectedErrorMagnitude);
				FFTCorrectedErrorPhaseChannel->curve()->setSamples(FFTCorrectedErrorPhase);
				auto FFTCorrectedErrorMagnitudeMinMax = std::minmax_element(FFTCorrectedErrorMagnitude.begin(), FFTCorrectedErrorMagnitude.end());
				auto FFTCorrectedErrorPhaseMinMax = std::minmax_element(FFTCorrectedErrorPhase.begin(), FFTCorrectedErrorPhase.end());
				double FFTCorrectedErrorPlotMin = *FFTCorrectedErrorMagnitudeMinMax.first < *FFTCorrectedErrorPhaseMinMax.first ? *FFTCorrectedErrorMagnitudeMinMax.first : *FFTCorrectedErrorPhaseMinMax.first;
				double FFTCorrectedErrorPlotMax = *FFTCorrectedErrorMagnitudeMinMax.second > *FFTCorrectedErrorPhaseMinMax.second ? *FFTCorrectedErrorMagnitudeMinMax.second : *FFTCorrectedErrorPhaseMinMax.second;
				FFTCorrectedErrorYPlotAxis->setInterval(FFTCorrectedErrorPlotMin, FFTCorrectedErrorPlotMax);
				FFTCorrectedErrorXPlotAxis->setMax(FFTCorrectedErrorMagnitude.size());
				FFTCorrectedErrorPlotWidget->replot();
			}
		}
		else{
			if(samplesCollected == totalSamplesCount) 
			{
				computeSineCosineOfAngles(graphDataList);
				canCalibrate(true); 

				calibrationLogWrite(m_admtController->calibrate(vector<double>(graphDataList.begin(), graphDataList.end()), cycleCount, samplesPerCycle));

				flashHarmonicValues();

				populateAngleErrorGraphs();
			}
			else{
				resetToZero = true;
			}
		}
	});
	connect(watcher, SIGNAL(finished()), watcher, SLOT(deleteLater()));
	watcher->setFuture(future);
}

void HarmonicCalibration::populateAngleErrorGraphs()
{
	QVector<double> angleError = QVector<double>(m_admtController->angleError.begin(), m_admtController->angleError.end());
	QVector<double> FFTAngleErrorMagnitude = QVector<double>(m_admtController->FFTAngleErrorMagnitude.begin(), m_admtController->FFTAngleErrorMagnitude.end());
	QVector<double> FFTAngleErrorPhase = QVector<double>(m_admtController->FFTAngleErrorPhase.begin(), m_admtController->FFTAngleErrorPhase.end());

	angleErrorPlotChannel->curve()->setSamples(angleError);
	auto angleErrorMinMax = std::minmax_element(angleError.begin(), angleError.end());
	angleErrorYPlotAxis->setInterval(*angleErrorMinMax.first, *angleErrorMinMax.second);
	angleErrorXPlotAxis->setInterval(0, angleError.size());
	angleErrorPlotWidget->replot();
	
	FFTAngleErrorMagnitudeChannel->curve()->setSamples(FFTAngleErrorMagnitude);
	auto angleErrorMagnitudeMinMax = std::minmax_element(FFTAngleErrorMagnitude.begin(), FFTAngleErrorMagnitude.end());
	FFTAngleErrorPhaseChannel->curve()->setSamples(FFTAngleErrorPhase);
	auto angleErrorPhaseMinMax = std::minmax_element(FFTAngleErrorPhase.begin(), FFTAngleErrorPhase.end());
	double FFTAngleErrorPlotMin = *angleErrorMagnitudeMinMax.first < *angleErrorPhaseMinMax.first ? *angleErrorMagnitudeMinMax.first : *angleErrorPhaseMinMax.first;
	double FFTAngleErrorPlotMax = *angleErrorMagnitudeMinMax.second > *angleErrorPhaseMinMax.second ? *angleErrorMagnitudeMinMax.second : *angleErrorPhaseMinMax.second;
	FFTAngleErrorYPlotAxis->setInterval(FFTAngleErrorPlotMin, FFTAngleErrorPlotMax);
	FFTAngleErrorXPlotAxis->setInterval(0, FFTAngleErrorMagnitude.size());
	FFTAngleErrorPlotWidget->replot();

	resultDataTabWidget->setCurrentIndex(0); // Set tab to Angle Error
}

void HarmonicCalibration::canStartMotor(bool value)
{
	calibrationStartMotorButton->setEnabled(value);
}

void HarmonicCalibration::calibrateData()
{
	calibrationLogWrite("==== Calibration Start ====");
	
	calibrationLogWrite(m_admtController->calibrate(vector<double>(graphDataList.begin(), graphDataList.end()), cycleCount, samplesPerCycle));

	flashHarmonicValues();

	QVector<double> angleError = QVector<double>(m_admtController->angleError.begin(), m_admtController->angleError.end());
	QVector<double> FFTAngleErrorMagnitude = QVector<double>(m_admtController->FFTAngleErrorMagnitude.begin(), m_admtController->FFTAngleErrorMagnitude.end());
	QVector<double> FFTAngleErrorPhase = QVector<double>(m_admtController->FFTAngleErrorPhase.begin(), m_admtController->FFTAngleErrorPhase.end());

	angleErrorPlotChannel->curve()->setSamples(angleError);
	auto angleErrorMinMax = std::minmax_element(angleError.begin(), angleError.end());
	angleErrorYPlotAxis->setInterval(*angleErrorMinMax.first, *angleErrorMinMax.second);
	angleErrorXPlotAxis->setInterval(0, angleError.size());
	angleErrorPlotWidget->replot();
	
	FFTAngleErrorMagnitudeChannel->curve()->setSamples(FFTAngleErrorMagnitude);
	auto angleErrorMagnitudeMinMax = std::minmax_element(FFTAngleErrorMagnitude.begin(), FFTAngleErrorMagnitude.end());
	FFTAngleErrorPhaseChannel->curve()->setSamples(FFTAngleErrorPhase);
	auto angleErrorPhaseMinMax = std::minmax_element(FFTAngleErrorPhase.begin(), FFTAngleErrorPhase.end());
	double FFTAngleErrorPlotMin = *angleErrorMagnitudeMinMax.first < *angleErrorPhaseMinMax.first ? *angleErrorMagnitudeMinMax.first : *angleErrorPhaseMinMax.first;
	double FFTAngleErrorPlotMax = *angleErrorMagnitudeMinMax.second > *angleErrorPhaseMinMax.second ? *angleErrorMagnitudeMinMax.second : *angleErrorPhaseMinMax.second;
	FFTAngleErrorYPlotAxis->setInterval(FFTAngleErrorPlotMin, FFTAngleErrorPlotMax);
	FFTAngleErrorXPlotAxis->setInterval(0, FFTAngleErrorMagnitude.size());
	FFTAngleErrorPlotWidget->replot();
}	

void HarmonicCalibration::flashHarmonicValues()
{
	uint32_t *h1MagCurrent = new uint32_t, 
			 *h1PhaseCurrent = new uint32_t, 
			 *h2MagCurrent = new uint32_t,
			 *h2PhaseCurrent = new uint32_t,
			 *h3MagCurrent = new uint32_t,
			 *h3PhaseCurrent = new uint32_t,
			 *h8MagCurrent = new uint32_t,
			 *h8PhaseCurrent = new uint32_t;

	if(changeCNVPage(0x02)){
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1MAG), h1MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1PH), h1PhaseCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2MAG), h2MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2PH), h2PhaseCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3MAG), h3MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3PH), h3PhaseCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8MAG), h8MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8PH), h8PhaseCurrent);

		calibrationLogWrite();
		calibrationLogWrite("H1 Mag Current: " + QString::number(*h1MagCurrent));
		calibrationLogWrite("H1 Phase Current: " + QString::number(*h1PhaseCurrent));
		calibrationLogWrite("H2 Mag Current: " + QString::number(*h2MagCurrent));
		calibrationLogWrite("H2 Phase Current: " + QString::number(*h2PhaseCurrent));
		calibrationLogWrite("H3 Mag Current: " + QString::number(*h3MagCurrent));
		calibrationLogWrite("H3 Phase Current: " + QString::number(*h3PhaseCurrent));
		calibrationLogWrite("H8 Mag Current: " + QString::number(*h8MagCurrent));
		calibrationLogWrite("H8 Phase Current: " + QString::number(*h8PhaseCurrent));

		H1_MAG_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientMagnitude(static_cast<uint16_t>(m_admtController->HAR_MAG_1), static_cast<uint16_t>(*h1MagCurrent), "h1"));
		H1_PHASE_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientPhase(static_cast<uint16_t>(m_admtController->HAR_PHASE_1), static_cast<uint16_t>(*h1PhaseCurrent)));
		H2_MAG_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientMagnitude(static_cast<uint16_t>(m_admtController->HAR_MAG_2), static_cast<uint16_t>(*h2MagCurrent), "h2"));
		H2_PHASE_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientPhase(static_cast<uint16_t>(m_admtController->HAR_PHASE_2), static_cast<uint16_t>(*h2PhaseCurrent)));
		H3_MAG_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientMagnitude(static_cast<uint16_t>(m_admtController->HAR_MAG_3), static_cast<uint16_t>(*h3MagCurrent), "h3"));
		H3_PHASE_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientPhase(static_cast<uint16_t>(m_admtController->HAR_PHASE_3), static_cast<uint16_t>(*h3PhaseCurrent)));
		H8_MAG_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientMagnitude(static_cast<uint16_t>(m_admtController->HAR_MAG_8), static_cast<uint16_t>(*h8MagCurrent), "h8"));
		H8_PHASE_HEX = static_cast<uint32_t>(m_admtController->calculateHarmonicCoefficientPhase(static_cast<uint16_t>(m_admtController->HAR_PHASE_8), static_cast<uint16_t>(*h8PhaseCurrent)));

		calibrationLogWrite();
		calibrationLogWrite("H1 Mag Scaled: " + QString::number(H1_MAG_HEX));
		calibrationLogWrite("H1 Phase Scaled: " + QString::number(H1_PHASE_HEX));
		calibrationLogWrite("H2 Mag Scaled: " + QString::number(H2_MAG_HEX));
		calibrationLogWrite("H2 Phase Scaled: " + QString::number(H2_PHASE_HEX));
		calibrationLogWrite("H3 Mag Scaled: " + QString::number(H3_MAG_HEX));
		calibrationLogWrite("H3 Phase Scaled: " + QString::number(H3_PHASE_HEX));
		calibrationLogWrite("H8 Mag Scaled: " + QString::number(H8_MAG_HEX));
		calibrationLogWrite("H8 Phase Scaled: " + QString::number(H8_PHASE_HEX));

		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 0x01, 0x02);

		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1MAG),
											H1_MAG_HEX);
		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1PH),
											H1_PHASE_HEX);
		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2MAG),
											H2_MAG_HEX);
		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2PH),
											H2_PHASE_HEX);
		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3MAG),
											H3_MAG_HEX);
		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3PH),
											H3_PHASE_HEX);
		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8MAG),
											H8_MAG_HEX);
		m_admtController->writeDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), 
											m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8PH),
											H8_PHASE_HEX);

		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1MAG), h1MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H1PH), h1PhaseCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2MAG), h2MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H2PH), h2PhaseCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3MAG), h3MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H3PH), h3PhaseCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8MAG), h8MagCurrent);
		m_admtController->readDeviceRegistry(m_admtController->getDeviceId(ADMTController::Device::ADMT4000), m_admtController->getHarmonicRegister(ADMTController::HarmonicRegister::H8PH), h8PhaseCurrent);

		calibrationLogWrite();
		calibrationLogWrite("H1 Mag After Flash: " + QString::number(*h1MagCurrent));
		calibrationLogWrite("H1 Phase After Flash: " + QString::number(*h1PhaseCurrent));
		calibrationLogWrite("H2 Mag After Flash: " + QString::number(*h2MagCurrent));
		calibrationLogWrite("H2 Phase After Flash: " + QString::number(*h2PhaseCurrent));
		calibrationLogWrite("H3 Mag After Flash: " + QString::number(*h3MagCurrent));
		calibrationLogWrite("H3 Phase After Flash: " + QString::number(*h3PhaseCurrent));
		calibrationLogWrite("H8 Mag After Flash: " + QString::number(*h8MagCurrent));
		calibrationLogWrite("H8 Phase After Flash: " + QString::number(*h8PhaseCurrent));

		H1_MAG_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h1MagCurrent), "h1mag");
		H1_PHASE_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h1PhaseCurrent), "h1phase");
		H2_MAG_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h2MagCurrent), "h2mag");
		H2_PHASE_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h2PhaseCurrent), "h2phase");
		H3_MAG_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h3MagCurrent), "h3mag");
		H3_PHASE_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h3PhaseCurrent), "h3phase");
		H8_MAG_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h8MagCurrent), "h8mag");
		H8_PHASE_ANGLE = m_admtController->getActualHarmonicRegisterValue(static_cast<uint16_t>(*h8PhaseCurrent), "h8phase");

		calibrationLogWrite();
		calibrationLogWrite("H1 Mag Converted: " + QString::number(H1_MAG_ANGLE));
		calibrationLogWrite("H1 Phase Converted: " + QString::number(H1_PHASE_ANGLE));
		calibrationLogWrite("H2 Mag Converted: " + QString::number(H2_MAG_ANGLE));
		calibrationLogWrite("H2 Phase Converted: " + QString::number(H2_PHASE_ANGLE));
		calibrationLogWrite("H3 Mag Converted: " + QString::number(H3_MAG_ANGLE));
		calibrationLogWrite("H3 Phase Converted: " + QString::number(H3_PHASE_ANGLE));
		calibrationLogWrite("H8 Mag Converted: " + QString::number(H8_MAG_ANGLE));
		calibrationLogWrite("H8 Phase Converted: " + QString::number(H8_PHASE_ANGLE));

		isCalculatedCoeff = true;

		displayCalculatedCoeff();
	}
	else{
		calibrationLogWrite("Unabled to flash Harmonic Registers!");
	}
}

void HarmonicCalibration::postCalibrateData()
{
	calibrationLogWrite("==== Post Calibration Start ====\n");
	canCalibrate(false);
	canStartMotor(false);
	calibrationDataGraphTabWidget->setCurrentIndex(1);
	isPostCalibration = true;
	isStartMotor = true;
	resetToZero = true;
	startMotor();
}

void HarmonicCalibration::updateCalculatedCoeffAngle()
{
	calibrationH1MagLabel->setText(QString::number(H1_MAG_ANGLE) + "°");
	calibrationH2MagLabel->setText(QString::number(H2_MAG_ANGLE) + "°");
	calibrationH3MagLabel->setText(QString::number(H3_MAG_ANGLE) + "°");
	calibrationH8MagLabel->setText(QString::number(H8_MAG_ANGLE) + "°");
	calibrationH1PhaseLabel->setText("Φ " + QString::number(H1_PHASE_ANGLE));
	calibrationH2PhaseLabel->setText("Φ " + QString::number(H2_PHASE_ANGLE));
	calibrationH3PhaseLabel->setText("Φ " + QString::number(H3_PHASE_ANGLE));
	calibrationH8PhaseLabel->setText("Φ " + QString::number(H8_PHASE_ANGLE));
}

void HarmonicCalibration::resetCalculatedCoeffAngle()
{
	calibrationH1MagLabel->setText("--.--°");
	calibrationH2MagLabel->setText("--.--°");
	calibrationH3MagLabel->setText("--.--°");
	calibrationH8MagLabel->setText("--.--°");
	calibrationH1PhaseLabel->setText("Φ --.--");
	calibrationH2PhaseLabel->setText("Φ --.--");
	calibrationH3PhaseLabel->setText("Φ --.--");
	calibrationH8PhaseLabel->setText("Φ --.--");
}

void HarmonicCalibration::updateCalculatedCoeffHex()
{
	calibrationH1MagLabel->setText(QString("0x%1").arg(H1_MAG_HEX, 4, 16, QChar('0')));
	calibrationH2MagLabel->setText(QString("0x%1").arg(H2_MAG_HEX, 4, 16, QChar('0')));
	calibrationH3MagLabel->setText(QString("0x%1").arg(H3_MAG_HEX, 4, 16, QChar('0')));
	calibrationH8MagLabel->setText(QString("0x%1").arg(H8_MAG_HEX, 4, 16, QChar('0')));
	calibrationH1PhaseLabel->setText(QString("0x%1").arg(H1_PHASE_HEX, 4, 16, QChar('0')));
	calibrationH2PhaseLabel->setText(QString("0x%1").arg(H2_PHASE_HEX, 4, 16, QChar('0')));
	calibrationH3PhaseLabel->setText(QString("0x%1").arg(H3_PHASE_HEX, 4, 16, QChar('0')));
	calibrationH8PhaseLabel->setText(QString("0x%1").arg(H8_PHASE_HEX, 4, 16, QChar('0')));
}

void HarmonicCalibration::resetCalculatedCoeffHex()
{
	calibrationH1MagLabel->setText("0x----");
	calibrationH2MagLabel->setText("0x----");
	calibrationH3MagLabel->setText("0x----");
	calibrationH8MagLabel->setText("0x----");
	calibrationH1PhaseLabel->setText("0x----");
	calibrationH2PhaseLabel->setText("0x----");
	calibrationH3PhaseLabel->setText("0x----");
	calibrationH8PhaseLabel->setText("0x----");
}

void HarmonicCalibration::displayCalculatedCoeff()
{
	if(isAngleDisplayFormat){
		if(isCalculatedCoeff){
			updateCalculatedCoeffAngle();
		}
		else{
			resetCalculatedCoeffAngle();
		}
	}
	else{
		if(isCalculatedCoeff){
			updateCalculatedCoeffHex();
		}
		else{
			resetCalculatedCoeffHex();
		}
	}
}

void HarmonicCalibration::calibrationLogWrite(QString message)
{
	logsPlainTextEdit->appendPlainText(message);
}

void HarmonicCalibration::commandLogWrite(QString message)
{
	commandLogPlainTextEdit->appendPlainText(message);
}

void HarmonicCalibration::extractCalibrationData()
{
	QStringList filter;
	filter += QString(tr("Comma-separated values files (*.csv)"));
	filter += QString(tr("Tab-delimited values files (*.txt)"));
	filter += QString(tr("All Files(*)"));

	QString selectedFilter = filter[0];

	QString fileName = QFileDialog::getSaveFileName(this, tr("Export"), "", filter.join(";;"), &selectedFilter, QFileDialog::Options());

	if(fileName.split(".").size() <= 1) {
		// file name w/o extension. Let's append it
		QString ext = selectedFilter.split(".")[1].split(")")[0];
		fileName += "." + ext;
	}

	if(!fileName.isEmpty()) {
		bool withScopyHeader = false;
		FileManager fm("HarmonicCalibration");
		fm.open(fileName, FileManager::EXPORT);

		QVector<double> preCalibrationAngleErrorsFFTMagnitude(m_admtController->angle_errors_fft_pre.begin(), m_admtController->angle_errors_fft_pre.end());
		QVector<double> preCalibrationAngleErrorsFFTPhase(m_admtController->angle_errors_fft_phase_pre.begin(), m_admtController->angle_errors_fft_phase_pre.end());

		QVector<double> h1Mag = { H1_MAG_ANGLE };
		QVector<double> h2Mag = { H2_MAG_ANGLE };
		QVector<double> h3Mag = { H3_MAG_ANGLE };
		QVector<double> h8Mag = { H8_MAG_ANGLE };
		QVector<double> h1Phase = { H1_PHASE_ANGLE };
		QVector<double> h2Phase = { H2_PHASE_ANGLE };
		QVector<double> h3Phase = { H3_PHASE_ANGLE };
		QVector<double> h8Phase = { H8_PHASE_ANGLE };

		fm.save(graphDataList, "Raw Data");
		fm.save(preCalibrationAngleErrorsFFTMagnitude, "Pre-Calibration Angle Errors FFT Magnitude");
		fm.save(preCalibrationAngleErrorsFFTPhase, "Pre-Calibration Angle Errors FFT Phase");
		fm.save(h1Mag, "H1 Mag");
		fm.save(h2Mag, "H2 Mag");
		fm.save(h3Mag, "H3 Mag");
		fm.save(h8Mag, "H8 Mag");
		fm.save(h1Phase, "H1 Phase");
		fm.save(h2Phase, "H2 Phase");
		fm.save(h3Phase, "H3 Phase");
		fm.save(h8Phase, "H8 Phase");

		fm.performWrite(withScopyHeader);
	}
}

void HarmonicCalibration::importCalibrationData()
{
	QString fileName = QFileDialog::getOpenFileName(
		this, tr("Import"), "",
		tr("Comma-separated values files (*.csv);;"
		   "Tab-delimited values files (*.txt)"),
		nullptr, QFileDialog::Options());

	FileManager fm("HarmonicCalibration");

	try {
		fm.open(fileName, FileManager::IMPORT);

		graphDataList = fm.read(0);
		if(graphDataList.size() > 0)
		{
			calibrationRawDataPlotChannel->curve()->setSamples(graphDataList);
			calibrationRawDataXPlotAxis->setInterval(0, graphDataList.size());
			calibrationRawDataPlotWidget->replot();

			computeSineCosineOfAngles(graphDataList);
			canStartMotor(false);
			canCalibrate(true);
			calibrationLogWrite(m_admtController->calibrate(vector<double>(graphDataList.begin(), graphDataList.end()), cycleCount, samplesPerCycle));
			flashHarmonicValues();
			populateAngleErrorGraphs();
		}
	} catch(FileManagerException &ex) {
		calibrationLogWrite(QString(ex.what()));
	}
}

void HarmonicCalibration::computeSineCosineOfAngles(QVector<double> graphDataList)
{
	m_admtController->computeSineCosineOfAngles(vector<double>(graphDataList.begin(), graphDataList.end()));
	if(isPostCalibration){
		postCalibrationSineDataPlotChannel->curve()->setSamples(m_admtController->calibration_samples_sine_scaled.data(), m_admtController->calibration_samples_sine_scaled.size());
		postCalibrationCosineDataPlotChannel->curve()->setSamples(m_admtController->calibration_samples_cosine_scaled.data(), m_admtController->calibration_samples_cosine_scaled.size());
		postCalibrationRawDataPlotWidget->replot();
	}
	else{
		calibrationSineDataPlotChannel->curve()->setSamples(m_admtController->calibration_samples_sine_scaled.data(), m_admtController->calibration_samples_sine_scaled.size());
		calibrationCosineDataPlotChannel->curve()->setSamples(m_admtController->calibration_samples_cosine_scaled.data(), m_admtController->calibration_samples_cosine_scaled.size());
		calibrationRawDataPlotWidget->replot();
	}
}

void HarmonicCalibration::stepMotorAcquisition(double step)
{
	target_pos = current_pos + step;
	writeMotorAttributeValue(ADMTController::MotorAttribute::TARGET_POS, target_pos);
	readMotorAttributeValue(ADMTController::MotorAttribute::CURRENT_POS, current_pos);
	while(target_pos != current_pos) {
		readMotorAttributeValue(ADMTController::MotorAttribute::CURRENT_POS, current_pos);
	}
}

void HarmonicCalibration::clearRawDataList()
{
	clearCalibrationSamples();

	graphPostDataList.clear();
	postCalibrationRawDataPlotChannel->curve()->setData(nullptr);
	postCalibrationSineDataPlotChannel->curve()->setData(nullptr);
	postCalibrationCosineDataPlotChannel->curve()->setData(nullptr);
	postCalibrationRawDataPlotWidget->replot();

	angleErrorPlotChannel->curve()->setData(nullptr);
	angleErrorPlotWidget->replot();
	FFTAngleErrorMagnitudeChannel->curve()->setData(nullptr);
	FFTAngleErrorPhaseChannel->curve()->setData(nullptr);
	FFTAngleErrorPlotWidget->replot();
	correctedErrorPlotChannel->curve()->setData(nullptr);
	correctedErrorPlotWidget->replot();
	FFTCorrectedErrorMagnitudeChannel->curve()->setData(nullptr);
	FFTCorrectedErrorPhaseChannel->curve()->setData(nullptr);
	FFTCorrectedErrorPlotWidget->replot();

	canCalibrate(false);
	canStartMotor(true);
	isPostCalibration = false;
	isCalculatedCoeff = false;
	resetToZero = true;
	displayCalculatedCoeff();
}

void HarmonicCalibration::clearCalibrationSamples()
{
	graphDataList.clear();
	calibrationRawDataPlotChannel->curve()->setData(nullptr);
	calibrationSineDataPlotChannel->curve()->setData(nullptr);
	calibrationCosineDataPlotChannel->curve()->setData(nullptr);
	calibrationRawDataPlotWidget->replot();
}

void HarmonicCalibration::applyTextStyle(QWidget *widget, const QString& styleHelperColor, bool isBold)
{
	QString existingStyle = widget->styleSheet();
	QString style = QString(R"css(
								font-family: Open Sans;
								font-size: 16px;
								font-weight: &&fontweight&&;
								text-align: right;
								color: &&colorname&&;
							)css");
	style = style.replace(QString("&&colorname&&"), StyleHelper::getColor(styleHelperColor));
	QString fontWeight = QString("normal");
	if(isBold){
		fontWeight = QString("bold");
	}
	style = style.replace(QString("&&fontweight&&"), fontWeight);
	widget->setStyleSheet(existingStyle + style);
}

void HarmonicCalibration::applyLabelStyle(QLabel *widget)
{
	applyTextStyle(widget);
	QString existingStyle = widget->styleSheet();
	QString style = QString(R"css(
								background-color: black;
								border-radius: 4px;
								border: none;
							)css");
	widget->setStyleSheet(existingStyle + style);
	widget->setFixedHeight(30);
	widget->setContentsMargins(12, 4, 12, 4);
}

void HarmonicCalibration::applyTabWidgetStyle(QTabWidget *widget, const QString& styleHelperColor)
{
	QString style = QString(R"css(
		QTabWidget::tab-bar {
		 left: 5px; /* move to the right by 5px */
		}
		QTabBar::tab {
		 min-width: 100px;
		 min-height: 32px;
		 padding-bottom: 5px;
		 padding-left: 16px;
		 padding-right: 16px;
		 background-color: &&UIElementBackground&&;
		 font: normal;
		}
		QTabBar::tab:selected {
		 color: white;
		 border-bottom: 2px solid &&ScopyBlue&&;
		 margin-top: 0px;
		}
		)css");
	style.replace("&&ScopyBlue&&", StyleHelper::getColor(styleHelperColor));
	style.replace("&&UIElementBackground&&", StyleHelper::getColor("UIElementBackground"));
	widget->tabBar()->setStyleSheet(style);
}